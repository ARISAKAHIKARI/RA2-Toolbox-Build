#!/usr/bin/env python3
"""Exercise production toast creation, visibility and status predicates at DWORD wrap."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
p.add_argument('--label', default='current')
a = p.parse_args()
source = Path(__file__).resolve().parents[1] / 'source/main.cpp'
text = source.read_text()
notify = re.search(r'static void notify\(const std::wstring&s\)\{[^\n]+', text).group()
helper = re.search(r'static bool activeToast\(DWORD now\)\{[^\n]+', text)
visible = re.search(r'bool hasToast=([^;]+);', text).group(1)
expired = re.search(r'if\((.*?)\)\{const auto p=externalSampler.probe\(\);', text).group(1)
clear = re.search(r'resetPlacementEvent\(\);(.*?)fillTech\(true\);', text).group(1)
code = r'''
#include <cstdint>
#include <string>
#include <iostream>
using DWORD=uint32_t;
static DWORD clockNow=0,toastUntil=0;static int toastMs=2000;
static bool toastActive=false;static std::wstring toast,status;
static DWORD GetTickCount(){return clockNow;}
static void setStatus(const std::wstring&s){status=s;}
'''
code += (helper.group() + '\n' if helper else '') + notify + '\n'
code += 'static bool visible(DWORD now){bool panel=true;return ' + visible + ';}\n'
code += 'static bool expired(DWORD now){return ' + expired + ';}\n'
code += 'static void clear(){' + clear + '}\n'
code += r'''
static int checks=0,failures=0;
static void check(bool value,const char*label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<"\n";}}
static void reset(){clockNow=toastUntil=0;toastActive=false;toast.clear();status.clear();toastMs=2000;}
int main(){
 reset();check(!visible(0)&&expired(0),"no toast at clock zero");
 reset();check(!visible(0x90000000)&&expired(0x90000000),"no toast must not suppress status after half wrap");
 reset();clockNow=100;notify(L"test");check(visible(100)&&!expired(100),"ordinary creation");check(visible(2099),"last active millisecond");check(!visible(2100)&&expired(2100),"exact expiry");
 reset();clockNow=0xfffffff0;notify(L"wrap");check(visible(clockNow)&&!expired(clockNow),"creation across tick wrap");check(visible(0x100)&&!expired(0x100),"visible after wrapped clock");check(!visible(0x7c0)&&expired(0x7c0),"wrapped expiry");
 reset();clockNow=0xfffff830;notify(L"zero deadline");check(toastUntil==0&&visible(clockNow)&&!expired(clockNow),"zero deadline is valid when active");check(visible(0xffffffff),"zero deadline last millisecond");check(!visible(0)&&expired(0),"zero deadline expires at zero");
 reset();clockNow=0xfffffff0;notify(L"reset");clear();check(!visible(clockNow)&&expired(clockNow),"match reset clears active notification");
 reset();clockNow=0xfffffff0;notify(L"first");clockNow=400;notify(L"replacement");check(visible(2200)&&status==L"replacement", "replacement receives full duration");
 reset();clockNow=100;notify(L"completed");check(expired(2100),"status observation retires toast");check(!visible(100)&&expired(100),"retired toast cannot reappear after full wrap");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
'''
out = Path(a.output)
out.mkdir(parents=True, exist_ok=True)
cpp = out / f'toast_clock_{a.label}.cpp'
cpp.write_text(code)
exe = cpp.with_suffix('')
runs = []
for i, cmd in enumerate([['g++', '-std=c++17', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(cpp), '-o', str(exe)], [str(exe)]]):
    r = subprocess.run(cmd, capture_output=True, text=True)
    (out / f'toast_clock_{a.label}_{i}.log').write_text(r.stdout + r.stderr)
    print(r.stdout + r.stderr, end='')
    runs.append(dict(command=cmd, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr))
    if r.returncode:
        break
receipt = dict(source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), test_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), predicates=dict(visible=visible, expired=expired, clear=clear), verification_level='controlled_execution_of_extracted_production_toast_logic', not_verified=['Windows painting and OS timer'], runs=runs)
(out / f'toast_clock_{a.label}.receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
raise SystemExit(0 if all(r['exit_code'] == 0 for r in runs) else 1)

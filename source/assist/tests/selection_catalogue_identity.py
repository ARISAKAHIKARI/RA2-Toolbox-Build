#!/usr/bin/env python3
"""Run the actual Host/Runtime selection functions with controlled IPC seams."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
p.add_argument('--label', default='current')
p.add_argument('--source-root', type=Path)
a = p.parse_args()
root = a.source_root or Path(__file__).resolve().parents[3]
paths = [root / 'source/assist/source' / name for name in ('main.cpp', 'bridge.cpp', 'protocol.h')]
host, runtime, protocol = [path.read_text() for path in paths]

def function(text, signature):
    start = text.index(signature)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

fragments = [function(host, 'static void serviceSelection()'), function(host, 'static void sendSelection('), function(runtime, 'static void selectionCommand('), function(protocol, 'inline uint64_t typeListHash(')]
code = r'''
#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>
#include <functional>
using DWORD=uint32_t; using LONG=int32_t;
constexpr int MaxTypes=512,SelectType=16;
struct Type {char id[24]; uint16_t name[40]; int kind,index,naval,cost;};
struct Snapshot {uint32_t match=1; int typeCount=2,chat=0;Type types[MaxTypes]{};};
// Only OS/IPC dependencies are controlled; the functions below are extracted verbatim.
struct Shared {volatile uint32_t selectRequest=0,selectAck=0; int selectIndex=0,selectGlobal=0;uint32_t selectObjectID=0,selectTick=0,selectMatch=0;int selectResult=0,selectCount=0;uint8_t selectTypes[MaxTypes]{};uint64_t selectTypeHash=0;};
namespace delivery {constexpr uint32_t UnknownMatch=0;struct Item{int command=0,arg=0,arg2=0;uint32_t pid=0,match=0,created=0;int retries=0;uint32_t notBefore=0,token=0;uint64_t typeHash=0;};}
static Shared mailbox; static Shared *shared=&mailbox,*sh=&mailbox;
static Snapshot snap;static DWORD clockNow=100;static uint32_t pid=77,match=1,selectionAck=0,selectionLatency=0,pendingSelectionObject=0;
static bool hasSelection=false,enabled=true,diagEnabled=false,focused=true,fresh=true,chat=false;
static uint8_t pendingSelectionTypes[MaxTypes]{};static delivery::Item pendingSelection;
static int selectedCalls=0,selectedIndex=-100,wakeCalls=0,notifications=0;
static std::function<void()> connectAction;
static DWORD GetTickCount(){return clockNow;}
static bool commandFocus(){return focused;}
static bool commandFresh(DWORD){return fresh;}
static uint32_t commandMatch(){return snap.match;}
static void MemoryBarrier(){}
static void InterlockedIncrement(volatile LONG*p){++*p;}
static void notify(const std::wstring&){++notifications;}
static void diag(const std::wstring&){}
static void wakeGame(){++wakeCalls;}
static void ensureBridge(){if(connectAction)connectAction();shared=&mailbox;}
template<class T>static T get(uint32_t){return T(chat);}
static int selectType(uint32_t,int index,bool){++selectedCalls;selectedIndex=index;return 1;}
static int selectTypes(uint32_t,const uint8_t*mask,bool){++selectedCalls;for(int i=0;i<MaxTypes;++i)if(mask[i]){selectedIndex=i;break;}return 1;}
static int selectVariant(uint32_t,bool){++selectedCalls;selectedIndex=-2;return 1;}
static int selectIdleAircraft(uint32_t,uint32_t){++selectedCalls;selectedIndex=-4;return 1;}
'''
code += fragments[3] + '\n' + '\n'.join(fragments[:3])
code += r'''
static int checks=0,failures=0;
static void check(bool value,const char*label){++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<"\n";}}
static void reset(){mailbox={};shared=sh=&mailbox;snap={};strcpy(snap.types[0].id,"A");strcpy(snap.types[1].id,"B");snap.types[1].index=1;pid=77;match=1;clockNow=100;selectionAck=selectionLatency=pendingSelectionObject=0;hasSelection=false;enabled=focused=fresh=true;diagEnabled=chat=false;selectedCalls=wakeCalls=notifications=0;selectedIndex=-100;pendingSelection={};memset(pendingSelectionTypes,0,sizeof pendingSelectionTypes);connectAction={};}
static void reorder(){std::swap(snap.types[0],snap.types[1]);}
int main(){
 reset();sendSelection(0,false);selectionCommand(1,clockNow);check(selectedCalls==1&&sh->selectResult==1,"unchanged indexed selection accepted");
 reset();focused=false;sendSelection(0,false);check(hasSelection&&mailbox.selectRequest==0,"unfocused request remains bounded pending");reorder();focused=true;serviceSelection();check(!hasSelection&&mailbox.selectRequest==0,"Host rejects pending selection after catalogue reorder");
 reset();sendSelection(0,false);reorder();selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7&&sh->selectAck==sh->selectRequest,"Runtime rejects published index after reorder and acknowledges");
 reset();uint8_t mask[MaxTypes]{};mask[1]=1;sendSelection(-3,true,mask);reorder();selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7,"Runtime rejects stale union mask");
 reset();sendSelection(-3,true,mask);check(sh->selectTypes[1]==1&&sh->selectGlobal==1,"mask and scope preserved");selectionCommand(1,clockNow);check(selectedCalls==1&&selectedIndex==1,"unchanged union selection accepted");
 reset();sendSelection(-2,true);reorder();selectionCommand(1,clockNow);check(selectedCalls==1&&selectedIndex==-2,"variant selection independent of catalogue");
 reset();sendSelection(-4,false,nullptr,999);reorder();selectionCommand(1,clockNow);check(selectedCalls==1&&selectedIndex==-4&&sh->selectObjectID==999,"aircraft UID independent of catalogue");
 reset();sendSelection(0,false);match=2;selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7,"Runtime rejects other match");
 reset();sendSelection(0,false);clockNow+=1501;selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7,"Runtime rejects expired intent");
 reset();sendSelection(0,false);chat=true;selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-8,"Runtime rejects active chat");
 reset();focused=false;sendSelection(0,false);snap.match=2;focused=true;serviceSelection();check(!hasSelection&&sh->selectRequest==0,"Host rejects changed match");
 reset();focused=false;sendSelection(0,false);pid=88;focused=true;serviceSelection();check(!hasSelection&&sh->selectRequest==0,"Host rejects changed process");
 reset();focused=false;sendSelection(0,false);clockNow+=1501;focused=true;serviceSelection();check(!hasSelection&&sh->selectRequest==0,"Host rejects expired pending intent");
 reset();focused=false;sendSelection(0,false);clockNow+=1490;focused=true;serviceSelection();clockNow+=11;selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7,"publication preserves original input age");
 reset();shared=nullptr;connectAction=[](){reorder();};sendSelection(0,false);check(!hasSelection&&mailbox.selectRequest==0,"connection refresh cannot retag original catalogue");
 reset();snap.match=0;sendSelection(0,false);snap.match=1;serviceSelection();check(!hasSelection&&mailbox.selectRequest==0,"unknown match cannot acquire later selection identity");
 reset();enabled=false;sendSelection(0,false);check(mailbox.selectRequest==0&&!hasSelection,"disabled feature does not submit");
 reset();clockNow=0xffffff00;sendSelection(0,false);clockNow=0x100;selectionCommand(1,clockNow);check(selectedCalls==1,"age survives tick wrap");
 reset();sendSelection(0,false);sh->selectTypeHash=0;selectionCommand(1,clockNow);check(selectedCalls==0&&sh->selectResult==-7,"missing catalogue identity is rejected");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
'''
out = Path(a.output)
out.mkdir(parents=True, exist_ok=True)
cpp = out / f'selection_catalogue_{a.label}.cpp'
cpp.write_text(code)
exe = cpp.with_suffix('')
commands = [['g++', '-std=c++17', '-D_GLIBCXX_ASSERTIONS', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(cpp), '-o', str(exe)], [str(exe)]]
runs = []
for i, cmd in enumerate(commands):
    r = subprocess.run(cmd, capture_output=True, text=True)
    (out / f'selection_catalogue_{a.label}_{i}.log').write_text(r.stdout + r.stderr)
    print(r.stdout + r.stderr, end='')
    runs.append(dict(command=cmd, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr))
    if r.returncode:
        break
record = dict(sources={str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}, test_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), verification_level='controlled_execution_of_extracted_Host_and_Runtime_functions', not_verified=['Windows shared memory ABI execution', 'game selection and camera behavior'], runs=runs)
(out / f'selection_catalogue_{a.label}.receipt.json').write_text(json.dumps(record, indent=2) + '\n')
raise SystemExit(0 if all(r['exit_code'] == 0 for r in runs) else 1)

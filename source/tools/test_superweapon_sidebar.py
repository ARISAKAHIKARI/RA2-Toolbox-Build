#!/usr/bin/env python3
"""Compile actual detached-panel model and event handlers; never attach a game."""
import argparse,hashlib,json,os,pathlib,re,subprocess,time

def main():
 p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 source=pathlib.Path(__file__).resolve().parents[1];out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
 header=source/'assist/source/superweapon_sidebar_host.h';text=header.read_text(encoding='utf-8')
 assert text.count('class Panel {')==1
 # Only access control and relative include spelling differ from production.
 text=text.replace('class Panel {','class Panel {\npublic:',1)
 text=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+(header.parent/m.group(1)).resolve().as_posix()+'"',text)
 (out/'superweapon_sidebar_test_access.h').write_text(text,encoding='utf-8')
 results=[]
 for arch,target in [('x64','x86_64-windows-gnu'),('x86','x86-windows-gnu')]:
  for name in ['superweapon_sidebar_model','superweapon_sidebar_windows']:
   exe=out/(name+'-'+arch+'.exe');cmd=[str(pathlib.Path(a.zig).resolve()),'c++','-target',target,'-std=c++17','-O2','-UNDEBUG','-I'+str(out),str(source/'assist/tests'/ (name+'.cpp')),'-o',str(exe)]
   if name.endswith('windows'):cmd+=['-lgdiplus','-lgdi32','-luser32','-lkernel32']
   start=time.monotonic();built=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-'+arch+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8');assert built.returncode==0,built.stderr
   run=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'-'+arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');assert run.returncode==0,(name,arch,run.returncode,run.stderr)
   results.append({'fixture':name,'arch':arch,'passed':True,'seconds':round(time.monotonic()-start,3),'output':run.stdout.strip()});print(run.stdout.strip(),arch,flush=True)
 files=['assist/source/superweapon_sidebar_host.h','assist/source/superweapon_sidebar_model.h','assist/source/superweapon_sidebar_bridge.h','assist/tests/superweapon_sidebar_model.cpp','assist/tests/superweapon_sidebar_windows.cpp']
 report={'scope':'Actual host Panel::proc/update with mocked Win32, actual model and Runtime cancel helper; no live game and no GPU execution','results':results,'source_sha256':{f:hashlib.sha256((source/f).read_bytes()).hexdigest() for f in files}}
 (out/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8');print('PASS',out/'result.json')
if __name__=='__main__':main()

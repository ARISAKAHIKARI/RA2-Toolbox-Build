"""Build/run the exact product MSVC x86 SEH boundaries in an own-process fixture."""
import argparse,hashlib,json,os,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--build-dir',required=True);p.add_argument('--output',required=True);p.add_argument('--repeat',type=int,default=20);a=p.parse_args();assert 1<=a.repeat<=10000
repo=Path(__file__).resolve().parents[3];test=Path(__file__).resolve().parent;build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True);zig=str(Path(a.zig).resolve());obj=build/'sidebar-view-seh.o';exe=build/'sidebar-view-seh.exe'
files=['source/assist/tests/sidebar_view_guard_windows.py','source/assist/tests/sidebar_view_guard_seh_boundary.cpp','source/assist/tests/sidebar_view_guard_seh.cpp','source/assist/source/sidebar_view_guard.h','source/assist/source/view_zoom_native_guard.h'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={f:sha(repo/f)for f in files}
commands=[[zig,'c++','-target','x86-windows-msvc','-nostdlib','-nostdinc','-O2','-fms-extensions','-fno-stack-protector','-c',str(test/'sidebar_view_guard_seh_boundary.cpp'),'-o',str(obj)],[zig,'c++','-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-Wl,--image-base,0x400000',str(test/'sidebar_view_guard_seh.cpp'),str(obj),'-o',str(exe)]]
records=[];image_policy={};runs=[]
for cmd in commands:
 r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);records.append({'command':cmd,'exit_code':r.returncode,'stdout':r.stdout});print(r.stdout,flush=True)
 if r.returncode:break
if all(r['exit_code']==0 for r in records)and len(records)==2:
 # Zig 0.13's GNU driver does not accept /dynamicbase:no. Clear only the
 # test executable's IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE bit explicitly.
 # The product DLL/headers are untouched. Its 4MB BSS arena owns 0x692300
 # before main/CRT allocation; runtime asserts verify actual address/owner.
 raw=bytearray(exe.read_bytes());pe=struct.unpack_from('<I',raw,60)[0];optional=pe+24
 assert raw[:2]==b'MZ' and raw[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',raw,pe+4)[0]==0x14c and struct.unpack_from('<H',raw,optional)[0]==0x10b
 base=struct.unpack_from('<I',raw,optional+28)[0];size=struct.unpack_from('<I',raw,optional+56)[0];dllflags=struct.unpack_from('<H',raw,optional+70)[0]
 assert base==0x400000 and base+size>0x692305
 struct.pack_into('<H',raw,optional+70,dllflags&~0x40);exe.write_bytes(raw)
 image_policy={'scope':'own fixture executable only; production code and guard bytes unchanged','base':base,'size_of_image':size,'absolute_query':0x692300,'arena_bytes':0x400000,'dll_characteristics_before':dllflags,'dll_characteristics_after':dllflags&~0x40,'loader_owned_BSS_query_arena':True,'allocation_retry_used':False}
 for index in range(a.repeat):
  r=subprocess.run([str(exe)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);runs.append({'run':index+1,'exit_code':r.returncode,'stdout':r.stdout})
  if r.returncode:break
 record={'command':[str(exe)],'exit_code':0 if len(runs)==a.repeat and all(x['exit_code']==0 for x in runs)else runs[-1]['exit_code'],'stdout':runs[-1]['stdout'],'fresh_process_runs':len(runs)};records.append(record);print(record['stdout'],flush=True);print('Fresh process repetitions:',len(runs),'/',a.repeat,flush=True)
assert before=={f:sha(repo/f)for f in files},'sources changed during test'
checks=['boundary19 normal result, real AV, nested Capture and LIFO finally','boundary19 rejected Capture and LastError preservation','boundary19 modeled uncertain geometry keeps wide allocations committed','boundary21 actual thiscall six-argument query stub success and RET24','boundary21 real AV, query depth finally, nested query AV and LIFO','boundary21 query rejection does not execute engine stub','boundary22 normal, action AV and both unlock-finally invocations','boundary22 unlock AV still ends Capture and executes remaining unlock','pure Wnd Enter/Leave and mouse guards preserve LastError and catch AV']
result={'pass':len(records)==3 and all(r['exit_code']==0 for r in records),'scope':'Real Windows x86 MSVC SEH from exact production headers; model Capture/native callbacks and loader-owned fixture BSS executable stub at 0x692300. No game process, PE execution, visual rendering or netplay.','windows_test_process_executed':len(records)==3,'game_process_executed':False,'assertions_enabled':True,'source_sha256':before,'source_hash_stable':True,'object_sha256':sha(obj)if obj.exists()else None,'program_sha256':sha(exe)if exe.exists()else None,'checks':checks,'commands':records,'fixture_image_policy':image_policy,'repeat_requested':a.repeat,'process_runs':runs}
out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');raise SystemExit(not result['pass'])

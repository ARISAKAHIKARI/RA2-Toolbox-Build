#!/usr/bin/env python3
"""Run the actual manager DLL: every first client must protect its live thread."""
import argparse, importlib.util, json, hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
rows=[]
for owner in (1,2,3):
 spec=importlib.util.spec_from_file_location('binding_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(args.game),str(args.dll)];spec.loader.exec_module(m)
 modules={1:m.dll.OPTIONAL_HEADER.ImageBase,2:m.cursor_dll.OPTIONAL_HEADER.ImageBase,3:m.monitor_dll.OPTIONAL_HEADER.ImageBase}
 try:
  q=m.request(0,owner,modules[owner]);q.bindThread=1
  result,q=m.manager_call(q);assert result==1
  q.operation=1;assert m.manager_call(q)[0]==1
  m.current_thread=778
  a=m.request(0);a.bindThread=1
  result,_=m.manager_call(a);assert result==-2,f'live binding stolen: owner={owner}, result={result}'
  result,state=m.manager_call(m.request(3));assert result==1 and state.gameThread==777
  m.dead_threads.add(777)
  result,a=m.manager_call(a);assert result==1
  a.operation=1;assert m.manager_call(a)[0]==1
  # All three modules remain usable on the migrated, confirmed game thread.
  for client in (1,2,3):
   q=m.request(0,client,modules[client]);result,q=m.manager_call(q);assert result==1
   q.operation=1;assert m.manager_call(q)[0]==1
  rows.append({'initial_client':owner,'status':'PASS'})
 except AssertionError as e:rows.append({'initial_client':owner,'status':'FAIL','error':str(e)})
 print(rows[-1],flush=True)
args.output.write_text(json.dumps({'scope':'actual compiled manager in x86 emulator; mocked Win32 thread lifecycle','inputs':{n:hashlib.sha256(args.dll.with_name(n).read_bytes()).hexdigest() for n in ('GameRuntime.dll',)},'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

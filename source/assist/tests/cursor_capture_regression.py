#!/usr/bin/env python3
"""Real patched cursor predicate must use the current capture decision only."""
import argparse,importlib.util,json,hashlib,ctypes as C
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args();rows=[]
for case in ('foreign_owner','wrong_thread','key_released'):
 spec=importlib.util.spec_from_file_location('cursor_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(args.game),str(args.dll)];spec.loader.exec_module(m)
 try:
  m.hook();s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
  s.flags=131072;s.csHoldKey=67;m.u.mem_write(m.SH,bytes(s));m.held_keys.add(67);m.hook()
  original=bytes.fromhex('51a058feb000')
  trampoline=next(m.HEAP+0x3e0000+i*4096 for i in range(m.va_count) if bytes(m.u.mem_read(m.HEAP+0x3e0000+i*4096,6))==original)
  original_calls=[]
  def resumed(uc,a,size,_):original_calls.append(a);m.ret(0,0)
  m.u.hook_add(UC_HOOK_CODE,resumed,begin=trampoline,end=trampoline)
  m.run(0x731bf0);assert m.u.reg_read(UC_X86_REG_EAX)&255==1 and not original_calls
  lease=None
  if case=='foreign_owner':
   q=m.request(0,2,m.cursor_dll.OPTIONAL_HEADER.ImageBase);result,lease=m.manager_call(q);assert result==1
  elif case=='wrong_thread':m.current_thread=778
  else:m.held_keys.clear()
  m.u.reg_write(UC_X86_REG_EAX,0);m.run(0x731bf0)
  assert len(original_calls)==1,'denied/non-forcing capture reused prior override; original predicate skipped'
  assert m.u.reg_read(UC_X86_REG_EAX)&255==0
  if lease:lease.operation=1;assert m.manager_call(lease)[0]==1
  rows.append({'case':case,'status':'PASS'})
 except AssertionError as e:rows.append({'case':case,'status':'FAIL','error':str(e)})
 print(rows[-1],flush=True)
args.output.write_text(json.dumps({'scope':'actual Assist detour/guard; original predicate is a counted return stub','inputs':{n:hashlib.sha256(args.dll.with_name(n).read_bytes()).hexdigest() for n in ('GameRuntime.dll',)},'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

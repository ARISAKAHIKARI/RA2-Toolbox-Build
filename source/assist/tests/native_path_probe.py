"""Original EXE branch probes with synthetic objects; not gameplay/netplay or planning exploit reproduction."""
import importlib.util,sys,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
import argparse
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);args=p.parse_args()
base=Path(__file__).resolve().parents[2]
game=Path(args.game);dll=Path(args.dll)
def fixture():
 spec=importlib.util.spec_from_file_location('f',base/'assist/tests/entry_runtime_regression.py');m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(game),str(dll)];spec.loader.exec_module(m);return m
rows=[]
for capability in (0,1):
 m=fixture();m.b(m.engT+0xec2,capability)
 vt=m.alloc(0x600);m.w(m.eng,vt);fn=m.STUB+0xe000;m.w(vt+0x184,fn)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:m.ret(17,0),begin=fn,end=fn)
 reached=[]
 def stop(uc,a,size,_):reached.append(a);uc.reg_write(UC_X86_REG_EIP,m.STOP)
 for at in (0x51a4e6,0x51a7f4):m.u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at)
 m.u.reg_write(UC_X86_REG_ESI,m.eng);m.u.reg_write(UC_X86_REG_ESP,m.STACK+0x1e000);m.u.emu_start(0x51a4bf,m.STOP,count=1000)
 assert reached==[0x51a4e6 if capability else 0x51a7f4],reached
 rows.append({'case':'native_sabotage_gate','C4':capability,'branch':hex(reached[0]),'status':'PASS'})
for limit in (0,1):
 m=fixture();m.execute_native_find_factory=True;m.canbuild_full_result=0;m.canbuild_limit_result=limit
 vt=m.alloc(0x100);bt=m.alloc(0x100);m.w(m.t,vt);m.w(m.buildT,bt)
 for table,kind in ((vt,40),(bt,7)):
  for offset,val in ((0x2c,kind),(0x70,1)):
   fn=m.STUB+0xe100+(table-vt)*2+offset;m.w(table+offset,fn);m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_,value=val:m.ret(value,0),begin=fn,end=fn)
 m.w(vt+0x94,0x5f7900);m.w(m.buildT+0xeb8,40);m.b(m.building+0x660,1);m.b(m.building+0x3d3,1);m.vector(m.house+0x68,[m.building])
 # Docking helper is unrelated to technology: fixture says no dock special case.
 m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:m.ret(0,0),begin=0x65ae30,end=0x65ae30)
 m.run(0x5f7900,(0,1,1,m.house),m.t)
 assert m.u.reg_read(UC_X86_REG_EAX)==(m.building if limit else 0)
 assert m.canbuild_calls and all(call[2:]==(1,1) for call in m.canbuild_calls),m.canbuild_calls
 rows.append({'case':'native_FindFactory_same_arguments_as_BeginProduction','full_prereq_result':0,'limit_result':limit,'buildLimitOnly_args':[list(c[2:]) for c in m.canbuild_calls],'status':'PASS'})
Path(args.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')
for r in rows:print(r)

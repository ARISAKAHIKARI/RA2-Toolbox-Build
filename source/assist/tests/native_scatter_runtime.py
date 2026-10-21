"""Execute the EXE's Scatter receiver and UnitClass Scatter/PrimaryFacing gates.
Object resolution, COM locomotor, map lookup and final SetDestination are mocked.
This verifies dispatch/guards, not live pathfinding, bomb avoidance or networking.
"""
import argparse,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('base',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec);args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args

def check(name,fn):
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()

def execute(case):
 m=n.fixture(3);m.b(m.unit+0x14,7);m.b(m.unit+0x90,1);m.b(m.unit+0x81,0);m.b(m.unit+0x418,0)
 m.w(m.unit+0x5a4,0);m.w(m.unit+0xb4,0xffffffff)
 packet=m.alloc();m.b(packet,7);m.b(packet+2,0);m.w(packet+7,10);m.b(packet+11,0x34)
 if case=='dead':m.b(m.unit+0x90,0)
 if case=='limbo':m.b(m.unit+0x81,1)
 if case=='not_foot':m.b(m.unit+0x14,0)
 if case=='rotating':
  m.u.mem_write(m.unit+0x39c,struct.pack('<h',8));m.w(m.unit+0x390,m.r(0xa8ed84));m.w(m.unit+0x398,30)
 if case=='destination':m.w(m.unit+0x5a4,m.crateCell)
 if case=='deployed':m.b(m.unit+0x6e0,1)
 if case=='expired_turn':
  m.u.mem_write(m.unit+0x39c,struct.pack('<h',8));m.w(m.unit+0x390,m.r(0xa8ed84)-30);m.w(m.unit+0x398,30)
 loco=m.alloc();vt=m.alloc();m.w(loco,vt);m.w(m.unit+0x674,loco)
 getcls=m.STUB+0xe200;release=m.STUB+0xe210;canmove=m.STUB+0xe220;m.w(vt+12,getcls);m.w(vt+8,release);m.w(vt+0x60,canmove)
 mission=m.alloc();m.b(mission+9,1);calls=[];destinations=[];maps=[]
 def arg(i=0):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+i*4)
 def stub(uc,at,size,_):
  if at==0x6e6f20:m.ret(m.unit,0)
  elif at==0x6386e0:m.ret(0,0)
  elif at==0x743a50:
   calls.append((arg(),arg(1),arg(2)));assert (arg(),arg(1),arg(2))==(0x8a39b8,1,0)
   assert m.r(m.unit+0x6b2)&255==1
  elif at==m.r(0x7f5c70+0x28c):m.ret(1,0)
  elif at==0x45aea0:m.w(m.u.reg_read(UC_X86_REG_ECX),loco);m.ret(0,1)
  elif at==getcls:m.u.mem_write(arg(1),bytes(16));m.ret(0,2)
  elif at==release:m.ret(0,1)
  elif at==canmove:m.ret(1,1)
  elif at==0x5b3a00:m.ret(mission,0)
  elif at==m.r(0x7f5c70+0x4c):m.u.mem_write(arg(),struct.pack('<iii',10000,10000,0));m.ret(arg(),2)
  elif at==0x56dc20:
   maps.append(1);out=arg();m.u.mem_write(out,struct.pack('<hh',40,39));m.ret(out,15)
  elif at==0x5657a0:m.ret(m.crateCell,1)
  elif at==m.r(0x7f5c70+0x480):destinations.append((arg(),arg(1)));m.ret(0,2)
 for at in (0x6e6f20,0x6386e0,0x743a50,m.r(0x7f5c70+0x28c),0x45aea0,getcls,release,canmove,0x5b3a00,m.r(0x7f5c70+0x4c),0x56dc20,0x5657a0,m.r(0x7f5c70+0x480)):
  m.u.hook_add(UC_HOOK_CODE,stub,begin=at,end=at)
 m.run(0x4c6cb0,(),packet)
 assert len(calls)==int(case not in ('dead','limbo','not_foot'))
 assert len(destinations)==int(case in ('idle','expired_turn')),(case,calls,destinations,maps)
 if destinations:assert destinations==[(m.crateCell,1)]
 assert not m.codeWrites
for case in ('idle','rotating','expired_turn','destination','deployed','dead','limbo','not_foot'):check(case,lambda case=case:execute(case))
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))

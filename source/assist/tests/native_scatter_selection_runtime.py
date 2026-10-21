"""Run supplied EXE's vehicle/infantry Scatter direction and cell-selection code.
The actual angle lookup, facing conversion, neighbour loop and state branches run.
World queries, locomotor, RNG result and final mission/destination writes are mocked.
This is not live pathfinding, projectile physics or multiplayer validation.
No game executable or extracted instruction bytes are included in this test.
"""
import argparse,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
spec=importlib.util.spec_from_file_location('base',Path(__file__).with_name('no_interception_runtime.py'))
n=importlib.util.module_from_spec(spec);args=sys.argv
sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))]
spec.loader.exec_module(n);sys.argv=args
DIRS=((0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1))
rows=[]

class Probe:
 def __init__(self,infantry=False):
  self.infantry=infantry;m=self.m=n.fixture(3)
  self.actor=m.eng if infantry else m.unit;self.vtable=m.r(self.actor)
  self.entry=0x51d0d0 if infantry else 0x743a50
  self.source=m.alloc();self.mission=m.alloc();m.b(self.mission+9,1)
  self.loco=m.alloc();vt=m.alloc();m.w(self.loco,vt);m.w(self.actor+0x674,self.loco)
  self.getcls=m.STUB+0xe200;self.release=m.STUB+0xe210;self.canmove=m.STUB+0xe220;self.locomoving=m.STUB+0xe230;self.stop=m.STUB+0xe240
  for offset,fn in ((12,self.getcls),(8,self.release),(0x60,self.canmove),(0x10,self.locomoving),(0x40,self.stop)):m.w(vt+offset,fn)
  m.u.mem_write(self.actor+0x9c,struct.pack('<iii',40*256+128,40*256+128,0))
  if infantry:m.w(self.actor+0x6c4,0xffffffff)
  m.b(m.r(0x8871e0)+0x17ed,1)
  # Execute the EXE's static initializer, rather than inventing direction data.
  m.run(0x49f2f0)
  assert tuple(struct.unpack_from('<hh',m.u.mem_read(0x89f688,32),i*4) for i in range(8))==DIRS
  self.cells={};self.by_pointer={}
  for x in range(38,43):
   for y in range(38,43):
    cell=m.alloc();m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));self.cells[x,y]=cell;self.by_pointer[cell]=(x,y)
  self.virtuals={name:m.r(self.vtable+offset) for name,offset in [('allowed',0x28c),('kind',0x2c),('coords',0x4c),('height',0xbc),('enter',0x1ac),('mission',0x1e8),('destination',0x480),('center',0x48)]}
  targets=set(self.virtuals.values())|{0x45aea0,self.getcls,self.release,self.canmove,self.locomoving,self.stop,0x5b3a00,0x65c7e0,0x5657a0,0x578460,0x5f5f00,0x6d6410,0x56dc20,0x50b730}
  for at in targets:m.u.hook_add(UC_HOOK_CODE,self.stub,begin=at,end=at)

 def stub(self,uc,at,size,_):
  m=self.m;arg=lambda i=0:m.r(m.u.reg_read(UC_X86_REG_ESP)+4+i*4);v=self.virtuals
  if at==v['allowed']:m.ret(1,0)
  elif at==v['kind']:m.ret(15 if self.infantry else 30,0)
  elif at==0x45aea0:m.w(m.u.reg_read(UC_X86_REG_ECX),self.loco);m.ret(0,1)
  elif at==self.getcls:m.u.mem_write(arg(1),bytes(16));m.ret(0,2)
  elif at in (self.release,self.stop):m.ret(0,1)
  elif at==self.canmove:m.ret(1,1)
  elif at==self.locomoving:m.ret(0,1)
  elif at==0x50b730:m.ret(1,0)
  elif at==0x5b3a00:m.ret(self.mission,0)
  elif at==0x65c7e0:
   self.ranges.append((arg(),arg(1)));assert arg()<=self.random<=arg(1)
   m.ret(self.random,2)
  elif at in (v['coords'],v['center']):
   out=arg();m.u.mem_write(out,bytes(m.u.mem_read(self.actor+0x9c,12)));m.ret(out,2 if at==v['coords'] else 1)
  elif at==v['height']:m.ret(0,1)
  elif at==0x5657a0:
   coord=struct.unpack('<hh',m.u.mem_read(arg(),4));m.ret(self.cells[coord],1)
  elif at==0x578460:m.ret(1,2)
  elif at==0x5f5f00:m.ret(0,0)
  elif at==v['enter']:
   coord=self.by_pointer[arg()];direction=arg(1);self.attempts.append(direction)
   assert coord==(40+DIRS[direction][0],40+DIRS[direction][1])
   m.ret(int(direction in self.blocked),5)
  elif at==0x6d6410:
   out=arg();x,y,z=struct.unpack('<iii',m.u.mem_read(arg(1),12));coord=(x//256,y//256)
   if self.project_mismatch:coord=(0,0)
   m.u.mem_write(out,struct.pack('<hh',*coord));m.ret(out,2)
  elif at==0x56dc20:
   self.nearby+=1;out=arg();m.u.mem_write(out,struct.pack('<hh',41,40));m.ret(out,15)
  elif at==v['mission']:self.missions.append((arg(),arg(1)));m.ret(1,2)
  elif at==v['destination']:self.destinations.append(self.by_pointer[arg()]);m.ret(0,2)

 def run(self,direction=0,random=None,blocked=(),default=False,rotating=False,destination=False,project_mismatch=False,flagged=()):
  m=self.m;self.random=(2 if self.infantry else 1) if random is None else random
  self.blocked=set(blocked);self.project_mismatch=project_mismatch
  self.ranges=[];self.attempts=[];self.missions=[];self.destinations=[];self.nearby=0
  for xy,cell in self.cells.items():
   delta=(xy[0]-40,xy[1]-40);m.w(cell+0x140,0x100 if delta in [DIRS[d] for d in flagged] else 0)
  m.w(self.actor+0x5a4,self.cells[41,40] if destination else 0)
  m.u.mem_write(self.actor+0x39c,struct.pack('<h',8 if rotating else 0));m.w(self.actor+0x390,m.r(0xa8ed84));m.w(self.actor+0x398,30 if rotating else 0)
  dx,dy=DIRS[direction];coord=(40*256+128+dx*1024,40*256+128+dy*1024,2048)
  m.u.mem_write(self.source,struct.pack('<iii',*( (0,0,0) if default else coord)))
  m.run(self.entry,(self.source,1,0),self.actor)
  assert not m.codeWrites
  return {'attempts':self.attempts,'destinations':self.destinations,'rng_ranges':self.ranges,'nearby':self.nearby,'missions':self.missions}

def check(name,fn):
 try:
  detail=fn();rows.append({'case':name,'status':'PASS','detail':detail});print('PASS',name,flush=True)
 except Exception as e:
  rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()

for infantry in (False,True):
 q=Probe(infantry);name='infantry' if infantry else 'vehicle';spread=2 if infantry else 1
 for direction in range(8):
  for random in range(spread*2+1):
   def directional(direction=direction,random=random):
    r=q.run(direction,random);expected=(direction+4+random-spread)&7
    assert r['attempts']==[expected],r
    assert r['destinations']==[(40+DIRS[expected][0],40+DIRS[expected][1])],r
    assert r['rng_ranges']==[(0,spread*2)] and r['missions']==[(2,0)] and r['nearby']==0,r
    return r
   check(f'{name}_source_{direction}_random_{random}',directional)
 def obstacle():
  r=q.run(0,blocked=(4,5,6));assert r['attempts']==[4,5,6,7] and r['destinations']==[(39,39)],r;return r
 check(f'{name}_blocked_clockwise',obstacle)
 def blocked():
  r=q.run(0,blocked=range(8));assert r['attempts']==[4,5,6,7,0,1,2,3] and not r['destinations'],r;return r
 check(f'{name}_all_blocked',blocked)
 def fallback():
  r=q.run(0,project_mismatch=True);assert len(r['attempts'])==8 and r['destinations']==[(40,41)],r;return r
 check(f'{name}_fallback_first_passable',fallback)
 def flag():
  r=q.run(0,flagged=(4,));assert r['attempts']==[4,5] and r['destinations']==[(39,41)],r;return r
 check(f'{name}_flag_100_prefers_other',flag)
 def defaults():
  r=q.run(default=True);assert r['nearby']==1 and not r['attempts'] and r['destinations']==[(41,40)],r;return r
 check(f'{name}_default_uses_nearby',defaults)
 for gate in ('rotating','destination'):
  def guard(gate=gate):
   r=q.run(**{gate:True});assert bool(r['destinations'])==infantry,r;return r
  check(f'{name}_{gate}_gate',guard)

Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

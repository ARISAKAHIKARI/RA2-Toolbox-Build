"""Shipped x86 Runtime -> native Move constructor -> SP/MP queues.
Synthetic bullets, world, Win32 and passability boundary; no game physics/netplay.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('helper',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
DIRECTIONS=((1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1),(0,-1),(1,-1))
def setup(mode,case,v):
 m=n.fixture(mode);m.w(m.unit+0x194,0);m.b(m.t+0x231,1);m.target=m.enemyUnit;m.b(m.target+0x134,1);m.w(m.target+0xa4,256)
 m.u.mem_write(m.unit+0x9c,struct.pack('<iii',40*256+128,40*256+128,0));m.w(m.unit+0xac,5)
 m.cellxy={};m.attempts=[]
 for y in range(37,44):
  for x in range(37,44):
   c=m.alloc();m.u.mem_write(c+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,c);m.cellxy[c]=(x,y)
 def hook(at,fn):m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:fn(),begin=at,end=at)
 def arg(i):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 hook(0x578460,lambda:m.ret(1,2));hook(0x5f5f00,lambda:m.ret(0,0))
 # The actual per-object CanEnterCell is the read-only world boundary.
 def enter():
  xy=m.cellxy[arg(0)];m.attempts.append(xy);m.ret(int(case=='blocked' or case=='fallback' and xy==(38,40)),5)
 hook(m.r(m.r(m.unit)+0x1ac),enter)
 m.bullet=m.alloc();m.w(m.bullet,0x7e46e4);m.w(m.bullet+0x10,701);m.b(m.bullet+0x90,1);m.w(m.bullet+0xac,m.alloc());m.w(m.bullet+0xb0,m.target);m.w(m.bullet+0x10c,m.unit);m.w(m.bullet+0x6c,40)
 m.u.mem_write(m.bullet+0xe8,struct.pack('<ddd',v[0]*30,v[1]*30,-5));m.u.mem_write(m.bullet+0x134,struct.pack('<iiiiii',30*256+128,40*256+128,512,40*256+128,40*256+128,0));m.vector(0xa8ed40,[m.bullet])
 if case=='moving':m.w(m.unit+0x5a4,m.crateCell);m.w(m.unit+0xac,2)
 if case=='turning':m.u.mem_write(m.unit+0x39c,struct.pack('<h',8));m.w(m.unit+0x390,m.r(0xa8ed84));m.w(m.unit+0x398,50)
 if case=='foreign':m.w(m.unit+0x21c,m.enemy)
 if case=='self':m.w(m.target+0x21c,m.house)
 if case=='healer':m.w(m.bullet+0x6c,-10)
 if case=='ground':m.w(m.bullet+0x10c,m.r(m.cells+(40*512+40)*4))
 if case=='zero_velocity':m.u.mem_write(m.bullet+0xe8,bytes(24))
 if case=='zero_direction':m.u.mem_write(m.bullet+0xe8,bytes(24));m.u.mem_write(m.bullet+0x134,bytes(24))
 if case=='salvo':
  other=m.alloc();m.u.mem_write(other,bytes(m.u.mem_read(m.bullet,0x200)));m.w(other+0x10,702);m.u.mem_write(other+0xe8,struct.pack('<ddd',0,30,-5));m.vector(0xa8ed40,[m.bullet,other])
 s=n.state(m);s.dodgeEnabled=case!='off';s.dodgeMode=0;s.computeApplied=0 # default must not wait for host calculation
 if case=='filter':s.featureConfigSeq+=2;s.featureConfig.filters[18].custom=1
 n.save(m,s)
 if case=='chat':m.b(0xa8bc60+25,1)
 return m
for mode in (0,3,4,5):
 cases=[('direction'+str(i),v) for i,v in enumerate(DIRECTIONS)]+[(x,(1,0)) for x in ('fallback','blocked','moving','turning','foreign','self','healer','ground','zero_velocity','zero_direction','salvo','off','filter','chat','new_shot','same_shot','near_1','near_4','near_12','fixed_window')]
 for case,v in cases:
  try:
   m=setup(mode,case,v);before=bytes(m.u.mem_read(m.unit,0x800));s=n.tick(m);expected=case not in ('blocked','foreign','healer','zero_direction','off','filter','chat')
   assert n.count(m)==int(expected),(case,n.count(m),s.dodgeActive,s.data.lastSubmitError)
   if expected:
    b=n.packets(m);assert b[0]==4 and b[12]==2 and b[14:19]==bytes(5) and b[23]==11
    cell=struct.unpack_from('<I',b,19)[0];dx=(cell%1000-40)*256;dy=(cell//1000-40)*256
    assert dx*v[0]+dy*v[1]<0 and 320**2<=dx*dx+dy*dy<=704**2,(case,cell)
    if case=='direction0':assert cell==40038
    if case=='fallback':assert cell!=40038 and m.attempts[0]==(38,40)
   assert bytes(m.u.mem_read(m.unit,0x800))==before and not m.codeWrites
   first_frame=m.r(0xa8ed84)
   if case.startswith('near_') or case=='fixed_window':
    gap=int(case.split('_')[1]) if case.startswith('near_') else 10
    m.w(0xa8ed84,first_frame+gap-1);m.w(m.bullet+0x10,702);m.u.mem_write(m.bullet+0xe8,struct.pack('<ddd',0,30,-5));n.tick(m)
    assert n.count(m)==1,'nearby shot interrupted accepted Move'
    if case=='fixed_window':
     m.w(0xa8ed84,first_frame+12);m.w(m.bullet+0x10,703);n.tick(m)
     assert n.count(m)==2,'suppressed shot extended merge window'
     expected=2
   for _ in range(3):n.tick(m)
   assert n.count(m)==int(expected),'same shot must not repeat'
   if case=='new_shot':
    m.w(0xa8ed84,first_frame+12)
    m.w(m.bullet+0x10,703);m.u.mem_write(m.bullet+0xe8,struct.pack('<ddd',0,30,-5));n.tick(m)
    assert n.count(m)==2 and struct.unpack_from('<I',n.packets(m),111+19)[0]==38040
   rows.append({'case':f'{mode}_{case}','status':'PASS'})
  except Exception as e:rows.append({'case':f'{mode}_{case}','status':'FAIL','error':repr(e)});traceback.print_exc()
  print(rows[-1],flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))

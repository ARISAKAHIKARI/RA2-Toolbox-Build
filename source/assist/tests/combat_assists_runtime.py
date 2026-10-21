"""Actual Runtime and original weapon/range/event functions in a controlled x86 world; not live game/netplay or projectile physics."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,subprocess,sys,tempfile,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('advanced',Path(__file__).with_name('advanced_range_runtime.py'));v=importlib.util.module_from_spec(spec);args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))]
# The reusable fixture module exits after its optional case runner.
try:spec.loader.exec_module(v)
except SystemExit as ex:assert not ex.code
sys.argv=args;n=v.n
root=Path(__file__).resolve().parents[2];tmp=tempfile.TemporaryDirectory();cpp=Path(tmp.name)/'host.cpp';host=Path(tmp.name)/'host'
cpp.write_text('#include "'+str(root/'assist/source/burst_compute.h')+'"\n#include "'+str(root/'assist/source/dodge_compute.h')+'"\n#include <cstdio>\nint main(int argc,char**){using namespace assist;if(argc>1){dodgecompute::Input i;if(fread(&i,sizeof i,1,stdin)!=1)return 2;auto r=dodgecompute::calculate(i);fwrite(&r,sizeof r,1,stdout);}else{burstcompute::Input i;if(fread(&i,sizeof i,1,stdin)!=1)return 2;auto r=burstcompute::calculate(i);fwrite(&r,sizeof r,1,stdout);}}')
subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def compute(m,which):
 s=n.state(m)
 if getattr(s,which+'Task')==getattr(s,which+'Computed'):return
 payload=subprocess.run([str(host)]+(['dodge'] if which=='dodge' else []),input=bytes(getattr(s,which+'Input')),stdout=subprocess.PIPE,check=True).stdout
 typ=m.BurstResult if which=='burst' else m.DodgeResult;assert len(payload)==C.sizeof(typ);setattr(s,which+'Result',typ.from_buffer_copy(payload));setattr(s,which+'Computed',getattr(s,which+'Task'));setattr(s,which+'ResultSeq',getattr(s,which+'ResultSeq')+2);n.save(m,s)
def tick(m,inside,which):
 if not inside:compute(m,which)
 return n.tick(m)
def prepare(mode,inside,kind):
 m=v.setup(mode,True,'ifv_elite' if kind in ('elite','elite_single') else 'ifv_7');s=n.state(m);s.flags=0;s.computeApplied=8191 if inside else 2047;s.burstEnabled=1;s.burstHoldEnabled=0;s.featureConfigSeq+=2;s.featureConfig.filters[17].custom=0;s.heartbeat=m.clock;n.save(m,s)
 m.w(m.unit+0xac,1);m.w(m.unit+0xb4,-1);m.w(m.unit+0x5a4,0);m.w(m.unit+0x2b4,m.target);m.w(m.unit+0x3b8,1);m.w(m.unit+0x2ec,m.r(0xa8ed84));m.w(m.actual+0x9c,4 if kind=='elite' else 2)
 if kind=='elite':m.w(m.r(m.t+0x898+4*28)+0x9c,1)
 return m
def burst(mode,inside,kind):
 m=prepare(mode,inside,kind);s=n.state(m)
 if kind=='elite_single':m.w(m.actual+0x9c,1)
 if kind=='burst_one':m.w(m.actual+0x9c,1)
 if kind=='completed':m.w(m.unit+0x3b8,2)
 if kind=='not_fired':m.w(m.unit+0x3b8,0)
 if kind=='moving':m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell)
 if kind=='disabled':s.burstEnabled=0
 if kind=='filter':s.featureConfig.filters[17].custom=1
 if kind=='spawn_owner':m.w(m.unit+0x2d4,m.target)
 if kind=='spawned':m.b(m.t+0xd54,1)
 if kind=='spawn_parent':m.w(m.t+0xd58,m.t);m.w(m.t+0xd5c,3)
 if kind=='missile_spawn':m.b(m.t+0xd68,1)
 if kind=='aircraft':m.w(m.unit,0x7e22a4)
 if kind=='airborne':m.b(m.unit+0x134,1)
 if kind=='hold_spawned':
  s.burstEnabled=0;s.burstHoldEnabled=1;s.burstHeld=1;m.b(m.unit+0x83,1);m.b(m.t+0xd54,1)
 if kind in ('hold','hold_unselected','hold_unfocused','hold_stale'):
  s.burstEnabled=0;s.burstHoldEnabled=1;s.burstHeld=1;s.foreground=kind!='hold_unfocused';m.b(m.unit+0x83,kind!='hold_unselected')
  if kind=='hold_stale':
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.w(m.SH+m.Shared.heartbeat.offset,m.clock-1000),begin=m.entry,end=m.entry)
 if kind=='observer':m.b(m.house+0x1f5,1)
 n.save(m,s);before=bytes(m.u.mem_read(m.unit,0x800));s=tick(m,inside,'burst')
 if not inside:s=tick(m,inside,'burst')
 expected=kind in ('normal','elite','hold','release','weapon_changed','conflict','fast_cycle','same_frame')
 assert n.count(m)==int(expected),(kind,n.count(m),s.burstInput.count,s.data.lastSubmitError)
 assert before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites
 if not expected:return
 assert n.packets(m)[12]==13 and n.packets(m)[14:24]==bytes(10)
 if kind in ('fast_cycle','same_frame'):
  # Never expose a Stop observation: the ordered Attack must still follow.
  s=tick(m,inside,'burst')
  if not inside:s=tick(m,inside,'burst')
  assert n.count(m)==2,'restore next frame without waiting to sample Stop'
  for _ in range(3):tick(m,inside,'burst');assert n.count(m)==2
  if kind=='same_frame':
   # Observe another firing event after an earlier sample of this frame.
   m.w(m.unit+0x2ec,m.r(m.unit+0x2ec)+1)
   s=m.hook()
   if not inside:
    # First consume the pending pre-shot result, then calculate the fresh sample.
    for _ in range(2):compute(m,'burst');s=m.hook()
  else:
   m.w(m.unit+0x2ec,m.r(m.unit+0x2ec)+1);s=tick(m,inside,'burst')
   if not inside:s=tick(m,inside,'burst')
  assert n.count(m)==3 and n.packets(m)[222+12]==13,'new shot starts next cycle without an idle scan'
  s=tick(m,inside,'burst')
  if not inside:s=tick(m,inside,'burst')
  assert n.count(m)==4 and n.packets(m)[333+12]==1
  assert not m.codeWrites;return
 # Execute original SetTarget(nullptr), which clears CurrentBurstIndex but
 # preserves the current short rearm timer. Other stop effects are controlled.
 boundary=bytes(m.u.mem_read(0x6fcf53,6))
 if boundary==bytes.fromhex('399eb4020000'):
  timer=bytes(m.u.mem_read(m.unit+0x2ec,12));m.run(0x6fcdb0,(0,),m.unit);assert m.r(m.unit+0x2b4)==0 and m.r(m.unit+0x3b8)==0 and bytes(m.u.mem_read(m.unit+0x2ec,12))==timer
 else:
  # Supplied LBW disk image jumps to an absent startup-populated trampoline.
  # Do not patch it or claim its engine behavior was executed. Only advance
  # the controlled Stop observation to test our restoration state machine.
  assert boundary[0]==0xe9;m.w(m.unit+0x2b4,0);m.w(m.unit+0x3b8,0)
 m.w(m.unit+0xac,13)
 if kind in ('release','hold'):s=n.state(m);s.burstEnabled=s.burstHeld=s.burstHoldEnabled=0;n.save(m,s)
 if kind=='weapon_changed':m.w(m.unit+0x138,0);m.w(m.r(m.t+0x898)+0xb4,9*256);m.w(m.r(m.t+0x898)+0x9c,1)
 for _ in range(3):s=tick(m,inside,'burst')
 assert n.count(m)==2,(kind,'restore',n.count(m),s.burstActive)
 packet=n.packets(m)[111:222];assert packet[12]==1 and packet[14:19]==struct.pack('<IB',40,0x34) and packet[19:24]==bytes(5)
 for _ in range(3):tick(m,inside,'burst');assert n.count(m)==2,'wait for new firing evidence'
 assert not m.codeWrites

def dodge(mode,inside,kind):
 m=prepare(mode,inside,'normal');s=n.state(m);s.burstEnabled=kind=='conflict';s.dodgeEnabled=1;n.save(m,s)
 m.w(m.unit+0x194,0)
 # Enemy vehicle is actively trying to crush this ground unit.
 t=m.r(m.target+0x6c4);m.b(t+0xd29,1);m.w(m.target+0x9c,10256);m.w(m.target+0xa0,10000);m.w(m.target+0xac,2);m.w(m.target+0x2b4,m.unit)
 aircase=kind in ('air','own_air','allied_air','own_healer','own_air_not_targeted','prefire','new_shot','same_shot','ground_shot','late_shot','pending_burst','turning_burst','moving_burst','settled','native_busy','already_rotating','queued_move','manual','foreign','timeout','clicked_backend')
 if aircase:
  m.w(m.target,0x7e22a4);m.b(m.target+0x134,1);m.w(m.target+0xa4,256);m.w(m.target+0xac,1);m.w(t+0x898,m.actual);m.w(m.target+0x2f4,0)
 if kind in ('own_air','own_healer','own_air_not_targeted'):m.w(m.target+0x21c,m.house)
 if kind=='allied_air':m.w(m.house+0x5788,1<<m.r(m.enemy+0x30))
 if kind=='own_healer':m.w(m.actual+0xa4,-40)
 if kind=='own_air_not_targeted':m.w(m.target+0x2b4,0)
 if aircase and kind!='prefire':
  m.bullet=m.alloc();m.w(m.bullet,0x7e46e4);m.w(m.bullet+0x10,701);m.b(m.bullet+0x90,1);m.w(m.bullet+0xac,m.alloc());m.w(m.bullet+0xb0,m.target);m.w(m.bullet+0x10c,m.unit if kind!='own_air_not_targeted' else m.target)
  m.w(m.bullet+0x6c,-40 if kind=='own_healer' else 40);m.w(m.bullet+0x140,m.r(m.unit+0x9c));m.w(m.bullet+0x144,m.r(m.unit+0xa0));m.u.mem_write(m.bullet+0xe8,struct.pack('<ddd',-30,0,-5));m.vector(0xa8ed40,[m.bullet])
 if kind=='allied':m.w(m.target+0x21c,m.house)
 if kind=='not_targeted':m.w(m.target+0x2b4,0)
 if kind=='resistant':m.b(m.t+0xd2a,1)
 if kind=='disabled':s=n.state(m);s.dodgeEnabled=0;n.save(m,s)
 if kind=='filter':s=n.state(m);s.featureConfigSeq+=2;s.featureConfig.filters[18].custom=1;n.save(m,s)
 if kind=='observer':m.b(m.house+0x1f5,1)
 if kind=='foreign':m.w(m.unit+0x21c,m.enemy)
 if kind=='native_busy':m.w(m.unit+0x5a4,m.crateCell)
 if kind=='queued_move':m.w(m.unit+0xb4,2)
 if kind=='already_rotating':
  m.u.mem_write(m.unit+0x39c,struct.pack('<h',8));m.w(m.unit+0x390,m.r(0xa8ed84));m.w(m.unit+0x398,50)
 if kind=='clicked_backend':s=n.state(m);s.objectQueueMode=1;n.save(m,s)
 for x in range(37,42):
  for y in range(37,42):
   cell=m.alloc();m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,cell)
 if kind=='blocked':
  for x in range(37,42):
   for y in range(37,42):m.w(m.r(m.cells+(y*512+x)*4)+0x124,1)
 if kind=='ground_shot':m.w(m.bullet+0x10c,m.r(m.cells+(39*512+39)*4))
 if kind=='late_shot':
  m.vector(0xa8ed40,[]);tick(m,inside,'dodge');assert n.count(m)==0
  m.vector(0xa8ed40,[m.bullet]);s=m.hook()
  if not inside:compute(m,'dodge');s=m.hook()
  assert n.count(m)==1,'shot arriving after an earlier sample of the same frame'
  return
 before=bytes(m.u.mem_read(m.unit,0x800));s=tick(m,inside,'dodge')
 if not inside:
  if kind=='conflict':compute(m,'burst')
  s=tick(m,inside,'dodge')
 expected=kind in ('crush','air','own_air','allied_air','conflict','release','new_shot','same_shot','ground_shot','late_shot','blocked','pending_burst','turning_burst','moving_burst','settled','manual','timeout','clicked_backend')
 assert n.count(m)==int(expected),(kind,n.count(m),s.dodgeInput.count,s.dodgeActive,s.data.lastSubmitError)
 assert before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites
 if not expected:
  if kind in ('native_busy','already_rotating','queued_move'):
   # An existing native movement consumes this shot; don't dodge it late.
   m.w(m.unit+0x5a4,0);m.w(m.unit+0xb4,-1);m.w(m.unit+0x398,0)
   for _ in range(5):tick(m,inside,'dodge')
   assert n.count(m)==0
  return
 packet=n.packets(m)[:111];assert packet[0]==7 and packet[7:12]==struct.pack('<IB',m.r(m.unit+0x10),0x34)
 if kind=='conflict':assert n.state(m).burstIssued==0
 if kind in ('new_shot','same_shot','pending_burst','turning_burst','moving_burst','settled'):
  for k in range(20):
   if kind!='same_shot':m.w(m.bullet+0x10,702+k)
   if kind=='turning_burst':
    m.u.mem_write(m.unit+0x39c,struct.pack('<h',8));m.w(m.unit+0x390,m.r(0xa8ed84));m.w(m.unit+0x398,50)
   if kind in ('moving_burst','settled'):
    m.w(m.unit+0x5a4,m.crateCell);m.w(m.unit+0x9c,10000+k*8)
   tick(m,inside,'dodge')
   assert n.count(m)==1,'fresh shots must not replace a queued/turning/moving scatter'
  if kind=='settled':
   m.w(m.unit+0x5a4,0);m.w(m.unit+0xb4,-1)
   for _ in range(5):tick(m,inside,'dodge')
   assert n.count(m)==1,'same in-flight projectile after arrival is not a new dodge'
   m.w(m.bullet+0x10,999)
   for _ in range(3):tick(m,inside,'dodge')
   assert n.count(m)==2,'new shot after native movement completes can scatter once'
  return
 if kind=='manual':
  # Selection/order tracker observes a new user Move, cancelling automation.
  m.queryObject=m.unit;n.pointer(m,4)
  before_manual=n.count(m)
  m.vector(0xa8ed40,[])
  for _ in range(12):tick(m,inside,'dodge')
  assert n.count(m)==before_manual,'never restore an older attack after a manual order'
  return
 if kind=='timeout':
  m.vector(0xa8ed40,[]);m.w(0xa8ed84,m.r(0xa8ed84)+301)
  for _ in range(4):tick(m,inside,'dodge')
  assert n.count(m)==1 and n.state(m).dodgeActive==0,'unobserved scatter expires without a replacement mission'
  return
 # Mark the native event as executed, with an ongoing move longer than the
 # old six-frame restoration timer. Our runtime must never write the unit.
 m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);m.w(m.unit+0x2b4,0);m.b(m.unit+0x6b2,1)
 m.w(m.target+0x2b4,0)
 if aircase:m.vector(0xa8ed40,[])
 if kind=='release':s=n.state(m);s.dodgeEnabled=0;n.save(m,s)
 before=bytes(m.u.mem_read(m.unit,0x800))
 for _ in range(12):s=tick(m,inside,'dodge')
 assert n.count(m)==1 and bytes(m.u.mem_read(m.unit,0x800))==before,'no early Attack/Guard and no local unit writes'
 m.w(m.unit+0x5a4,0);m.w(m.unit+0xb4,-1)
 for _ in range(5):s=tick(m,inside,'dodge')
 assert n.count(m)==1 and s.dodgeActive==0,(kind,'settled',n.count(m),s.dodgeActive)
 assert not m.codeWrites

for mode in (0,3):
 for inside in (True,False):
  for kind in ('normal','elite','elite_single','burst_one','completed','not_fired','moving','disabled','filter','hold','hold_unselected','hold_unfocused','hold_stale','observer','release','weapon_changed','spawn_owner','spawned','spawn_parent','missile_spawn','aircraft','airborne','hold_spawned','fast_cycle','same_frame'):check(f'burst_{mode}_{inside}_{kind}',lambda m=mode,i=inside,k=kind:burst(m,i,k))
  for kind in ('crush','air','own_air','allied_air','own_healer','own_air_not_targeted','allied','not_targeted','resistant','disabled','filter','observer','blocked','conflict','release','prefire','new_shot','same_shot','ground_shot','late_shot','pending_burst','turning_burst','moving_burst','settled','native_busy','already_rotating','queued_move','manual','foreign','timeout','clicked_backend'):check(f'dodge_{mode}_{inside}_{kind}',lambda m=mode,i=inside,k=kind:dodge(m,i,k))
Path(a.output).write_text(json.dumps({'scope':__doc__,'native_stop_reset_executed':bytes(n.fixture(0).game.get_data(0x6fcf53-0x400000,6))==bytes.fromhex('399eb4020000'),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))

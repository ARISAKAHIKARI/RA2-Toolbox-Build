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
 if kind in ('hold','hold_unselected','hold_unfocused','hold_stale'):
  s.burstEnabled=0;s.burstHoldEnabled=1;s.burstHeld=1;s.foreground=kind!='hold_unfocused';m.b(m.unit+0x83,kind!='hold_unselected')
  if kind=='hold_stale':
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.w(m.SH+m.Shared.heartbeat.offset,m.clock-1000),begin=m.entry,end=m.entry)
 if kind=='observer':m.b(m.house+0x1f5,1)
 n.save(m,s);before=bytes(m.u.mem_read(m.unit,0x800));s=tick(m,inside,'burst')
 if not inside:s=tick(m,inside,'burst')
 expected=kind in ('normal','elite','hold','release','weapon_changed','conflict')
 assert n.count(m)==int(expected),(kind,n.count(m),s.burstInput.count,s.data.lastSubmitError)
 assert before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites
 if not expected:return
 assert n.packets(m)[12]==13 and n.packets(m)[14:24]==bytes(10)
 for _ in range(3):s=tick(m,inside,'burst');assert n.count(m)==1,'never repeat pending Stop'
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
 if kind=='air':
  m.w(m.target,0x7e22a4);m.b(m.target+0x134,1);m.w(m.target+0xa4,256);m.w(m.target+0xac,1);m.w(t+0x898,m.actual);m.w(m.target+0x2f4,0)
 if kind=='allied':m.w(m.target+0x21c,m.house)
 if kind=='not_targeted':m.w(m.target+0x2b4,0)
 if kind=='resistant':m.b(m.t+0xd2a,1)
 if kind=='disabled':s=n.state(m);s.dodgeEnabled=0;n.save(m,s)
 if kind=='filter':s=n.state(m);s.featureConfigSeq+=2;s.featureConfig.filters[18].custom=1;n.save(m,s)
 if kind=='observer':m.b(m.house+0x1f5,1)
 for x in range(37,42):
  for y in range(37,42):
   cell=m.alloc();m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,cell)
 if kind=='blocked':
  for x in range(37,42):
   for y in range(37,42):m.w(m.r(m.cells+(y*512+x)*4)+0x124,1)
 before=bytes(m.u.mem_read(m.unit,0x800));s=tick(m,inside,'dodge')
 if not inside:
  if kind=='conflict':compute(m,'burst')
  s=tick(m,inside,'dodge')
 expected=kind in ('crush','air','conflict','release')
 assert n.count(m)==int(expected),(kind,n.count(m),s.dodgeInput.count,s.dodgeActive)
 assert before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites
 if not expected:return
 packet=n.packets(m)[:111];assert packet[12]==2;cell=struct.unpack_from('<I',packet,19)[0];assert cell%1000<39,'move away from approaching threat'
 if kind=='conflict':assert n.state(m).burstIssued==0
 dest=m.r(m.cells+((cell//1000)*512+cell%1000)*4);m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,dest);m.w(m.unit+0x2b4,0)
 m.w(m.target+0x2b4,0)
 if kind=='release':s=n.state(m);s.dodgeEnabled=0;n.save(m,s)
 for _ in range(9):s=tick(m,inside,'dodge')
 assert n.count(m)==2,(kind,'restore',n.count(m),s.dodgeActive)
 assert n.packets(m)[111+12]==1,'restore original valid in-range attack'
 assert not m.codeWrites
for mode in (0,3):
 for inside in (True,False):
  for kind in ('normal','elite','elite_single','burst_one','completed','not_fired','moving','disabled','filter','hold','hold_unselected','hold_unfocused','hold_stale','observer','release','weapon_changed'):check(f'burst_{mode}_{inside}_{kind}',lambda m=mode,i=inside,k=kind:burst(m,i,k))
  for kind in ('crush','air','allied','not_targeted','resistant','disabled','filter','observer','blocked','conflict','release'):check(f'dodge_{mode}_{inside}_{kind}',lambda m=mode,i=inside,k=kind:dodge(m,i,k))
Path(a.output).write_text(json.dumps({'scope':__doc__,'native_stop_reset_executed':bytes(n.fixture(0).game.get_data(0x6fcf53-0x400000,6))==bytes.fromhex('399eb4020000'),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))

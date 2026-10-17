"""Execute the current x86 Runtime and original constructors across startup boundaries.
Synthetic world/Win32; no complete game, network, rendering or economy simulation.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode=3,flags=0,frame=0):
 spec=importlib.util.spec_from_file_location('startup_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.w(0xa8ed84,frame);s=state(m);s.flags=flags;s.externalDisplay=1;save(m,s)
 m.w(m.building+0x6c,100);m.w(m.unit+0x6c,100)
 m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.u.mem_write(m.t+0x388,struct.pack('<d',3.))
 m.writes=[];m.calls=[]
 for at,n in [(m.house,0x20000),(m.enemy,0x20000),(m.t,0x2000),(m.engT,0x2000),(m.buildT,0x2000),(m.unit,0x2000),(m.eng,0x2000),(m.building,0x2000)]:
  m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,addr,size,value,_:m.writes.append((addr,size)),begin=at,end=at+n-1)
 for section in m.game.sections:
  if section.Name.rstrip(b'\0')==b'.text':
   at=m.game.OPTIONAL_HEADER.ImageBase+section.VirtualAddress
   m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,addr,size,value,_:m.writes.append((addr,size)),begin=at,end=at+section.Misc_VirtualSize-1)
 for at in (0x4c6860,0x4c65e0,0x646e90,0x692300,0x6a6300):m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:m.calls.append(addr),begin=at,end=at)
 return m
def count(m):return m.r(0x8b41f8)+m.r(0xa802c8)
def issue(m,c):
 s=state(m);s.command=c;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);return m.hook()
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});print('FAIL',name,flush=True);traceback.print_exc()
flags={'autoload_idle':0,'auto_cs':512|131072,'magnetron':1024|262144,'aircraft':2097152,'disk_primary':4194304,'sell_vehicle':16777216,'all':(1<<26)-1}
for mode in (0,5,3,4):
 for label,mask in flags.items():
  def idle(mode=mode,mask=mask):
   m=fixture(mode,mask)
   for frame in (0,0,0,1,1,2,10,30):m.w(0xa8ed84,frame);s=m.hook()
   assert s.data.commandStatus==1 and s.data.match==1 and not count(m)
   assert not m.writes and not m.calls,(m.writes,m.calls)
   assert not s.data.inputPatchMask and not s.data.hookInstalled
  check(f'idle_start_{mode}_{label}_zero_orders_zero_mutations',idle)
 for boundary in ('menu','dialog','tactical_inactive','negative_frame','frame_max','observer','run_flag','unsupported_mode'):
  def boundary_test(mode=mode,boundary=boundary):
   m=fixture(mode,(1<<26)-1);s=m.hook();epoch=s.data.match
   # Actual pending manual auto-board uses the known-good event path.
   s.command=15;s.request+=1;s.requestMatch=epoch;s.requestTick=m.clock;save(m,s)
   if boundary=='menu':m.w(0xa8eda0,1)
   elif boundary=='dialog':m.w(0xa8ed8c,1)
   elif boundary=='tactical_inactive':m.b(0xa8e378,0)
   elif boundary=='negative_frame':m.w(0xa8ed84,-1)
   elif boundary=='frame_max':m.w(0xa8ed84,0x7fffffff)
   elif boundary=='observer':m.w(0xac1198,m.house)
   elif boundary=='run_flag':m.b(0xa8e9a0,0)
   elif boundary=='unsupported_mode':m.w(0xa8b238,2)
   for _ in range(3):s=m.hook()
   assert not count(m) and not m.writes and not m.calls,(boundary,count(m),m.calls)
   assert s.data.commandStatus==0
   m.w(0xa8eda0,0);m.w(0xa8ed8c,0);m.b(0xa8e378,1);m.w(0xa8ed84,0);m.w(0xac1198,0);m.b(0xa8e9a0,1);m.w(0xa8b238,mode)
   s=m.hook();assert s.data.commandStatus==1
   if boundary!='observer':assert s.data.match==epoch and count(m)==1,(boundary,s.data.match,epoch,count(m),s.result)
   else:assert s.data.match!=epoch and count(m)==0
  check(f'boundary_{mode}_{boundary}_blocks_then_recovers',boundary_test)
 def loading(mode=mode):
  m=fixture(mode,(1<<26)-1,-1);m.b(0xa8e378,0)
  for _ in range(3):s=m.hook()
  assert s.data.commandStatus==0 and s.data.match==0 and not count(m) and not m.calls
  m.w(0xa8ed84,0);m.b(0xa8e378,1);s=m.hook()
  assert s.data.commandStatus==1 and s.data.match==1
  s=issue(m,15);assert s.ack==s.request and s.result==1 and count(m)==1 and not m.writes
  at=0x8b4204 if mode in (0,5) else 0xa802d4
  packet=bytes(m.u.mem_read(at,111));assert packet[0]==4 and packet[12]==7
  assert struct.unpack_from('<I',packet,3)[0]==(1 if mode in (0,5) else 0)
  timing=(0xa70204 if mode in (0,5) else 0xa83a54);assert m.r(timing)==m.clock
  for _ in range(6):m.hook()
  assert count(m)==1 # same frame callbacks cannot duplicate a committed event
 check(f'first_frame_{mode}_autoload_original_frame_timing_once',loading)
 def repair(mode=mode):
  m=fixture(mode,64);m.w(m.building+0x6c,50);s=m.hook();assert count(m)==1
  for _ in range(30):m.hook()
  assert count(m)==1 and not m.writes
 check(f'first_frame_{mode}_auto_repair_only_real_damage_once',repair)
 for mutation in ('frame','house','mode','menu'):
  def timer_boundary(mode=mode,mutation=mutation):
   m=fixture(mode);m.hook()
   def change():
    if mutation=='frame':m.w(0xa8ed84,1)
    elif mutation=='house':m.w(0xa83d4c,m.enemy)
    elif mutation=='mode':m.w(0xa8b238,5 if mode==3 else 3)
    else:m.w(0xa8eda0,1)
   m.timer_side_effect=change
   issue(m,15);assert not count(m) and not m.writes,(mutation,count(m))
  check(f'final_commit_{mode}_timer_changes_{mutation}_rejects',timer_boundary)
# Same native auto-board payload with all feature switches enabled; automatic
# repair is the only extra event when actual damage exists (different actor).
for mode in (0,5,3,4):
 def combined(mode=mode):
  base=fixture(mode);base.hook();issue(base,15)
  m=fixture(mode,(1<<26)-1);m.w(m.building+0x6c,50);m.hook();issue(m,15)
  assert count(base)==1 and count(m)==2 and not m.writes
  at=0x8b4204 if mode in (0,5) else 0xa802d4
  assert m.u.mem_read(at,1)==bytes([0x15])
  assert bytes(m.u.mem_read(at+111,111))==bytes(base.u.mem_read(at,111))
  for _ in range(5):m.hook()
  assert count(m)==2
 check(f'all_features_{mode}_autoload_payload_unchanged_repair_separate',combined)
# A restart using the same House must not accept prior-frame requests.
for mode in (3,4):
 def restart(mode=mode):
  m=fixture(mode,0,100);s=m.hook();old=s.data.match
  s.command=15;s.request+=1;s.requestMatch=old;s.requestTick=m.clock;save(m,s);m.w(0xa8ed84,0);s=m.hook()
  assert s.data.match!=old and C.c_int32(s.result).value==-7 and not count(m)
  s=issue(m,15);assert s.result==1 and count(m)==1
 check(f'same_house_restart_{mode}_stale_request_rejected_new_accepted',restart)
Path(a.output).write_text(json.dumps({'inputs':{'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},'scope':__doc__,'results':rows},indent=2)+'\n')
print('TOTAL',len(rows));raise SystemExit(int(any(r['status']!='PASS' for r in rows)))

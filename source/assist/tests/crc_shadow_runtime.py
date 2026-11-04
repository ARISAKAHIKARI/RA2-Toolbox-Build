"""Actual compiled experimental MapIsClear CRC adapter on a synthetic world.

Native reveal is stopped AFTER its real flag write and BEFORE cells/Visionary.
The real CRC and installed leaves run; Win32 and logger are fixture sinks.
No complete Reveal, world tick, platform transport or multiplayer execution.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('netfixture',Path(__file__).with_name('network_house_runtime.py'));n=importlib.util.module_from_spec(spec)
argv=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__helpers_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))]
try:spec.loader.exec_module(n)
except SystemExit as e:assert not e.code
finally:sys.argv=argv
SITES={0x64dcca:bytes.fromhex('8a9941020000'),0x4f576b:bytes.fromhex('889d41020000'),0x4fc328:bytes.fromhex('c6814102000001'),
 0x577aca:bytes.fromhex('889941020000'),0x577bc7:bytes.fromhex('c6814102000000'),0x577ca7:bytes.fromhex('c6814102000000'),
 0x577db0:bytes.fromhex('889941020000'),0x577f48:bytes.fromhex('c6814102000001')}
END={address+len(raw) for address,raw in SITES.items()}
GPRS=n.GPRS

def check(name,fn):
 if a.filter and a.filter not in name:return
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()

def state(m):return n.n.state(m)
def save(m,s):return n.n.save(m,s)
def tick(m):return n.n.tick(m)
def enable(m,on=True):
 s=state(m);s.crcMapShadow=int(on);save(m,s);return tick(m)
def installed(m):
 for address,raw in SITES.items():assert bytes(m.u.mem_read(address,len(raw)))[0]==0xe9,hex(address)
def restored(m):
 for address,raw in SITES.items():assert bytes(m.u.mem_read(address,len(raw)))==raw,hex(address)
def setup(mode=3):
 m=n.setup(mode,1);m.u.hook_del(m.native_stub_hooks[0x577d90]);m.b(m.house+0x241,0);m.b(m.enemy+0x241,0)
 s=state(m);s.flags|=67108864;save(m,s);tick(m)
 m.w(0x87f7e8+316,m.alloc(8));m.w(0x87f7e8+320,512*512)
 # Retain the actual constructor-independent flag write through our real hook;
 # skip only the unmodeled cells/Visionary suffix of Reveal.
 m.leaf_return=m.STUB+0xed00;m.u.mem_write(m.leaf_return,b'\xc3')
 m.leaf_target=None;m.leaf_finish=None
 def tail(uc,at,size,_):
  if at==m.leaf_target:
   m.leaf_finish();uc.reg_write(UC_X86_REG_EIP,m.leaf_return)
  elif at==0x577db6:uc.reg_write(UC_X86_REG_EIP,0x577f1e)
 for address in END:m.u.hook_add(UC_HOOK_CODE,tail,begin=address,end=address)
 return m

def leaf(m,address,house=None,value=0,expected=None):
 house=m.house if house is None else house
 if address==0x577db0:
  # Direct leaf invocation owns no Reveal prologue; temporary switch below.
  pass
 before={};after={};frames=[]
 def enter(uc,at,size,_):
  before['gpr']=tuple(uc.reg_read(r) for r in GPRS);before['flags']=uc.reg_read(UC_X86_REG_EFLAGS);before['fp']=n.fp(m)
 def end():
  uc=m.u
  after['gpr']=tuple(uc.reg_read(r) for r in GPRS);after['flags']=uc.reg_read(UC_X86_REG_EFLAGS);after['fp']=n.fp(m)
 start=m.u.hook_add(UC_HOOK_CODE,enter,begin=address,end=address)
 m.leaf_target=address+len(SITES[address]);m.leaf_finish=end
 m.u.reg_write(UC_X86_REG_EBX,0x12345600|value);m.u.reg_write(UC_X86_REG_EBP,house if address==0x4f576b else 0x12345678)
 try:m.run(address,(),house)
 finally:m.u.hook_del(start);m.leaf_target=None;m.leaf_finish=None
 assert after and before
 desired=list(before['gpr'])
 if address==0x64dcca:desired[1]=(desired[1]&0xffffff00)|(m.u.mem_read(house+0x241,1)[0] if expected is None else expected)
 assert tuple(desired)==after['gpr'],(hex(address),desired,after['gpr'])
 assert before['flags']==after['flags'] and before['fp']==after['fp'],hex(address)
 return m.u.reg_read(UC_X86_REG_EBX)&255

def reveal(m):
 s=state(m);s.request+=1;s.command=9;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=tick(m)
 assert s.ack==s.request and s.result==1,(s.result,s.data.lastSubmitError,s.crcMapShadowStatus)
 return s

def crc_world(m):
 m.vector(0xa83de8,[]);m.vector(0x8b4108,[m.unit]);m.vector(0xa8eb40,[m.building])
 for obj,x,y in [(m.unit,34560,45670),(m.building,56780,67890)]:
  m.w(obj+0x9c,x);m.w(obj+0xa0,y);m.w(obj+0xa4,512);m.w(obj+0x388,0x3200);m.w(obj+0x3a0,0x6700)
 for layer in range(5):m.vector(0x8a0360+24*layer,[m.unit,m.building] if layer==0 else [])
 m.vector(0x87f778,[m.unit]);m.rng=m.r(0xa8b230)+0x218
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,0),begin=0x650a90,end=0x650a90)
 def compute():
  m.w(m.rng+4,0);m.w(m.rng+8,103)
  for i in range(250):m.w(m.rng+12+4*i,(((i+1)*0x01020304)^(getattr(m,'rng_variant',0) if i==0 else 0))&0xffffffff)
  calls=[];hook=m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:calls.append(at),begin=0x64dab0,end=0x64dab0)
  try:m.run(0x64dab0)
  finally:m.u.hook_del(hook)
  assert len(calls)==1 and [m.r(m.rng+4),m.r(m.rng+8)]==[1,104]
  return m.r(0xac51fc)
 return compute

def default_off():
 m=setup();restored(m);assert state(m).crcMapShadow==0
 return {'all_eight_sites_original':True}
check('default_off',default_off)

def tool_and_restore():
 m=setup();s=enable(m);assert s.crcMapShadowStatus==1,s.crcMapShadowStatus;installed(m)
 reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==1
 leaf(m,0x64dcca,expected=0);s=enable(m,False);assert s.crcMapShadowStatus==0;restored(m)
 assert m.u.mem_read(m.house+0x241,1)[0]==0
 return {'tracked_tool_flag':True,'restored_original_flag_and_eight_sites':True}
check('tool_reveal_and_restore',tool_and_restore)

def native_crc():
 m=setup();assert enable(m).crcMapShadowStatus==1;compute=crc_world(m);baseline=compute()
 reveal(m);assert compute()==baseline
 m.w(m.unit+0x9c,34570);moved=compute();assert moved!=baseline
 m.rng_variant=1;random_changed=compute();assert random_changed!=moved
 enable(m,False);assert compute()==random_changed
 return {'shadow_crc_matches_original':True,'normal_motion_still_changes_crc':True,'normal_rng_still_changes_crc':True,'no_extra_crc_or_rng_calls':True}
check('actual_native_crc',native_crc)

for address,value in [(0x4fc328,1),(0x577aca,0),(0x577bc7,0),(0x577ca7,0),(0x577db0,1),(0x577f48,1)]:
 def natural(address=address,value=value):
  m=setup();assert enable(m).crcMapShadowStatus==1;reveal(m);leaf(m,address,value=value)
  expected=1 if address in (0x4fc328,0x577f48) else 0 if address in (0x577bc7,0x577ca7) else value
  assert m.u.mem_read(m.house+0x241,1)[0]==expected;leaf(m,0x64dcca,expected=expected)
  enable(m,False);assert m.u.mem_read(m.house+0x241,1)[0]==expected;restored(m)
  return {'natural_value':expected,'same_value_write_recorded':expected==1}
 check('natural_write_'+hex(address),natural)

for case in ['singleplayer','mode','late','conflict','writer_conflict','mod','unknown_write','pointer_reuse','constructor','pending_off','frame_rewind','heartbeat_stop']:
 def boundary(case=case):
  m=setup(5 if case=='singleplayer' else 3)
  if case=='mode':s=state(m);s.flags&=~67108864;save(m,s)
  if case=='late':reveal(m)
  if case=='conflict':m.u.mem_write(0x64dcca,b'\x90')
  if case=='writer_conflict':m.u.mem_write(0x577f48,b'\x90')
  if case=='mod':m.legacy_loaded=True
  s=enable(m)
  if case in ('singleplayer','mode','late','conflict','writer_conflict','mod'):
   assert s.crcMapShadowStatus<0,(case,s.crcMapShadowStatus)
   if case not in ('conflict','writer_conflict'):restored(m)
   return {'rejected_before_activation':True,'runtime_status':s.crcMapShadowStatus}
  assert s.crcMapShadowStatus==1;reveal(m)
  if case=='unknown_write':
   m.b(m.house+0x241,0);leaf(m,0x64dcca,expected=0);s=tick(m);assert s.crcMapShadowStatus<0
  elif case=='pointer_reuse':
   m.w(m.house+0x10,101);leaf(m,0x64dcca,expected=1);enable(m,False);assert m.u.mem_read(m.house+0x241,1)[0]==1
  elif case=='pending_off':
   s=state(m);s.crcMapShadow=0;save(m,s);leaf(m,0x64dcca,expected=0)
   tick(m);assert m.u.mem_read(m.house+0x241,1)[0]==0;restored(m)
  elif case=='frame_rewind':
   m.w(0xa8ed84,0);s=tick(m);assert s.crcMapShadowStatus<0
   assert m.u.mem_read(m.house+0x241,1)[0]==1;restored(m)
  elif case=='constructor':
   leaf(m,0x4f576b,value=0);leaf(m,0x64dcca,expected=0);enable(m,False);assert m.u.mem_read(m.house+0x241,1)[0]==0
  else:
   s=state(m);s.stop=1;save(m,s);s=tick(m);assert s.stop==2
   assert m.u.mem_read(m.house+0x241,1)[0]==0;restored(m)
  return {'no_stale_original_applied':True}
 check('boundary_'+case,boundary)

Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

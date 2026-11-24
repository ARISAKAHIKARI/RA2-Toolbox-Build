"""Actual compiled single-site CRC adapter on a synthetic x86 game world.

The real Map::Reveal prologue and the 0x577DB0 trampoline execute. The fixture
stops at 0x577DB6, the exact native continuation, so Cell/Visionary are neither
simulated nor mirrored. The real CRC reader executes; transport/logging are sinks.
No live Windows game, platform peer or multiplayer acceptance is claimed.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser()
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='')
a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('netfixture',Path(__file__).with_name('network_house_runtime.py'))
n=importlib.util.module_from_spec(spec)
argv=sys.argv
sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__helpers_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))]
try:spec.loader.exec_module(n)
except SystemExit as e:assert not e.code
finally:sys.argv=argv

SITE=0x577db0
ORIGINAL=bytes.fromhex('889941020000')
OTHER_SITES={
 0x64dcca:bytes.fromhex('8a9941020000'),
 0x4f576b:bytes.fromhex('889d41020000'),
 0x4fc328:bytes.fromhex('c6814102000001'),
 0x577aca:bytes.fromhex('889941020000'),
 0x577bc7:bytes.fromhex('c6814102000000'),
 0x577ca7:bytes.fromhex('c6814102000000'),
 0x577f48:bytes.fromhex('c6814102000001')
}


def check(name,fn):
 if a.filter and a.filter not in name:return
 try:
  rows.append({'case':name,'status':'PASS',**(fn() or {})})
  print('PASS',name,flush=True)
 except Exception as e:
  rows.append({'case':name,'status':'FAIL','error':repr(e)})
  traceback.print_exc()


def state(m):return n.n.state(m)
def save(m,s):return n.n.save(m,s)
def tick(m):return n.n.tick(m)


def enable(m,on=True):
 s=state(m);s.crcMapShadow=int(on);save(m,s);return tick(m)


def installed(m):
 actual=bytes(m.u.mem_read(SITE,len(ORIGINAL)))
 assert actual[0]==0xe9 and actual[5]==0x90 and actual!=ORIGINAL,actual.hex()
 rel=int.from_bytes(actual[1:5],'little',signed=True)
 target=SITE+5+rel
 base=m.dll.OPTIONAL_HEADER.ImageBase
 assert base<=target<base+m.dll.OPTIONAL_HEADER.SizeOfImage,hex(target)
 for address,raw in OTHER_SITES.items():
  assert bytes(m.u.mem_read(address,len(raw)))==raw,hex(address)


def restored(m):
 assert bytes(m.u.mem_read(SITE,len(ORIGINAL)))==ORIGINAL
 for address,raw in OTHER_SITES.items():
  assert bytes(m.u.mem_read(address,len(raw)))==raw,hex(address)


def setup(mode=3):
 m=n.setup(mode,1)
 m.u.hook_del(m.native_stub_hooks[0x577d90])
 m.b(m.house+0x241,0);m.b(m.enemy+0x241,0)
 s=state(m);s.flags|=67108864;save(m,s);tick(m)
 m.w(0x87f7e8+316,m.alloc(8));m.w(0x87f7e8+320,512*512)
 # Confirm the production trampoline returns to the exact original continuation.
 # The suffix is deliberately not emulated: it remains entirely native in-game.
 def continuation(uc,at,size,_):
  if at==0x577db6:uc.reg_write(UC_X86_REG_EIP,0x577f1e)
 m.u.hook_add(UC_HOOK_CODE,continuation,begin=0x577db6,end=0x577db6)
 return m


def native_reveal(m,house=None):
 house=m.house if house is None else house
 m.run(0x577d90,(house,),0x87f7e8)


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
 return {'only_site_original':True}
check('default_off',default_off)


def tool_reveal_and_restore():
 m=setup();s=enable(m);assert s.crcMapShadowStatus==1,s.crcMapShadowStatus;installed(m)
 reveal(m)
 assert m.u.mem_read(m.house+0x241,1)[0]==0
 assert m.u.mem_read(m.enemy+0x241,1)[0]==0
 s=enable(m,False);assert s.crcMapShadowStatus==0;restored(m)
 assert m.u.mem_read(m.house+0x241,1)[0]==0
 native_reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==1
 return {'tool_write_suppressed':True,'unload_did_not_restore_game_memory':True,'native_reveal_after_unload':True}
check('tool_reveal_and_restore',tool_reveal_and_restore)


def native_reveal_while_hook_active():
 m=setup();assert enable(m).crcMapShadowStatus==1;installed(m)
 native_reveal(m)
 assert m.u.mem_read(m.house+0x241,1)[0]==1
 installed(m)
 return {'unscoped_native_write_unchanged':True}
check('native_reveal_while_hook_active',native_reveal_while_hook_active)


def actual_native_crc():
 m=setup();assert enable(m).crcMapShadowStatus==1;compute=crc_world(m);baseline=compute()
 reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==0;assert compute()==baseline
 native_reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==1;native=compute()
 assert native!=baseline
 m.w(m.unit+0x9c,34570);moved=compute();assert moved!=native
 m.rng_variant=1;random_changed=compute();assert random_changed!=moved
 enable(m,False);assert compute()==random_changed;restored(m)
 return {'tool_reveal_crc_matches_pre_reveal':True,'native_reveal_changes_crc':True,
         'normal_motion_and_rng_still_change_crc':True,'no_extra_crc_or_rng_calls':True}
check('actual_native_crc',actual_native_crc)


def identity_change_fails_open():
 m=setup();assert enable(m).crcMapShadowStatus==1;installed(m)
 fired=[]
 def mutate(uc,at,size,_):
  if not fired:
   fired.append(True);m.w(m.house+0x10,101)
 h=m.u.hook_add(UC_HOOK_CODE,mutate,begin=SITE,end=SITE)
 try:reveal(m)
 finally:m.u.hook_del(h)
 assert fired and m.u.mem_read(m.house+0x241,1)[0]==1
 s=tick(m);assert s.crcMapShadowStatus<0,s.crcMapShadowStatus
 restored(m)
 return {'identity_change_used_native_store':True,'hook_disabled_after_fault':True}
check('identity_change_fails_open',identity_change_fails_open)


def late_enable():
 m=setup();native_reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==1
 s=enable(m);assert s.crcMapShadowStatus==1,s.crcMapShadowStatus;installed(m)
 reveal(m);assert m.u.mem_read(m.house+0x241,1)[0]==1
 return {'late_enable_safe_for_next_tool_reveal':True}
check('late_enable',late_enable)


for case in ('singleplayer','mode','conflict','mod','pending_off','heartbeat_stop','frame_rewind'):
 def boundary(case=case):
  m=setup(5 if case=='singleplayer' else 3)
  if case=='mode':
   s=state(m);s.flags&=~67108864;save(m,s)
  if case=='conflict':m.u.mem_write(SITE,b'\x90'*len(ORIGINAL))
  if case=='mod':m.legacy_loaded=True
  s=enable(m)
  if case in ('singleplayer','mode','conflict','mod'):
   assert s.crcMapShadowStatus<0,(case,s.crcMapShadowStatus)
   if case=='conflict':
    for address,raw in OTHER_SITES.items():assert bytes(m.u.mem_read(address,len(raw)))==raw
   else:restored(m)
   return {'rejected_before_activation':True,'runtime_status':s.crcMapShadowStatus}
  assert s.crcMapShadowStatus==1;installed(m)
  if case=='pending_off':
   s=state(m);s.crcMapShadow=0;save(m,s);s=tick(m);assert s.crcMapShadowStatus==0;restored(m)
  elif case=='heartbeat_stop':
   s=state(m);s.stop=1;save(m,s);s=tick(m);assert s.stop==2;restored(m)
  elif case=='frame_rewind':
   m.w(0xa8ed84,0);s=tick(m);assert s.crcMapShadowStatus==1;installed(m)
  return {'single_site_lifecycle':True}
 check('boundary_'+case,boundary)


Path(a.output).write_text(json.dumps({
 'scope':__doc__,
 'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
 'game_tested':False,'multiplayer_tested':False,'results':rows
},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

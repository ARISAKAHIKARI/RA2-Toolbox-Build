"""Execute the compiled pause gate, Runtime keys and ownership in x86 emulation.

Original private PE is mapped read-only from disk; callback/input/render and the
ordinary MainLoop body are modeled. This checks gate ABI and actual-frame
observation, not native draw output, full simulation or multiplayer behavior.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct, traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from pdb_fixture_symbol import function_address

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True)
a=p.parse_args();game_hash=hashlib.sha256(Path(a.game).read_bytes()).hexdigest()
# Both supported private samples retain the checked native loop/input/render ABI.
assert game_hash in {'247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0',
 '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'}
rows=[]
def check(name,fn):
 try:fn();rows.append({'case':name,'status':'PASS'})
 except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()

def fixture(mode=5):
 spec=importlib.util.spec_from_file_location('pause_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.trace=[];m.body_advance=False;m.w(0xA8B238,mode);m.b(0xA8ED80,1);m.w(0xA8B230,m.alloc())
 def hook(at,fn):return m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:fn(),begin=at,end=at)
 m.pause_hook=hook
 def simple(label,argc=0,result=0):
  m.trace.append(label);m.ret(result,argc)
 hook(0x48D080,lambda:simple('callback'))
 hook(0x4F42F0,lambda:simple('mark',1))
 hook(0x4F4320,lambda:simple('input',3))
 hook(0x4F4480,lambda:simple('render'))
 update=0x700010
 hook(update,lambda:simple('update'))
 m.tactical=m.r(0x887324);table=m.alloc();m.w(m.tactical,table);m.w(table+0x5c,update)
 def original_body():
  m.trace.append('ordinary-loop')
  if m.body_advance:m.w(0xA8ED84,m.r(0xA8ED84)+1)
  m.ret(0,0)
 hook(0x55D365,original_body)
 for at,name in m.imports.items():
  if name=='MsgWaitForMultipleObjects':
   m.u.hook_del(m.native_stub_hooks[at]);hook(at,lambda:simple('wait',5))
 m.hook();s=state(m);s.moFeatures=16;s.pauseViewKeys[0]=0x74;s.pauseViewKeys[1]=0x75;save(m,s);m.hook()
 return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def message(m,kind,key,repeat=False):
 ptr=m.CONTROL+0x300;m.u.mem_write(ptr,struct.pack('<7I',0,kind,key,0x40000000 if repeat else 0,0,0,0))
 if kind in (0x100,0x104):m.held_keys.add(key)
 if kind in (0x101,0x105):m.held_keys.discard(key)
 m.run(m.entry,(0,1,ptr))
 return m.r(ptr+4)
def tap(m,key):
 assert message(m,0x100,key)==0
 assert message(m,0x101,key)==0
def loop(m):
 m.trace.clear();m.run(0x55D360);return m.trace[:]
def ready(m):assert state(m).pauseViewStatus==1 and bytes(m.u.mem_read(0x55D360,1))==b'\xe9'

def core():
 m=fixture();ready(m);frame=m.r(0xA8ED84);tap(m,0x74)
 assert state(m).pauseViewStatus==2
 trace=loop(m);assert trace==['callback','mark','input','render','update','wait'],trace
 assert m.r(0xA8ED84)==frame
 tap(m,0x75);assert state(m).pauseViewStatus==3
 assert loop(m)==['ordinary-loop'] and m.r(0xA8ED84)==frame
 assert loop(m)==['ordinary-loop'] # no frame advanced: do not consume the step
 m.body_advance=True;assert loop(m)==['ordinary-loop'] and m.r(0xA8ED84)==frame+1
 assert loop(m)==['callback','mark','input','render','update','wait']
 assert state(m).pauseViewStatus==2 and m.r(0xA8ED84)==frame+1
 tap(m,0x74);assert state(m).pauseViewStatus==1
 assert loop(m)==['ordinary-loop']
check('pause-step-observed-frame-and-native-service-order',core)

for reason in ('focus','foreground','menu','heartbeat','stop','scenario','rewind'):
 def reset_case(reason=reason):
  m=fixture();ready(m);tap(m,0x74);tap(m,0x75)
  original=bytes(m.u.mem_read(0x55D360,5))
  if reason=='focus':m.b(0xA8ED80,0)
  if reason=='foreground':s=state(m);s.foreground=0;save(m,s)
  if reason=='menu':m.w(0xA8EDA0,1)
  if reason=='heartbeat':s=state(m);s.heartbeat=m.clock-3001;save(m,s)
  if reason=='stop':s=state(m);s.stop=1;save(m,s)
  if reason=='scenario':m.w(0xA8B230,m.r(0xA8B230)+4)
  if reason=='rewind':m.w(0xA8ED84,m.r(0xA8ED84)-1)
  assert loop(m)==['ordinary-loop']
  # The gate itself cancels pause without mutating its owned entry bytes.
  assert bytes(m.u.mem_read(0x55D360,5))==original
 check('cancel-step-'+reason,reset_case)

for mode in (0,3,4,5,6):
 def session_case(mode=mode):
  m=fixture(mode);s=state(m)
  assert s.pauseViewStatus==(1 if mode in (0,5) else 0 if mode==6 else -1),(mode,s.pauseViewStatus)
  assert (bytes(m.u.mem_read(0x55D360,1))==b'\xe9')==(mode in (0,5))
  if mode not in (0,5):assert message(m,0x100,0x74)==0x100
 check('session-whitelist-'+str(mode),session_case)

def repeat_and_policy():
 m=fixture();ready(m);assert message(m,0x100,0x74)==0
 assert state(m).pauseViewStatus==2
 message(m,0x100,0x74,True);assert state(m).pauseViewStatus==2
 assert message(m,0x101,0x74)==0
 s=state(m);s.pauseViewKeys[0]=0x76;save(m,s);m.hook()
 assert message(m,0x100,0x74)==0x100
 message(m,0x101,0x74)
 tap(m,0x76);assert state(m).pauseViewStatus==1
 s=state(m);s.moFeatures=0;save(m,s);m.hook()
 assert state(m).pauseViewStatus==0 and bytes(m.u.mem_read(0x55D360,5))==b'\xa0\xa0\xe9\xa8\0'
check('repeat-rebinding-and-explicit-owned-restoration',repeat_and_policy)

def signature_failure():
 m=fixture(3);m.w(0xA8B238,5);m.u.mem_write(0x55D365,b'\x90');m.hook()
 assert state(m).pauseViewStatus==-2
 assert bytes(m.u.mem_read(0x55D360,5))==b'\xa0\xa0\xe9\xa8\0'
check('incompatible-native-stack-context-refused',signature_failure)

assert hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==game_hash
result={'scope':__doc__,'game_sha256':game_hash,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'passed':all(r['status']=='PASS' for r in rows),'checks':len(rows),'results':rows,'game_tested':False,'multiplayer_tested':False}
Path(a.output).write_text(json.dumps(result,indent=2)+'\n')
print(('PASS' if result['passed'] else 'FAIL')+': compiled pause gate '+str(len(rows))+' cases')
raise SystemExit(not result['passed'])

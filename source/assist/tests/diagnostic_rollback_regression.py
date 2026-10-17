"""Regression guard for 0.7.12's unconditional engine diagnostic patching.
Executes the actual DLL in Unicorn with synthetic Win32/world. This proves
patch/queue behavior in the fixture, not a real platform disconnect or its cause.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct
from pathlib import Path
from unicorn import UC_HOOK_MEM_WRITE

p=argparse.ArgumentParser()
p.add_argument('game');p.add_argument('dll');p.add_argument('--baseline',required=True)
p.add_argument('--output',required=True);a=p.parse_args();results=[]
sites={0x64cc61:5,0x64c5af:5,0x64c6e9:7,0x64c6f2:6}
def fixture(dll=None):
 spec=importlib.util.spec_from_file_location('rollback_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,dll or a.dll];spec.loader.exec_module(m);return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def check(name,fn):
 fn();results.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
def baseline():
 m=fixture(a.baseline);m.w(0xa8b238,3);s=state(m);s.flags=32768;s.externalDisplay=1;save(m,s)
 before=bytes(m.u.mem_read(0x64c5af,5));s=m.hook()
 assert s.data.status==1 and s.data.inputPatchMask==0
 assert bytes(m.u.mem_read(0x64c5af,5))!=before
 assert bytes(m.u.mem_read(0x64c5af,1))==b'\xe9'
check('reproduce_0712_late_detour_with_only_ore_enabled',baseline)
for mode in (0,3,4):
 for scenario in ('startup','auto_board','observer','stop_reconnect'):
  def run(mode=mode,scenario=scenario):
   m=fixture();m.w(0xa8b238,mode);s=state(m);s.flags=32768;s.externalDisplay=1;save(m,s)
   before={at:bytes(m.u.mem_read(at,n)) for at,n in sites.items()};writes=[]
   h=m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,n,value,_:writes.append((at,n)),begin=0x401000,end=0x7e0fff)
   if scenario=='observer':m.b(m.house+0x1f5,1)
   for frame in (0,1,2,100,168,348):m.w(0xa8ed84,frame);s=m.hook()
   assert s.data.status==(0 if scenario=='observer' else 1)
   if scenario=='auto_board':
    m.u.mem_write(m.t+0x388,struct.pack('<d',4.0))
    m.u.mem_write(m.engT+0x380,struct.pack('<d',1.0))
    s.command=15;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook()
    assert s.ack==s.request and s.data.loadIssued==1
    assert m.r(0x8b41f8 if mode==0 else 0xa802c8)>0
   elif scenario=='stop_reconnect':
    s.stop=1;save(m,s);s=m.hook();assert s.stop==2
    s.stop=0;save(m,s);s=m.hook();assert s.data.status==1
   m.u.hook_del(h)
   assert not writes,('unexpected executable-memory write',writes)
   assert s.data.inputPatchMask==0
   for at,b in before.items():assert bytes(m.u.mem_read(at,len(b)))==b
  check(f'{mode}_{scenario}_no_inline_game_patches',run)
Path(a.output).write_text(json.dumps({
 'inputs':{'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
           'game_exe':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
           'baseline_0712':hashlib.sha256(Path(a.baseline).read_bytes()).hexdigest()},
 'results':results,'scope':__doc__,'real_netplay_tested':False,
 'platform_disconnect_reproduced':False},indent=2)+'\n')

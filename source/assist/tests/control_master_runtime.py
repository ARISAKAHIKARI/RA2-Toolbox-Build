"""Actual compiled Runtime/EntryManager control lifecycle with mocked Win32.
No running game, external platform, network peers or real disconnect is tested.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
initial_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()
SITES=(0x6339BA,0x637E40,0x50B6F0)
AFTER=tuple(bytes.fromhex(s) for s in ('e8577dedff','e8d138edff','e91b000000'))
def fixture(session=3):
 spec=importlib.util.spec_from_file_location('control_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,session);m.w(0xa8b54c,1);m.w(0xa8b230,m.alloc());s=state(m);s.flags=0;s.externalDisplay=1;save(m,s)
 m.originals=tuple(bytes(m.u.mem_read(at,5)) for at in SITES);return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def setmode(m,mode):s=state(m);s.controlAll=mode;save(m,s);return tick(m)
def sites(m):return tuple(bytes(m.u.mem_read(at,5)) for at in SITES)
def stable_data(m):return bytes(m.u.mem_read(m.house+0x1ec,2))+bytes(m.u.mem_read(m.enemy+0x1ec,2))+bytes(m.u.mem_read(0xa83d4c,4))+bytes(m.u.mem_read(m.enemyUnit+0x21c,4))
def assert_master(m,s):assert s.playerControlStatus==2 and sites(m)==AFTER,(s.playerControlStatus,s.data.commandStatus,[x.hex() for x in sites(m)])
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def cold(session):
 m=fixture(session);before=stable_data(m);m.w(m.SH+0x700000+16,0);m.w(0xa8ed84,5000);s=tick(m);assert s.seq==0 and sites(m)==m.originals
 s.controlAll=2;save(m,s);m.w(m.SH+0x700000+16,1);s=tick(m);assert_master(m,s);epoch=s.data.match
 s=setmode(m,0);assert s.playerControlStatus==0 and sites(m)==m.originals
 s=setmode(m,2);assert_master(m,s);assert s.data.match==epoch and stable_data(m)==before
 return {'cold_frame':5002,'same_epoch_on_off_on':True,'House_flags_CurrentPlayer_objectOwner_unchanged':True}
for session in (0,3):check('cold_master_session_'+str(session),lambda session=session:cold(session))
def switches():
 m=fixture();s=setmode(m,1);assert s.playerControlStatus==1 and sites(m)!=m.originals and sites(m)!=AFTER
 s=setmode(m,2);assert_master(m,s)
 s=setmode(m,1);assert s.playerControlStatus==1 and sites(m)!=AFTER
 s=setmode(m,0);assert s.playerControlStatus==0 and sites(m)==m.originals
 return {'native_master_native_off':True}
check('mode_switches',switches)
def focus():
 m=fixture();s=setmode(m,2);assert_master(m,s);epoch=s.data.match
 s.foreground=0;save(m,s);s=tick(m);assert_master(m,s)
 m.w(0xa8eda0,1);s=tick(m);assert s.data.commandStatus==10 and sites(m)==AFTER
 m.w(0xa8eda0,0);s.foreground=1;save(m,s);s=tick(m);assert_master(m,s);assert s.data.match==epoch
 return {'background_and_menu_retain_group':True}
check('focus_menu_retention',focus)
def suspended_off():
 m=fixture();assert_master(m,setmode(m,2));m.b(0xa8e9a0,0);s=setmode(m,0)
 assert s.data.commandStatus==9 and s.playerControlStatus==0 and sites(m)==m.originals
 return {'explicit_off_restores_during_pause':True}
check('paused_explicit_off',suspended_off)
def forbidden_companions():
 m=fixture();s=state(m);s.controlAll=2;s.controlCommandOwner=1;s.networkHousePreserve=1;save(m,s);s=tick(m);assert_master(m,s)
 for at in (0x646ef5,0x646fd2,0x6ffc93,0x6ffe2b,0x649ca0,0x649e1e,0x64b9d4):
  q=m.request(3);q.address=at;result,stats=m.manager_call(q);assert result==1 and stats.active==0,hex(at)
 return {'forged_saved_companion_options_cannot_install':True}
check('master_companion_exclusion',forbidden_companions)
def foreign():
 m=fixture();assert_master(m,setmode(m,2));m.b(0x50b6f0,0xcc);s=setmode(m,0)
 assert s.playerControlStatus==-2 and sites(m)[:2]==AFTER[:2] and bytes(m.u.mem_read(0x50b6f0,1))==b'\xcc'
 s.stop=1;save(m,s);s=tick(m);assert s.stop!=2 and sites(m)[:2]==AFTER[:2]
 return {'foreign_common_preserved_strict_owner_calls_retained_detach_pending':True}
check('foreign_common_retains_cleanup_responsibility',foreign)
def bad_tail():
 m=fixture();m.b(0x50b716,0xcc);s=setmode(m,2);assert s.playerControlStatus==-2 and sites(m)==m.originals
 return {'unknown_strict_leaf_rejects_entire_group':True}
check('target_abi_preflight',bad_tail)
def manager():
 m=fixture();tick(m);q=m.request(0);result,lease=m.manager_call(q);assert result==1
 def patch(at,before,after):
  q=m.request(2);q.token=lease.token;q.address=at;q.length=5
  for i in range(5):q.before[i]=before[i];q.after[i]=after[i]
  return m.manager_call(q)[0]
 # Common cannot be independently forced true without owned strict CALLs.
 assert patch(SITES[2],m.originals[2],AFTER[2])==-5
 # An arbitrary in-game destination never gains the exact Master exception.
 bad=bytes.fromhex('e91c000000');assert patch(SITES[2],m.originals[2],bad)==-5
 assert sites(m)==m.originals
 lease.operation=1;assert m.manager_call(lease)[0]==1
 return {'direct_common_without_owned_calls_denied':True,'arbitrary_game_target_denied':True}
check('actual_manager_patch_admission',manager)
final_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();assert initial_hash==final_hash,'DLL changed during fixture'
Path(a.output).parent.mkdir(parents=True,exist_ok=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':final_hash,'hash_stable_during_test':True,'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

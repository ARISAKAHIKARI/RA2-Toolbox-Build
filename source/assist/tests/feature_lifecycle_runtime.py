"""Compiled Runtime lifecycle in a synthetic playable world; Win32 is mocked.
No platform connection, full simulation or multiplayer disconnect is tested.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, traceback
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--detailed-gates',action='store_true');a=p.parse_args();rows=[]
def fixture():
 spec=importlib.util.spec_from_file_location('lifecycle_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 # These historic toggle tests have no remote peer/transport. Explicitly
 # identify the isolated multiplayer fixture as a one-player session.
 m.w(0xa8b238,3);m.w(0xa8b54c,1)
 # The older entry fixture predates scenario-identity ownership tracking. A
 # playable synthetic world must provide the ScenarioClass singleton too.
 m.w(0xa8b230,m.alloc());s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
SITE=0x4a8eb0;RAW=bytes.fromhex('a14c3da800')
def placement(pre_enabled):
 m=fixture();s=state(m);s.flags=65536 if pre_enabled else 0;save(m,s);s=tick(m);epoch=s.data.match
 if not pre_enabled:
  assert bytes(m.u.mem_read(SITE,5))==RAW
  s.flags=65536;save(m,s);s=tick(m)
 assert s.data.match==epoch and s.data.placementStatus==1 and bytes(m.u.mem_read(SITE,1))==b'\xe9'
 s.flags=0;save(m,s);s=tick(m)
 assert s.data.placementStatus==0 and bytes(m.u.mem_read(SITE,5))==RAW
 s.flags=65536;save(m,s);s=tick(m)
 assert s.data.match==epoch and s.data.placementStatus==1 and bytes(m.u.mem_read(SITE,1))==b'\xe9'
 return {'same_epoch_on_off_on':True}
for pre in (False,True):check('placement_'+('before_first_ready' if pre else 'midgame'),lambda pre=pre:placement(pre))
def cold(display):
 m=fixture();s=state(m);s.flags=0;s.externalDisplay=display;save(m,s)
 # The live game already advanced while no tactical subscription existed.
 m.w(0xa8ed84,5000);m.w(m.CONTROL+16,0)
 s=tick(m);assert s.seq==0 and bytes(m.u.mem_read(SITE,5))==RAW
 # First enable creates the tactical subscription and publishes its options
 # before the first Assist callback, exactly as ensureBridge does.
 s.flags=65536;save(m,s);m.w(m.CONTROL+16,1);s=tick(m)
 assert s.data.frame>=5000 and s.data.commandStatus==1 and s.data.placementStatus==1
 epoch=s.data.match;s.flags=0;save(m,s);s=tick(m)
 assert s.data.match==epoch and s.data.placementStatus==0 and bytes(m.u.mem_read(SITE,5))==RAW
 s.flags=65536;save(m,s);s=tick(m)
 assert s.data.match==epoch and s.data.placementStatus==1
 return {'first_tactical_callback_at_frame':5002,'external_display':display,'cold_enable_off_on':True}
for display in (0,1):check('cold_transport_first_enable_display_'+str(display),lambda display=display:cold(display))
def suspension():
 m=fixture();s=tick(m);epoch=s.data.match;m.b(0xa8e9a0,0);s.flags=65536;save(m,s);s=tick(m)
 assert s.data.commandStatus==(9 if a.detailed_gates else 0) and s.data.match==epoch and bytes(m.u.mem_read(SITE,5))==RAW
 m.b(0xa8e9a0,1);s=tick(m)
 assert s.data.match==epoch and s.data.commandStatus==1 and s.data.placementStatus==1
 return {'deferred_then_applied':True}
check('suspension_defers_request',suspension)
def focus():
 m=fixture();s=tick(m);epoch=s.data.match;s.foreground=0;s.flags=65536;save(m,s);s=tick(m)
 assert s.data.commandStatus==1 and s.data.match==epoch and s.data.placementStatus==1
 s.foreground=1;save(m,s);s=tick(m)
 assert s.data.match==epoch and s.data.placementStatus==1
 return {'background_does_not_revoke_world_or_option':True}
check('focus_toggle_same_world',focus)
def menu():
 m=fixture();s=tick(m);epoch=s.data.match;m.w(0xa8eda0,1);s.flags=65536;save(m,s);s=tick(m)
 assert s.data.commandStatus==(10 if a.detailed_gates else 0) and s.data.match==epoch and bytes(m.u.mem_read(SITE,5))==RAW
 m.w(0xa8eda0,0);s=tick(m)
 assert s.data.commandStatus==1 and s.data.match==epoch and s.data.placementStatus==1
 return {'menu_gate_then_automatic_apply':True}
check('menu_defers_request',menu)
def computation():
 m=fixture();s=tick(m);epoch=s.data.match;old=s.computeApplied;s.computeRequested=old^1;save(m,s);s=tick(m)
 assert s.computeApplied==old and s.data.match==epoch
 m.w(0xa8ed84,0);s=m.hook()
 assert s.data.match!=epoch and s.computeApplied==(old^1)
 return {'midgame_pending_then_new_epoch_applied':True}
check('compute_split_new_epoch_only',computation)
def signature():
 m=fixture();raw=bytes(m.u.mem_read(0x6aa600,8));m.b(0x6aa600,0xcc);s=tick(m)
 assert s.data.status==1 and s.data.commandStatus==3
 m.u.mem_write(0x6aa600,raw);m.clock+=1100;s=tick(m)
 assert s.data.status==1 and s.data.commandStatus==1
 return {'temporary_failure_retried':True,'exact_signature_still_required':True}
check('startup_signature_failure_retried',signature)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

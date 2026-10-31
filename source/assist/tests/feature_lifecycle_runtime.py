"""Compiled Runtime lifecycle in a synthetic playable world; Win32 is mocked.
No platform connection, full simulation or multiplayer disconnect is tested.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, traceback
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture():
 spec=importlib.util.spec_from_file_location('lifecycle_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,3);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);return m
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
 return {'same_epoch_toggle':True}
for pre in (False,True):check('placement_'+('before_first_ready' if pre else 'midgame'),lambda pre=pre:placement(pre))
def suspension():
 m=fixture();s=tick(m);epoch=s.data.match;m.b(0xa8e9a0,0);s.flags=65536;save(m,s);s=tick(m)
 assert s.data.commandStatus==0 and s.data.match==epoch and bytes(m.u.mem_read(SITE,5))==RAW
 m.b(0xa8e9a0,1);s=tick(m)
 assert s.data.match==epoch and s.data.commandStatus==1 and s.data.placementStatus==1
 return {'deferred_then_applied':True}
check('suspension_defers_request',suspension)
def computation():
 m=fixture();s=tick(m);epoch=s.data.match;old=s.computeApplied;s.computeRequested=old^1;save(m,s);s=tick(m)
 assert s.computeApplied==old and s.data.match==epoch
 m.w(0xa8ed84,0);s=m.hook()
 assert s.data.match!=epoch and s.computeApplied==(old^1)
 return {'midgame_pending_then_new_epoch_applied':True}
check('compute_split_new_epoch_only',computation)
def signature():
 m=fixture();raw=bytes(m.u.mem_read(0x6aa600,8));m.b(0x6aa600,0xcc);s=tick(m)
 assert s.data.status==-1
 m.u.mem_write(0x6aa600,raw);s=tick(m);assert s.data.status==-1
 return {'first_failure_is_cached':True,'not_evidence_of_platform_mismatch':True}
check('startup_signature_failure_cached',signature)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

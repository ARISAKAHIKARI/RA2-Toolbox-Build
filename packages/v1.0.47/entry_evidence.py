"""Strict receipts for the pre.6 feature-entry checks.

This module validates existing executed reports. It does not execute tests,
manufacture successful evidence, or claim Windows/game acceptance.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re

BINDINGS = {
 'console_display_entry': (
  {'source/suite/tests/console_display_entry_test.py','source/assist/tests/settings_dependencies.cpp'} |
  {'source/suite/tests/fixtures/entry_pre1475/'+p for p in ('display_settings.h','video_settings_ui.h','settings_dependencies.h')},
  {'source/suite/source/'+p for p in ('display_settings.h','video_settings_ui.h','settings_categories.h','modern_console_main.h')} |
  {'source/assist/source/'+p for p in ('main.cpp','auto_settings_ui.h','extra_settings_io.h','settings_dependencies.h','watch_scene.h')}),
 'hotkey_entry_recovery': (
  {'source/assist/tests/hotkey_entry_recovery.py','source/assist/tests/hotkey_command_context.py'},
  {'source/assist/source/'+p for p in ('main.cpp','hotkey_host.h','hotkey_routes.h','middle_input_host.h','middle_input_owner.h','formation_host.h','automatic_hotkeys_host.h','automatic_features.h','input_ownership.h','control_link.h','protocol.h','read_only_lifecycle.h','additional_settings.h','type_hotkey.h','mo_orders.h')} | {'source/shared/source/key_gesture.h'}),
 'display_entry_clock': (
  {'source/assist/tests/display_entry_clock.py','source/assist/tests/hotkey_command_context.py','source/assist/tests/fixtures/entry_pre1475/display_entry_clocks.json','source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py'},
  {'source/assist/source/'+p for p in ('main.cpp','watch_host.h','idle_aircraft_selection.h','aircraft_indicator.h','clock_ms.h','watch_world_lifetime.h','world_lifecycle.h','protocol.h','read_only_lifecycle.h')}),
 'read_only_lifecycle': (
  {'source/assist/tests/read_only_lifecycle.cpp'},
  {'source/assist/source/'+p for p in ('read_only_lifecycle.h','protocol.h','main.cpp','bridge.cpp')}),
 'read_only_lifecycle_integration': (
  {'source/assist/tests/'+p for p in ('read_only_lifecycle_integration.py','read_only_lifecycle.cpp',
   'cooperative_read_only_recovery.py','fps_read_only_recovery.py','native_readonly_pairing.py',
   'formation_quick_bridge_test.py','fixtures/read_only_pre1475/main.cpp',
   'fixtures/read_only_pre_cooperative/read_only_lifecycle.h','fixtures/entry_pre1475/single_player_fps_runtime.h',
   'fixtures/read_only_pre_pairing/native_message_policy.h')} |
  {'source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py'},
  {'source/assist/source/'+p for p in ('read_only_lifecycle.h','protocol.h','main.cpp','bridge.cpp','pointer_host.h',
   'native_message_policy.h','tech_ui_breakpoint_bridge.h','activation_diagnostic.h','single_player_fps_runtime.h',
   'single_player_fps.h','cooperative_bridge.h','cooperative_plan.h','cooperative_types.h','plan_freshness.h',
   'formation_bridge.h','world_frame_runtime.h','world_lifecycle.h','session_epoch.h')}),
}
CLOCK_BASELINE_SHA = '4889fb338cd23508036b5dc0d4be14161798a3de2e1ac194c9ae6ff76eba5ff3'
CLOCK_FAILURES = {'render_clock_behind':5,'render_clock_far_ahead':5,
 'stale_real_clock_render_frozen':4,'aircraft_at_500ms':5,'aircraft_after_500ms':1,
 'display_before_2000ms':1,'display_at_2000ms':4,'zero_stamp_rejected':1,
 'watch_read_failure_recovery':1,'selection_epoch_recovery':2}
CONSOLE_FIXTURES = {
 'display_settings.h':'a26736afe50c92049789f77340695d5ffbd56f5b0f3fb8d37de3a2b1d2673772',
 'settings_dependencies.h':'b9671635669d63170a297d65cd02e37978be783822c30a80399f22ab9c5794f1',
 'video_settings_ui.h':'1e584429f6ab18a3e22cd69931e707bf17edc5ffb3cf3bc6f989a9b667cfc240'}
CONSOLE_FAILURES = ['FAIL missing SuperweaponPanel display spec','FAIL missing DetailedStatus display spec',
 'FAIL live camera motion owner is independent of unavailable renderer',
 'FAIL live camera motion visible switch is independently enabled','FAIL motion switch reaches shared setting owner']
INPUT_CASES = {'formation_current_match_wm','formation_old_match_must_not_steal',
 'formation_lease_blocks_immediate_wm','automatic_busy_command','automatic_busy_selection',
 'fanout_local_once_command_waits','busy_game_command_remains_serial',
 'local_focus_recovery','local_chat_recovery','local_pause_recovery',
 'formation_old_wm_after_reregister','formation_old_wm_before_context_resample',
 'formation_old_wm_after_epoch_change','formation_owner_cancel_preserves_fresh_message','formation_context_timestamp_wrap'}

READONLY_COMMIT = '18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9'
READONLY_BASELINE = 'source/assist/tests/fixtures/read_only_pre1475/main.cpp'
READONLY_BASELINE_SHA = 'cedea56e1c259dca6c425b55f3e7e98dbe1ad3926f1fcb9b843c27d439c4fdc1'
READONLY_HOST_CASES = {'enabled_control','paused_secondary_cold','paused_secondary_cursor_only',
 'paused_secondary_after_warm','reenable_control','paused_without_secondary_control'}
READONLY_RUNTIME_CASES = {'normal_control_entry','cold_readonly_publication','pending_ack_no_replay',
 'delayed_retirement_fast_resume','held_order_retirement_recovery','readonly_world_replacement',
 'game_pause_and_focus_loss','world_unreadable_exit_recovery','cold_readonly_compute_initialization',
 'same_match_compute_preserved','paused_match_compute_initialization','demand_disable_reenable'}
READONLY_FAILURE_CASES = {'paused_secondary_cold':2,'paused_secondary_cursor_only':2,'paused_secondary_after_warm':1}
READONLY_FAILURES = ['CHECK_FAILED paused secondary cold start receives new world samples',
 'CHECK_FAILED paused secondary requests its producer'] * 2 + ['CHECK_FAILED pausing preserves continued fresh secondary publication']
STAGES = {
 'cooperative_recovery': {
  'suite':'cooperative_read_only_recovery','driver':'source/assist/tests/cooperative_read_only_recovery.py',
  'fixture':'source/assist/tests/fixtures/read_only_pre_cooperative/read_only_lifecycle.h',
  'fixture_sha256':'8bb040dd7ee9081d1ed0b1fbc09bf36445a173a22e19dd45d9934cb228281d87',
  'replaced':'source/assist/source/read_only_lifecycle.h','checks':11,'scenarios':None,
  'failures':['FAIL completed old cooperative click must not be replayed after pause/resume',
              'FAIL unconsumed old cooperative click must be cancelled by pause'],
  'production':{'source/assist/source/'+p for p in ('protocol.h','read_only_lifecycle.h','cooperative_bridge.h','cooperative_plan.h')}},
 'fps_recovery': {
  'suite':'fps_read_only_recovery','driver':'source/assist/tests/fps_read_only_recovery.py',
  'fixture':'source/assist/tests/fixtures/entry_pre1475/single_player_fps_runtime.h',
  'fixture_sha256':'3e4d867c0586b96bdb59eae730784d90607a9ea86a453411fd40d6ac16a8f39d',
  'replaced':'source/assist/source/single_player_fps_runtime.h','checks':16,'scenarios':6,
  'failures':['FAIL readonly request immediately closes FPS admission','FAIL readonly request immediately closes FPS callback',
   'FAIL retirement maintenance must not rearm FPS','FAIL FPS retirement permits observation acknowledgement',
   'FAIL unacknowledged rapid resume keeps FPS fenced','FAIL pending retirement cannot acquire FPS tag',
   'FAIL Runtime active retirement keeps FPS fenced after boolean clear','FAIL Runtime active retirement cannot rearm FPS'],
  'production':{'source/assist/source/'+p for p in ('protocol.h','read_only_lifecycle.h','single_player_fps_runtime.h','single_player_fps.h','tech_ui_breakpoint_bridge.h')}},
 'input_pairs': {
  'suite':'native_readonly_pairing','driver':'source/assist/tests/native_readonly_pairing.py',
  'fixture':'source/assist/tests/fixtures/read_only_pre_pairing/native_message_policy.h',
  'fixture_sha256':'959b707cedaca064e1b3c44030b152da14d697035093e1e0007d22bd07bf8569',
  'replaced':'source/assist/source/native_message_policy.h','checks':188,'scenarios':45,
  'failures':['FAIL read-only release balances formation down after real reset'] * 18 +
   ['FAIL readonly autorepeat preserves original release ownership'] +
   ['FAIL original release remains balanced after acknowledged resume'] * 18,
  'production':{'source/assist/source/'+p for p in ('native_message_policy.h','read_only_lifecycle.h','formation_bridge.h')}},
}

def digest(data):return hashlib.sha256(data).hexdigest()
def require(ok, reason):
 if not ok:raise ValueError(reason)
def unique(text):
 def pairs(rows):
  out={}
  for k,v in rows:
   if k in out:raise ValueError('Duplicate JSON key: '+k)
   out[k]=v
  return out
 return json.loads(text,object_pairs_hook=pairs)
def integer(value, minimum=0):return type(value) is int and value>=minimum
def summary(row, scenarios, checks, failure='failures', passed=True):
 return (isinstance(row,dict) and row.get('passed') is passed and
  integer(row.get('scenarios'),scenarios) and integer(row.get('checks'),checks) and
  type(row.get(failure)) is int and (row[failure]==0 if passed else row[failure]>0))
def cases_ok(rows, result, required=(), failures=None):
 if not isinstance(rows,list) or len(rows)!=result.get('scenarios'):return False
 names=[x.get('case') for x in rows if isinstance(x,dict)]
 if len(names)!=len(rows) or any(not isinstance(x,str) or not x for x in names) or len(set(names))!=len(names) or not set(required)<=set(names):return False
 if failures is None:return all(x.get('passed') is True and x.get('failures',0)==0 for x in rows)
 return all(x.get('passed') is (x['case'] not in failures) and type(x.get('failures')) is int and x['failures']==failures.get(x['case'],0) for x in rows)
def zero(row,key):return type(row.get(key)) is int and row[key]==0
def sha_string(value):return isinstance(value,str) and re.fullmatch('[0-9a-f]{64}',value) is not None
def strings(value):return isinstance(value,list) and bool(value) and all(isinstance(v,str) and v for v in value)
def source_map(value):
 return isinstance(value,dict) and bool(value) and all(isinstance(p,str) and p.startswith('source/') and
  '\\' not in p and not any(v in ('','..','.') for v in p.split('/')) and sha_string(d) for p,d in value.items())
def json_rows(text):return [unique(line) for line in text.splitlines() if line.startswith('{')]

def lifecycle_branch(value,suite,scenarios,checks,required,failures=None):
 if not isinstance(value,dict):return False
 expected=failures or {}
 result=value.get('summary',{})
 return (zero(value,'compile_exit_code') and type(value.get('run_exit_code')) is int and value['run_exit_code']==int(bool(expected)) and
  strings(value.get('compile_command')) and '-UNDEBUG' in value['compile_command'] and strings(value.get('run_command')) and
  isinstance(result,dict) and type(result.get('passed')) is bool and all(type(result.get(k)) is int for k in ('scenarios','checks','failures')) and
  result=={'suite':suite,'passed':not bool(expected),'scenarios':scenarios,'checks':checks,'failures':sum(expected.values())} and
  cases_ok(value.get('cases'),result,required,failures=expected) and
  isinstance(value.get('stdout'),str) and isinstance(value.get('stderr'),str) and
  sha_string(value.get('run_log_sha256')) and sha_string(value.get('generated_cpp_sha256')) and
  source_map(value.get('header_dependencies')))

def recovery_stage(name,value):
 spec=STAGES[name]
 if not isinstance(value,dict) or type(value.get('schema')) is not int or value['schema']!=1 or value.get('suite')!=spec['suite'] or value.get('passed') is not True:return False
 current=value.get('current',{});negative=value.get('negative_control',{});old=negative.get('result',{})
 for row,failures in ((current,0),(old,len(spec['failures']))):
  execution=row.get('execution',{})
  if not (zero(row,'compile_exit') and type(row.get('run_exit')) is int and row['run_exit']==int(bool(failures)) and
   strings(row.get('compile_argv')) and integer(execution.get('checks')) and execution['checks']==spec['checks'] and
   type(execution.get('failures')) is int and execution['failures']==failures and
   (execution.get('scenarios') is None if spec['scenarios'] is None else type(execution.get('scenarios')) is int and execution['scenarios']==spec['scenarios']) and
   sha_string(row.get('generated_cpp_sha256')) and sha_string(row.get('raw_run_sha256')) and source_map(row.get('sources'))):return False
 if not (negative.get('passed') is True and negative.get('fixture_sha256')==spec['fixture_sha256'] and
  negative.get('expected_failures')==spec['failures'] and negative.get('observed_failures')==spec['failures'] and
  source_map(value.get('sources')) and {spec['driver'],spec['fixture']}<=set(value['sources']) and
  value['sources'][spec['fixture']]==spec['fixture_sha256'] and spec['production']<=set(current['sources'])):return False
 if name=='fps_recovery' and negative.get('provenance_commit')!=READONLY_COMMIT:return False
 if name!='fps_recovery' and not isinstance(negative.get('fixture_provenance'),str):return False
 if any(value['sources'].get(path)!=digest for path,digest in current['sources'].items()):return False
 return old['sources']==(current['sources']|{spec['replaced']:spec['fixture_sha256']})

def lifecycle_integration(report):
 if not (report.get('suite')=='read_only_lifecycle_integration' and report.get('passed') is True and
  report.get('windows_executed') is False and report.get('game_tested') is False and source_map(report.get('sources')) and
  set().union(*BINDINGS['read_only_lifecycle_integration'])<=set(report['sources']) and
  report['sources'].get(READONLY_BASELINE)==READONLY_BASELINE_SHA and sha_string(report.get('run_log_sha256'))):return False
 host=report.get('host_current',{});runtime=report.get('runtime_current',{});old=report.get('negative_control',{})
 if not (lifecycle_branch(host,'paused_secondary_lifecycle',6,21,READONLY_HOST_CASES) and
  lifecycle_branch(runtime,'read_only_runtime_integration',12,48,READONLY_RUNTIME_CASES) and
  lifecycle_branch(old,'paused_secondary_lifecycle',6,21,READONLY_HOST_CASES,READONLY_FAILURE_CASES) and
  old.get('reproduced') is True and old.get('source_commit')==READONLY_COMMIT and old.get('fixture_sha256')==READONLY_BASELINE_SHA and
  old.get('expected_failure_cases')==sorted(READONLY_FAILURE_CASES)):return False
 stages=report.get('additional_stages')
 if not isinstance(stages,dict) or set(stages)!=set(STAGES):return False
 for name,stage in stages.items():
  if not (isinstance(stage,dict) and zero(stage,'exit_code') and strings(stage.get('command')) and
   '--negative-control' in stage['command'] and '--out' in stage['command'] and
   any(v.replace('\\','/').endswith(STAGES[name]['driver']) for v in stage['command']) and
   isinstance(stage.get('driver_stdout'),str) and isinstance(stage.get('driver_stderr'),str) and
   sha_string(stage.get('driver_log_sha256')) and sha_string(stage.get('report_sha256')) and
   isinstance(stage.get('raw_logs'),dict) and {'current/run.log','negative-control/run.log'}<=set(stage['raw_logs']) and
   all(isinstance(p,str) and sha_string(d) for p,d in stage['raw_logs'].items()) and
   recovery_stage(name,stage.get('report'))):return False
  if any(report['sources'].get(path)!=digest for path,digest in stage['report']['sources'].items()):return False
 expected_checks=69+sum(stage['report']['current']['execution']['checks'] for stage in stages.values())
 expected_scenarios=18+sum(stage['report']['current']['execution'].get('scenarios',0) for stage in stages.values())
 return type(report.get('checks')) is int and report['checks']==expected_checks and type(report.get('scenarios')) is int and report['scenarios']==expected_scenarios

def success(name,text):
 try:
  r=unique(text)
  if not isinstance(r,dict) or r.get('passed') is not True:return False
  if name=='hotkey_entry_recovery':
   return zero(r,'compile_exit') and zero(r,'run_exit') and summary(r.get('summary'),33,77) and cases_ok(r.get('cases'),r['summary'],INPUT_CASES)
  if name=='console_display_entry':
   n=r.get('negative_control',{});old=n.get('result',{})
   return (zero(r,'compile_exit') and zero(r,'run_exit') and summary(r.get('execution'),9,47,'failed') and
    n.get('passed') is True and n.get('provenance_commit')=='18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9' and
    n.get('fixture_sha256')==CONSOLE_FIXTURES and type(n.get('driver_exit')) is int and n['driver_exit']==1 and
    n.get('expected_failures')==CONSOLE_FAILURES and n.get('observed_failures')==CONSOLE_FAILURES and
    zero(old,'compile_exit') and type(old.get('run_exit')) is int and old['run_exit']==1 and old.get('passed') is False and
    old.get('execution')=={'passed':False,'scenarios':7,'checks':16,'failed':5})
  if name=='display_entry_clock':
   cur=r.get('results',{}).get('current',{});old=r.get('results',{}).get('pre5',{})
   return (r.get('baseline_fixture_sha256')==CLOCK_BASELINE_SHA and r.get('windows_executed') is False and r.get('game_tested') is False and
    zero(cur,'compile_exit_code') and zero(cur,'exit_code') and summary(cur.get('summary'),14,54) and cases_ok(cur.get('cases'),cur['summary']) and
    zero(old,'compile_exit_code') and type(old.get('exit_code')) is int and old['exit_code']==1 and
    old.get('summary')=={'suite':'display_entry_clock','passed':False,'scenarios':14,'checks':54,'failures':29} and
    cases_ok(old.get('cases'),old['summary'],failures=CLOCK_FAILURES))
  if name=='read_only_lifecycle':
   return r.get('suite')==name and summary(r,8,68) and isinstance(r.get('scope'),str) and bool(r['scope'])
  if name=='read_only_lifecycle_integration':
   return lifecycle_integration(r)
 except (ValueError,TypeError,KeyError,AttributeError,IndexError):return False
 return False

def contained(root,relative):
 require(isinstance(relative,str) and relative and '\\' not in relative,'Invalid entry evidence path')
 parts=Path(relative).parts
 require(not Path(relative).is_absolute() and all(p not in ('.','..') for p in parts) and
  Path(relative).as_posix()==relative,'Entry evidence path is not canonical relative path: '+relative)
 path=(root/relative).resolve()
 require(path.is_relative_to(root.resolve()),'Entry evidence escaped report directory: '+relative)
 return path

def merge_sources(target,values):
 require(source_map(values),'Entry source map is empty or malformed')
 for path,value in values.items():
  require(path not in target or target[path]==value,'Conflicting entry source identities: '+path)
  target[path]=value

def lifecycle_logs(report,root,expected,embedded):
 """Verify actual binary streams plus nested driver/report receipt bytes."""
 def add(relative,raw=None,sha=None):
  path=contained(root,relative);actual=path.read_bytes()
  require(raw is None or actual==raw,'Entry execution bytes differ: '+relative)
  require(sha is None or digest(actual)==sha,'Entry execution digest differs: '+relative)
  expected[path]=actual
  return actual
 merge_sources(embedded,report['sources'])
 combined=b''
 for key,directory in (('host_current','host-current'),('negative_control','host-pre5-negative'),('runtime_current','runtime-current')):
  value=report[key];raw=(value['stdout']+value['stderr']).encode()
  add(directory+'/run.log',raw,value['run_log_sha256']);combined+=raw
  require(json_rows(raw.decode())==value['cases']+[value['summary']],'Lifecycle cases differ from binary execution: '+directory)
  failures=[line for line in raw.decode().splitlines() if line.startswith('CHECK_FAILED ')]
  require(failures==(READONLY_FAILURES if key=='negative_control' else []),'Lifecycle assertion failures differ: '+directory)
  merge_sources(embedded,value['header_dependencies'])
 for name,stage in report['additional_stages'].items():
  spec=STAGES[name];child=stage['report']
  driver=(stage['driver_stdout']+stage['driver_stderr']).encode()
  add(name+'/driver.log',driver,stage['driver_log_sha256'])
  rows=json_rows(stage['driver_stdout'])
  execution=child['current']['execution']
  require(len(rows)==1 and rows[0].get('suite')==spec['suite'] and rows[0].get('passed') is True and
   rows[0].get('negative_control') is True and type(rows[0].get('checks')) is int and rows[0]['checks']==execution['checks'] and
   type(rows[0].get('failures')) is int and rows[0]['failures']==0 and
   ('scenarios' not in rows[0] or type(rows[0]['scenarios']) is int and rows[0]['scenarios']==execution.get('scenarios')),
   'Nested driver stdout is not its actual stage result: '+name)
  raw_report=add(name+'/result.json',sha=stage['report_sha256'])
  require(unique(raw_report)==child,'Nested stage report differs from parent receipt: '+name)
  merge_sources(embedded,child['sources'])
  for relative,raw_digest in stage['raw_logs'].items():
   require(relative.endswith('.log'),'Nested raw evidence must be a log: '+name)
   contained(root/name,relative)
   combined+=add(name+'/'+relative,sha=raw_digest)
  for branch,value in (('current',child['current']),('negative-control',child['negative_control']['result'])):
   raw=add(name+'/'+branch+'/run.log',sha=value['raw_run_sha256'])
   require(json_rows(raw.decode())==[value['execution']],'Nested binary summary differs: '+name+'/'+branch)
   failures=[line for line in raw.decode().splitlines() if line.startswith('FAIL ')]
   require(failures==(spec['failures'] if branch=='negative-control' else []),'Nested negative failures differ: '+name+'/'+branch)
   if branch=='current':merge_sources(embedded,value['sources'])
   else:
    normalized={spec['fixture'] if p==spec['replaced'] else p:d for p,d in value['sources'].items()}
    merge_sources(embedded,normalized)
 add('run.log',combined,report['run_log_sha256'])

def validate(name,report,report_path,check,source_inputs,tracked):
 """Bind raw execution and embedded dependencies to the completed frozen build."""
 require(success(name,json.dumps(report)),'Malformed entry execution report: '+name)
 root=report_path.parent.resolve()
 expected={}
 embedded={}
 if name=='hotkey_entry_recovery':
  expected[root/'run.log']=(report['stdout']+report['stderr']).encode()
  require(digest(expected[root/'run.log'])==report['run_log_sha256'],'Entry run log differs from its executed digest')
  rows=[unique(x) for x in report['stdout'].splitlines()]
  require(rows==report['cases']+[report['summary']],'Entry cases differ from actual execution')
  embedded.update(report['source_sha256'])
  for path,value in report.get('production_header_sha256',{}).items():
   parts=Path(path).parts
   require('source' in parts,'Entry dependency outside source')
   embedded['/'.join(parts[parts.index('source'):])]=value
 elif name=='console_display_entry':
  current=(json.dumps(report['execution'],separators=(',',':'))+'\n').encode()
  old=report['negative_control'];oldraw=('\n'.join(CONSOLE_FAILURES)+'\n'+json.dumps(old['result']['execution'],separators=(',',':'))+'\n').encode()
  expected[root/'run.log']=current;expected[root/'pre5/run.log']=oldraw
  require(digest(oldraw)==old['run_log_sha256'],'Console negative run digest differs')
  for value in (report,old['result']):embedded.update({'source/'+p:d for p,d in value['sources'].items()})
  path=root/'settings-dependencies.log';raw=path.read_bytes()
  result=re.fullmatch(r'PASS: settings dependencies \((\d+) controls\), independent consumers and saved-child restoration\s*',raw.decode())
  require(result is not None and int(result.group(1))>=101,'Missing actual settings dependency checks')
  expected[path]=raw
 elif name=='display_entry_clock':
  for branch,value in report['results'].items():
   path=root/branch/'run.log';raw=path.read_bytes()
   rows=[unique(x) for x in raw.decode().splitlines() if x.startswith('{')]
   require(rows==value['cases']+[value['summary']],'Clock cases differ from actual execution')
   require(digest(raw)==value['run_log_sha256'],'Clock run digest differs')
   expected[path]=raw
  embedded.update(report['source_sha256'])
 elif name=='read_only_lifecycle_integration':
  lifecycle_logs(report,root,expected,embedded)
 elif name=='read_only_lifecycle':
  pass # The actual C++ stdout is the outer report; source bindings are checked below.
 supporting=check.get('supporting_logs',{})
 require(isinstance(supporting,dict),'Invalid entry supporting bindings: '+name)
 normalized={}
 for key,value in supporting.items():
  require(isinstance(key,str) and '\\' not in key,'Invalid supporting log path')
  path=(Path(key) if Path(key).is_absolute() else root/key).resolve()
  require(path.is_relative_to(root) and path.suffix in ('.log','.json') and path not in normalized,
   'Supporting evidence outside report directory or duplicated: '+key)
  normalized[path]=value
 require(set(expected)<=set(normalized),'Missing current/negative entry run bindings: '+name)
 logs={}
 for path,value in normalized.items():
  raw=path.read_bytes()
  require(path not in expected or raw==expected[path],'Entry execution bytes changed: '+str(path))
  require(isinstance(value,dict) and type(value.get('bytes')) is int and value=={'bytes':len(raw),'sha256':digest(raw)},'Entry supporting bytes changed: '+str(path))
  logs[str(path)]=value
 if name=='read_only_lifecycle':
  for key,required in zip(('test_source_hashes','production_source_hashes'),BINDINGS[name]):
   bindings=check.get(key,{})
   require(isinstance(bindings,dict) and required<=set(bindings),'Missing read-only helper source bindings')
   merge_sources(embedded,bindings)
 require(embedded and all(isinstance(p,str) and p in source_inputs and d==source_inputs[p]['sha256']==digest(tracked(p)) for p,d in embedded.items()),'Entry report sources differ from frozen source: '+name)
 return logs

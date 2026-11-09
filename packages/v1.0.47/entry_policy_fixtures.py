"""Synthetic entry receipts for packaging-policy tests only.

These do not execute production and must never be used as release evidence.
"""
import json


def make(package, name, sources):
 e=package.ENTRY
 hashes={p:package.sha(v) for p,v in sources.items()}
 def encoded(value):return (json.dumps(value,separators=(',',':'))+'\n').encode()
 def source_map(paths):return {p:hashes[p] for p in sorted(paths)}
 inputs=set().union(*e.BINDINGS[name])
 support={}
 if name=='hotkey_entry_recovery':
  names=sorted(e.INPUT_CASES)+['synthetic-entry-'+str(i) for i in range(33-len(e.INPUT_CASES))]
  cases=[{'case':n,'passed':True} for n in names]
  summary={'checks':77,'scenarios':33,'failures':0,'passed':True}
  raw=b''.join(encoded(row) for row in cases+[summary]);support['run.log']=raw
  r={'passed':True,'compile_exit':0,'run_exit':0,'cases':cases,'summary':summary,
     'source_sha256':source_map(inputs),'production_header_sha256':{},'stdout':raw.decode(),'stderr':'',
     'run_log_sha256':package.sha(raw)}
 elif name=='console_display_entry':
  current={'passed':True,'scenarios':9,'checks':47,'failed':0}
  old={'passed':False,'scenarios':7,'checks':16,'failed':5}
  paths={p for p in inputs if '/fixtures/' not in p and '/tests/' not in p}
  fixtures={p for p in inputs if '/fixtures/' in p}
  current_sources={p.removeprefix('source/'):hashes[p] for p in paths}
  old_sources=current_sources|{p.removeprefix('source/'):hashes[p] for p in fixtures}
  # The older header paths replace their current counterparts in the report.
  for p in ('suite/source/display_settings.h','suite/source/video_settings_ui.h','assist/source/settings_dependencies.h'):
   old_sources.pop(p,None)
  raw=('\n'.join(e.CONSOLE_FAILURES)+'\n').encode()+encoded(old)
  support={'run.log':encoded(current),'pre5/run.log':raw,
   'settings-dependencies.log':b'PASS: settings dependencies (101 controls), independent consumers and saved-child restoration\n'}
  r={'passed':True,'compile_exit':0,'run_exit':0,'execution':current,'sources':current_sources,
     'negative_control':{'passed':True,'provenance_commit':'18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9',
      'fixture_sha256':e.CONSOLE_FIXTURES,'driver_exit':1,'expected_failures':e.CONSOLE_FAILURES,
      'observed_failures':e.CONSOLE_FAILURES,'run_log_sha256':package.sha(raw),
      'result':{'passed':False,'compile_exit':0,'run_exit':1,'execution':old,'sources':old_sources}}}
 elif name=='display_entry_clock':
  names=sorted(e.CLOCK_FAILURES)+['same_clock_recent','wrap_recent','confirmed_display_not_input','input_context_guards']
  r={'passed':True,'windows_executed':False,'game_tested':False,
     'baseline_fixture_sha256':e.CLOCK_BASELINE_SHA,'source_sha256':source_map(inputs),'results':{}}
  for branch in ('current','pre5'):
   failures=e.CLOCK_FAILURES if branch=='pre5' else {}
   cases=[{'case':n,'passed':n not in failures,'failures':failures.get(n,0)} for n in names]
   summary={'suite':name,'passed':branch=='current','scenarios':14,'checks':54,'failures':sum(failures.values())}
   raw=b''.join(encoded(row) for row in cases+[summary]);support[branch+'/run.log']=raw
   r['results'][branch]={'compile_exit_code':0,'exit_code':int(branch!='current'),
                        'summary':summary,'cases':cases,'run_log_sha256':package.sha(raw)}
 elif name=='read_only_lifecycle':
  r={'suite':name,'passed':True,'scenarios':8,'checks':68,'failures':0,
     'scope':'Synthetic packaging-policy receipt only; no production execution.'}
 elif name=='read_only_lifecycle_integration':
  host_names=['enabled_control','paused_secondary_cold','paused_secondary_cursor_only',
   'paused_secondary_after_warm','reenable_control','paused_without_secondary_control']
  host_failures={'paused_secondary_cold':e.READONLY_FAILURES[:2],
   'paused_secondary_cursor_only':e.READONLY_FAILURES[2:4],
   'paused_secondary_after_warm':e.READONLY_FAILURES[4:]}
  headers={p for p in inputs if '/source/' in p and '/fixtures/' not in p and p.endswith('.h')}

  def branch(directory,suite,names,checks,negative=False):
   cases=[];raw=b''
   for item in names:
    failed=host_failures.get(item,[]) if negative else []
    case={'case':item,'passed':not bool(failed),'failures':len(failed)}
    raw+=b''.join((line+'\n').encode() for line in failed)+encoded(case);cases.append(case)
   summary={'suite':suite,'passed':not negative,'scenarios':len(names),'checks':checks,
            'failures':5 if negative else 0}
   raw+=encoded(summary);support[directory+'/run.log']=raw
   return {'compile_command':['synthetic-policy-compiler','-std=c++17','-UNDEBUG'],
    'compile_exit_code':0,'run_command':['synthetic-policy-binary'], 'run_exit_code':int(negative),
    'summary':summary,'cases':cases,'stdout':raw.decode(),'stderr':'',
    'run_log_sha256':package.sha(raw),'generated_cpp_sha256':package.sha(('synthetic '+directory).encode()),
    'header_dependencies':source_map(headers)}

  host=branch('host-current','paused_secondary_lifecycle',host_names,21)
  old=branch('host-pre5-negative','paused_secondary_lifecycle',host_names,21,True)
  runtime=branch('runtime-current','read_only_runtime_integration',sorted(e.READONLY_RUNTIME_CASES),48)
  old.update(reproduced=True,source_commit=e.READONLY_COMMIT,fixture_sha256=e.READONLY_BASELINE_SHA,
             expected_failure_cases=sorted(e.READONLY_FAILURE_CASES))
  stages={}
  for stage_name,spec in e.STAGES.items():
   production=source_map(spec['production'])
   child_sources=production|source_map({spec['driver'],spec['fixture']})
   parts={}
   raw_logs={}
   for part in ('current','negative-control'):
    negative=part=='negative-control';failure_lines=spec['failures'] if negative else []
    execution={'checks':spec['checks'],'failures':len(failure_lines)}
    if spec['scenarios'] is not None:execution['scenarios']=spec['scenarios']
    if stage_name=='cooperative_recovery':execution['submissions']=2 if negative else 1
    raw=b''.join((line+'\n').encode() for line in failure_lines)+encoded(execution)
    raw_logs[part+'/run.log']=package.sha(raw);support[stage_name+'/'+part+'/run.log']=raw
    value={'scope':'Synthetic policy fixture; production was not executed.',
     'sources':production|({spec['replaced']:spec['fixture_sha256']} if negative else {}),
     'generated_cpp_sha256':package.sha(('synthetic '+stage_name+'/'+part).encode()),
     'compile_argv':['synthetic-policy-compiler','-std=c++17'], 'compile_exit':0,
     'run_exit':int(negative),'execution':execution,'raw_run_sha256':package.sha(raw)}
    parts[part]=value
    support[stage_name+'/'+part+'/result.json']=encoded(value)
    support[stage_name+'/'+part+'/compile.log']=b'Synthetic compile receipt only; no compiler was executed.\n'
   negative={'passed':True,'fixture_sha256':spec['fixture_sha256'],
    'expected_failures':spec['failures'],'observed_failures':spec['failures'],'result':parts['negative-control']}
   if stage_name=='fps_recovery':negative['provenance_commit']=e.READONLY_COMMIT
   else:negative['fixture_provenance']='Verified development intermediate; synthetic policy receipt does not execute it.'
   child={'schema':1,'suite':spec['suite'],'scope':'Synthetic policy fixture only.',
    'passed':True,'sources':child_sources,'current':parts['current'],'negative_control':negative}
   driver={'suite':spec['suite'],'passed':True,'checks':spec['checks'],'failures':0,'negative_control':True}
   if stage_name=='input_pairs':driver['scenarios']=spec['scenarios']
   driver_raw=encoded(driver);child_raw=encoded(child)
   support[stage_name+'/driver.log']=driver_raw;support[stage_name+'/result.json']=child_raw
   stages[stage_name]={'command':['synthetic-policy-python',spec['driver'],'--negative-control','--out',stage_name],
    'exit_code':0,'driver_stdout':driver_raw.decode(),'driver_stderr':'','driver_log_sha256':package.sha(driver_raw),
    'report':child,'report_sha256':package.sha(child_raw),'raw_logs':raw_logs}
  raw=support['host-current/run.log']+support['host-pre5-negative/run.log']+support['runtime-current/run.log']
  for stage_name,stage in stages.items():
   for path in stage['raw_logs']:raw+=support[stage_name+'/'+path]
  support['run.log']=raw
  r={'suite':name,'passed':True,'scope':'Synthetic packaging-policy receipt only; no production execution.',
   'sources':source_map(inputs),'host_current':host,'runtime_current':runtime,'negative_control':old,
   'additional_stages':stages,'checks':69+sum(s['checks'] for s in e.STAGES.values()),
   'scenarios':18+sum(s['scenarios'] or 0 for s in e.STAGES.values()),
   'windows_executed':False,'game_tested':False,'run_log_sha256':package.sha(raw)}
 else:raise ValueError(name)
 return encoded(r).decode(),support

#!/usr/bin/env python3
"""Current DLL suites (ordinary features; opt-in patch lifecycle has separate tests). Needs Unicorn and pefile; no game EXE is bundled."""
from pathlib import Path
import argparse,concurrent.futures,subprocess,sys,json,hashlib
R=Path(__file__).resolve().parents[1]
FILTER='repair,auto_board,auto_repair,combat_selection,empty_transports,damaged_selection,idle_,clear_beacons,type_selection,cancel_automatic,weapon_variant,capacity,iron,production_notice,psychic,crates,frozen,install,ore_reader,ejection,enemy_sell_guard,placement_guard,chat_,old_match,expired_,mailbox_,combined_features,queue_full,outlist_wrap,missing_game_timer,smart_unload,cooperative_,non_positional,different_backends,negative_controls'
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('reference',type=Path);p.add_argument('--jobs',type=int,default=2);p.add_argument('--startup-only',action='store_true');p.add_argument('--only',default='');a=p.parse_args();a.game=a.game.resolve();a.reference=a.reference.resolve()
CASES=[('render_boundary_runtime',a.game,'render-boundaries-current'),('render_reveal_runtime',a.game,'render-current'),('experimental_lifecycle_runtime',a.game,'experimental-lifecycle-current'),('hotkey_fanout_runtime',a.game,'fanout-user'),('hotkey_fanout_runtime',a.reference,'fanout-reference'),('feature_type_runtime',a.game,'feature-types-current'),('config_runtime_regression',a.game,'config-current'),('startup_runtime_regression',a.game,'startup-user'),('startup_runtime_regression',a.reference,'startup-reference'),('no_interception_runtime',a.game,'no-interception-user'),('no_interception_runtime',a.reference,'no-interception-reference'),('ore_priority_runtime',a.game,'ore-priority-user'),('ore_priority_runtime',a.reference,'ore-priority-reference'),('entry_binding_regression',a.game,'binding-current'),('selection_gateway_regression',a.game,'selection-current'),('bridge_diagnostics_regression',a.game,'bridge-current'),('timing_runtime_regression',a.game,'timing-current'),('robustness_runtime_regression',a.game,'robustness-current'),('two_peer_sync',a.game,'two-peer-current')]
previous=[]
selected=set(a.only.split(',')) if a.only else ({'startup-user','startup-reference'} if a.startup_only else set())
if selected:
 assert selected <= {x[2] for x in CASES}, 'unknown output selection'
 prior=json.loads((R/'CURRENT_REGRESSION_RUN.json').read_text())
 assert prior['dll_sha256']==hashlib.sha256((R/'GameRuntime.dll').read_bytes()).hexdigest()
 previous=[x for x in prior['runs'] if Path(x['output']).stem not in selected]
 assert len(previous)==len(CASES)-len(selected) and all(x['exit_code']==0 for x in previous)
 for x in previous:
  evidence=json.loads((R/x['output']).read_text())
  assert evidence['inputs']['GameRuntime.dll']==prior['dll_sha256'] and all(y['status']=='PASS' for y in evidence['results'])
 CASES=[x for x in CASES if x[2] in selected]
def run(c):
 script,game,out=c;args=[sys.executable,f'assist/tests/{script}.py',str(game),'GameRuntime.dll','--output',out+'.json']
 if script=='two_peer_sync':args+=['--filter',FILTER]
 (R/(out+'.json')).unlink(missing_ok=True)
 with (R/(out+'.log')).open('w') as f:ret=subprocess.run(args,cwd=R,stdout=f,stderr=subprocess.STDOUT).returncode
 result={'script':script,'output':out+'.json','exit_code':ret}
 print(json.dumps(result),flush=True)
 if ret:print((R/(out+'.log')).read_text()[-2000:],flush=True)
 return result
with concurrent.futures.ThreadPoolExecutor(max_workers=max(1,min(a.jobs,2))) as pool:results=list(pool.map(run,CASES))
(R/'CURRENT_REGRESSION_RUN.json').write_text(json.dumps({'dll_sha256':hashlib.sha256((R/'GameRuntime.dll').read_bytes()).hexdigest(),'runs':previous+results},indent=2)+'\n')
sys.exit(int(any(r['exit_code'] for r in results)))

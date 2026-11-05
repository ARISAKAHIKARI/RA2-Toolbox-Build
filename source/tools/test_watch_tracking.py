"""Source-bound MCV conversion, container-chain and fixed-live-camera regression."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--baseline-source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[1];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
baseline_files=['external_reader/source/watch_focus.h','external_reader/source/watch_slots.h','assist/source/watch_scene.h','assist/source/watch_host.h','assist/source/watch_windows.h']
known=['fb6b0f0c3ab20524edbe3c6362eee96dd1b0acae5ab292086ac48ac7f149aa7a','56a2810e7506a5175fc03b9ed4a8b176df67b655839c97cc2547b8d2b7fc3351','00fb8cc5d34c51d6bfb7dae964390c2a9eca8646a964bc5f1f36ef799d15a082','b6ea53149ec2450453b211b01ae83b5c8a7859e5ae347b288b3d02b4123e159d','2f36729a303948b410a0ccfe278d0e809f72776828b816b89cd5c30d94f8d554']
base={n:sha(a.baseline_source/n) for n in baseline_files};assert list(base.values())==known,'Not the frozen v1.0.41 baseline'
old=[]
for name in baseline_files[:2]:
 text=(a.baseline_source/name).read_text(encoding='utf-8-sig');text='\n'.join(l for l in text.splitlines() if not l.startswith('#include') and l!='#pragma once');old.append(text.replace('namespace external { namespace watch {','namespace external { namespace before_watch {'))
(out/'watch_tracking_before.inc').write_text('\n'.join(old)+'\n',encoding='utf-8')
paths=baseline_files+['external_reader/source/watch_transition.h','assist/tests/watch_tracking.cpp','tools/test_watch_tracking.py']
paths+=['assist/tests/watch_tracking_renderer.cpp','assist/tests/watch_world_performance.cpp','assist/source/watch_current_anchor.h']
support=(source/'assist/tests/watch_world_performance.cpp').read_text(encoding='utf-8')
shared=support[support.index('static std::vector<uint8_t> memory'):support.index('static std::shared_ptr<worldscene::Snapshot> world')]
shared+=support[support.index('using BaseAssets='):support.index('static void watchRun')]
(out/'watch_tracking_renderer_support.inc').write_text(shared,encoding='utf-8')
before={n:sha(source/n) for n in paths};results=[]
for name in ['watch_tracking','watch_tracking_renderer']:
 exe=out/(name+'.exe');cmd=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(source/'assist/tests'/(name+'.cpp')),'-I'+str(out),'-o',str(exe)]
 if name=='watch_tracking_renderer':cmd+=[str(source/'third_party/xbrz/xbrz.cpp'),'-lgdiplus','-lgdi32','-lole32','-luser32','-ladvapi32']
 build=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8');(out/(name+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
 run=subprocess.run([str(exe)],capture_output=True,text=True,encoding='utf-8');(out/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();results.append({'name':name,'status':'PASS','executed':True,'scope':'Production watch headers plus pinned actual old functions; controlled memory, not a running game.','stdout':run.stdout})
assert before=={n:sha(source/n) for n in paths},'Production source changed while testing'
result={'passed':True,'scope':__doc__,'source_hash_stable':True,'source_sha256':before,'baseline_commit':'513aeb4cf03238cce956bd35e0c9381c2a7cc484','baseline_sha256':base,'results':results,'game_tested':False,'windows_visual_tested':False,'multiplayer_tested':False}
(out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

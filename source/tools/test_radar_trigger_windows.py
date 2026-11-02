"""Read-only native geometry replay and compiled minimap trigger regressions.

Requires pefile/unicorn for the supplied private PE. Never opens a game process.
--render-baseline-root contains the three 2bb rendering headers used by the
existing renderer regressions; native-trigger comparisons use the actual PE.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout
import hashlib
import io
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import time


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zig', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--render-baseline-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    out = args.output.resolve();out.mkdir(parents=True, exist_ok=True)
    baseline = args.render_baseline_root.resolve() / 'source/external_reader/source'
    headers = {name:(baseline/name).read_text(encoding='utf-8-sig') for name in ('radar_scene.h','gpu_scene_record.h','radar_live_shroud.h')}
    text = headers['radar_scene.h'];text = 'namespace external { namespace radar {\n'+text[text.index('template<class Assets>class SceneRenderer'):]
    (out/'scene_reference.h').write_text(text.replace('class SceneRenderer','class BeforeSceneRenderer').replace('explicit SceneRenderer(','explicit BeforeSceneRenderer('),encoding='utf-8')
    text = headers['gpu_scene_record.h'];text = 'namespace external { namespace camera {\n'+text[text.index('class GpuSceneRecord'):]
    (out/'record_reference.h').write_text(text.replace('GpuSceneRecord','BeforeGpuSceneRecord'),encoding='utf-8')
    text = headers['radar_live_shroud.h'];text = text[text.index('class Live'):];prefix='namespace external { namespace radar { namespace shroudmask {\n'
    (out/'live_reference.h').write_text(prefix+text.replace('class Live','class BeforeLive')+'\n'+prefix+text.replace('class Live','class BeforeLiftedLive').replace('candidate.cells.size()>=65536','candidate.cells.size()>=512*512'),encoding='utf-8')
    changed = ['external_reader/source/radar_map.h','external_reader/source/radar_publication.h','external_reader/source/radar_live_shroud.h','assist/source/minimap_overlay.h']
    tests = ['radar_trigger_projection','radar_visibility_publication','radar_boundary','radar_output_scene','radar_live_exploration','radar_sampler_compact','radar_large_map','scene_bank_publication']
    paths = changed + ['assist/tests/'+name+'.cpp' for name in tests]+['assist/tests/radar_trigger_native.py','tools/test_radar_trigger_windows.py']
    hashes = {name:sha(source/name) for name in paths}
    cases = [(name,arch) for name in tests for arch in ('x64','x86') if arch=='x64' or name=='radar_trigger_projection']
    def execute(case):
        name,arch=case;exe=out/(name+'-'+arch+'.exe')
        command=[str(args.zig.resolve()),'c++','-target','x86_64-windows-gnu' if arch=='x64' else 'x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-w',str(source/'assist/tests'/(name+'.cpp'))]
        for macro,header in [('RA2_REFERENCE_SCENE','scene_reference.h'),('RA2_REFERENCE_RECORD','record_reference.h'),('RA2_REFERENCE_LIVE','live_reference.h')]: command += ['-D'+macro+'="'+str((out/header).as_posix())+'"']
        command += [str(source/'third_party/xbrz/xbrz.cpp'),'-o',str(exe),'-lgdi32','-luser32']
        started=time.monotonic();built=subprocess.run(command,cwd=source,capture_output=True,text=True,timeout=600)
        (out/(name+'-'+arch+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8')
        result={'name':name,'arch':arch,'command':command,'build_exit_code':built.returncode,'seconds':time.monotonic()-started}
        if built.returncode:return result
        ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=300)
        result.update(exit_code=ran.returncode,output=ran.stdout+ran.stderr,exe_sha256=sha(exe))
        print(name+' '+arch+' '+('PASS' if ran.returncode==0 else 'FAIL'),flush=True)
        return result
    with ThreadPoolExecutor(max_workers=max(1,min(3,args.jobs))) as pool: results=list(pool.map(execute,cases))
    native=[]
    for arch in ('x64','x86'):
        if any(x['name']=='radar_trigger_projection' and x['arch']==arch and x.get('exit_code')==0 for x in results):
            output=out/('native-cpp-'+arch+'.json');old=sys.argv;capture=io.StringIO()
            try:
                sys.argv=[str(source/'assist/tests/radar_trigger_native.py'),str(args.game.resolve()),'--cpp-exe',str(out/('radar_trigger_projection-'+arch+'.exe')),'--output',str(output)]
                with redirect_stdout(capture):runpy.run_path(sys.argv[0],run_name='__main__')
            finally:sys.argv=old
            native.append(json.loads(output.read_text(encoding='utf-8')));print(capture.getvalue(),end='',flush=True)
    assert hashes == {name:sha(source/name) for name in paths},'Source changed during fixture compilation'
    passed=all(x['build_exit_code']==0 and x.get('exit_code')==0 for x in results) and len(native)==2 and all(x['passed'] for x in native)
    report={'passed':passed,'baseline_commit':'8437c37','source_sha256':hashes,'results':results,'native':native,
            'render_regression_baseline_sha256':{name:sha(baseline/name) for name in headers},'game_process_touched':False,'hardware_gpu_tested':False}
    (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    if not passed:raise SystemExit(1)


if __name__=='__main__':main()

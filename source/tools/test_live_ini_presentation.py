"""Source-bound live INI bootstrap regression over real local Win32 RPM.

The synthetic CCINI has readable roots with a failed or duplicate trailing
section. This reproduces a possible render stall; it does not prove the user's
running game has that defect. No game memory, hook, GPU, or native ABI is used.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
fixture=source/'assist/tests/fixtures/live_ini_baseline';manifest=json.loads((fixture/'manifest.json').read_text(encoding='utf-8'));sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(fixture/'live_ini.h')==manifest['fixture_sha256']
inputs=['external_reader/source/live_ini.h','external_reader/source/camera_assets.h','assist/tests/live_ini_presentation.cpp','assist/tests/fixtures/live_ini_baseline/live_ini.h','assist/tests/fixtures/live_ini_baseline/manifest.json'];before={n:sha(source/n) for n in inputs}
exe=out/'live_ini_presentation.exe';command=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(source/'assist/tests/live_ini_presentation.cpp'),'-o',str(exe),'-lgdi32','-luser32','-lshlwapi','-lshell32','-lole32']
env=dict(os.environ,PYTHONUTF8='1');build=subprocess.run(command,env=env,capture_output=True,text=True,encoding='utf-8');(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
run=subprocess.run([str(exe)],env=env,capture_output=True,text=True,encoding='utf-8');(out/'run.log').write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode()
assert before=={n:sha(source/n) for n in inputs}
result={'passed':True,'scope':__doc__,'command':command,'source_sha256':before,'source_unchanged':True,'baseline':manifest,'stdout':run.stdout,'game_tested':False,'multiplayer_tested':False};(out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

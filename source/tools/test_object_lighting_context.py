"""Compare actual current object lighting to its pinned production baseline.

Executes an isolated local process, with fake game fields but the real Win32
ReadProcessMemory boundary. No game, native function, or GPU is executed.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--zig',type=Path,required=True)
p.add_argument('--build-dir',type=Path,required=True)
a=p.parse_args();source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
fixture=source/'assist/tests/fixtures/object_lighting_baseline'
manifest=json.loads((fixture/'manifest.json').read_text(encoding='utf-8'))
assert hashlib.sha256((fixture/'object.inc').read_bytes()).hexdigest()==manifest['fixture_sha256']
inputs=['external_reader/source/scene_lighting.h','assist/source/formation_engine.h','assist/tests/object_lighting_context.cpp','assist/tests/fixtures/object_lighting_baseline/object.inc','assist/tests/fixtures/object_lighting_baseline/manifest.json']
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
before={n:sha(source/n) for n in inputs}
exe=out/'object_lighting_context.exe'
command=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(source/'assist/tests/object_lighting_context.cpp'),'-o',str(exe)]
env=dict(os.environ,PYTHONUTF8='1')
build=subprocess.run(command,env=env,capture_output=True,text=True,encoding='utf-8');(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
run=subprocess.run([str(exe)],env=env,capture_output=True,text=True,encoding='utf-8');(out/'run.log').write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode()
stable=before=={n:sha(source/n) for n in inputs};assert stable
result={'passed':True,'scope':__doc__,'command':command,'production_sha256':before,'source_unchanged':stable,'baseline':manifest,'stdout':run.stdout,'game_tested':False,'multiplayer_tested':False}
(out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

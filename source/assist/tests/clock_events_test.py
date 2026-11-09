"""Execute current production waits and publication transport at Win32 seams."""
from pathlib import Path
import argparse, ast, hashlib, json, re, subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];source=repo/'source';out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
seeds=[Path(__file__),Path(__file__).with_name('clock_events.cpp'),Path(__file__).with_name('world_frame_notifications.cpp')]
inputs={};pending=seeds+[source/'tests/winmock/windows.h',Path(__file__).with_name('world_frame_core_test.py')]
while pending:
 path=pending.pop().resolve()
 if path in inputs:continue
 raw=path.read_bytes();inputs[path]=hashlib.sha256(raw).hexdigest()
 for inc in re.findall(r'^\s*#include\s*"([^"]+)"',raw.decode(),re.M):
  child=(path.parent/inc).resolve()
  if child.is_file() and child.is_relative_to(source):pending.append(child)
results=[]
def run(name,command):
 build=subprocess.run(command,capture_output=True,text=True);(out/(name+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode()
 binary=out/name;result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
 (out/(name+'-run.log')).write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
 results.append(dict(name=name,command=command,compile_exit_code=build.returncode,exit_code=result.returncode,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
flags=['g++','-std=c++17','-O2','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all']
(out/'windows.h').write_text('#include "'+str(Path(__file__).with_name('clock_events_windows.h'))+'"\n')
run('clock-events',flags+['-D_WIN32','-I'+str(out),str(seeds[1]),'-o',str(out/'clock-events')])
module=ast.parse(Path(__file__).with_name('world_frame_core_test.py').read_text())
windows=(source/'tests/winmock/windows.h').read_text()+next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.AugAssign) and isinstance(n.target,ast.Name) and n.target.id=='windows')
(out/'windows.h').write_text(windows)
run('world-notifications',flags+['-I'+str(out),str(seeds[2]),'-o',str(out/'world-notifications')])
assert all(hashlib.sha256(path.read_bytes()).hexdigest()==sha for path,sha in inputs.items())
(out/'receipt.json').write_text(json.dumps(dict(passed=True,source_bindings={str(p.relative_to(repo)):s for p,s in inputs.items()},source_unchanged=True,results=results,
 windows_executed=False,game_tested=False,scope='Actual production Recurring/DeadlineWake Win32 path over controlled events and real threads; actual Client/Server over controlled mappings. Counts do not establish Windows timing or game FPS.'),indent=2)+'\n')

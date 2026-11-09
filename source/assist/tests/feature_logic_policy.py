"""Execute production feature policies; optional frozen pre-fix headers form negative controls.

No game process, Windows native event consumer, or physical GPU is executed.
The quick-placement trace is compared between runs by the caller; wall-clock
fixture times include instrumented map lookups and are not game frame times.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

HERE=Path(__file__).resolve().parent
SRC=HERE.parent/'source'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--baseline',type=Path)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
names=['formation.h','formation_quick.h','external_planner.h']
headers=[];hashes={}
for name in names:
    original=(a.baseline/name) if a.baseline else SRC/name
    hashes[str(original)]=hashlib.sha256(original.read_bytes()).hexdigest()
    if not a.baseline:
        headers.append('#include '+json.dumps(str(original)))
        continue
    # Keep the frozen production body, resolving only its relative includes.
    text=original.read_text()
    if name in ('formation.h','formation_quick.h'):
        text=text.replace('namespace formation {','namespace formation_baseline {')
    def resolve(m):
        target=(a.out/m[1]) if m[1] in names else (SRC/m[1]).resolve()
        return '#include '+json.dumps(str(target))
    text=re.sub(r'#include "([^"]+)"',resolve,text)
    target=a.out/name;target.write_text(text);headers.append('#include '+json.dumps(str(target)))
headers.append('namespace featureformation=assist::'+('formation_baseline' if a.baseline else 'formation')+';')
(a.out/'feature_logic_headers.h').write_text('\n'.join(headers)+'\n')
exe=a.out/'fixture'
compile_cmd=['g++','-std=c++17','-O2','-UNDEBUG','-MMD','-MF',str(a.out/'production.d'),'-I'+str(a.out),str(HERE/'feature_logic_policy.cpp'),'-o',str(exe)]
build=subprocess.run(compile_cmd,capture_output=True,text=True)
(a.out/'build.log').write_text(build.stdout+build.stderr);build.check_returncode()
run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
(a.out/'run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='')
(a.out/'result.json').write_text(json.dumps(dict(scope=__doc__,compile_command=compile_cmd,compile_exit_code=build.returncode,command=[str(exe)],exit_code=run.returncode,source_sha256=hashes,game_tested=False),indent=2)+'\n')
run.check_returncode()

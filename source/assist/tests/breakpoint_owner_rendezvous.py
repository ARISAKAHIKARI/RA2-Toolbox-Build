"""Execute the complete production DR Service against controlled Win32 boundaries.

The production Lease, transaction, dispatch, witness, identity and cleanup code
is included verbatim. Only Win32 APIs and the two native marker instructions are
replaced. This is not Windows execution, a live game test or a DR latency claim.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--core',type=Path,default=Path(__file__).parents[1]/'source/tech_ui_breakpoint_core.h')
p.add_argument('--out',type=Path,required=True)
p.add_argument('--cxx')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;core=a.core.resolve();source=here/'breakpoint_owner_rendezvous.cpp'
text=core.read_text(encoding='utf-8').replace('#include <windows.h>','')
for name in ('ownerRendezvousTrap','ownerRendezvousWitness'):
    pattern=r'static __declspec\(naked\) void '+name+r'\(\)\{__asm \{.*?\}\}'
    text,n=re.subn(pattern,'static void '+name+'();',text,flags=re.S)
    assert n==1,(name,n)
text=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+str((core.parent/m[1]).resolve())+'"',text)
generated=a.out/'production_core.h';generated.write_text(text,encoding='utf-8')
exe=a.out.resolve()/('owner-rendezvous.exe' if os.name=='nt' else 'owner-rendezvous')
compiler=shlex.split(a.cxx or os.environ['CXX']) if a.cxx or os.environ.get('CXX') else ([os.environ.get('ZIG_EXE') or os.environ['ZIG'],'c++'] if os.environ.get('ZIG_EXE') or os.environ.get('ZIG') else [shutil.which('g++') or 'c++'])
command=[*compiler,'-std=c++17','-O2','-UNDEBUG','-Wall','-Wextra','-Wno-misleading-indentation','-I',str(a.out.resolve()),str(source),'-o',str(exe)]
build=subprocess.run(command,capture_output=True,text=True)
(a.out/'build.log').write_text(build.stdout+build.stderr)
if build.returncode:raise RuntimeError(build.stdout+build.stderr)
run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
(a.out/'run.log').write_text(run.stdout+run.stderr)
if run.returncode:raise RuntimeError(run.stdout+run.stderr)
result=json.loads(run.stdout);assert result['pass'];print(run.stdout,end='')
paths=[core,core.parent/'tech_ui_breakpoint_hot.h',core.parent/'tech_ui_breakpoint_rendezvous_guard.h',core.parent/'../../shared/source/runtime_seh_scope.h',source,Path(__file__).resolve()]
report={'pass':True,'scope':__doc__,'source_sha256':{str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'command':command,'result':result,'windows_execution':False,'game_execution':False}
(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


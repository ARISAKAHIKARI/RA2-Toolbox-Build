"""Build the actual production x86 SEH boundaries for standalone DR fixtures."""
from pathlib import Path
import subprocess

def build_guard(zig, directory):
    directory=Path(directory).resolve();directory.mkdir(parents=True,exist_ok=True)
    source=Path(__file__).resolve().parents[1]/'source'
    guard=(source/'guard.cpp').read_text(encoding='utf-8')
    start=guard.index('extern "C" void RuntimeScopedCall(')
    opening=guard.index('{',start);end=opening+1;depth=1
    while depth:
        depth+=(guard[end]=='{')-(guard[end]=='}');end+=1
    boundary=directory/'rendezvous-production-seh.cpp'
    boundary.write_text(guard[start:end]+'\n#include "'+str(source/'tech_ui_breakpoint_rendezvous_guard.h')+'"\n',encoding='utf-8')
    obj=directory/'rendezvous-production-seh.o'
    command=[str(Path(zig).resolve()),'c++','-target','x86-windows-msvc','-nostdlib','-nostdinc','-std=c++17','-O2','-fms-extensions','-fno-stack-protector','-c',str(boundary),'-o',str(obj)]
    run=subprocess.run(command,capture_output=True,text=True)
    (directory/'seh-build.log').write_text(run.stdout+run.stderr,encoding='utf-8')
    if run.returncode:raise RuntimeError(run.stdout+run.stderr)
    return obj

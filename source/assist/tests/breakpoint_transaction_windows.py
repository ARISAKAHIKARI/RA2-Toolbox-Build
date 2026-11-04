"""Build/run real Windows x86 delayed-worker transaction ordering and DR/VEH checks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--zig',type=Path,required=True)
p.add_argument('--build-dir',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();here=Path(__file__).resolve().parent
a.build_dir.mkdir(parents=True,exist_ok=True);exe=a.build_dir.resolve()/'breakpoint-transaction.exe'
source=here/'breakpoint_transaction_windows.cpp'
flags=['-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fms-extensions','-fasm-blocks']
subprocess.run([str(a.zig.resolve()),'c++',*flags,str(source),'-o',str(exe),'-lkernel32'],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
print(r.stdout,end='');print(r.stderr,end='');assert r.returncode==0,r.returncode
paths=[source,here/'tech_ui_breakpoint_windows.cpp',here.parent/'source/tech_ui_breakpoint_core.h']
result={'pass':True,'scope':__doc__,'compiler_flags':flags,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'stdout':r.stdout,'game_tested':False,'multiplayer_tested':False}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

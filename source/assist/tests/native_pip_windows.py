"""Execute production adapter + exact MSVC x86 SEH wrapper in isolated children."""
from pathlib import Path
import argparse,hashlib,json,subprocess,struct
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;obj=a.out/'guard.obj';exe=a.out/'native-pip.exe';rows=[]
commands=[[str(a.zig),'c++','-target','x86-windows-msvc','-nostdlib','-nostdinc','-O2','-fms-extensions','-fno-stack-protector','-c',str(here/'native_pip_guard_boundary.cpp'),'-o',str(obj)],
 [str(a.zig),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fno-builtin','-Wl,--image-base,0x20000000',str(here/'native_pip_bridge.cpp'),str(obj),'-o',str(exe)]]
for n,cmd in enumerate(commands):
 r=subprocess.run(cmd,capture_output=True,text=True);(a.out/f'build-{n}.log').write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
data=bytearray(exe.read_bytes());pe=struct.unpack_from('<I',data,0x3C)[0];opt=pe+24;assert struct.unpack_from('<H',data,opt)[0]==0x10B;assert struct.unpack_from('<I',data,opt+28)[0]==0x20000000
# Isolated test image only: retain fixed high image base so the low synthetic
# engine-data arena cannot overlap the child executable. No code bytes change.
flags=struct.unpack_from('<H',data,opt+70)[0];struct.pack_into('<H',data,opt+70,flags&~0x40);exe.write_bytes(data)
for run in range(3):
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);(a.out/f'run-{run}.log').write_text(r.stdout+r.stderr);print(r.returncode,r.stdout+r.stderr);rows.append(dict(run=run,exit=r.returncode,passed=r.returncode==0))
 if r.returncode:break
sources={name:hashlib.sha256((here/name).read_bytes()).hexdigest() for name in ['native_pip_bridge.cpp','native_pip_guard_boundary.cpp','../source/native_pip_core.h','../source/native_pip_bridge.h','../source/native_pip_guard.h']}
passed=len(rows)==3 and all(x['passed'] for x in rows);(a.out/'result.json').write_text(json.dumps(dict(passed=passed,windows_x86_executed=True,game_tested=False,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),sources=sources,runs=rows),indent=2)+'\n');assert passed

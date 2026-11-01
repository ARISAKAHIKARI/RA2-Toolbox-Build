#!/usr/bin/env python3
"""Build/run the x86 native tooltip/frame fixture. Never opens a game process."""
from pathlib import Path
import argparse, json, os, struct, subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--zig',required=True);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 source=Path(__file__).resolve().parent.parent;out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 exe=out/'cursor_tooltip_windows.exe'
 cmd=[str(Path(a.zig).resolve()),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG',
  '-fms-extensions','-fasm-blocks','-fno-stack-protector','-fno-builtin','-fno-exceptions','-fno-rtti',
  str(source/'assist/tests/cursor_tooltip_windows.cpp'),'-Wl,--image-base,0x400000',
  '-o',str(exe),'-lkernel32','-luser32','-lgdi32','-lwinmm']
 build=subprocess.run(cmd,capture_output=True,text=True)
 (out/'fixture-build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
 if build.returncode:print(build.stdout+build.stderr);raise SystemExit(build.returncode)
 # This generated test executable alone needs fixed VAs. Zig 0.13's driver
 # lacks GNU --disable-dynamicbase; clear ASLR in this fixture's PE header.
 # Production binaries and private game executables are never modified.
 image=bytearray(exe.read_bytes());pe=struct.unpack_from('<I',image,0x3c)[0];opt=pe+24
 assert image[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',image,opt)[0]==0x10b
 assert struct.unpack_from('<I',image,opt+28)[0]==0x400000
 flags=struct.unpack_from('<H',image,opt+70)[0]
 struct.pack_into('<H',image,opt+70,flags&~0x40);exe.write_bytes(image)
 if os.name!='nt':raise RuntimeError('Actual Windows execution required')
 run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
 (out/'fixture.log').write_text(run.stdout+run.stderr,encoding='utf-8')
 (out/'fixture.json').write_text(json.dumps(dict(build_command=cmd,build_exit=build.returncode,test_exit=run.returncode,
  stdout=run.stdout,stderr=run.stderr,scope='Synthetic x86 Win32 process; no game process opened',game_tested=False,multiplayer_tested=False),indent=2),encoding='utf-8')
 print(run.stdout+run.stderr,flush=True)
 raise SystemExit(run.returncode)
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Build/run the x86 native tooltip/frame fixture. Never opens a game process."""
from pathlib import Path
import argparse, hashlib, json, os, struct, subprocess

def native_function_hashes(data):
 pe=struct.unpack_from('<I',data,0x3c)[0];sections=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0]
 base=struct.unpack_from('<I',data,pe+24+28)[0];out={}
 for address,size in [(0x479050,38),(0x478ba0,0x20e),(0x724ad0,0xad)]:
  for i in range(sections):
   at=pe+24+optional+i*40;va,raw_size,raw=struct.unpack_from('<III',data,at+12)
   if va<=address-base and address-base+size<=va+raw_size:
    offset=raw+address-base-va;out[hex(address)]=hashlib.sha256(data[offset:offset+size]).hexdigest();break
  else:raise RuntimeError('Required native function is not in a complete PE section')
 return out

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--zig',required=True);p.add_argument('--output',required=True,type=Path);p.add_argument('--game-exe',type=Path);a=p.parse_args()
 game_hash=None;function_hashes={}
 if a.game_exe:
  game_hash=hashlib.sha256(a.game_exe.read_bytes()).hexdigest()
  if game_hash not in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0','247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0'}:raise RuntimeError('Unsupported private PE; fixture refuses unknown native code')
  function_hashes=native_function_hashes(a.game_exe.read_bytes())
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
 run=subprocess.run([str(exe)]+([str(a.game_exe.resolve())] if a.game_exe else []),capture_output=True,text=True,timeout=30)
 if a.game_exe and hashlib.sha256(a.game_exe.read_bytes()).hexdigest()!=game_hash:raise RuntimeError('Private PE changed during fixture; result rejected')
 (out/'fixture.log').write_text(run.stdout+run.stderr,encoding='utf-8')
 (out/'fixture.json').write_text(json.dumps(dict(build_command=cmd,build_exit=build.returncode,test_exit=run.returncode,
  stdout=run.stdout,stderr=run.stderr,private_pe_sha256=game_hash,native_function_sha256=function_hashes,scope='Synthetic x86 Win32 process; optional native tooltip code read from private PE; no game process opened',game_tested=False,multiplayer_tested=False),indent=2),encoding='utf-8')
 print(run.stdout+run.stderr,flush=True)
 raise SystemExit(run.returncode)
if __name__=='__main__':main()

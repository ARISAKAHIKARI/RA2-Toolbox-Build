"""Execute unpatched native shroud/fog drawers with replacement alpha SHP data.
Private EXE and matching runtime DLL are inputs, never outputs. This proves
the leaf blitter's material semantics only, not resource lifetime or netplay.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'))
n=importlib.util.module_from_spec(spec);saved=sys.argv
try:
 sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(a.output.with_suffix('.helpers.json'))]
 spec.loader.exec_module(n)
finally:sys.argv=saved
rows=[]
for source in (2,64,127,254):
 for replacement in ('original','clear','half'):
  value=source if replacement=='original' or source>127 else 127 if replacement=='clear' else (source+128)//2
  for fog in (False,True):
   m=n.fixture(5);s=n.state(m);s.renderReveal=0;n.save(m,s);n.tick(m)
   shape=m.alloc(0x1000);pixels=m.alloc(0x2000);alpha=m.alloc(0x100);surface=m.alloc(0x100);point=m.alloc(8);clip=m.alloc(16)
   m.u.mem_write(shape,bytes(32)+bytes([value])*1024);m.u.mem_write(shape+6,struct.pack('<H',1));m.u.mem_write(shape+8,struct.pack('<4h',0,0,32,32));m.w(shape+28,32)
   m.u.mem_write(pixels,struct.pack('<1024H',*([127]*1024)))
   for offset,v in ((0,0x7e2070),(4,32),(8,32),(12,0),(16,2),(20,pixels)):m.w(surface+offset,v)
   for offset,v in ((0x10,0),(0x14,surface),(0x18,pixels),(0x1c,pixels+2048),(0x20,2048),(0x28,32),(0x2c,32)):m.w(alpha+offset,v)
   for address in (0x89e790,0x89e794,0x89e760):m.w(address,shape)
   m.w(0x87e8a4,alpha);m.b(0x89e7c5,1);m.b(0x89e7c6,1);m.w(0x886fa4,0)
   m.u.mem_write(point,struct.pack('<2i',0,0));m.u.mem_write(clip,struct.pack('<4i',0,0,32,32))
   original=m.u.mem_read(0x47efe0,0x2b0);m.codeWrites.clear();outside=[]
   def watch(u,access,address,size,value,_):
    allowed=(pixels<=address and address+size<=pixels+2048) or (surface+12<=address and address+size<=surface+16) or (m.STACK<=address<m.STACK+0x20000) or address==0
    if not allowed:outside.append((hex(address),size))
   m.u.hook_add(UC_HOOK_MEM_WRITE,watch)
   m.run(0x47f250 if fog else 0x47efe0,(point,clip,0),m.crateCell)
   result=set(struct.unpack('<1024H',m.u.mem_read(pixels,2048)));expected=127 if value==254 else value
   assert result=={expected},(source,replacement,fog,result,expected)
   assert not outside and not m.codeWrites,(outside,m.codeWrites)
   assert m.r(surface+12)==0 and original==m.u.mem_read(0x47efe0,0x2b0)
   rows.append(dict(source=source,replacement=replacement,fog=fog,material=value,alpha=expected,status='PASS'))
report=dict(scope=__doc__,exe_sha256=hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),dll_sha256=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),engine_code_writes=0,windows_tested=False,resource_lifetime_tested=False,multiplayer_tested=False,cases=rows)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS {len(rows)} native replacement-material cases: clear=127, half=(alpha+128)/2, FE untouched; no engine code/simulation writes')

"""Exercise the shipped x86 Runtime and native SHP leaves on controlled data.
The supplied private EXE is an input, never an output. This fixture is not
Windows visual testing, a resource-loader test, or multiplayer validation.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

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
for session in (5,3):
 m=n.fixture(session)
 # Full header/frame count with small crops keeps emulation bounded while the
 # portable Controller regression covers the maximum 60x30 frame planes.
 header=8+47*24;plane=8*8;size=header+47*plane
 def bank(seed):
  at=m.alloc(size);data=bytearray(size);struct.pack_into('<4H',data,0,0,60,30,47)
  for frame in range(47):
   h=8+24*frame;struct.pack_into('<4H',data,h,0,0,8,8);struct.pack_into('<I',data,h+20,header+frame*plane)
   for i in range(plane):data[header+frame*plane+i]=[2,64,127,254][(i+frame+seed)%4]
  m.u.mem_write(at,bytes(data));return at,bytes(data)
 first,raw1=bank(0);second,raw2=bank(1)
 for slot,value in ((0x89e790,first),(0x89e794,first),(0x89e760,second)):m.w(slot,value)
 m.b(0x89e7c5,1);m.b(0x89e7c6,1);m.w(0x886fa4,0)
 pixels=m.alloc(0x2000);alpha=m.alloc(0x100);surface=m.alloc(0x100);point=m.alloc(8);clip=m.alloc(16)
 for offset,value in ((0,0x7e2070),(4,32),(8,32),(12,0),(16,2),(20,pixels)):m.w(surface+offset,value)
 for offset,value in ((0x10,0),(0x14,surface),(0x18,pixels),(0x1c,pixels+2048),(0x20,2048),(0x28,32),(0x2c,32)):m.w(alpha+offset,value)
 m.w(0x87e8a4,alpha);m.u.mem_write(point,struct.pack('<2i',0,0));m.u.mem_write(clip,struct.pack('<4i',0,0,8,8))
 text_before=bytes(m.u.mem_read(m.textAt,m.textSize));m.codeWrites.clear();m.process_memory_writes=[]
 def switch(mode):
  s=n.state(m);s.renderReveal=mode;n.save(m,s);return n.tick(m)
 def expected(raw,mode):
  data=bytearray(raw)
  for i in range(header,len(data)):
   value=data[i]
   if value<=127:data[i]=127 if mode==4 else (value+128)//2
  return bytes(data)
 def check_banks(mode):
  assert bytes(m.u.mem_read(first,size))==expected(raw1,mode)
  assert bytes(m.u.mem_read(second,size))==expected(raw2,mode)
  assert [m.r(x) for x in (0x89e790,0x89e794,0x89e760)]==[first,first,second]
 def draw(fog,raw,mode):
  m.u.mem_write(pixels,struct.pack('<1024H',*([127]*1024)))
  m.run(0x47f250 if fog else 0x47efe0,(point,clip,0),m.crateCell)
  out=struct.unpack('<1024H',m.u.mem_read(pixels,2048));source=expected(raw,mode)[header:header+plane]
  for y in range(8):
   for x in range(8):
    v=source[y*8+x];assert out[y*32+x]==(v if v<=127 else 127),(session,fog,mode,x,y,out[y*32+x],v)
  assert all(out[y*32+x]==127 for y in range(32) for x in range(32) if y>=8 or x>=8)
 for mode in (4,5):
  s=switch(mode);assert s.data.renderRevealStatus==1,(session,mode,s.data.renderRevealStatus)
  check_banks(mode);draw(False,raw1,mode);draw(True,raw2,mode)
  before=len(m.process_memory_writes);n.tick(m);assert len(m.process_memory_writes)==before
  rows.append(dict(session=session,mode=mode,native_shroud='PASS',native_fog='PASS',stable_no_writes=True))
 s=switch(0);assert s.data.renderRevealStatus==0
 assert bytes(m.u.mem_read(first,size))==raw1 and bytes(m.u.mem_read(second,size))==raw2
 s=switch(4);assert s.data.renderRevealStatus==1
 # Material-only selection does not patch either engine drawing leaf. The
 # explicit legacy mode does; returning to material must remove that hook.
 assert not m.codeWrites and bytes(m.u.mem_read(m.textAt,m.textSize))==text_before
 s=switch(1);assert s.data.renderRevealStatus==1
 assert bytes(m.u.mem_read(first,size))==raw1 and bytes(m.u.mem_read(second,size))==raw2
 assert m.codeWrites
 s=switch(4);assert s.data.renderRevealStatus==1;check_banks(4)
 assert bytes(m.u.mem_read(m.textAt,m.textSize))==text_before
 s=n.state(m);s.stop=1;n.save(m,s);n.tick(m)
 assert bytes(m.u.mem_read(first,size))==raw1 and bytes(m.u.mem_read(second,size))==raw2
 assert bytes(m.u.mem_read(m.textAt,m.textSize))==text_before
 assert all((first+header<=p and p+n<=first+size) or (second+header<=p and p+n<=second+size) for p,n in m.process_memory_writes)
 assert n.count(m)==0
 rows.append(dict(session=session,restore='PASS',stop='PASS',legacy_hook_transition='PASS',material_only_code_writes=0,queued_events=0))
report=dict(scope=__doc__,exe_sha256=hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),dll_sha256=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),windows_tested=False,multiplayer_tested=False,cases=rows)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS {len(rows)} actual Runtime/native shroud and fog cases: clear/half, warm ticks, legacy transition, off and stop; material mode does not patch code or enqueue events')

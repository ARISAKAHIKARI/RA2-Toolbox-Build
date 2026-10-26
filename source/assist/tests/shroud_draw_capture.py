"""Capture actual DLL Shroud/Fog drawing cost and write boundaries.

Requires Python 3 with pefile and unicorn (for example: pip install pefile unicorn).
Usage: python shroud_draw_capture.py path/to/capture-fixture.exe path/to/GameRuntime.dll --output draw.json

Uses the neighboring no_interception_runtime.py / entry_runtime_regression.py
fixtures. Six cases exercise native, transparent and half-transparent Shroud
and Fog on a loaded, uncompressed 32x32 SHP with a native 32x32 BSurface.
The original bounds/pixel/alpha-address helpers and native surface vtable run;
Win32 and the separate redraw-on-setting boundary are mocked. Lazy resource
loading, the upper Cell::DrawShroud selector and real Windows SEH dispatch are
not exercised. Counts are emulated memory operations and manager calls, not
Windows timings, multiplayer validation or evidence about CRC causality.
"""
import argparse,sys,struct,json,hashlib,importlib.util,ctypes as C
from pathlib import Path
from collections import Counter
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_READ
from unicorn.x86_const import *

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('game',help='Private executable capture containing the supported original code')
parser.add_argument('dll',help='Matching compiled GameRuntime.dll')
parser.add_argument('--output',required=True,help='JSON capture report')
args=parser.parse_args();game=args.game;dll=args.dll;output=Path(args.output)
output.parent.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('draw_support',Path(__file__).with_name('no_interception_runtime.py'))
n=importlib.util.module_from_spec(spec);saved_argv=sys.argv
try:
 sys.argv=[str(spec.origin),game,dll,'--filter','__fixture__','--output',str(output.with_suffix('.helpers.json'))]
 spec.loader.exec_module(n)
finally:
 sys.argv=saved_argv

rows=[]
for mode in (0,1,2):
 for fog in (False,True):
  m=n.fixture(5);m.vector(0xa8da74,[]);m.w(0xa8b230,m.alloc(0x2000))
  # Redraw-on-setting is outside this isolated leaf experiment.
  m.u.hook_add(UC_HOOK_CODE,lambda u,a,z,_:m.ret(0,5),begin=0x6d2790,end=0x6d2790)
  s=n.state(m);s.renderReveal=mode;n.save(m,s);s=n.tick(m)
  if mode:assert s.data.renderRevealStatus==1,s.data.renderRevealStatus
  shape=m.alloc(0x1000);buf=m.alloc(0x2000);alpha=m.alloc(0x100);surface=m.alloc(0x100);point=m.alloc(8);clip=m.alloc(16)
  m.u.mem_write(shape,bytes(32)+bytes([64])*1024);m.u.mem_write(shape+6,struct.pack('<H',1));m.u.mem_write(shape+8,struct.pack('<4h',0,0,32,32));m.w(shape+28,32)
  m.u.mem_write(buf,struct.pack('<1024H',*([127]*1024)))
  m.w(surface,0x7e2070);m.w(surface+4,32);m.w(surface+8,32);m.w(surface+12,0);m.w(surface+16,2);m.w(surface+20,buf)
  for offset,value in ((0x10,0),(0x14,surface),(0x18,buf),(0x1c,buf+2048),(0x20,2048),(0x28,32),(0x2c,32)):m.w(alpha+offset,value)
  m.w(0x87e8a4,alpha);m.w(0x89e790,shape);m.w(0x89e794,shape);m.w(0x89e760,shape);m.b(0x89e7c5,1);m.b(0x89e7c6,1);m.w(0x886fa4,0)
  m.u.mem_write(point,struct.pack('<2i',0,0));m.u.mem_write(clip,struct.pack('<4i',0,0,32,32))
  ops=Counter();writes=Counter();reads=Counter();outside=[];calls=Counter()
  base=m.dll.OPTIONAL_HEADER.ImageBase;end=base+m.dll.OPTIONAL_HEADER.SizeOfImage
  def manager(u,a,z,_):ops[m.r(m.r(u.reg_read(UC_X86_REG_ESP)+4)+8)]+=1
  def watchwrite(u,access,a,size,value,_):
   if buf<=a and a+size<=buf+2048:cat='alpha_pixels'
   elif surface+12<=a and a+size<=surface+16:cat='surface_lock_count'
   elif a==0 and size==4:cat='seh_chain_fixture'
   elif m.STACK<=a<m.STACK+0x20000:cat='stack'
   elif base<=a<end:cat='runtime_dll'
   elif m.SH<=a<m.SH+C.sizeof(m.Shared):cat='tool_shared'
   else:cat='other';outside.append((hex(a),size,hex(value)))
   writes[cat]+=1
  m.u.hook_add(UC_HOOK_CODE,manager,begin=m.manager_export,end=m.manager_export)
  m.u.hook_add(UC_HOOK_MEM_WRITE,watchwrite)
  def watchread(u,access,a,size,value,_):
   if buf<=a and a+size<=buf+2048:reads['alpha_pixels']+=1
   if shape+32<=a and a+size<=shape+32+1024:reads['source_pixels']+=1
  m.u.hook_add(UC_HOOK_MEM_READ,watchread)
  for at in (0x69e7e0,0x69e740,0x69e580,0x4114b0,0x4115f0,0x411570):
   m.u.hook_add(UC_HOOK_CODE,lambda u,a,z,_:calls.update([hex(a)]),begin=at,end=at)
  fpregs=(UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_MXCSR)+tuple(globals()[f'UC_X86_REG_FP{i}'] for i in range(8))+tuple(globals()[f'UC_X86_REG_XMM{i}'] for i in range(8))
  for i in range(8):m.u.reg_write(globals()[f'UC_X86_REG_FP{i}'],(0x9000000000000000+i*0x100000000000000,0x4000+i))
  m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_FPSW,0x2800);m.u.reg_write(UC_X86_REG_FPTAG,0);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80)
  m.u.reg_write(UC_X86_REG_XMM0,0x0123456789abcdeffedcba9876543210)
  fpbefore=tuple(m.u.reg_read(r) for r in fpregs)
  m.run(0x47f250 if fog else 0x47efe0,(point,clip,0),m.crateCell)
  assert tuple(m.u.reg_read(r) for r in fpregs)==fpbefore,'FP state mismatch'
  values=Counter(struct.unpack('<1024H',m.u.mem_read(buf,2048)))
  expected=127 if mode==1 else 96 if mode==2 else 64
  assert values=={expected:1024},(mode,fog,values)
  assert m.r(surface+12)==0
  assert not outside,outside[:10]
  rows.append(dict(case=f'draw_mode_{mode}_{"fog" if fog else "shroud"}',status='PASS',mode=mode,fog=fog,manager_operations=dict(ops),writes=dict(writes),pixel_reads=dict(reads),native_calls=dict(calls),fp_state_preserved=True,alpha_values=dict(values),surface_lock_final=0))
  print(rows[-1],flush=True)
output.write_text(json.dumps({
 'scope':__doc__,
 'dll_sha256':hashlib.sha256(Path(dll).read_bytes()).hexdigest(),
 'game_fixture_sha256':hashlib.sha256(Path(game).read_bytes()).hexdigest(),
 'game_tested':False,'multiplayer_tested':False,'windows_timing_tested':False,
 'manager_operation_names':{'1':'Leave','5':'Capture','6':'Validate'},
 'results':rows,
},indent=2)+'\n')
print(f'TOTAL {len(rows)} PASS',flush=True)

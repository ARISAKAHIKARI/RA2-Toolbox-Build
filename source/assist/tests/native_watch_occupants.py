"""Check Occupants layout against validity-checked original x86 slices.
Private input is never copied to the output. This runs field-selection slices,
not full garrison entry/exit, game updates, object destruction or rendering.
"""
from pathlib import Path
import argparse,hashlib,json,struct
from native_bib_capture import Capture,SUPPORTED_SHA
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EAX

def verify(path,stage):
 c=Capture(path,stage);u=c.emulator();base=0x20000000;u.mem_map(base,0x10000)
 def put(p,v):u.mem_write(p,struct.pack('<I',v))
 put(base+0x684,0x12345678);put(base+0x688,base+0x1000);put(base+0x68C,128)
 cases=0
 for count in (1,2,5,128):
  put(base+0x694,count)
  for n in range(count):put(base+0x1000+n*4,0x30000000+n*0x100)
  u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,base+0x8000);u.reg_write(UC_X86_REG_EDX,0)
  # Count +694, reverse index, Items +688, member pointer [Items+index*4].
  u.emu_start(0x458098,0x4580B4,count=20)
  assert u.reg_read(UC_X86_REG_EDI)==0x30000000+(count-1)*0x100;cases+=1
  u.reg_write(UC_X86_REG_ESI,base)
  # Vector vtable +684, Capacity +68C and vector-this +684.
  u.emu_start(0x458704,0x458716,count=10)
  assert u.reg_read(UC_X86_REG_EDX)==0x12345678 and u.reg_read(UC_X86_REG_EBX)==128 and u.reg_read(UC_X86_REG_EDI)==base+0x684;cases+=1
 for count in (0,1,128):
  put(base+0x694,count);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x4581F0,0x4581F6,count=2)
  assert u.reg_read(UC_X86_REG_EAX)==count;cases+=1
 live=valid=nonempty=0
 for b in c.vector(0xA8EB40):
  if c.word(b)!=0x7E3EBC or c.signed(b+0x6C)<=0:continue
  live+=1;capacity=c.signed(b+0x68C);count=c.signed(b+0x694)
  assert 0<=count<=min(capacity,128) and capacity<=65536
  valid+=1;nonempty+=count>0
 ranges=[(0x458098,0x4580B4),(0x458704,0x458716),(0x4581F0,0x4581F6)]
 return {'stage':stage,'controlled_slice_cases':cases,'live_buildings':live,'valid_occupant_headers':valid,'nonempty_occupant_vectors':nonempty,
  'code_ranges':[{'start':hex(a),'end':hex(b),'sha256':hashlib.sha256(c.read(a,b-a)).hexdigest()} for a,b in ranges]}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('capture');p.add_argument('--output',type=Path);args=p.parse_args()
 result={'supported_executable_sha256':SUPPORTED_SHA,'evidence':'original x86 field-selection slices with synthetic vector memory; captured header inventory','stages':[verify(args.capture,s) for s in ('01-off','02-on','03-off')],
  'limitations':['No captured building has occupants in these stages.','No full game garrison entry/exit, rasterizer, Win32 composition or multiplayer execution.']}
 if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result))

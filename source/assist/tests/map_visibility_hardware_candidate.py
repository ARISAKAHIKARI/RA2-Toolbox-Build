"""Replay the original mode0 shroud-frame gate with a hardware-style context edit.

Only models an execution breakpoint clearing ZF at 6D88FC. Does not install a
Windows breakpoint or execute unit/radar rendering, selection or map revealing.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_EFLAGS,UC_X86_REG_EIP
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
raw=a.game.read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=raw);u=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image());cell=0x20000000;u.mem_map(cell,4096)
rows=[];writes=[];force=False;hits=0
def code(cpu,pc,size,user):
 global hits
 if pc==0x6D88FC:
  hits+=1
  if force:cpu.reg_write(UC_X86_REG_EFLAGS,cpu.reg_read(UC_X86_REG_EFLAGS)&~0x40)
 if pc==0x6D8908:cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,lambda cpu,access,at,size,value,user:writes.append((at,size)))
for bits in (0,8,16,24,128,255):
 for force in (False,True):
  u.mem_write(cell+0x12C,struct.pack('<I',bits));u.reg_write(UC_X86_REG_EAX,cell);u.reg_write(UC_X86_REG_EDI,0xFFFFFFFF);u.reg_write(UC_X86_REG_EFLAGS,2);writes.clear();hits=0
  u.emu_start(0x6D88F4,0x6D8908,count=100)
  edi=u.reg_read(UC_X86_REG_EDI);expected=0xFFFFFFFF if force or bits&0x18 else 0xFFFFFFFE
  assert u.reg_read(UC_X86_REG_EIP)==0x6D8908 and edi==expected and hits==1
  assert not writes and bytes(u.mem_read(cell+0x12C,4))==struct.pack('<I',bits)
  rows.append(dict(cell_visibility_bits=bits,force_branch=force,shroud_frame_accumulator=edi,game_memory_writes=0,pass_=True))
assert hashlib.sha256(a.game.read_bytes()).hexdigest()==sha
out=dict(passed=True,scope=__doc__,game_sha256=sha,cases=rows,checks=len(rows),candidate_slots=1,
 sample_has_existing_forwarders=['0x6D88FE -> 0xB7D891 -> 0x6D8903','0x6D8903 -> 0xB7D89B -> 0x6D8908'],
 game_process_accessed=False,windows_veh_tested=False,full_map_rendering_tested=False,multiplayer_tested=False)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(out,indent=2)+'\n');print('PASS:',len(rows),'native visibility-branch cases; one-slot candidate, not complete map reveal')

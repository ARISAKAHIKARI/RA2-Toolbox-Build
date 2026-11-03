"""Read-only disassembly and native displaced-instruction replay for MO viewport ABI.

This never installs the 17-site candidate. It verifies original bytes, complete
instruction boundaries and actual original register/stack behavior in Unicorn.
It does not verify a replacement adapter, GPU rendering, Windows input or netplay.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
raw=Path(a.game).read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
# Public candidate policy already present in the previously reviewed upstream
# integration. These are expected bytes, not replacement instructions.
sites=[
 (0x4f4480,'83ec08a11c738800','render-enter'),(0x4f4492,'a314738800','cursor-suspend'),
 (0x4f44cb,'a11c738800','map-resume'),(0x4f44fd,'8b16508bce','chrome-suspend'),
 (0x4f4505,'8b0d1c738800','chrome-resume'),(0x4f451b,'a019b5b000','map-complete'),
 (0x692b60,'83ec1853555657','edge-input'),(0x692300,'8b44241083ec18','click-query'),
 (0x6930a0,'a078e3a80083ec1c','button-message'),(0x6d7840,'83ec485657','shroud-current-surface'),
 (0x6a7089,'8b116a01ff520c','tooltip-process'),(0x7f7b60,'60a37b00','mouse-get-coords-pointer'),
 (0x478bd1,'8b15a06f8800','mouse-A-origin'),(0x478bd7,'a1a86f8800','mouse-A-width'),
 (0x478c17,'bfa06f8800','mouse-A-rectangle'),(0x478e4a,'8b35a06f8800','mouse-A-origin-second'),
 (0x478e50,'8b1da86f8800','mouse-A-width-second')]
rows=[]
def fnv(data):
 h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 return '%08X'%h
def base():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
 for at,n in ((0x21000000,0x10000),(0x30000000,0x20000),(0x31000000,0x1000)):u.mem_map(at,n)
 def w(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
 def r(at):return int.from_bytes(u.mem_read(at,4),'little')
 regs={UC_X86_REG_EAX:0x11223344,UC_X86_REG_EBX:0x22334455,UC_X86_REG_ECX:0x21000100,UC_X86_REG_EDX:0x33445566,UC_X86_REG_ESI:0x21000200,UC_X86_REG_EDI:0x44556677,UC_X86_REG_EBP:0x55667788,UC_X86_REG_ESP:0x30010000,UC_X86_REG_EFLAGS:0x202}
 for reg,value in regs.items():u.reg_write(reg,value)
 w(0x21000100,0x21000300);w(0x21000200,0x21000300);w(0x2100030c,0x31000040)
 u.mem_write(0x31000040,b'\x31\xc0\xc2\x04\x00') # explicit modeled tooltip virtual call
 w(0x88731c,0x21000500);w(0x887314,0x21000600);u.mem_write(0x886fa0,struct.pack('<4i',168,0,632,568));u.mem_write(0xb0b519,b'\x07');u.mem_write(0xa8e378,b'\x09')
 for i in range(8):w(0x30010000+i*4,0x60000000+i)
 return u,w,r,regs
for at,expected,name in sites:
 expected=bytes.fromhex(expected);actual=pe.get_data(at-0x400000,len(expected));assert actual==expected,hex(at)
 row={'site':'%08X'%at,'bytes':len(expected),'role':name,'original_fnv1a':fnv(actual)}
 if at==0x7f7b60:
  assert int.from_bytes(actual,'little')==0x7ba360
  u,w,r,regs=base();w(0x2100011c,77);w(0x21000120,88);w(0x30010000,0x31000000);w(0x30010004,0x21000400)
  u.emu_start(0x7ba360,0x31000000,count=1000);assert r(0x21000400)==77 and r(0x21000404)==88 and u.reg_read(UC_X86_REG_EAX)==0x21000400 and u.reg_read(UC_X86_REG_ESP)==0x30010008
  row['replay']='actual original virtual target returns raw physical point and RET4'
 else:
  u,w,r,regs=base();instructions=[]
  u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:instructions.append({'at':'%08X'%pc,'size':size}) if at<=pc<at+len(expected) else None)
  u.emu_start(at,at+len(expected),count=1000);assert u.reg_read(UC_X86_REG_EIP)==at+len(expected)
  assert sum(i['size'] for i in instructions)==len(actual);row['instruction_boundaries']=instructions
  sp=u.reg_read(UC_X86_REG_ESP);eax=u.reg_read(UC_X86_REG_EAX);ecx=u.reg_read(UC_X86_REG_ECX)
  if at==0x4f4480:assert sp==regs[UC_X86_REG_ESP]-8 and eax==0x21000500
  elif at==0x4f4492:assert sp==regs[UC_X86_REG_ESP] and r(0x887314)==regs[UC_X86_REG_EAX]
  elif at==0x4f44cb:assert eax==0x21000500 and sp==regs[UC_X86_REG_ESP]
  elif at==0x4f44fd:assert sp==regs[UC_X86_REG_ESP]-4 and r(sp)==regs[UC_X86_REG_EAX] and ecx==regs[UC_X86_REG_ESI] and u.reg_read(UC_X86_REG_EDX)==0x21000300
  elif at==0x4f4505:assert ecx==0x21000500 and sp==regs[UC_X86_REG_ESP]
  elif at==0x4f451b:assert eax==0x11223307 and sp==regs[UC_X86_REG_ESP]
  elif at==0x692b60:assert sp==regs[UC_X86_REG_ESP]-40 and [r(sp+i*4)for i in range(4)]==[regs[reg]for reg in (UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX)]
  elif at==0x692300:assert eax==0x60000004 and sp==regs[UC_X86_REG_ESP]-24
  elif at==0x6930a0:assert eax==0x11223309 and sp==regs[UC_X86_REG_ESP]-28
  elif at==0x6d7840:assert sp==regs[UC_X86_REG_ESP]-80 and r(sp)==regs[UC_X86_REG_EDI] and r(sp+4)==regs[UC_X86_REG_ESI]
  elif at==0x6a7089:assert eax==0 and sp==regs[UC_X86_REG_ESP] and u.reg_read(UC_X86_REG_EDX)==0x21000300
  elif at==0x478bd1:assert u.reg_read(UC_X86_REG_EDX)==168
  elif at==0x478bd7:assert eax==632
  elif at==0x478c17:assert u.reg_read(UC_X86_REG_EDI)==0x886fa0
  elif at==0x478e4a:assert u.reg_read(UC_X86_REG_ESI)==168
  elif at==0x478e50:assert u.reg_read(UC_X86_REG_EBX)==632
  row['replay']='actual displaced native instructions reach aligned continuation with expected registers/stack'
 row['status']='PASS';rows.append(row)
assert hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==sha
result={'scope':__doc__,'game_sha256':sha,'upstream_commit':'33ed558bd38f36b5012eddcc9c0fb109cc68e38a','candidate_installed':False,'replacement_adapter_verified':False,'game_process_opened':False,'multiplayer_tested':False,'results':rows,'pass':True}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('PASS: 17 original native sites/complete instruction boundaries/register-stack replays; replacement adapter and rendering NOT verified')

from pathlib import Path
import argparse,hashlib,json,struct
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX

p=argparse.ArgumentParser();p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
data=Path(a.dll).read_bytes()
assert hashlib.sha256(data).hexdigest()=='58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d'
rows=[]
for base in (0x10000000,0x18000000):
 pe=pefile.PE(data=data);pe.relocate_image(base)
 image=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(base,(len(image)+4095)&~4095);u.mem_write(base,image)
 stack=0x30000000;stop=0x31000000;u.mem_map(stack,0x10000);u.mem_map(stop,0x1000)
 u.mem_write(stack+0x8000,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack+0x8000);u.reg_write(UC_X86_REG_ECX,0x12345678)
 def check(emu,pc,size,_):
  if pc==stop:emu.emu_stop()
 u.hook_add(UC_HOOK_CODE,check);u.emu_start(base+0x1080,stop+1,count=100)
 raw=bytes(u.mem_read(base+0x3b1d4,5));assert raw[0]==0xe9
 target=(0x6d8640+5+struct.unpack('<i',raw[1:])[0])&0xffffffff
 assert target==base+0x2a50
 assert u.reg_read(UC_X86_REG_ESP)==stack+0x8004
 rows.append({'module_base':hex(base),'initializer_rva':'0x1080','descriptor_data_rva':'0x3b1d4','game_site':'0x6d8640','bytes':raw.hex(),'target_rva':hex(target-base),'cdecl_stack_balanced':True,'volatile_ecx_after':hex(u.reg_read(UC_X86_REG_ECX))})
out={'scope':'Actual official ordinary DLL CRT patch-data constructor only, under Unicorn; no host DLL execution or live game','dll_sha256':hashlib.sha256(data).hexdigest(),'cases':rows}
Path(a.output).write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))

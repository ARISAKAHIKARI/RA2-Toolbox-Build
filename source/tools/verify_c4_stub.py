"""Emulate compiled C4 flag-load stub, with callback answer mocked; register/flags/stack/x87/SSE preservation only."""
from pathlib import Path
import argparse,struct,json,hashlib
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('--reference',required=True);p.add_argument('--output',required=True);a=p.parse_args();path=Path(__file__).resolve().parents[1]/'GameRuntime.dll';dll=pefile.PE(str(path));game=pefile.PE(a.reference);base=dll.OPTIONAL_HEADER.ImageBase;mapped=dll.get_memory_mapped_image();site=0x51a4d8
assert game.get_data(site-game.OPTIONAL_HEADER.ImageBase,6)==bytes.fromhex('8A 81 C2 0E 00 00')
prefix=bytes.fromhex('9C 60 89 E3 81 EC 10 02 00 00 83 E4 F0 0F AE 04 24 FC');starts=[];at=0
while (at:=mapped.find(prefix,at))>=0:
 block=mapped[at:at+48]
 if bytes.fromhex('6A 00 56 6A 04 E8') in block:starts.append(at)
 at+=len(prefix)
assert len(starts)==1
start=starts[0];block=mapped[start:start+48];call=block.index(b'\xe8',len(prefix));guard=base+start+call+5+struct.unpack_from('<i',block,call+1)[0];rows=[]
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_FPCW,UC_X86_REG_MXCSR]+list(range(UC_X86_REG_XMM0,UC_X86_REG_XMM0+8))
for on in (0,1):
 for native in (0,1):
  for df in (0,0x400):
   u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(dll.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,mapped);u.mem_map(0x400000,0x800000);u.mem_map(0x2000000,0x20000);u.mem_map(0x3000000,0x10000)
   values=[0x12345678,0x56565656,0x2000000,0xdeadbeef,0x2003000,0x78787878,0x90909090,0x300f000,0x202|df,0x37f,0x1f80]+[int.from_bytes(bytes([i+1])*16,'little') for i in range(8)]
   for reg,val in zip(regs,values):u.reg_write(reg,val)
   u.mem_write(0x2000000+0xec2,bytes([native]));finished=[]
   def code(u,ip,size,_):
    if ip==guard:
     sp=u.reg_read(UC_X86_REG_ESP);ret,op,obj,house=struct.unpack('<4I',u.mem_read(sp,16));assert (op,obj,house)==(4,0x2003000,0);assert not u.reg_read(UC_X86_REG_EFLAGS)&0x400
     for reg in (UC_X86_REG_ECX,UC_X86_REG_EDX):u.reg_write(reg,0xbad)
     for i in range(8):u.reg_write(UC_X86_REG_XMM0+i,0)
     u.reg_write(UC_X86_REG_MXCSR,0x3f80);u.reg_write(UC_X86_REG_FPCW,0x27f);u.reg_write(UC_X86_REG_EAX,on);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
    elif ip==site+6:finished.append(ip);u.emu_stop()
   u.hook_add(UC_HOOK_CODE,code);u.emu_start(base+start,0,count=200)
   expected=values.copy();expected[0]=(values[0]&0xffffff00)|(1 if on else native)
   assert finished==[site+6] and [u.reg_read(reg) for reg in regs]==expected
   assert u.mem_read(0x2000000+0xec2,1)==bytes([native]);rows.append({'enabled':on,'native_C4':native,'DF':df,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');print('PASS: 8 compiled C4 stub cases; only AL changes, all other registers/flags/stack/x87/SSE restored')

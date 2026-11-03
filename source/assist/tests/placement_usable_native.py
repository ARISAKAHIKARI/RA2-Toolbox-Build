import struct,hashlib,json,sys,argparse
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args();f=args.game;data=f.read_bytes();assert hashlib.sha256(data).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=data);u=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase;u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x200000);stop=0x20100000;stack=0x20108000;cell=0x20001000;mp=0x87F7E8
def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
u.mem_write(mp+0xF4,struct.pack('<6i',64,64,0,0,64,64))
errors=[];fixed_errors=[];total=0;old_mismatch_count=0
for x in range(1,140):
 for y in range(1,90):
  if abs(x+y-64)>12 and abs(x+y-192)>20 and abs(x-y-64)>3 and abs(y-x-64)>3:continue
  for level in (0,1,2,6,12):
   for slope in (0,1,2,15,255):
    u.mem_write(cell+0x24,struct.pack('<hh',x,y));u.mem_write(cell+0x11b,bytes((level,slope)));sp=stack;u.mem_write(sp,struct.pack('<III',stop,cell,1));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,mp);u.emu_start(0x578540,stop,count=5000);actual=bool(u.reg_read(UC_X86_REG_EAX)&0xff)
    legal=x+y>64 and x-y<64 and y-x<64 and x+y<=192
    z=level+(1 if slope and x+y<64+4+level else 0)
    fixed=x+y>64+z and x+y<=64+2*64+2+z and x-y<2*64-64 and y-x<64
    expected=legal and fixed
    total+=1
    if actual!=expected:
     old_mismatch_count+=1
     if len(errors)<20:errors.append(dict(x=x,y=y,level=level,slope=slope,native=actual,old_tool=expected,fixed=fixed))
    if actual!=fixed and len(fixed_errors)<20:fixed_errors.append(dict(x=x,y=y,level=level,slope=slope,native=actual,fixed=fixed))
assert total==89225 and old_mismatch_count>0 and not fixed_errors
assert hashlib.sha256(f.read_bytes()).digest()==hashlib.sha256(data).digest()
result=dict(status='PASS',total=total,old_mismatch_count=old_mismatch_count,fixed_mismatches=0,first_old_mismatches=errors,game_sha256=hashlib.sha256(data).hexdigest(),game_process_opened=False,boundary='Native 578540(true), synthetic indexed Cell values; this does not prove such Cells are allocated on every real map.')
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(json.dumps(result))

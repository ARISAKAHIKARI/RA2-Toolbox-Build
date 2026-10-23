"""Execute original LightConvert::UpdateColors field selection up to ramp generation.
Tests native tint state/ABI, not palette rasterization or real Windows output.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
pe=pefile.PE(a.game);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x10000)
obj,stack=0x20000000,0x2000F000
def w(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
def values(at):return struct.unpack('<iii',u.mem_read(at,12))
u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,data:uc.emu_stop(),begin=0x556192,end=0x556192)
rows=[]
for name,r,g,b,tinted,want in [('normal',1000,800,1200,0,(1000,800,1200)),('flash',1300,500,800,1,(1300,500,800)),('restore',-1,-1,-1,0,(1000,800,1200)),('flash_restore',-1,-1,-1,1,(1300,500,800)),('clamp',3000,-10,0,0,(2000,0,0))]:
 w(obj+0x16c,53);w(stack,0)
 for n,v in enumerate((r,g,b,tinted)):w(stack+4+n*4,v)
 u.reg_write(UC_X86_REG_ECX,obj);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x556090,0,count=1000)
 assert u.reg_read(UC_X86_REG_EIP)==0x556192
 assert u.mem_read(obj+0x1b0,1)[0]==tinted
 assert values(obj+(0x1a4 if tinted else 0x198))==want
 rows.append({'case':name,'status':'PASS','active_tint':want})
out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS native normal/tinted RGB offsets, restore and bounds',len(rows))

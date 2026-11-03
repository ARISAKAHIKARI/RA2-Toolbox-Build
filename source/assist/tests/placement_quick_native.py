"""Replay original Q/W completed-building cache setters and getters; no game process."""
import argparse, hashlib, json, struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
raw=a.game.read_bytes();digest=hashlib.sha256(raw).hexdigest();assert digest=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=raw);u=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
u.mem_map(0x20000000,0x20000);u.mem_map(0x21000000,0x10000);u.mem_map(0x22000000,4096)
product,other,typ=0x20001000,0x20002000,0x20003000;stack=0x21008000;stop=0x22000000
def w(p,v):u.mem_write(p,struct.pack('<I',v&0xffffffff))
def r(p):return struct.unpack('<I',u.mem_read(p,4))[0]
def run(pc,self):
 u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,self)
 u.emu_start(pc,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 return u.reg_read(UC_X86_REG_EAX)
w(product+0x520,typ);rows=[]
for cat,slot in ((0,0xB0FE5C),(5,0xB0FE60)):
 w(0xB0FE5C,0);w(0xB0FE60,0);w(typ+0xE08,cat);run(0x734250,product);assert r(slot)==product
 assert run(0x7342A0 if cat==0 else 0x7342B0,0)==product
 rows.append({'case':'native_set_and_get','build_category':cat,'pass':True})
for q,wval,obj,want in ((product,other,product,(0,other)),(other,product,product,(other,0)),(other,other,product,(other,other)),(product,other,0,(0,0))):
 w(0xB0FE5C,q);w(0xB0FE60,wval);run(0x734270,obj);actual=(r(0xB0FE5C),r(0xB0FE60));assert actual==want
 rows.append({'case':'native_clear','input_q':q,'input_w':wval,'object':obj,'output':actual,'pass':True})
w(0xB0FE5C,product);w(0xB0FE60,product);run(0x734270,product);assert r(0xB0FE5C)==0 and r(0xB0FE60)==product
run(0x734270,product);assert r(0xB0FE60)==0;rows.append({'case':'both_equal_requires_two_calls','pass':True})
assert hashlib.sha256(a.game.read_bytes()).hexdigest()==digest
result={'status':'PASS','game_sha256':digest,'cases':rows,'count':len(rows),'game_process_opened':False,'boundary':'Original x86 cache setter/getters and object-specific clear on synthetic memory; Q UI rendering and real Place receiver are not executed.'}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(json.dumps(result))

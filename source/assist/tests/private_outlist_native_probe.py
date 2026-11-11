import json,struct,hashlib
from pathlib import Path
from unicorn import *
from unicorn.x86_const import *
import argparse
p=argparse.ArgumentParser();p.add_argument('--memory',required=True);p.add_argument('--vectors',required=True);p.add_argument('--output',required=True);a=p.parse_args()
b=Path(a.memory).read_bytes()
vectors=[(int(k),int(p),int(h),bytes.fromhex(w),bytes.fromhex(e)) for k,p,h,w,e in (line.split() for line in Path(a.vectors).read_text().splitlines())]
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,b);u.mem_map(0x20000000,0x100000)
stack=0x200f0000;stop=0x200f8000;clock=stop+16;send=stop+32;flush=stop+48;iface=0x20042000;vt=0x20043000;buf=0x20044000;house=0x20000000
w=lambda at,x:u.mem_write(at,struct.pack('<I',x&0xffffffff))
r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
u.mem_write(clock,b'\xb8'+struct.pack('<I',5678)+b'\xc3');u.mem_write(send,b'\xb8\x01\x00\x00\x00\xc2\x10\x00');u.mem_write(flush,b'\x31\xc0\xc3');w(0x7e1530,clock);w(iface,vt);w(vt+4,flush);w(vt+8,send)
sent=[]
def hook(uc,at,size,_):
 if at==0x4068e0:
  sp=uc.reg_read(UC_X86_REG_ESP);uc.reg_write(UC_X86_REG_EIP,r(sp));uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EAX,0)
 if at==send:
  sp=uc.reg_read(UC_X86_REG_ESP);sent.append(bytes(uc.mem_read(r(sp+4),r(sp+8))))
u.hook_add(UC_HOOK_CODE,hook)
def run(at,this,*args):
 w(stack,stop)
 for i,x in enumerate(args):w(stack+4+i*4,x)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,this);u.emu_start(at,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4+4*len(args)
rows=[]
for kind,proto,owner,expected,events in vectors:
 w(house+0x30,owner);w(0xa83d4c,house);w(0xa8ed84,1230);w(0xa8b24c,proto);w(0xa8b554,5);w(0xa8b54c,2);w(0xa8b238,3);w(0xac51fc,0x12345678)
 u.mem_write(0xa8ed74,b'\x00');w(0xa802c8,1);w(0xa802cc,0);w(0xa802d0,1);w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
 u.mem_write(0xafa338,bytes(32))
 if kind<256:
  event=bytearray(events);struct.pack_into('<I',event,3,1230);u.mem_write(0xa802d4,bytes(event))
  u.mem_write(buf,bytes(4096));sent.clear();u.reg_write(UC_X86_REG_EDX,buf);run(0x649ca0,iface,4096,15,65535)
  assert len(sent)==1 and sent[0]==expected,(kind,proto,owner,sent[0].hex(),expected.hex())
  assert bytes(u.mem_read(0x8b4204,111))==events,(kind,proto,'native local copy')
 if proto:
  w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0);u.mem_write(buf,expected);u.reg_write(UC_X86_REG_EDX,len(expected));run(0x64bdd0,buf)
  n=len(events)//111
  assert r(0x8b41f8)==1+n,(kind,proto,owner,'decoder count',r(0x8b41f8),1+n)
  actual=bytes(u.mem_read(0x8b4204+111,len(events)))
  # Decoder scratch trailing bytes outside each event's serialized payload
  # are unspecified. Compare every defined field using the wire length table.
  lengths={1:5,2:5,3:4,4:23,5:24,6:5,7:5,8:0,9:5,10:5,11:16,13:4,14:12,15:12,16:12,17:5,18:8,21:5,22:5,23:4,24:4,30:10,42:10,43:0,44:5,45:4,46:12}
  for i in range(n):
   e=events[i*111:(i+1)*111];v=actual[i*111:(i+1)*111]
   size=7+lengths[e[0]]
   assert v[:size]==e[:size],(kind,proto,owner,i,v[:size].hex(),e[:size].hex())
 rows.append({'kind':kind,'protocol':proto,'house':owner,'events':len(events)//111,'bytes':len(expected),'wire_sha256':hashlib.sha256(expected).hexdigest()})
Path(a.output).write_text(json.dumps({'memory_sha256':hashlib.sha256(b).hexdigest(),'cases':rows,'live_network_tested':False},indent=2))
print('PASS',len(rows),'native wire/decoder cases, including mixed 24-command batches')

"""Original 0x5414C0 broadcast preflight and return-value semantics.
Peer Send callbacks are controlled; this is not a network delivery test.
"""
from pathlib import Path
import struct,hashlib,json,sys
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
game=Path(sys.argv[1]);out=Path(sys.argv[2]);b=game.read_bytes();pe=pefile.PE(data=b)
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x100000)
root=0x20000000;peers=[0x20001000,0x20002000];queues=[0x20003000,0x20004000];vt=0x20005000;stub=0x20006000;buf=0x20007000;stack=0x200f0000;stop=0x200ff000
w=lambda at,v:u.mem_write(at,struct.pack('<I',v&0xffffffff))
r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
calls=[];returns=[1,1]
def hook(uc,at,size,_):
 if at!=stub:return
 sp=uc.reg_read(UC_X86_REG_ESP);peer=uc.reg_read(UC_X86_REG_ECX);i=peers.index(peer)
 args=[r(sp+4+j*4) for j in range(4)];assert args[:3]==[buf,15,1]
 calls.append({'peer':i,'recipient':args[3],'return':returns[i]})
 uc.reg_write(UC_X86_REG_EAX,returns[i]);uc.reg_write(UC_X86_REG_EIP,r(sp));uc.reg_write(UC_X86_REG_ESP,sp+20)
u.hook_add(UC_HOOK_CODE,hook)
def reset():
 calls.clear();returns[:]=[1,1];u.mem_write(root,bytes(0x200));u.mem_write(root+4,b'\x01\x01');w(root+0x44,2);w(root+0x50,peers[0]);w(vt+8,stub)
 w(0xa8b238,3);u.mem_write(0xa8dba0,b'\x00')
 for i,(peer,q) in enumerate(zip(peers,queues)):
  w(root+0x28+i*4,peer);w(peer,vt);w(peer+4,q);w(q+4,8);w(q+0x28,2)
def send():
 for i,v in enumerate([stop,buf,15,1,0xffffffff]):w(stack+i*4,v)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,root);u.emu_start(0x5414c0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+20
 return u.reg_read(UC_X86_REG_EAX)
rows=[]
for name,setup,expected,count in [
 ('broadcast_success',lambda:None,1,2),
 ('broadcast_child_failure_not_propagated',lambda:returns.__setitem__(1,0),1,2),
 ('full_second_peer_preflight_rejects_all',lambda:w(queues[1]+0x28,8),0,0),
 ('inactive_transport',lambda:u.mem_write(root+4,b'\x00'),0,0),
 ('zero_connections',lambda:w(root+0x44,0),0,0),
 ('relay_child_failure_propagated',lambda:(w(0xa8b238,4),u.mem_write(0xa8dba0,b'\x01'),returns.__setitem__(0,0)),0,1)]:
 reset();setup();result=send();assert result==expected and len(calls)==count,(name,result,calls)
 rows.append({'case':name,'result':result,'peer_calls':list(calls)})
out.write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(b).hexdigest(),'cases':rows,'live_network_tested':False},indent=2)+'\n')
print('PASS',len(rows),'original Send boundary cases; broadcast success is not per-peer acknowledgement')

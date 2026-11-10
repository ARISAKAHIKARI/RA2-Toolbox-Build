"""Captured native instruction probes; synthetic mouse/objects. No real game or network."""
import argparse,struct,json,hashlib
from pathlib import Path
from unicorn import *
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('--memory',required=True);p.add_argument('--output',required=True);a=p.parse_args();b=Path(a.memory).read_bytes()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,b);u.mem_map(0x20000000,0x200000)
stack=0x201f0000;stop=0x201f8000;scenario=0x20000000;mouse=0x20010000;vt=0x20011000;building=0x20020000;buildingVT=0x20021000;owner=0x20030000;event=0x20040000;houseArray=0x20050000
w=lambda at,x:u.mem_write(at,struct.pack('<I',x&0xffffffff));r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
counters={'depth':0,'pops':0,'refresh':0,'sell':0,'queued':[]}
def ret(uc,value=0,n=0):
 sp=uc.reg_read(UC_X86_REG_ESP);uc.reg_write(UC_X86_REG_EIP,r(sp));uc.reg_write(UC_X86_REG_ESP,sp+4+n);uc.reg_write(UC_X86_REG_EAX,value)
def hook(uc,at,size,_):
 if at==stop+16:ret(uc,counters['depth'])
 elif at==stop+32:counters['depth']-=1;counters['pops']+=1;ret(uc)
 elif at==0x54f720:counters['refresh']+=1;ret(uc)
 elif at==0x6e6f20:ret(uc,building)
 elif at in (0x6386e0,0x4068e0):ret(uc)
 elif at==stop+48:ret(uc,6) # RTTI Building
 elif at==stop+64:ret(uc,5) # Guard mission, not already selling
 elif at==stop+80:
  sp=uc.reg_read(UC_X86_REG_ESP);counters['queued'].append(r(sp+4));ret(uc,n=8)
 elif at==stop+96:ret(uc)
 elif at==0x447110:counters['sell']+=1
 elif at==0x50b6f0:ret(uc,0) # AI is not local, so no local sale audio
u.hook_add(UC_HOOK_CODE,hook)
def run(at,this=0):
 w(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,this);u.emu_start(at,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
for off in range(16,112,16):u.mem_write(stop+off,b'\xc3')
w(0xa8b230,scenario);w(0x887640,mouse);w(mouse,vt);w(vt+0x28,stop+16);w(vt+0x10,stop+32);w(0x87f770,0x20060000)
rows=[]
for depth in (0,1,5,64):
 counters.update(depth=depth,pops=0,refresh=0);u.mem_write(scenario+0x35a2,b'\x01');u.mem_write(0xa8ed9c,b'\x01');w(0xb07784,19);run(0x684290)
 assert counters['depth']==0 and counters['pops']==depth and counters['refresh']==1
 assert u.mem_read(scenario+0x35a2,1)==b'\x00' and u.mem_read(0xa8ed9c,1)==b'\x00' and r(0xb07784)==0
 rows.append({'unlock_initial_depth':depth,'native_pops':depth,'passed':True})
w(0xa8022c,houseArray);w(houseArray,owner);w(houseArray+4,owner);w(owner+0x30,1);w(building,buildingVT);w(building+0x21c,owner);u.mem_write(building+0x90,b'\x01');u.mem_write(building+0x6e9,b'\x01');w(buildingVT+0x2c,stop+48);w(buildingVT+0x184,stop+64);w(buildingVT+0x1e8,stop+80);w(buildingVT+0x1ec,stop+96);w(buildingVT+0x1a0,0x447110)
for sender in (0,1):
 counters.update(sell=0,queued=[]);packet=struct.pack('<BBBIIB',0x16,0,sender,1234,7,0x34)+bytes(99);u.mem_write(event,packet);run(0x4c6cb0,event)
 assert counters['sell']==(sender==1)
 assert counters['queued']==([19] if sender==1 else [])
 rows.append({'event_house':sender,'building_owner':1,'native_Selling_19_requested':sender==1,'passed':True})
Path(a.output).write_text(json.dumps({'scope':__doc__,'memory_sha256':hashlib.sha256(b).hexdigest(),'cases':rows,'game_tested':False,'multiplayer_tested':False,'refund_or_removal_tested':False},indent=2))
print('PASS native unlock count/refresh and Sell owner/Building Selling mission probes:',len(rows))

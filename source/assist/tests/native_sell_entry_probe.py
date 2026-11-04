"""Controlled original Sell/ClickedEvent/cursor eligibility; no full refund/removal or transport."""
from pathlib import Path
import argparse,json,struct,hashlib
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('memory',help='private unpacked image capture, base VA 0x400000')
parser.add_argument('--output',required=True)
args=parser.parse_args()
mem=Path(args.memory).read_bytes()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,mem);u.mem_map(0x20000000,0x100000)
obj=0x20000000;vt=0x20010000;comvt=0x20011000;houses=[0x20020000,0x20040000];typ=0x20060000;rules=0x20064000;stack=0x200f0000;stop=0x200f8000
stub=0x20070000;calls=[];rows=[]
def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def b(a,v):u.mem_write(a,bytes([v]))
def skip(args=0,ret=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+args*4);u.reg_write(UC_X86_REG_EAX,ret)
def hook(uc,at,size,_):
 if at==stub+0x30:calls.append('NextMission');skip()
 elif at==0x750920:calls.append('sound');skip(2)
 elif at==stub+0x40:skip(0,765)
 elif at==stub+0x50:calls.append('type_operation');skip(1)
 elif at==stub+0x60:calls.append('object_D4');skip()
 elif at==stub+0x70:calls.append('object_F8');skip()
 elif at==0x639fd0:calls.append('planning');skip()
 elif at==0x692a29:calls.append('cursor_reject');uc.emu_stop()
 elif at==0x692b06:calls.append('cursor_done');uc.emu_stop()
u.hook_add(UC_HOOK_CODE,hook)
u.mem_write(stub,b'\x8b\x81\xac\x00\x00\x00\xc3') # current mission
u.mem_write(stub+0x10,b'\xb8\x06\x00\x00\x00\xc3')
u.mem_write(stub+0x20,b'\x8b\x44\x24\x04\x8b\x40\x0c\xc2\x04\x00')
u.mem_write(stub+0x80,b'\x8b\x81\x1c\x02\x00\x00\xc3')
u.mem_write(stub+0x90,b'\x31\xc0\xc3')
def setup(local,owner,session=3):
 calls.clear();u.mem_write(obj,bytes(0x8000));u.mem_write(typ,bytes(0x2000));
 for i,h in enumerate(houses):u.mem_write(h,bytes(0x20000));w(h+0x30,i);b(h+0x1ec,i==local);b(h+0x1ed,i==local)
 w(obj,vt);w(obj+4,comvt);w(obj+0x10,123);w(obj+0x21c,houses[owner]);w(obj+0x520,typ);w(typ,vt);w(obj+0xac,5);w(obj+0xb4,-1);b(obj+0x6e9,1);w(obj+0x534,1)
 for off,addr in [(0x184,stub),(0x1e8,0x5b35e0),(0x1ec,stub+0x30),(0x2c,stub+0x10),(0x3c,stub+0x80),(0x98,0x4494c0),(0x80,stub+0x90),(0x84,stub+0x50),(0xd4,stub+0x60),(0xf8,stub+0x70)]:w(vt+off,addr)
 w(comvt+0x10,stub+0x20);w(0xa83d4c,houses[local]);w(0xa8b238,session);w(0xa802c8,0);w(0xa802cc,0);w(0xa802d0,0);w(0xa8ed84,1234);w(0x7e1530,stub+0x40);b(0xac4cf4,0);w(0x8871e0,rules)
def run(at,self,*args):
 w(stack,stop)
 for i,x in enumerate(args):w(stack+4+4*i,x)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,self);u.emu_start(at,stop,count=4000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
 assert u.reg_read(UC_X86_REG_ESP)==stack+4+4*len(args)
 return u.reg_read(UC_X86_REG_EAX)
for mode in [0,3,4,5]:
 for local in [0,1]:
  for owner in [0,1]:
   setup(local,owner,mode);originalOwner=r(obj+0x21c)
   run(0x447110,obj,-1)
   assert r(obj+0xb4)==19 and 'NextMission' in calls and r(0xa802c8)==0 and r(obj+0x21c)==originalOwner and r(0xa83d4c)==houses[local]
   assert ('sound' in calls)==(local==owner)
   rows.append(dict(path='Building::Sell(-1)',session=mode,local=local,owner=owner,queued_mission=19,calls=calls[:],outlist=0,status='PASS'))
for local in [0,1]:
 for owner in [0,1]:
  for planning in [0,1]:
   setup(local,owner);b(0xac4cf4,planning);run(0x6ffe00,obj,0x16)
   assert r(obj+0xb4)==0xffffffff and r(obj+0x21c)==houses[owner]
   if planning:assert r(0xa802c8)==0 and calls==['planning']
   else:assert r(0xa802c8)==1 and bytes(u.mem_read(0xa802d4,3))==bytes([0x16,0,local]) and r(0xa802db)==123
   rows.append(dict(path='ClickedEvent(Sell)',local=local,owner=owner,planning=planning,outlist=r(0xa802c8),event_house=None if planning else local,mission_unchanged=True,status='PASS'))
for local in [0,1]:
 for owner in [0,1]:
  setup(local,owner);u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x6929cd,stop,count=1000)
  assert ('cursor_done' in calls)==(owner==local)
  if owner==local:assert r(stack+0x10)==12
  rows.append(dict(path='Sell cursor 6929CD',local=local,owner=owner,allow=owner==local,status='PASS'))
for branch in ['no_gate','type_gate']:
 setup(0,1);b(obj+0x6e9,0);b(typ+0x16c0,branch=='type_gate');run(0x447110,obj,-1)
 assert calls==([] if branch=='no_gate' else ['type_operation','object_D4','object_F8'])
 assert r(0xa802c8)==0
 rows.append(dict(path='Building::Sell alternate branch',branch=branch,calls=calls[:],status='PASS'))
Path(args.output).write_text(json.dumps({'scope':__doc__,'capture_sha256':hashlib.sha256(mem).hexdigest(),'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','results':rows},indent=2)+'\n')
print('PASS',len(rows))

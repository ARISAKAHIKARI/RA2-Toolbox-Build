"""Original no-options constructor/receiver/House end-state setter on synthetic Houses.
No network, real UI click, House AI destruction or unit damage is executed.
"""
import argparse,json,struct,hashlib
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
b=Path(a.game).read_bytes();pe=pefile.PE(data=b);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x100000)
houses=[0x20000000,0x20020000];items=0x20040000;event=0x20041000;stack=0x200f0000;stop=0x200f8000;calls=[]
def w(at,x):u.mem_write(at,struct.pack('<I',x&0xffffffff))
def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def b8(at,x):u.mem_write(at,bytes([x]))
def hook(uc,at,size,_):
 if at==0x4fc980:calls.append(uc.reg_read(UC_X86_REG_ECX))
 if at==0x4068e0:
  sp=uc.reg_read(UC_X86_REG_ESP);uc.reg_write(UC_X86_REG_EIP,r(sp));uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EAX,0)
u.hook_add(UC_HOOK_CODE,hook)
def run(at,self,*args):
 w(stack,stop)
 for i,x in enumerate(args):w(stack+4+i*4,x)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,self);u.emu_start(at,stop,count=3000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert u.reg_read(UC_X86_REG_ESP)==stack+4+4*len(args)
for i,h in enumerate(houses):w(items+i*4,h)
w(0xa8022c,items);w(0xa8ed84,1234);rows=[]
for target in (0,1):
 for local in (0,1):
  for state,off in [('active',None),('game_over',0x1f6),('winner',0x1f7),('loser',0x1f8)]:
   for i,h in enumerate(houses):u.mem_write(h,bytes(0x20000));w(h+0x30,i)
   w(0xa83d4c,houses[local]);w(0xac1198,houses[local]) # observer identity does not replace event target
   if off:b8(houses[target]+off,1)
   calls.clear();before=[bytes(u.mem_read(h,0x20000)) for h in houses]
   u.mem_write(event,bytes(111));run(0x4c66c0,event,target,8);assert bytes(u.mem_read(event,3))==bytes([8,0,target]);run(0x4c6cb0,event)
   assert calls==[houses[target]] and bytes(u.mem_read(houses[1-target],0x20000))==before[1-target]
   if state=='active':assert bytes(u.mem_read(houses[target]+0x1f6,1))==b'\x01' and r(houses[target]+0x298)==1234 and r(houses[target]+0x2a0)==0
   else:assert bytes(u.mem_read(houses[target],0x20000))==before[target]
   rows.append({'event':'Destruct','house':target,'local_observer_house':local,'initial':state,'effect':'game_over_flag_and_zero_timer' if state=='active' else 'no_change','status':'PASS'})
for target in (0,1):
 calls.clear();b8(0xa83d48,0);u.mem_write(event,bytes(111));run(0x4c66c0,event,target,0x13);run(0x4c6cb0,event);assert not calls and bytes(u.mem_read(0xa83d48,1))==b'\x01';rows.append({'event':'Exit','house':target,'effect':'global_A83D48_set_no_Destruct_call','status':'PASS'})
for kind in (8,0x13):
 u.mem_write(event,bytes(111));run(0x4c66c0,event,-1,kind);assert bytes(u.mem_read(event,3))==b'\x00\x00\xff';rows.append({'event':kind,'house':-1,'effect':'constructor_empty_event_not_executed','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(b).hexdigest(),'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','results':rows},indent=2)+'\n');print('PASS',len(rows),'constructor and received Destruct/Exit cases; no transport or full destruction')

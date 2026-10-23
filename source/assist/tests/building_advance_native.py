"""Original Building Mission_Attack RANGE branch, Sleep body and normal wake.
Combat queries/SetTarget are doubles; mission transitions and animation-ready
fragment are original code. Not the full animation loop, projectiles or netplay.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
pe=pefile.PE(a.game);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x10000)
building,typ,target,stack,stop=0x20000000,0x20002000,0x20004000,0x2000f000,0x2000e000
w=lambda at,v:u.mem_write(at,struct.pack('<I',v&0xffffffff))
r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
queries={r(0x7e3ebc+0x2e4):(0,1),r(0x7e3ebc+0x3c0):(8,3),r(0x7e3ebc+0x430):(0,0)}
cleared=[]
def ret(value,n):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+n*4)
def hook(uc,at,size,data):
 if at==stop or at==0x45121f:uc.emu_stop()
 elif at==r(0x7e3ebc+0x3c8):
  value=r(u.reg_read(UC_X86_REG_ESP)+4);cleared.append(value);w(building+0x2b4,value);ret(0,1)
 elif at in queries:ret(*queries[at])
u.hook_add(UC_HOOK_CODE,hook)
def reset():
 u.mem_write(building,bytes(0x2000));w(building,0x7e3ebc);w(building+0x520,typ);w(building+0x2b4,target);w(building+0xac,1);w(building+0xb4,-1);w(0xa8ed84,100)
def run(at,*args):
 w(stack,stop)
 for n,v in enumerate(args):w(stack+4+n*4,v)
 u.reg_write(UC_X86_REG_ECX,building);u.reg_write(UC_X86_REG_ESI,building);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(at,stop+1,count=10000)
 assert u.reg_read(UC_X86_REG_EIP) in (stop,0x45121f)
 return u.reg_read(UC_X86_REG_EAX)&255
rows=[]
reset();run(0x44acf0);assert cleared==[0] and r(building+0x2b4)==0 and r(building+0xac)==5
rows.append({'case':'native_out_of_range_attack_clears_target_and_guards','status':'PASS'})
for requested in (1,5):
 reset();w(building+0xac,0);u.mem_write(building+0x6dd,b'\0')
 # Sleep performs no acquisition or attack. A queued wake waits for animation.
 before=bytes(u.mem_read(building,0x800));run(0x5b2e10);assert bytes(u.mem_read(building,0x800))==before
 run(0x5b35e0,requested,1);assert r(building+0xac)==0 and r(building+0xb4)==requested
 assert run(0x454250)==0
 # The original no-active-animation path re-enables readiness. Execute this
 # fragment only; don't claim the rest of Building::AI/animation was simulated.
 w(building+0x534,-1);run(0x451205);assert run(0x454250)==1
 run(0x5b3570);assert r(building+0xac)==requested and r(building+0xb4)==0xffffffff
 rows.append({'case':f'sleep_normal_animation_ready_then_{requested}','status':'PASS'})
out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');print('PASS',len(rows),'original Building range rejection, passive Sleep and ordinary Attack/Guard transitions')

"""Execute original SW permission branches/Grant/CanFire in a synthetic x86 world.
Requires the user's private supported PE; never opens a live game or sends events.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
sha=hashlib.sha256(a.game.read_bytes()).hexdigest();assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(str(a.game));image=pe.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase;u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,image)
heap=0x2000000;u.mem_map(heap,0x100000);stack=0x3000000;u.mem_map(stack,0x20000)
house=heap;sw=heap+0x10000;typ=heap+0x20000;country=heap+0x30000;items=heap+0x40000;timers=heap+0x50000;sp=stack+0x10000;stop=stack+0x18000
w=lambda at,v:u.mem_write(at,struct.pack('<I',v&0xffffffff));b=lambda at,v:u.mem_write(at,bytes([v]));r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0];byte=lambda at:u.mem_read(at,1)[0]
rows=[];seams=[]
def seed():
 u.mem_write(heap,bytes(0x60000));w(house+0x258,items);w(items,sw);w(house+0x34,country);w(sw+0x24,-1);w(sw+0x28,typ);w(sw+0x2c,house);w(sw+0x30,-1);b(sw+0x60,1);w(typ+0xb0,900);b(typ+0xf4,1);w(0xa8ed84,100);w(0xa83d54,timers);w(0xa83d58,16);w(0xa83d60,0);b(0xa83d5d,1)
def execute(at,ends,args=()):
 w(sp,stop)
 for i,x in enumerate(args):w(sp+4+i*4,x)
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,sw);u.reg_write(UC_X86_REG_EBP,house);u.reg_write(UC_X86_REG_EDI,0);u.reg_write(UC_X86_REG_EAX,0)
 hit=[]
 def hook(uc,pc,size,_):
  if pc in ends:hit.append(pc);uc.emu_stop()
 h=u.hook_add(UC_HOOK_CODE,hook)
 try:u.emu_start(at,stop,count=100000)
 finally:u.hook_del(h)
 return hit[0]if hit else stop
for allowed in (0,1):
 for disableable in (0,1):
  seed();b(0xa8b263,allowed);b(typ+0xe7,disableable)
  reached=execute(0x50b298,{0x50b2bb,0x50b358});expected=not disableable or bool(allowed);assert (reached==0x50b2bb)==expected
  rows.append(dict(case='native_house_grant_gate',allowed=allowed,disableable=disableable,grant_path=expected))
  # Existing-building discovery has found a grant source, but this native
  # option gate can still clear it and send the instance to Lose.
  b(sp+0x12,1);execute(0x50b078,{0x50b0a0});keep=byte(sp+0x12);assert bool(keep)==expected
  rows.append(dict(case='native_existing_source_gate',allowed=allowed,disableable=disableable,source_retained=bool(keep)))
for once in (0,1):
 seed();b(sw+0x6d,1);b(sw+0x6e,once);b(0xa8b263,0)
 reached=execute(0x50af47,{0x50af7e,0x50b1ac});assert (reached==0x50b1ac)==bool(once)
 rows.append(dict(case='native_present_instance_maintenance',one_time=once,skip_normal_revoke=bool(once)))
for suspended in (0,1):
 for ready in (0,1):
  seed();b(sw+0x70,suspended);b(sw+0x6f,ready);execute(0x6cc360,set());answer=u.reg_read(UC_X86_REG_EAX)&255;assert bool(answer)==(bool(ready)and not suspended)
  rows.append(dict(case='native_can_fire',suspended=suspended,ready=ready,result=answer))
seed();b(typ+0xe7,1);b(0xa8b263,0);execute(0x6cb560,set(),(0,0,0));assert byte(sw+0x6d)==1 and r(0xa83d60)==1 and r(timers)==sw and r(sw+0x38)==900
rows.append(dict(case='native_normal_grant_with_session_disabled',present=True,timer_registered=True,recharge_frames=900,session_flag_unchanged=byte(0xa8b263)==0))
# Re-grant is idempotent and does not restart a charging existing instance.
before=bytes(u.mem_read(sw,0x80));execute(0x6cb560,set(),(0,0,0));assert bytes(u.mem_read(sw,0x80))==before and r(0xa83d60)==1
rows.append(dict(case='native_repeat_grant_idempotent',pass_=True))
seed();b(typ+0xe7,1);b(0xa8b263,0);execute(0x6cb560,set(),(1,0,0));assert byte(sw+0x6d)==1 and byte(sw+0x6e)==1 and byte(sw+0x6f)==1 and r(0xa83d60)==1
rows.append(dict(case='native_one_time_grant_with_session_disabled',present=True,one_time=True,ready=True,timer_registered=True,session_flag_unchanged=byte(0xa8b263)==0))
a.output.parent.mkdir(parents=True,exist_ok=True);report=dict(pass_=True,supported_pe_sha256=sha,cases=rows,seams=seams,game_process_accessed=False,game_network_events_sent=False,launch_effects_executed=False,scope=__doc__);a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print('PASS:',len(rows),'original superweapon grant/revoke/readiness branches; no game or launch effects')

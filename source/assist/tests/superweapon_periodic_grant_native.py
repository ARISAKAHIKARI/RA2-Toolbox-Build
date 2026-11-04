"""Private native trigger34/regrant/retention and ordinary Fire_SW admission.

Synthetic x86 memory only. Stops before Launch; no effects or network peers.
Local trigger presentation calls are explicit UI sinks in one separate case.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_EAX,UC_X86_REG_EIP

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game',type=Path);p.add_argument('--output',required=True,type=Path)
a=p.parse_args();sha=hashlib.sha256(a.game.read_bytes()).hexdigest()
assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(str(a.game));u=Uc(UC_ARCH_X86,UC_MODE_32)
base=pe.OPTIONAL_HEADER.ImageBase;u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
heap=0x2000000;u.mem_map(heap,0x100000);stack=0x3000000;u.mem_map(stack,0x20000)
house=heap;sw=heap+0x10000;typ=heap+0x20000;country=heap+0x30000
items=heap+0x40000;timers=heap+0x50000;action=heap+0x60000;cell=heap+0x61000
sp=stack+0x10000;stop=stack+0x18000;rows=[];ui=[];ui_sinks=False
w=lambda at,v:u.mem_write(at,struct.pack('<I',v&0xffffffff))
b=lambda at,v:u.mem_write(at,bytes([int(v)]))
r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
byte=lambda at:u.mem_read(at,1)[0]

def seed():
 u.mem_write(heap,bytes(0x70000));w(house+0x258,items);w(house+0x264,1);w(items,sw)
 w(house+0x34,country);w(sw+0x24,-1);w(sw+0x28,typ);w(sw+0x2c,house)
 w(sw+0x30,-1);b(sw+0x60,1);w(typ+0xb0,900);b(typ+0xf4,1);b(typ+0xe7,1)
 w(0xa8ed84,100);w(0xa83d4c,0);b(0xa8b263,0)
 w(0xa83d54,timers);w(0xa83d58,16);w(0xa83d60,0);b(0xa83d5d,1)
 w(action+0x90,0);u.mem_write(cell,struct.pack('<hh',20,30))

def native(at,args=(),this=sw,ends=()):
 w(sp,stop)
 for i,value in enumerate(args):w(sp+4+i*4,value)
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,this)
 u.reg_write(UC_X86_REG_EBP,house);u.reg_write(UC_X86_REG_EDI,0);u.reg_write(UC_X86_REG_EAX,0)
 hit=[]
 def hook(uc,pc,size,_):
  if pc in ends:hit.append(pc);uc.emu_stop();return
  if ui_sinks and pc in (0x6a6300,0x6abc60,0x6a60a0):
   esp=u.reg_read(UC_X86_REG_ESP);count=2 if pc==0x6a6300 else 1
   ui.append(dict(entry=hex(pc),ecx=u.reg_read(UC_X86_REG_ECX),args=[r(esp+4+i*4)for i in range(count)]))
   u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,esp+4+4*count);u.reg_write(UC_X86_REG_EIP,r(esp))
 h=u.hook_add(UC_HOOK_CODE,hook)
 try:u.emu_start(at,stop,count=200000)
 finally:u.hook_del(h)
 if hit:return hit[0]
 assert u.reg_read(UC_X86_REG_EIP)==stop,'native instruction budget exceeded'
 assert u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args),'native ABI mismatch'
 return stop

seed();native(0x6e1c40,(house,0,0,cell),this=action)
assert (byte(sw+0x6d),byte(sw+0x6e),byte(sw+0x60))==(1,0,0)
assert r(sw+0x30)==100 and r(sw+0x38)==900 and not byte(0xa8b263)
rows.append(dict(case='native_trigger34_disabled_session',present=1,one_time=0,can_hold=0,timer_start=100,duration=900,ready=byte(sw+0x6f)))
original=bytes(u.mem_read(sw,0x80))
for tick in range(101,281):
 w(0xa8ed84,tick);native(0x6e1c40,(house,0,0,cell),this=action)
 assert bytes(u.mem_read(sw,0x80))==original
rows.append(dict(case='native_trigger34_repeated_without_loss',start_unchanged=True,one_call_each_game_frame=180,session_flag_unchanged=True))
for can_hold,once,defeated in ((1,0,0),(0,0,0),(1,1,0),(0,0,1)):
 seed();b(sw+0x6d,1);b(sw+0x60,can_hold);b(sw+0x6e,once);b(house+0x1f5,defeated)
 reached=native(0x50af47,ends=(0x50af7e,0x50b1ac))
 expected=(not can_hold or once)and not defeated
 assert (reached==0x50b1ac)==bool(expected)
 rows.append(dict(case='native_existing_instance_retention',can_hold=can_hold,one_time=once,defeated=defeated,skip_normal_source_revoke=bool(expected)))

seed();w(0xa83d4c,house);ui_sinks=True
native(0x6e1c40,(house,0,0,cell),this=action);ui_sinks=False
assert ui[0]['entry']=='0x6a6300' and ui[0]['args']==[31,0]
assert byte(sw+0x6d)==1 and byte(sw+0x60)==0
rows.append(dict(case='native_trigger34_local_ui',calls=ui,native_grant_executed=True,ui_functions_modeled=True))

# Use the original House entry and original ClickFire; stop at Launch so no
# warhead, timer reset after firing, world effects, or network is exercised.
for present,ready,started in ((0,0,0),(0,1,1),(1,0,1),(1,1,0),(1,1,1)):
 seed();b(sw+0x6d,present);b(sw+0x6f,ready);w(sw+0x30,100 if started else -1)
 reached=native(0x4fae50,(0,cell),this=house,ends=(0x6cc390,))
 allowed=reached==0x6cc390;expected=bool(present and ready and started)
 assert allowed==expected,(present,ready,started,hex(reached))
 rows.append(dict(case='native_ordinary_house_fire_admission',present=present,ready=ready,timer_started=started,reached_launch=allowed))

report=dict(pass_=True,private_game_sha256=sha,scope=__doc__,cases=rows,game_process_accessed=False,network_events_sent=False,launch_effects_executed=False,ui_sinks_in_one_case=['0x6a6300','0x6abc60','0x6a60a0'])
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('PASS:',len(rows),'native permanent-grant/regrant/retention/fire-admission cases; no Launch effects or netplay')

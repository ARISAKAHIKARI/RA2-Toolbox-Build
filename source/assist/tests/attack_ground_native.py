"""Execute supported original YR range checks, SetTarget and direct Fire(self).
Map/visibility/coordinate/weapon/muzzle/locomotor and bullet-allocation IO are
synthetic. Original range, complete fire-error overrides, Fire, rearm and ammo
paths execute unchanged. This is not projectile physics or real-game/MP validation.
Private executable bytes are read locally and never included in the output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser()
p.add_argument('game',type=Path)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
digest=hashlib.sha256(a.game.read_bytes()).hexdigest()
SUPPORTED={'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0',
           'c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
if digest not in SUPPORTED:raise SystemExit('Unsupported private executable SHA-256')
image=pefile.PE(str(a.game))
base=image.OPTIONAL_HEADER.ImageBase
mapped=image.get_memory_mapped_image()
rows=[]

def check(name,source,target,range_value,minimum,expected,fire_error=False,cooldown=0,ammo=-1,area_fire=False,shot=False,unit=False,canonicalize=False,facing=0x3fff):
 u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(base,(image.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
 u.mem_write(base,mapped)
 S=0x2000000;u.mem_map(S,0x100000)
 actor=S+0x1000;cell=S+0x3000;weapon=S+0x5000;projectile=S+0x6000
 weapon_struct=S+0x7000;vt=S+0x10000;cell_vt=S+0x11000;stack=S+0x90000
 type_object=S+0x20000;house=S+0x22000;warhead=S+0x24000
 stop=S+0xF0000;stub=S+0xF0100
 def w(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 def ret(value,n):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff)
  u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+n)
 handlers={}
 def bind(table,offset,fn):
  nonlocal stub
  at=stub;stub+=16;w(table+offset,at);handlers[at]=fn
 def coords():
  obj=u.reg_read(UC_X86_REG_ECX);out=r(u.reg_read(UC_X86_REG_ESP)+4)
  u.mem_write(out,bytes(u.mem_read(obj+0x9c,12)));ret(out,4)
 w(actor,vt);w(cell,cell_vt);w(weapon_struct,weapon);w(weapon+0xa0,projectile)
 w(weapon+0xb4,range_value);w(weapon+0xb8,minimum)
 if area_fire:u.mem_write(weapon+0x134,b'\x01')
 u.mem_write(actor+0x9c,struct.pack('<iii',*source))
 u.mem_write(cell+0x9c,struct.pack('<iii',*target))
 for table,kind in [(vt,1 if unit else 15),(cell_vt,11)]:
  bind(table,0x48,coords)
  bind(table,0xa4,coords)
  bind(table,0x58,coords)
  bind(table,0x2c,lambda kind=kind:ret(kind,0))
  bind(table,0x50,lambda kind=kind:ret(int(kind==11),0))
  bind(table,0x54,lambda:ret(0,0))
  bind(table,0x400,lambda:ret(0,0))
 bind(vt,0x3f8,lambda:ret(weapon_struct,4))
 bind(vt,0x3a8,lambda:None)
 w(vt+0x3a8,0x6f77b0)
 for table in [vt,cell_vt]:
  bind(table,0x84,lambda:ret(type_object,0))
  bind(table,0x1bc,lambda:ret(type_object,0))
  bind(table,0x68,lambda:ret(0,8))
  for offset in [0x1d4,0x1d8,0x1dc,0x37c,0x160]:bind(table,offset,lambda:ret(0,0))
 w(actor+0x21c,house);w(actor+0x38,130);w(actor+0x2fc,ammo)
 w(weapon+0xac,warhead);w(weapon+0x9c,1)
 u.mem_write(projectile+0x2a5,b'\x01')
 u.mem_write(warhead+0xa0,struct.pack('<11d',*([1.0]*11)))
 u.mem_write(actor+0x14,b'\x01');u.mem_write(house+0x1ec,b'\x01')
 w(actor+0x2ec,-1);w(actor+0x2f4,cooldown)
 handlers[0x565730]=lambda:ret(cell,4)
 handlers[0x5657a0]=lambda:ret(cell,4)
 handlers[0x578080]=lambda:ret(target[2],4)
 handlers[0x4cc310]=lambda:ret(0,8)
 if shot or canonicalize or name.endswith("_override"):
  # Native firing/SetTarget paths, with synthetic object, muzzle and bullet IO.
  # Combat/rearm/ammo instructions themselves execute unchanged.
  w(actor+0x14,7);w(actor+0x6c0,type_object);w(actor+0x6c4,-1)
  if unit:w(actor+0x6c4,type_object);w(actor+0x6d8,-1);w(actor+0x6c0,-1);w(type_object+0xe40,-1);w(actor+0x388,facing)
  w(actor+0x6c,130);w(actor+0x90,1);w(actor+0x2ec,-1)
  w(weapon+0xa4,100);w(weapon+0xa8,100);w(actor+0x578,0)
  w(type_object+0xa0,0);w(actor+0x38,130)
  u.mem_write(actor+0x160,struct.pack('<d',1.0));u.mem_write(house+0x188,struct.pack('<d',1.0))
  w(0xb0eb30,1);w(0xa8ed84,100)
  loco=S+0x26000;lvt=S+0x27000;w(actor+0x674,loco);w(loco,lvt)
  bind(lvt,0x8c,lambda:ret(0,4));bind(lvt,0x10,lambda:ret(0,4))
  bind(vt,0x3c,lambda:ret(house,0))
  bind(vt,0x78,lambda:ret(2,0))
  bind(vt,0x80,lambda:ret(1,0))
  bind(vt,0x390,lambda:None);w(vt+0x390,0x70d670)
  bind(vt,0x318,lambda:ret(30,4))
  bind(vt,0x124,lambda:ret(0,4));bind(vt,0x3fc,lambda:ret(0,0))
  bind(vt,0xd4,lambda:ret(0,0))
  bind(vt,0x3c0,lambda:None);w(vt+0x3c0,0x740fd0 if unit else 0x51c8b0)
  def muzzle():
   out=r(u.reg_read(UC_X86_REG_ESP)+4)
   u.mem_write(out,struct.pack('<iii',*source));ret(out,20)
  bind(vt,0xb0,muzzle)
  def real_facing():
   out=r(u.reg_read(UC_X86_REG_ESP)+4);w(out,0);ret(out,4)
  bind(vt,0x308,real_facing)
  handlers[0x70bcb0]=coords
  handlers[0x50b6f0]=lambda:ret(0,0)
  handlers[0x487c10]=lambda:ret(0,0)
  bullet=S+0x30000;bvt=S+0x31000;w(bullet,bvt);w(bullet+0xac,projectile)
  bind(bvt,0xd4,lambda:ret(0,0))
  bind(bvt,0x1f0,lambda:ret(1,8))
  handlers[0x773070]=lambda:ret(100,4)
  def create_bullet():
   assert r(u.reg_read(UC_X86_REG_ESP)+4)==actor
   assert u.reg_read(UC_X86_REG_EDX)==actor
   ret(bullet,20)
  handlers[0x46b050]=create_bullet
  handlers[0x46b260]=lambda:ret(0,4)
  handlers[0x46b270]=lambda:ret(0,0)
 seen=set();trail=[]
 def hook(uc,address,size,data):
  seen.add(address)
  trail.append(address)
  if len(trail)>40:trail.pop(0)
  if address==stop or canonicalize and address==0x6fcf44:uc.emu_stop()
  elif address in handlers:handlers[address]()
 u.hook_add(UC_HOOK_CODE,hook)
 sp=stack;u.mem_write(sp,struct.pack('<IIII',stop,actor if name.startswith('self') else cell,0,1))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,actor)
 entry=0x6fcdb0 if canonicalize else (0x741340 if unit else 0x51df60) if shot else (0x740fd0 if unit else 0x51c8b0) if fire_error and name.endswith('_override') else 0x6fc0b0 if fire_error else 0x6f77b0
 try:u.emu_start(entry,stop,count=100000)
 except Exception as error:raise RuntimeError(f'{name}: {error}; EIP={u.reg_read(UC_X86_REG_EIP):08x}; tail={[hex(x) for x in trail]}') from error
 actual=r(actor+0x2b4)==cell if canonicalize else bool(u.reg_read(UC_X86_REG_EAX)) if shot else u.reg_read(UC_X86_REG_EAX) if fire_error else bool(u.reg_read(UC_X86_REG_EAX)&255)
 assert actual==expected,(name,actual,expected,[hex(x) for x in trail])
 assert u.reg_read(UC_X86_REG_EIP)==(0x6fcf44 if canonicalize else stop)
 if not canonicalize:assert u.reg_read(UC_X86_REG_ESP)==sp+(12 if shot else 16 if fire_error else 12)
 assert fire_error or shot or canonicalize or range_value==-512 or 0x6f7220 in seen
 if shot:
  assert 0x6fdd50 in seen and 0x6ff2a4 in seen and 0x70d670 in seen
  assert r(actor+0x2ec)==100 and r(actor+0x2f4)==30
  assert r(actor+0x2fc)==ammo-1
  assert r(actor+0x2b4)==0  # direct Fire does not install a self target/mission
  # A second native readiness query sees the actual rearm timer just started.
  u.mem_write(sp,struct.pack('<IIII',stop,actor,0,1))
  u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,actor)
  u.emu_start(0x740fd0 if unit else 0x51c8b0,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EAX)==3 and r(actor+0x2fc)==ammo-1
 rows.append({'case':name,'status':'PASS','range':range_value,'minimum':minimum,
              'result':actual,'full_common_fire_error':fire_error,'direct_fire':shot,'settarget_canonicalization':canonicalize,'native_distance_comparison':0x6f75f2 in seen})
 print('PASS',name,actual)

check('self_zero_subcell',(2600,2700,0),(2688,2688,0),0,0,True)
check('cell_zero_subcell',(2600,2700,0),(2688,2688,0),0,0,False)
check('self_zero_high_ground',(2600,2700,512),(2688,2688,512),0,0,True)
check('self_minimum_range',(2600,2700,0),(2688,2688,0),256,1,False)
check('cell_normal_range',(2600,2700,0),(2688,2688,0),256,0,True)
check('cell_zero_centered',(2688,2688,0),(2688,2688,0),0,0,True)
check('self_zero_common_fire_error',(2600,2700,0),(2688,2688,0),0,0,0,True)
check('cell_zero_common_fire_error',(2600,2700,0),(2688,2688,0),0,0,8,True)
check('self_zero_reloading',(2600,2700,0),(2688,2688,0),0,0,3,True,cooldown=20)
check('self_zero_out_of_ammo',(2600,2700,0),(2688,2688,0),0,0,1,True,ammo=0)
check('self_area_fire_zero_subcell',(2600,2700,0),(2688,2688,0),0,0,False,area_fire=True)
check('cell_area_fire_zero_subcell',(2600,2700,0),(2688,2688,0),0,0,True,area_fire=True)
check('self_settarget_becomes_cell',(2600,2700,0),(2688,2688,0),0,0,True,canonicalize=True)
check('self_infantry_ready_override',(2600,2700,0),(2688,2688,0),0,0,0,fire_error=True,ammo=3)
check('self_unit_ready_override',(2600,2700,0),(2688,2688,0),0,0,0,fire_error=True,unit=True,ammo=3)
check('self_infantry_reloading_override',(2600,2700,0),(2688,2688,0),0,0,3,fire_error=True,cooldown=20)
check('self_infantry_empty_override',(2600,2700,0),(2688,2688,0),0,0,1,fire_error=True,ammo=0)
check('self_unit_facing_override',(2600,2700,0),(2688,2688,0),0,0,2,fire_error=True,unit=True,ammo=3,facing=0)
check('self_infantry_native_shot',(2600,2700,0),(2688,2688,0),0,0,True,shot=True,ammo=3)
check('self_unit_native_shot',(2600,2700,0),(2688,2688,0),0,0,True,shot=True,unit=True,ammo=3)
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps({'scope':__doc__,'game_sha256':digest,'results':rows,
 'game_tested':False,'multiplayer_tested':False},indent=2)+'\n',encoding='utf-8')

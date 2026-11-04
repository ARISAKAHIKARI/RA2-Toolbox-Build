"""Execute supported original YR range checks for own-object versus own-cell targeting.
Map lookup, visibility, vtable coordinate/weapon access and obstacle checks are
synthetic. The original range arithmetic and comparisons execute unchanged.
This is not full firing, projectile physics, Windows or multiplayer validation.
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

def check(name,source,target,range_value,minimum,expected,fire_error=False,cooldown=0,ammo=-1,area_fire=False):
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
 for table,kind in [(vt,15),(cell_vt,11)]:
  bind(table,0x48,coords)
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
 seen=set();trail=[]
 def hook(uc,address,size,data):
  seen.add(address)
  trail.append(address)
  if len(trail)>12:trail.pop(0)
  if address==stop:uc.emu_stop()
  elif address in handlers:handlers[address]()
 u.hook_add(UC_HOOK_CODE,hook)
 sp=stack;u.mem_write(sp,struct.pack('<IIII',stop,actor if name.startswith('self') else cell,0,1))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,actor)
 try:u.emu_start(0x6fc0b0 if fire_error else 0x6f77b0,stop,count=20000)
 except Exception as error:raise RuntimeError(f'{name}: {error}; EIP={u.reg_read(UC_X86_REG_EIP):08x}; tail={[hex(x) for x in trail]}') from error
 actual=u.reg_read(UC_X86_REG_EAX) if fire_error else bool(u.reg_read(UC_X86_REG_EAX)&255)
 assert actual==expected,(name,actual,expected)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 assert u.reg_read(UC_X86_REG_ESP)==sp+(16 if fire_error else 12)
 assert fire_error or range_value==-512 or 0x6f7220 in seen
 rows.append({'case':name,'status':'PASS','range':range_value,'minimum':minimum,
              'result':actual,'full_common_fire_error':fire_error,'native_distance_comparison':0x6f75f2 in seen})
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
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps({'scope':__doc__,'game_sha256':digest,'results':rows,
 'game_tested':False,'multiplayer_tested':False},indent=2)+'\n')

"""Execute original cell-light, overlay-offset and Z-projection functions.
Only special-light query results are substituted. Controlled memory is not a
live render, DWM/DirectDraw test, or proof of multiplayer behavior.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
pe=pefile.PE(a.game);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image());u.mem_map(0x20000000,0x20000)
cell,scenario,types,kind,out,stack,end=0x20000000,0x20001000,0x20008000,0x20009000,0x2000A000,0x2001F000,0x2001FF00
mode=0;rows=[]
def w(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
def byte(at,value):u.mem_write(at,bytes([value&255]))
def word(at,value):u.mem_write(at,struct.pack('<h',value))
def query(uc,at,size,data):
 sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 uc.reg_write(UC_X86_REG_EAX,int(mode=={0x53A100:1,0x53B400:2,0x53A110:3}[at]));uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
for at in (0x53A100,0x53B400,0x53A110):u.hook_add(UC_HOOK_CODE,query,begin=at,end=at)
def run(at,this,args=(),edx=0):
 w(stack,end)
 for n,v in enumerate(args):w(stack+4+n*4,v)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,this);u.reg_write(UC_X86_REG_EDX,edx);u.emu_start(at,end,count=3000)
 assert u.reg_read(UC_X86_REG_EIP)==end
 return u.reg_read(UC_X86_REG_EAX)
w(0xA8B230,scenario)
for mode in range(4):
 for ambient,ground,level,altitude,offset,scale in [(70,40,35,0,0,65536),(70,40,35,8,75,49152),(10,500,20,1,-50,65536),(200,0,100,10,100,81920)]:
  for g,l in ((0x3540,0x3544),(0x3558,0x355C),(0x358C,0x3590),(0x3570,0x3574)):w(scenario+g,987);w(scenario+l,123)
  g,l=((0x3540,0x3544),(0x3558,0x355C),(0x358C,0x3590),(0x3570,0x3574))[mode]
  w(scenario+0x352C,ambient);w(scenario+g,ground);w(scenario+l,level);w(cell+0x104,scale);word(cell+0x108,offset);byte(cell+0x11B,altitude)
  run(0x484680,cell);normal=ambient*10+offset+altitude*level-ground
  clamp=lambda x:max(0,min(2000,x))
  expected=(clamp(normal),clamp(normal*scale>>16),clamp((normal+4*level)*scale>>16));actual=struct.unpack('<hhh',u.mem_read(cell+0x10A,6));assert actual==expected,(mode,actual,expected)
  rows.append({'case':f'cell_light_{mode}_{altitude}_{ambient}','values':actual,'status':'PASS'})
w(0xA83D84,types);w(0x886FA4,60)
for name,index,flag,typeindex,land,bridge,frame,level,offset in [
 ('ordinary',1,0,0,0,0,0,0,0),('wall',2,0x2A8,0,0,0,0,3,-12),('ore',3,0x2A9,0,0,0,0,6,-12),
 ('veins',4,0x2AA,0,0,0,0,2,-12),('type126',5,0,126,0,0,0,1,-12),('rock126',126,0,0,9,0,0,4,-2),
 ('bridge_start',6,0,0,0,1,0,2,-16),('bridge_middle',6,0,0,0,1,9,2,-31),('bridge_end17',6,0,0,0,1,17,2,-31),
 ('bridge_end18',6,0,0,0,1,18,2,-16),('overlay239',239,0,0,0,0,0,3,-15)]:
 u.mem_write(kind,bytes(0x300));w(types+index*4,kind);w(kind+0x294,typeindex);w(kind+0x298,land)
 if flag:byte(kind+flag,1)
 w(cell+0x44,index);byte(cell+0x140,0x80 if bridge else 0);byte(cell+0x11E,frame);byte(cell+0x11B,level)
 run(0x480110,cell,(out,));actual=struct.unpack('<ii',u.mem_read(out,8));assert actual==(30,60+15-level*15+offset),(name,actual)
 rows.append({'case':name,'offset':actual,'status':'PASS'})
for scale in (.0866025403784,.144337567,.25):
 u.mem_write(0xB0CD48,struct.pack('<d',scale))
 for z in (-104,0,104,416,727,728,1040):
  actual=struct.unpack('<i',struct.pack('<I',run(0x6D20E0,z)))[0];expected=int(z*scale+int(z>=728)+.5);assert actual==expected,(z,scale,actual,expected)
  rows.append({'case':f'projection_{scale}_{z}','height':actual,'status':'PASS'})
blocks={hex(at):hashlib.sha256(pe.get_data(at-0x400000,n)).hexdigest() for at,n in [(0x484680,0x227),(0x480110,0x6D),(0x5FDCC0,0x57),(0x6D20E0,0x3C)]}
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'code_sha256':blocks,'results':rows},indent=2)+'\n')
print('PASS original cell lighting / overlay height / projection:',len(rows))

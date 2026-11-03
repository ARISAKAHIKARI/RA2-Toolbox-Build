"""Native flat-map projection plus explicit physical-A/logical-R roundtrip contract.

Only original CoordsToClient/ClientToCoords/CoordsToCell execute in Unicorn.
The map and Tactical inverse matrix are synthetic. No map-only zoom adapter,
hardware rendering, real Windows input, elevation or netplay is validated.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
raw=Path(a.game).read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=raw);u=Uc(UC_ARCH_X86,UC_MODE_32);image=pe.get_memory_mapped_image();u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
STACK=0x30000000;HEAP=0x21000000;STOP=0x31000000
for at,n in ((STACK,0x20000),(HEAP,0x300000),(STOP,0x1000)):u.mem_map(at,n)
u.reg_write(UC_X86_REG_CR4,0x600)
def w(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def run(at,args,self):
 sp=STACK+0x10000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),STOP,*args));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,self);u.emu_start(at,STOP,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==sp+4+len(args)*4
tac=HEAP;point=HEAP+0x2000;out=HEAP+0x2100;world=HEAP+0x2200;cells=HEAP+0x10000;cell=HEAP+0x200000
w(0x887324,tac);w(0x87f924,cells);w(0x87f928,512*512);u.mem_write(cells,struct.pack('<I',cell)*(512*512))
u.mem_write(tac+0xde4,struct.pack('<12f',256/60,256/30,0,0,-256/60,256/30,0,0,0,0,0,0));w(tac+0xb0,-400);w(tac+0xb4,1465);u.mem_write(world,struct.pack('<3i',50*256+128,60*256+128,0))
rows=[]
for ax in (0,168):
 for percent in (50,75,100,125,150):
  aw,ah=632,568;rw,rh=((aw*100+percent//2)//percent,(ah*100+percent//2)//percent)
  physical=(ax,0,aw,ah);logical=(0,0,rw,rh)
  u.mem_write(0x886fa0,struct.pack('<4i',*logical));u.mem_write(0xb0ce28,struct.pack('<4i',*logical))
  run(0x6d2140,(world,out),tac);render_point=struct.unpack('<2i',u.mem_read(out,8));assert render_point==(100,200)
  screen=(ax+render_point[0]*aw//rw,render_point[1]*ah//rh)
  assert ax<=screen[0]<ax+aw and 0<=screen[1]<ah
  query=((screen[0]-ax)*rw//aw,screen[1]*rh//ah)
  u.mem_write(point,struct.pack('<2i',*query));run(0x6d6590,(out,point),tac);xy=struct.unpack('<2h',u.mem_read(out,4));assert xy==(50,60),(physical,logical,query,xy)
  # Omitting the A/R transformation is observable even without a renderer:
  # passing a physical sidebar-offset/magnified point to native changes cell.
  u.mem_write(point,struct.pack('<2i',*screen));run(0x6d6590,(out,point),tac);unmapped=struct.unpack('<2h',u.mem_read(out,4))
  if percent!=100 or ax:assert unmapped!=(50,60),(physical,logical,screen,unmapped)
  rows.append({'physical_A':physical,'logical_R':logical,'percent':percent,'native_render_point':render_point,'screen_point':screen,'mapped_native_query_point':query,'native_cell':xy,'unmapped_physical_query_cell':unmapped,'status':'PASS'})
assert hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==sha
result={'scope':__doc__,'game_sha256':sha,'candidate_installed':False,'game_process_opened':False,'physical_logical_map_fields_added_to_product':False,'results':rows,'pass':True}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('PASS: 10 native projection/query A-R contract cases; unmapped physical input demonstrably selects a different cell')

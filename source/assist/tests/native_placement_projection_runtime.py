from pathlib import Path
import argparse,struct,json,hashlib
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
parser=argparse.ArgumentParser(description='Replay original native projection routines on a synthetic map; never runs or writes the game.')
parser.add_argument('game',type=Path);parser.add_argument('--output',required=True,type=Path);args=parser.parse_args()
p=args.game;raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0';pe=pefile.PE(data=raw)
u=Uc(UC_ARCH_X86,UC_MODE_32);image=pe.get_memory_mapped_image();u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(image)+4095)&~4095);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,image)
STACK=0x30000000;HEAP=0x21000000;STOP=0x31000000
for at,n in [(STACK,0x20000),(HEAP,0x300000),(STOP,0x1000)]:u.mem_map(at,n)
u.reg_write(UC_X86_REG_CR4,0x600)
def w(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def run(at,args,self):
 sp=STACK+0x10000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),STOP,*args));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,self)
 u.emu_start(at,STOP,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==sp+4+len(args)*4
tac=HEAP;point=HEAP+0x2000;out=HEAP+0x2100;world=HEAP+0x2200;cells=HEAP+0x10000;cell=HEAP+0x200000
w(0x887324,tac);w(0x87f924,cells);w(0x87f928,512*512)
u.mem_write(cells,struct.pack('<I',cell)*(512*512))
u.mem_write(tac+0xde4,struct.pack('<12f',256/60,256/30,0,0,-256/60,256/30,0,0,0,0,0,0))
w(tac+0xb0,-400);w(tac+0xb4,1465)
u.mem_write(world,struct.pack('<3i',50*256+128,60*256+128,0))
rows=[]
for vx,vy in [(0,0),(168,0),(0,32),(168,32)]:
 u.mem_write(0x886fa0,struct.pack('<4i',vx,vy,632,568));u.mem_write(0xb0ce28,struct.pack('<4i',vx,vy,632,568))
 run(0x6d2140,(world,out),tac);local=struct.unpack('<2i',u.mem_read(out,8))
 u.mem_write(point,struct.pack('<2i',*local));run(0x6d2280,(out,point),tac);inverse=struct.unpack('<3i',u.mem_read(out,12))
 absolute=(local[0]+vx,local[1]+vy);u.mem_write(point,struct.pack('<2i',*absolute));run(0x6d6590,(out,point),tac);xy=struct.unpack('<2h',u.mem_read(out,4))
 rows.append({'viewport_origin':[vx,vy],'native_coords_to_client':local,'inverse_of_native_local':inverse,'native_coords_to_cell_input':absolute,'native_cell':xy})
 assert local==(100,200) and xy==(50,60)
 assert abs(inverse[0]-(50*256+128))<=1 and abs(inverse[1]-(60*256+128))<=1 and inverse[2]==0
height_rows=[]
# Runtime initializes this global; the raw image contains zero. Supply each
# modeled actual projection scale explicitly and execute the original routine.
u.mem_write(0x886fa0,struct.pack('<4i',0,0,800,600));u.mem_write(0xb0ce28,struct.pack('<4i',0,0,800,600))
for scale in (.0866025403784,.15,.25):
 u.mem_write(0xB0CD48,struct.pack('<d',scale))
 for z in (0,104,208,416,728,832):
  u.mem_write(world,struct.pack('<3i',50*256+128,60*256+128,z));run(0x6d2140,(world,out),tac);xy=struct.unpack('<2i',u.mem_read(out,8))
  expected=(100,200-int(z*scale+int(z>=728)+.5));assert xy==expected,(scale,z,xy,expected)
  height_rows.append(dict(scale=scale,z=z,native_client=xy,expected=expected))
r={'scope':'Native CoordsToClient/ClientToCoords/CoordsToCell on a synthetic flat map and inverse matrix, without a DLL or game process','game_sha256':hashlib.sha256(raw).hexdigest(),'passed':True,'rows':rows,'height_rows':height_rows}
assert hashlib.sha256(p.read_bytes()).hexdigest()==r['game_sha256']
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

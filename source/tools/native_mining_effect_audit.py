"""Read-only native mining-draw and Drive admission oracle.

Runs original instructions with explicit object-coordinate, visibility, maximum
speed, Tactical projection and final SHP-draw boundaries. No game is called or
modified; no private executable bytes or resource pixels are written to output.
Requires pefile and Unicorn. This does not certify a live game's final picture.
"""
from pathlib import Path
import json,struct,hashlib,math
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
DATA,STACK,STOP=0x2000000,0x2100000,0x2200000

def run(path):
 raw=Path(path).read_bytes();pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase;im=pe.get_memory_mapped_image()
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)&~4095);u.mem_write(base,im)
 for at in (DATA,STACK,STOP):u.mem_map(at,0x10000)
 def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def ret(value,cleanup):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,r(sp))
 obj,typ,vt,loco,lvt,clip=DATA,DATA+0x2000,DATA+0x4000,DATA+0x5000,DATA+0x5100,DATA+0x6000
 w(obj,vt);w(obj+0x6c4,typ);w(obj+0x674,loco);w(loco,0x7E7EB0);w(loco+8,obj)
 for slot,dst in [(0xac,0x200),(0x1d4,0x300),(0x1d8,0x400),(0x2ec,0x500),(0x538,0x600)]:w(vt+slot,STOP+dst)
 w(0xB1CF98,DATA+0x7000);w(0x87F6C0,DATA+0x7100);w(0x887314,DATA+0x7200)
 u.mem_write(clip,struct.pack('<4i',0,0,800,600));u.mem_write(obj+0x9c,struct.pack('<3i',10000,11000,128))
 state={}
 def hook(_u,at,size,data):
  sp=u.reg_read(UC_X86_REG_ESP)
  if at==STOP+0x100:ret(state['moving'],4)
  elif at==STOP+0x200:
   out=r(sp+4);u.mem_write(out,bytes(u.mem_read(obj+0x9c,12)));ret(out,4)
  elif at in (STOP+0x300,STOP+0x400):ret(0,0)
  elif at==STOP+0x500:ret(17,0)
  elif at==STOP+0x600:ret(256,0)
  elif at==0x6D2140:
   src,dst=r(sp+4),r(sp+8);state['world']=list(struct.unpack('<3i',u.mem_read(src,12)));u.mem_write(dst,struct.pack('<2i',100,200));ret(dst,8)
  elif at==0x4AED70:
   args=[r(sp+4+i*4) for i in range(14)]
   state['draw']={'shape':args[0],'frame':args[1],'point':list(struct.unpack('<2i',u.mem_read(args[2],8))),'flags':args[4],'remap':args[5],'z':args[6],'brightness':args[8],'converter':u.reg_read(UC_X86_REG_EDX)};u.emu_stop()
  elif at in (0x73D29C,STOP+0x806):u.emu_stop()
 u.hook_add(UC_HOOK_CODE,hook)
 trig=[]
 for direction in range(8):
  signed=(direction<<13) if direction<4 else (direction<<13)-65536
  angle=(signed-16383)*struct.unpack_from('<d',im,0x7E2810-base)[0]
  pair=[]
  for fn in (0x4CAD00,0x4CACB0):
   sp=STACK+0x8000;w(sp,STOP+0x800);u.mem_write(sp+4,struct.pack('<d',angle))
   u.mem_write(STOP+0x800,b'\xdd\x1d'+struct.pack('<I',DATA+0x8000))
   u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_FPCW,0xE7F)
   u.emu_start(fn,STOP+0x900,count=2000);pair.append(struct.unpack('<d',u.mem_read(DATA+0x8000,8))[0])
  trig.append(pair)
 cases=[]
 headings=list(range(0,65536,4096))+[4095,4097,61439,61441,65535]
 for rawface in headings:
  for frame in (0,14,15,47):
   for phase in (0,3,29,8192,65536,2147483647,-1):
    for harvesting,harvester,moving in [(1,1,0),(0,1,0),(1,0,0),(1,1,1)]:
     state={'moving':moving};empty=bytes(u.mem_read(0x8A0790,12));u.mem_write(loco+0x30,empty+empty)
     if moving:u.mem_write(loco+0x30,struct.pack('<6i',12000,13000,128,12000,13000,128))
     u.mem_write(STACK,bytes(0x10000));sp=STACK+0x8000
     w(obj+0x388,rawface);w(obj+0x38c,rawface);w(obj+0x390,-1);w(obj+0x398,0);w(obj+0x39c,0);w(obj+0x538,phase);w(obj+0x278,0);w(0xA8ED84,frame)
     u.mem_write(obj+0x6d2,bytes([harvesting]));u.mem_write(typ+0xe0e,bytes([harvester]));w(sp+0x54,1000);w(sp+0x58,clip);w(sp+0x74,clip)
     u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EBP,1000);u.reg_write(UC_X86_REG_FPCW,0xE7F)
     u.emu_start(0x73D0D1,STOP,count=20000)
     active=bool(harvesting and harvester and not moving);assert bool(state.get('draw'))==active,state
     if active:
      direction=(((rawface>>12)+1)>>1)&7;assert state['world']==[10000+math.floor(trig[direction][0]*30),11000+math.floor(-trig[direction][1]*30),128],(rawface,state['world'],trig);signed=(phase+frame)&0xffffffff;signed=signed if signed<0x80000000 else signed-0x100000000;expected=((7-direction)*15+signed-int(signed/15)*15)&0xffffffff
      assert state['draw']['frame']==expected and state['draw']['shape']==DATA+0x7000 and state['draw']['converter']==DATA+0x7100 and state['draw']['flags']==0x2A00 and state['draw']['z']==15,state
      cases.append({'raw_facing':rawface,'frame':frame,'phase':phase,'world':state['world'],'draw':state['draw']})
 return {'sha256':hashlib.sha256(raw).hexdigest(),'ranges':{hex(a):{'end':hex(e),'sha256':hashlib.sha256(im[a-base:e-base]).hexdigest()} for a,e in [(0x73D0D1,0x73D29C),(0x4AF002,0x4AF080),(0x4AFB80,0x4AFC85),(0x4C9480,0x4C94AD)]},'native_cos_sin':trig,'cases':len(headings)*4*7*4,'active_cases':cases,'boundary_doubles':['native Drive Is_Moving and Is_Powered execute; max speed getter=256','Techno GetCoords','2 hidden checks','GetZ=17','Tactical projection=(100,200)','final SHP draw not executed']}
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--game',type=Path,nargs='+',required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args()
 results=[run(path) for path in args.game]
 report={'result':'PASS','scope':'Original x86 mining/Drive admission and frame/world-point math with explicit draw/projection/getter boundaries; no live-game image, GPU or multiplayer certification','samples':results}
 args.output.parent.mkdir(parents=True,exist_ok=True)
 args.output.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({'result':'PASS','samples':[{'sha256':sample['sha256'],'cases':sample['cases'],'draw_cases':len(sample['active_cases'])} for sample in results]}))

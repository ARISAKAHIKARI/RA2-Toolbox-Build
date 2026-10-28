import hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
DATA=0x2000000;STACK=0x2100000;STOP=0x2200000
def run(game):
 raw=Path(game).read_bytes();pe=pefile.PE(data=raw);im=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)&~4095);u.mem_write(base,im)
 for p in [DATA,STACK,STOP]:u.mem_map(p,0x10000)
 def w(p,v):u.mem_write(p,struct.pack('<I',v&0xffffffff))
 def r(p):return struct.unpack('<I',u.mem_read(p,4))[0]
 def ret(v,clean):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,v);u.reg_write(UC_X86_REG_ESP,sp+4+clean);u.reg_write(UC_X86_REG_EIP,r(sp))
 source=DATA;vt=DATA+0x1000;target=DATA+0x2000;destination=DATA+0x3000;last=DATA+0x4000;palette=DATA+0x5000;converter=DATA+0x6000;buffer=DATA+0x7000;queue=DATA+0x8000
 w(source,vt);w(vt+0x300,STOP+0x100);w(vt+0x438,0x4dc060);w(vt+0x48,STOP+0x200)
 for p in [target,destination,last]:w(p,vt)
 for p,coord in [(source,(1000,2000,0)),(target,(3000,4000,0)),(destination,(5000,6000,0)),(last,(7000,8000,0))]:u.mem_write(p+0x9c,struct.pack('<iii',*coord))
 w(0x87f6c4,converter);w(converter+4,2);w(converter+0x174,palette);u.mem_write(palette+6,struct.pack('<H',0x07e0));u.mem_write(palette+16,struct.pack('<H',0xf800));
 for p,v in zip([0x8a0dd0,0x8a0dd4,0x8a0dd8,0x8a0ddc,0x8a0de0,0x8a0de4],[11,3,0,3,5,2]):w(p,v)
 records=[];draws=[]
 def hook(uc,at,size,_):
  sp=uc.reg_read(UC_X86_REG_ESP)
  if at==STOP+0x100:
   out=r(sp+4);u.mem_write(out,bytes(u.mem_read(source+0x9c,12)));ret(out,8)
  elif at==STOP+0x200:
   out=r(sp+4);obj=uc.reg_read(UC_X86_REG_ECX);u.mem_write(out,bytes(u.mem_read(obj+0x9c,12)));ret(out,4)
  elif at==0x70bcb0:u.mem_write(buffer,bytes(u.mem_read(target+0x9c,12)));ret(buffer,4)
  elif at==0x568300:ret(0,4)
  elif at==0x7049c0:
   draws.append({'source':list(struct.unpack('<iii',u.mem_read(sp+4,12))),'destination':list(struct.unpack('<iii',u.mem_read(sp+16,12))),'rgb':list(u.mem_read(sp+28,3))});ret(0,36)
 u.hook_add(UC_HOOK_CODE,hook)
 for name,selected,enabled,start,targetP,destinationP,nav,expected in [
  ('unselected',0,1,40,0,destination,0,None),('option_off',1,0,40,0,destination,0,None),
  ('move',1,1,40,0,destination,0,(5000,6000,0)),('attack',1,1,40,target,destination,0,(3000,4000,0)),
  ('expired',1,1,35,0,destination,0,None),('paused',1,1,-1,0,destination,0,(5000,6000,0)),
  ('last_navigation_entry',1,1,40,0,destination,1,(7000,8000,0)),('no_target',1,1,40,0,0,0,None)]:
  draws.clear();u.mem_write(source+0x83,bytes([selected]));u.mem_write(0x843108,bytes([enabled]));w(0xa8ed84,50);w(0xb0ea80,start);w(0xb0ea88,15)
  w(source+0x2b4,targetP);w(source+0x5a4,destinationP);w(source+0x58c,queue);w(source+0x598,nav);w(queue,last)
  sp=STACK+0x8000;u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,source);u.emu_start(0x6d4735,0x6d479f,count=2000)
  assert u.reg_read(UC_X86_REG_ESP)==sp
  assert len(draws)==int(expected is not None),(name,draws)
  if expected is not None:
   assert draws[0]['source']==[1000,2000,0] and draws[0]['destination']==list(expected)
   assert draws[0]['rgb']==([248,0,0] if targetP else [0,252,0]),(name,draws)
  records.append({'case':name,'draws':list(draws)})
 return {'sample':Path(game).name,'sha256':hashlib.sha256(raw).hexdigest(),'cases':records,'ranges':{hex(a):{'end':hex(e),'sha256':hashlib.sha256(im[a-base:e-base]).hexdigest()} for a,e in [(0x6d4735,0x6d4756),(0x4dc060,0x4dc330)]}}
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description='Controlled native action-line gate/target/color audit; explicit coordinate boundary doubles, not a live game test.')
 parser.add_argument('--game',required=True,type=Path);parser.add_argument('--output',required=True,type=Path)
 args=parser.parse_args()
 result={'scope':'Actual native Tactical selected/option gate and Foot action-line target/timer/queue/color code. FLH, target-coordinate and endpoint virtuals are boundary doubles; no native geometry, planning-token rendering or live output proof.','sample':run(args.game)}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS: 8 native Tactical/Foot action-line selected, option, timer, target, last-navigation and palette-color cases')

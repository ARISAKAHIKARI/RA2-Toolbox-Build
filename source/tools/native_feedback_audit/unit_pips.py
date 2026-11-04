from pathlib import Path
import hashlib,json,struct
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXPECTED={3:'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0',4:'c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
DATA=0x2000000;STACK=0x2100000;STOP=0x2200000
def run(game):
 p=Path(game);raw=p.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest not in EXPECTED.values():raise ValueError('Unsupported engine SHA-256')
 number=next(key for key,value in EXPECTED.items() if value==digest)
 pe=pefile.PE(data=raw);im=pe.get_memory_mapped_image();b=pe.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(b,(len(im)+4095)&~4095);u.mem_write(b,im)
 for at in [DATA,STACK,STOP]:u.mem_map(at,0x10000)
 def w(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 def ret(value,cleanup):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,r(sp))
 source=DATA;typ=DATA+0x2000;child=DATA+0x4000;childType=DATA+0x5000;point=DATA+0x6000;original=point+0x10;clip=point+0x20;allocation=DATA+0x8000
 w(source,0x7F5C70);w(source+0x6C4,typ);w(typ,0x7F6218);w(source+0x214,-1);w(source+0x21C,DATA+0x7000)
 w(child,0x7EB058);w(child+0x6C0,childType);u.mem_write(childType+0x380,struct.pack('<d',1));w(childType+0xDFC,5)
 u.mem_write(point,struct.pack('<ii',90,210));u.mem_write(original,struct.pack('<ii',100,200));u.mem_write(clip,struct.pack('<4i',0,0,800,600))
 w(0xAC147C,DATA+0x7100);w(0xAC1480,DATA+0x7200);w(0x887314,DATA+0x7300);w(0x87F6C4,DATA+0x7400)
 draws=[];state={}
 def hook(uc,at,size,_):
  sp=u.reg_read(UC_X86_REG_ESP)
  if at==0x4AED70:
   draws.append({'shape':hex(r(sp+4)),'frame':r(sp+8),'point':list(struct.unpack('<ii',u.mem_read(r(sp+12),8))),'flags':hex(r(sp+20))});ret(1,56)
  elif at in [0x50D9C0,0x50D9D0]:ret(0,0)
  elif at==0x7C8E17:u.mem_write(allocation,b'\0'*4096);ret(allocation,0)
  elif at==0x7C8B3D:ret(0,0)
 u.hook_add(UC_HOOK_CODE,hook)
 cases=[]
 for name,mode,maximum,ammo,wrap,passengers,gap,expected in [
  ('ammo_3_of_5',1,5,3,0,0,0,[(13,[85,207]),(13,[89,207]),(13,[93,207])]),
  ('ammo_empty',1,5,0,0,0,0,[]),
  ('ammo_wrap',1,8,5,4,0,0,[(16,[85,207]),(15,[89,207]),(15,[93,207]),(15,[97,207])]),
  ('ifv_empty',3,1,0,0,0,0,[(0,[85,210])]),
  ('ifv_passenger',3,1,0,0,1,0,[(5,[85,210])]),
  ('apc_gap',3,3,0,0,1,1,[(5,[85,210]),(0,[97,210]),(0,[101,210])])]:
  draws.clear();state['ammo']=ammo;w(typ+0x3D4,mode);w(typ+0x684,maximum if mode==1 else 0);w(typ+0x3E4,wrap);w(typ+0x5E0,maximum if mode==3 else 0);u.mem_write(typ+0x805,bytes([gap]));w(source+0x2FC,ammo);w(source+0x114,passengers);w(source+0x118,child if passengers else 0)
  if b+len(im)>0xBAD02E:u.mem_write(0xBAD02E,b'\0')
  sp=STACK+0x8000;u.mem_write(sp,struct.pack('<4I',STOP,point,original,clip));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,source)
  try:u.emu_start(0x709A90,STOP,count=100000)
  except Exception as error:raise RuntimeError((number,name,hex(u.reg_read(UC_X86_REG_EIP)),hex(u.reg_read(UC_X86_REG_ESP)),draws)) from error
  assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==sp+16,(number,name)
  actual=[(d['frame'],d['point']) for d in draws];assert actual==expected,(number,name,actual,expected)
  assert all(d['shape']==hex(DATA+0x7200) for d in draws)
  cases.append({'name':name,'status':'PASS','draws':draws.copy()})
 return {'sample':number,'sha256':digest,'entry':'0x709A90','abi':'thiscall ret12','cases':cases,'boundary_doubles':['final SHP draw 0x4AED70','self-heal predicates 0x50D9C0/0x50D9D0 false','allocation/free'],'ranges':{hex(a):{'end':hex(e),'sha256':hashlib.sha256(im[a-b:e-b]).hexdigest()} for a,e in [(0x709A90,0x70A98A),(0x716290,0x7162F0),(0x6F6A8E,0x6F6AB6),(0x5F65A0,0x5F65BF)]}}
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description='Controlled actual native ammo/passenger pip draw scheduler; synthetic memory and final SHP/allocation/predicate boundaries, not game imagery.')
 parser.add_argument('--game',type=Path,action='append',required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args();reports=[run(path) for path in args.game]
 result={'scope':'Actual native pip draw scheduling, GetPipMax/GetPipFillLevel, frame/shape/point arguments; synthetic unit/type/passenger memory and explicit final draw/allocation/predicate boundaries. Not game/Windows output.','reports':reports}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS:',sum(len(row['cases']) for row in reports),'actual native ammo/passenger/wrap/gap draw scheduling cases')


import hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ_UNMAPPED
from unicorn.x86_const import *
MAP=0x87F7E8; DATA=0x2000000; STACK=0x2100000; STOP=0x2200000
def run(game):
 p=Path(game)
 raw=p.read_bytes();pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase;im=pe.get_memory_mapped_image()
 site=0x47ED25-base;active_blocked=False;target=None
 if im[site]==0xE9:
  target=0x47ED2A+struct.unpack_from('<i',im,site+1)[0]
  relative=target-base;active_blocked=relative>=0 and relative+32<=len(im) and not any(im[relative:relative+32])
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)&~4095);u.mem_write(base,im)
 for at in (DATA,STACK,STOP):u.mem_map(at,0x10000)
 def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def ret(value,cleanup):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+cleanup);u.reg_write(UC_X86_REG_EIP,r(sp))
 state={};cell=DATA;typ=DATA+0x2000;vt=DATA+0x4000;pos=DATA+0x5000;clip=DATA+0x5100
 w(vt+0x2c,STOP+0x100);w(typ,vt);w(typ+0x67c,0);w(typ+0xe58,0)
 u.mem_write(cell+0x24,struct.pack('<hh',70,80));w(MAP+0x11a8,typ);w(MAP+0x1194,typ)
 w(0x887314,DATA+0x6000);w(0x87f6c4,DATA+0x6100);w(0x8a03fc,DATA+0x6200)
 u.mem_write(pos,struct.pack('<ii',100,200));u.mem_write(clip,struct.pack('<4i',0,0,800,600))
 def hook(_u,at,size,data):
  if at==STOP+0x100:ret(7,0)
  elif at==0x578080:ret(0,4)
  elif at==0x586360:ret(0,4)
  elif at==0x47c620:state['can_exist_calls']+=1;ret(state['can_exist'],12)
  elif at==0x4aed70:
   sp=u.reg_read(UC_X86_REG_ESP);point=r(sp+12)
   state['draws'].append({'frame':r(sp+8),'point':list(struct.unpack('<ii',u.mem_read(point,8))),'flags':hex(r(sp+20))})
   ret(1,56)
  elif at in (0x568300,0x5657a0):
   sp=u.reg_read(UC_X86_REG_ESP);coords=list(struct.unpack('<hh',u.mem_read(r(sp+4),4)))
   state.setdefault('mark_coords',[]).append(coords)
   ret(1 if at==0x568300 else cell,4)
 u.hook_add(UC_HOOK_CODE,hook)
 def bad(_u,access,address,size,value,data):
  print("BAD",hex(u.reg_read(UC_X86_REG_EIP)),hex(address),size,"SP",hex(u.reg_read(UC_X86_REG_ESP)),state);return False
 u.hook_add(UC_HOOK_MEM_READ_UNMAPPED,bad)
 cases=[]
 for flags,prox,can,level,slope in [(2,1,1,0,0),(2,1,0,0,0),(2,0,1,0,0),(2,1,1,2,0),(2,1,0,-1,1),(2,1,0,2,4),(4,0,0,2,1),(0,1,1,0,0)]:
  if active_blocked and flags&2:continue
  u.mem_write(cell+0x11b,struct.pack('<bB',level,slope));w(cell+0x12c,flags);u.mem_write(MAP+0x1180,bytes([prox]))
  state={'can_exist':can,'can_exist_calls':0,'draws':[]}
  sp=STACK+0x8000;u.mem_write(sp,struct.pack('<4I',STOP,pos,clip,1));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,cell)
  u.emu_start(0x47ec90,STOP,count=1000)
  assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==sp+16
  expected=None if not flags else 1 if flags&4 else 0 if prox and can else 2+slope
  assert len(state['draws'])==int(expected is not None)
  if expected is not None:
   d=state['draws'][0];assert d['frame']==expected and d['point']==[130,200-level*15-1]
  cases.append({'flags':flags,'proximity':prox,'can_exist':can,'level':level,'slope':slope,'draws':state['draws'],'can_exist_calls':state['can_exist_calls']})
 # Execute the real Centered calculation followed by real frame-crop offset.
 centers=[]
 for cw,ch,x,y,cropx,cropy in [(60,30,130,169,0,0),(61,35,130,169,3,7),(60,60,-20,40,0,31)]:
  sp=STACK+0x8000
  for off,value in [(0x58,cw),(0x5c,ch),(0x10,x),(0x18,y),(0x1c,cropx),(0x20,cropy),(0xb8,0x20600)]:w(sp+off,value)
  u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(0x4af002,0x4af038,count=100)
  u.emu_start(0x4af064,0x4af080,count=100)
  actual=[struct.unpack('<i',u.mem_read(sp+o,4))[0] for o in (0x10,0x18)]
  assert actual==[x-cw//2+cropx,y-ch//2+cropy]
  centers.append({'canvas':[cw,ch],'anchor':[x,y],'frame_crop':[cropx,cropy],'result':actual})
 # Native MarkFoundation controls bit 2 and copy variant bit 4, not validity.
 marks=[];foundation=DATA+0x7000;origin=DATA+0x7100
 u.mem_write(foundation,struct.pack('<6h',0,0,1,0,0x7fff,0x7fff));u.mem_write(origin,struct.pack('<hh',70,80));w(MAP+0x117c,foundation);w(MAP+0x118c,foundation)
 for entry,bit in [(0x4a95a0,2),(0x4a9650,4)]:
  for mark in [1,0]:
   w(cell+0x12c,0x108|(0 if mark else bit));state={'mark_coords':[]}
   sp=STACK+0x8000;u.mem_write(sp,struct.pack('<3I',STOP,origin,mark));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,MAP)
   u.emu_start(entry,STOP,count=1000);assert u.reg_read(UC_X86_REG_ESP)==sp+12
   got=r(cell+0x12c);assert bool(got&bit)==bool(mark) and got&0x108==0x108
   marks.append({'function':hex(entry),'bit':bit,'mark':mark,'result_flags':got,'coords':state['mark_coords']})
 return {'sample':p.name,'active_draw_status':'BLOCKED' if active_blocked else 'PASS','active_draw_blocker':{'hook_site':'0x47ED25','target':hex(target),'target_32_bytes_all_zero':True,'native_bytes_not_restored':True} if active_blocked else None,'sha256':hashlib.sha256(raw).hexdigest(),'draw_cases':cases,'centered_cases':centers,'mark_cases':marks,'ranges':{hex(a):{'end':hex(e),'sha256':hashlib.sha256(im[a-base:e-base]).hexdigest()} for a,e in [(0x47ec90,0x47efd3),(0x4a95a0,0x4a96f6),(0x4af002,0x4af080)]}}
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser(description='Controlled original placement-frame/centered-pixel/membership audit, with explicit eligibility/draw boundaries.')
 parser.add_argument('--game',required=True,type=Path);parser.add_argument('--output',required=True,type=Path)
 args=parser.parse_args()
 sample=run(args.game)
 out={'scope':'Actual native instruction execution with draw, type-kind, terrain-height, shroud and cell-eligibility boundary doubles; no full placement validity or live-game image proof','sample':sample,'result':'PARTIAL_PASS_ACTIVE_DRAW_BLOCKED' if sample['active_draw_status']=='BLOCKED' else 'PASS'}
 args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n')
 print(json.dumps({'result':out['result'],'draw_cases':len(sample['draw_cases']),'centered_cases':len(sample['centered_cases']),'mark_cases':len(sample['mark_cases'])}))

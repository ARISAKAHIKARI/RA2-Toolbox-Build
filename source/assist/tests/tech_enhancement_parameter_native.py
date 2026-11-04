"""Read-only native TechLevel-only EFLAGS candidate contract; no real game/DR installation."""
import argparse,json,hashlib,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args();raw=Path(a.game).read_bytes();assert hashlib.sha256(raw).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0';pe=pefile.PE(data=raw)
H,T,HT,B,BT,VT,COUNTS,LIST,HEAP,STOP,STACK=0x20000000,0x20010000,0x20020000,0x20030000,0x20040000,0x20050000,0x20060000,0x20070000,0x20100000,0x20200000,0x300F0000
class Case:
 def __init__(self):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);u=self.u
  u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image())
  u.mem_map(H,0x300000);u.mem_map(0x30000000,0x100000)
  self.heap=HEAP;self.visited=set();self.mock=[]
  self.w(T,0x7F6218);self.w(B,VT+0x400);self.w(BT,0x7E4570)
  for v in (VT,VT+0x400,VT+0x800):
   self.w(v+0x2C,STOP+0x10);self.w(v+0x40,STOP+0x20);self.w(v+0x70,STOP+0x30);self.w(v+0x184,STOP+0x40)
  self.w(H+0x34,HT);self.w(HT+0xB8,0)
  self.w(H+0x1D4,10);self.w(H+0x1EC,1,1)
  self.w(T+0x634,10);self.w(T+0xDA0,-1);self.w(T+0xDA4,-1);self.w(T+0x3B8,100);self.w(T+0x6CC,1)
  for n,o in enumerate([0x5500,0x5514,0x5528,0x553C,0x5550,0x55A0,0x55B4,0x55C8,0x55DC]):
   self.w(H+o+4,COUNTS+n*0x100);self.w(H+o+8,16)
  self.w(0xA8B238,3);self.w(0xA83E40,0)
  self.w(H+0x6C,LIST);self.w(LIST,B);self.w(H+0x78,1)
  self.w(B+0x520,BT);self.w(B+0x21C,H);self.w(B+0x660,1,1);self.w(BT+0xEB8,40);self.w(BT+0x6CC,1)
  self.w(0xA83C6C,LIST+0x200);self.w(LIST+0x200,BT)
  u.hook_add(UC_HOOK_CODE,self.code)
 def w(self,a,v,n=4):self.u.mem_write(a,int(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def r(self,a):return int.from_bytes(self.u.mem_read(a,4),'little')
 def ret(self,value,args=0):
  u=self.u;s=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,self.r(s));u.reg_write(UC_X86_REG_ESP,s+4+args)
 def code(self,u,a,size,data):
  self.visited.add(a)
  if a==STOP:u.emu_stop()
  elif a==STOP+0x10:self.ret(40)
  elif a==STOP+0x20:self.ret(0)
  elif a==STOP+0x30:self.ret(self.r(u.reg_read(UC_X86_REG_ECX)+0x6CC))
  elif a==STOP+0x40:self.ret(0)
  elif a==0x7C8E17:
   s=u.reg_read(UC_X86_REG_ESP);n=self.r(s+4);self.mock.append('allocate');p=self.heap;self.heap+=(n+15)&~15;self.ret(p)
  elif a==0x7C8B3D:self.mock.append('free');self.ret(0)
 def vector(self,base,values):
  p=LIST+0x500 if base==T+0x638 else LIST+0x600
  self.w(base+4,p);self.w(base+8,len(values));self.w(base+0x10,len(values))
  for i,v in enumerate(values):self.w(p+4*i,v)
 def count(self,off,value):self.w(self.r(H+off+4),value)
 def run(self,entry,args):
  self.w(STACK,STOP)
  for i,a in enumerate(args):self.w(STACK+4+i*4,a)
  self.u.reg_write(UC_X86_REG_ESP,STACK);self.u.reg_write(UC_X86_REG_ECX,H if entry==0x4F7870 else T)
  self.u.emu_start(entry,STOP,count=100000)
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,hex(self.u.reg_read(UC_X86_REG_EIP))
  assert self.u.reg_read(UC_X86_REG_ESP)==STACK+4+4*len(args)
  n=self.u.reg_read(UC_X86_REG_EAX);return n if n<0x80000000 else n-0x100000000
rows=[];symbols={}
def flags_cmp(a,b,prior):
 a&=0xffffffff;b&=0xffffffff;r=(a-b)&0xffffffff;v=0
 if a<b:v|=1
 if (r&255).bit_count()%2==0:v|=4
 if (a^b^r)&16:v|=16
 if not r:v|=64
 if r&0x80000000:v|=128
 if ((a^b)&(a^r))&0x80000000:v|=0x800
 return (prior&~0x8d5)|v
for at,n in ((0x4f794a,13),(0x4f7b5c,16)):
 data=pe.get_data(at-0x400000,n);h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 symbols[hex(at)]={'length':n,'fnv1a':'%08X'%h}
codehash=lambda c:hashlib.sha256(b''.join(bytes(c.u.mem_read(0x400000+s.VirtualAddress,s.Misc_VirtualSize))for s in pe.sections if s.Characteristics&0x20000000)).hexdigest()
def run(c,ret,include,intercept):
 c.w(STACK,ret)
 for i,v in enumerate((T,0,include)):c.w(STACK+4+i*4,v)
 c.u.reg_write(UC_X86_REG_ESP,STACK);c.u.reg_write(UC_X86_REG_ECX,H);hits=[]
 code_before=codehash(c);type_before=bytes(c.u.mem_read(T,0x2000));house_before=bytes(c.u.mem_read(H,0x10000))
 def hook(u,at,n,_):
  if at==ret:u.emu_stop();return
  if not intercept or at not in (0x4f7951,0x4f7b66):return
  sp=u.reg_read(UC_X86_REG_ESP);assert c.r(sp+0x4c)==ret and c.r(sp+0x50)==T and c.r(sp+0x10)==H
  assert u.reg_read(UC_X86_REG_EBP)==H
  assert u.reg_read(UC_X86_REG_EDI if at==0x4f7951 else UC_X86_REG_ECX)==T
  regs=(UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP)
  before=tuple(u.reg_read(r)for r in regs);prior=u.reg_read(UC_X86_REG_EFLAGS);other=0xffffffff if at==0x4f7951 else u.reg_read(UC_X86_REG_EBX)
  u.reg_write(UC_X86_REG_EFLAGS,flags_cmp(1,other,prior));assert tuple(u.reg_read(r)for r in regs)==before
  hits.append(hex(at))
 c.u.hook_add(UC_HOOK_CODE,hook);c.u.emu_start(0x4f7870,ret,count=100000);assert c.u.reg_read(UC_X86_REG_EIP)==ret and c.u.reg_read(UC_X86_REG_ESP)==STACK+16
 result=c.u.reg_read(UC_X86_REG_EAX);result=result if result<0x80000000 else result-0x100000000
 assert codehash(c)==code_before and bytes(c.u.mem_read(T,0x2000))==type_before and bytes(c.u.mem_read(H,0x10000))==house_before
 return result,hits
cases=[('hidden_level_minus1',lambda c:None),('house_level0',lambda c:c.w(H+0x1d4,0)),('unbuildable',lambda c:c.w(T+0xc98,1,1)),('required_country',lambda c:c.w(T+0xda0,2)),('forbidden_country',lambda c:c.w(T+0xda4,1)),('missing_prerequisite',lambda c:c.vector(T+0x638,[0])),('owned_prerequisite',lambda c:(c.vector(T+0x638,[0]),c.count(0x5550,1))),('buildlimit0',lambda c:c.w(T+0x3b8,0)),('attained_positive_buildlimit',lambda c:(c.w(T+0x3b8,1),c.count(0x5514,1))),('attained_lifetime_buildlimit',lambda c:(c.w(T+0x3b8,-1),c.count(0x55b4,1))),('prereq_override_real_building',lambda c:(c.vector(T+0x654,[0]),c.count(0x5550,1)))]
for i in range(3):cases.append(('stolen_requirement_'+str(i),lambda c,i=i:c.w(T+0xd9b+i,1,1)))
for ret,include in ((0x6a97d7,0),(0x6aa786,1)):
 for name,setup in cases:
  subject=Case();subject.w(T+0x634,-1);setup(subject)
  reference=Case();reference.w(T+0x634,1);setup(reference)
  got,hits=run(subject,ret,include,True);expected,_=run(reference,ret,include,False);assert got==expected,(name,hex(ret),got,expected)
  rows.append({'case':name,'ui_return':hex(ret),'includeProduction':include,'native_result':got,'equals_synthetic_TypeLevel1_reference':True,'EFLAGS_only_sites':hits,'text_Type_House_unchanged':True})
for level in (2,10,100):
 subject=Case();subject.w(T,0x7eb610);subject.w(T+0x634,level);subject.w(H+0x1d4,1)
 reference=Case();reference.w(T,0x7eb610);reference.w(T+0x634,1);reference.w(H+0x1d4,1)
 got,hits=run(subject,0x6aa786,1,True);expected,_=run(reference,0x6aa786,1,False);assert got==expected==1
 rows.append({'case':'lunar_InfantryType_level_'+str(level),'native_result':got,'equals_synthetic_TypeLevel1_reference':True,'EFLAGS_only_sites':hits,'text_Type_House_unchanged':True})
result={'pass':True,'checks':len(rows),'private_game_sha256':hashlib.sha256(raw).hexdigest(),'query':'Can exact TechLevel=1 semantics be emulated for two native sidebar callers without text or Type rule writes?','candidate_sites':symbols,'interception':'execution breakpoint candidate at post-CMP Jcc 4F7951 and 4F7B66; emulation replaces only arithmetic EFLAGS as CMP(1,rhs), leaving Jcc/body original','actual_native_functions':['CanBuild 4F7870','vector copy','counter access','native UnitType/InfantryType vtables'],'not_tested':['hardware DR/VEH at these new sites','real Windows game/sidebar','AI other callers','MP synchronization','Factory/Produce completion'],'mocked':['malloc/free','synthetic Building mission'], 'game_process_opened':False,'production_or_tests_modified':False,'raw_exe_unchanged':hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==hashlib.sha256(raw).hexdigest(),'results':rows}
out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('PASS',len(rows),'native cases; candidate only, no real DR/VEH')

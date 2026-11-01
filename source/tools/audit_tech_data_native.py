"""Read-only private-PE research. Executes native CanBuild/FindFactory branches.
Only malloc/free and the synthetic Building mission method are mocked.
Never starts game, never edits private EXE or checkout source.
"""
import sys,json,hashlib,struct,argparse,re
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--exe',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
raw=args.exe.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=raw)
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
  self.writes=[]
  u.hook_add(UC_HOOK_CODE,self.code)
  u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,n,value,data:self.writes.append((at,n)) if not 0x30000000<=at<0x30100000 else None)
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
 def run(self,entry,args,this=None):
  self.writes=[]
  self.w(STACK,STOP)
  for i,a in enumerate(args):self.w(STACK+4+i*4,a)
  self.u.reg_write(UC_X86_REG_ESP,STACK);self.u.reg_write(UC_X86_REG_ECX,this if this is not None else H if entry==0x4F7870 else T)
  self.u.emu_start(entry,STOP,count=100000)
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,hex(self.u.reg_read(UC_X86_REG_EIP))
  assert self.u.reg_read(UC_X86_REG_ESP)==STACK+4+4*len(args)
  n=self.u.reg_read(UC_X86_REG_EAX);return n if n<0x80000000 else n-0x100000000
rows=[]
def check(name,setup,expected,limitonly=0,factory=False):
 c=Case();setup(c);r=c.run(0x5F7900,[0,1,1,H]) if factory else c.run(0x4F7870,[T,limitonly,1])
 assert r==expected,(name,r,expected)
 rows.append(dict(name=name,result=r,full_tech_entered=0x4F7889 in c.visited,build_limit_entered=0x4F7F9E in c.visited,allocator_calls=len(c.mock)))
check('ordinary eligible human',lambda c:None,1)
check('TechLevel higher than House blocks',lambda c:c.w(T+0x634,11),0)
check('TechLevel minus1 blocks',lambda c:c.w(T+0x634,-1),0)
check('lower TechLevel alone does not clear Unbuildable',lambda c:(c.w(T+0x634,1),c.w(T+0xC98,1,1)),0)
check('lower TechLevel alone does not clear RequiredHouses',lambda c:(c.w(T+0x634,1),c.w(T+0xDA0,2)),0)
check('lower TechLevel alone does not clear ForbiddenHouses',lambda c:(c.w(T+0x634,1),c.w(T+0xDA4,1)),0)
for i in range(3):check('lower TechLevel does not clear stolen requirement '+str(i),lambda c,i=i:(c.w(T+0x634,1),c.w(T+0xD9B+i,1,1)),0)
check('missing prerequisite still blocks low level',lambda c:(c.w(T+0x634,1),c.vector(T+0x638,[0])),0)
check('genuine prerequisite ownership passes',lambda c:(c.vector(T+0x638,[0]),c.count(0x5550,1)),1)
check('empty prerequisite count bypasses prerequisite gate',lambda c:(c.vector(T+0x638,[0]),c.w(T+0x648,0)),1)
check('BuildLimit zero means zero allowed',lambda c:c.w(T+0x3B8,0),0)
check('positive BuildLimit attained blocks',lambda c:(c.w(T+0x3B8,1),c.count(0x5514,1)),-1)
check('negative BuildLimit counts lifetime production',lambda c:(c.w(T+0x3B8,-1),c.count(0x55B4,1)),0)
def all_locked(c):
 c.w(T+0x634,-1);c.w(T+0xDA0,2);c.w(T+0xDA4,1)
 for i in range(3):c.w(T+0xD9B+i,1,1)
 c.vector(T+0x638,[0])
check('all scalar and prerequisite locks fail normal full check',all_locked,0)
check('native buildlimitOnly skips all tech locks',all_locked,1,limitonly=1)
check('native buildlimitOnly even bypasses Unbuildable',lambda c:(all_locked(c),c.w(T+0xC98,1,1)),1,limitonly=1)
check('prerequisite override real owned building bypasses all scalar tech locks',lambda c:(all_locked(c),c.vector(T+0x654,[0]),c.count(0x5550,1)),1)
check('override cannot bypass Unbuildable first gate',lambda c:(all_locked(c),c.vector(T+0x654,[0]),c.count(0x5550,1),c.w(T+0xC98,1,1)),0)
check('override cannot bypass BuildLimit',lambda c:(all_locked(c),c.vector(T+0x654,[0]),c.count(0x5550,1),c.w(T+0x3B8,0)),0)
check('native factory still accepts locked tech via limitOnly',all_locked,B,factory=True)
check('native factory OwnerFlags disjoint denies',lambda c:c.w(T+0x6CC,2),0,factory=True)
check('OwnerFlags union admits compatible owned factory',lambda c:c.w(T+0x6CC,3),B,factory=True)
check('DoubleOwned data flag allows owner mismatch in MP',lambda c:(c.w(T+0x6CC,2),c.w(T+0xC99,1,1)),B,factory=True)
check('DoubleOwned does not bypass owner mismatch in campaign',lambda c:(c.w(T+0x6CC,2),c.w(T+0xC99,1,1),c.w(0xA8B238,0)),0,factory=True)
check('no actual factory denies',lambda c:c.w(H+0x78,0),0,factory=True)
check('wrong factory class denies',lambda c:c.w(BT+0xEB8,16),0,factory=True)
check('unpowered factory denies',lambda c:c.w(B+0x660,0,1),0,factory=True)
check('naval mismatch denies',lambda c:c.w(T+0xCCE,1,1),0,factory=True)
check('native factory still enforces BuildLimit',lambda c:c.w(T+0x3B8,0),0,factory=True)
# Execute the current production rule-data field plan against actual native
# predicates; keep the limits, factory rules and global superweapon rules.
policy=(Path(__file__).resolve().parents[1]/'assist/source/tech_data_policy.h').read_text(encoding='utf-8')
fieldblock=policy.split('inline constexpr Field Fields[]={',1)[1].split('};',1)[0]
fields=[(int(off,16),0xFFFFFFFF if value=='UINT32_MAX' else int(value,0),int(size)) for off,value,size in re.findall(r'\{(0x[0-9A-Fa-f]+),([0-9A-Za-z_]+),([14])\}',fieldblock)]
assert len(fields)==9
for building in (False,True):
 c=Case();all_locked(c);c.w(T+0xC98,1,1);c.w(T+0x6CC,2)
 if building:c.w(T,0x7E4570);c.w(T+0x16F0,-1);c.w(BT+0xEB8,7)
 original=[bytes(c.u.mem_read(T+off,size)) for off,value,size in fields]
 codehash=hashlib.sha256(c.u.mem_read(0x401000,0x3C0000)).hexdigest()
 assert c.run(0x4F7870,[T,0,1])==0
 for off,value,size in fields:c.w(T+off,value,size)
 assert c.run(0x4F7870,[T,0,1])==1
 assert c.run(0x5F7900,[0,1,1,H])==B
 for (off,value,size),value_before in zip(fields,original):c.u.mem_write(T+off,value_before)
 assert c.run(0x4F7870,[T,0,1])==0
 assert codehash==hashlib.sha256(c.u.mem_read(0x401000,0x3C0000)).hexdigest()
 rows.append(dict(name='production nine-field data plan '+('BuildingType' if building else 'UnitType'),before=0,applied_full_CanBuild=1,applied_FindFactory=True,restored=0,native_code_unchanged=True))
c=Case();all_locked(c);c.w(T,0x7E4570);c.w(T+0x16F0,0);c.w(BT+0xEB8,7)
c.w(0xA8B263,0,1);c.w(0x8871E0,0x20080000);c.w(0x20080000+0x92C,0)
c.w(H+0x258,0x200B0000);c.w(0x200B0000,0x20090000);c.w(0x20090000+0x28,0x200A0000);c.w(0x200A0000+0xE7,1,1)
for off,value,size in fields:c.w(T+off,value,size)
assert c.run(0x4F7870,[T,0,1])==0 and c.r(T+0x16F0)==0
rows.append(dict(name='native superweapon restriction remains enforced by data plan',full_CanBuild=0,superweapon_association_unchanged=True))
# The actual Produce receiver calls this sidebar factory-link helper. With no
# cameo it must return false without adding/removing icons or issuing events.
for kind_value in (3,7,16,40):
 c=Case()
 for tab in range(4):c.w(0x87F7E8+0x1544+tab*3988+84,0)
 r=c.run(0x6A6140,[B,kind_value,0],this=0x87F7E8)
 assert r==0 and not c.writes,(kind_value,r,c.writes)
 rows.append(dict(name='native Produce sidebar link without cameo kind '+str(kind_value),result=r,non_stack_writes=len(c.writes)))
result=dict(status='PASS',checks=len(rows),private_executable_sha256=hashlib.sha256(raw).hexdigest(),no_private_exe_writes=True,no_running_process_operations=True,actual_native_functions=['0x4F7870 CanBuild','0x5F7900 FindFactory','0x4779E0 vector copy','0x49FAE0 counter access','0x748170/748180 native UnitType WhatAmI/GetArrayIndex','0x711EC0 native GetOwners (including DoubleOwned)','0x6A6140 native sidebar factory link with no cameo'],mocked=['malloc/free host runtime','synthetic Building current mission=0'],boundary='Synthetic unit/building-type/house/factory data only. Executes full native predicates; does not execute complete Produce receiver, production progress, sidebar, Windows UI, MOD overrides or multiplayer.',rows=rows)
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS',len(rows))

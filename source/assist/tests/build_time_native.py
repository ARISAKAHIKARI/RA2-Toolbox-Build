"""Controlled execution of the complete supported native TimeToBuild function.

Private PE bytes are inputs only; the receipt contains no code or game assets.
The fixture supplies synthetic House/Type/Rules state, not replacement services.
"""
import argparse, hashlib, json, struct
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

SHA='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
KINDS=[
 dict(name='building',cameo=7,kind=3,abstract=6,array=0xA83C68,vt=0x7E3EBC,tvt=0x7E4570,offset=0x520,getter=0x459EE0,what=0x459EC0,twhat=0x465D90,size=0x720,country=0x140,factory=0x5384),
 dict(name='unit',cameo=40,kind=2,abstract=1,array=0xA83CE0,vt=0x7F5C70,tvt=0x7F6218,offset=0x6C4,getter=0x741490,what=0x746E20,twhat=0x748170,size=0x8E8,country=0x138,factory=0x5380),
 dict(name='infantry',cameo=16,kind=1,abstract=15,array=0xA8E348,vt=0x7EB058,tvt=0x7EB610,offset=0x6C0,getter=0x51FAF0,what=0x523340,twhat=0x524D40,size=0x6F0,country=0x134,factory=0x537C),
 dict(name='aircraft',cameo=3,kind=0,abstract=2,array=0xA8B218,vt=0x7E22A4,tvt=0x7E2868,offset=0x6C4,getter=0x41C200,what=0x41C180,twhat=0x41CFB0,size=0x6D8,country=0x13C,factory=0x5378),
]
OBJ=0x20000000; TYPE=0x20010000; HOUSE=0x20020000; COUNTRY=0x20030000; RULES=0x20040000; STACK=0x30000000; STOP=0x3000F000
def fnv(data):
 x=2166136261
 for c in data:x=((x^c)*16777619)&0xffffffff
 return x

class Native:
 def __init__(self,game):
  self.data=game.read_bytes();assert hashlib.sha256(self.data).hexdigest()==SHA
  self.pe=pefile.PE(data=self.data);self.base=self.pe.OPTIONAL_HEADER.ImageBase
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.u.mem_map(self.base,(self.pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);self.u.mem_write(self.base,self.pe.get_memory_mapped_image())
  self.u.mem_map(OBJ,0x50000);self.u.mem_map(STACK,0x10000)
  self.reads=set();self.writes=set();self.code=set()
  self.u.hook_add(UC_HOOK_MEM_READ,lambda u,t,a,n,v,d:self.reads.add((a,n)))
  self.u.hook_add(UC_HOOK_MEM_WRITE,lambda u,t,a,n,v,d:self.writes.add((a,n)))
  self.u.hook_add(UC_HOOK_CODE,lambda u,a,n,d:self.code.add(a))
 def w(self,a,v,fmt='I'):self.u.mem_write(a,struct.pack('<'+fmt,v))
 def r(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def invoke(self,a,selfptr=OBJ,edx=0):
  self.u.mem_write(STACK,bytes(0x10000));self.w(STACK+0x8000,STOP)
  for reg in [UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP]:self.u.reg_write(reg,0)
  self.u.reg_write(UC_X86_REG_ESP,STACK+0x8000);self.u.reg_write(UC_X86_REG_ECX,selfptr);self.u.reg_write(UC_X86_REG_EDX,edx)
  self.u.reg_write(UC_X86_REG_FPCW,0x027F);self.u.reg_write(UC_X86_REG_FPSW,0);self.u.reg_write(UC_X86_REG_FPTAG,0xffff)
  self.reads.clear();self.writes.clear();self.code.clear()
  self.u.emu_start(a,STOP,count=100000)
  assert self.u.reg_read(UC_X86_REG_EIP)==STOP,'not a normal native return'
  assert self.u.reg_read(UC_X86_REG_ESP)==STACK+0x8004,'callee stack mismatch'
  assert not [(x,n) for x,n in self.writes if not STACK<=x<STACK+0x10000], 'native wrote outside scratch stack'
  return self.u.reg_read(UC_X86_REG_EAX)
 def case(self,k,label,expected,**kw):
  self.u.mem_write(OBJ,bytes(0x50000))
  self.w(OBJ,k['vt']);self.w(OBJ+k['offset'],TYPE);self.w(OBJ+0x21C,HOUSE)
  self.w(TYPE,k['tvt']);self.w(TYPE+0x610,kw.get('cost',1000));self.w(TYPE+0x608,kw.get('type_mult',1.0),'f')
  self.w(TYPE+0xE08,kw.get('buildcat',0));self.w(TYPE+0xCCE,kw.get('naval',0),'B');self.w(TYPE+0x1571,kw.get('wall',0),'B')
  self.w(HOUSE+0x34,COUNTRY);self.w(HOUSE+0x53A4,kw.get('power',100));self.w(HOUSE+0x53A8,kw.get('drain',100))
  for off in [0x134,0x138,0x13C,0x140,0x144]:self.w(COUNTRY+off,3.0,'f')
  country_off=0x144 if k['name']=='building' and kw.get('buildcat')==5 else k['country']
  self.w(COUNTRY+country_off,kw.get('country_mult',1.0),'f')
  for off in [0x5378,0x537C,0x5380,0x5384,0x5388]:self.w(HOUSE+off,9)
  factory_off=0x5388 if k['name']=='unit' and kw.get('naval') else k['factory']
  self.w(HOUSE+factory_off,kw.get('factories',1))
  self.w(0x8871E0,RULES);self.w(RULES+0x1748,kw.get('speed',1.0),'d')
  for off,val in [(0x570,kw.get('power_min',0.5)),(0x574,kw.get('power_max',0.8)),(0x578,kw.get('penalty',0.75)),(0x57C,kw.get('multiple',0.8))]:self.w(RULES+off,val,'f')
  self.w(RULES+0x758,kw.get('wall_mult',0.5),'d')
  actual=self.invoke(0x6F47A0)
  assert actual==expected,(k['name'],label,actual,expected)
  obj_reads=sorted((a-OBJ,n) for a,n in self.reads if OBJ<=a<OBJ+0x10000)
  assert obj_reads==[(0,4),(0x21C,4),(k['offset'],4)],obj_reads
  assert (COUNTRY+country_off,4) in self.reads
  assert (HOUSE+factory_off,4) in self.reads
  assert 0x7C5F00 in self.code and (0x6F4955 in self.code or 0x6F494D in self.code)
  return dict(name=k['name']+'_'+label,raw=actual,rate=max(1,min(255,actual//54)),frames=54*max(1,min(255,actual//54))+1,receiver_reads=obj_reads,country_offset=hex(country_off),factory_offset=hex(factory_off),native_instructions=len(self.code),writes_outside_scratch_stack=0,setup=kw)
 def evidence(self):
  result=[]
  for k in KINDS:
   assert self.r(k['vt']+0x84)==0x6F3270
   assert self.r(k['vt']+0x88)==k['getter']
   assert self.r(k['vt']+0x2C)==k['what']
   assert self.r(k['tvt']+0x2C)==k['twhat']
   assert self.r(k['tvt']+0x88)==0x711EE0
   assert self.invoke(self.r(k['vt']+0x30))==k['size']
   # Same type resolver called by Strip.Help, with synthetic vector item.
   self.w(k['array']+4,TYPE+0x3000);self.w(TYPE+0x3000,TYPE)
   assert self.invoke(0x48DCD0,k['cameo'],0)==TYPE
   result.append(self.case(k,'base',900))
   result.append(self.case(k,'country',1125,country_mult=1.25))
   result.append(self.case(k,'type',675,type_mult=.75))
   result.append(self.case(k,'low_power',1440,power=50))
   result.append(self.case(k,'low_power_maximum',1124,power=90))
   result.append(self.case(k,'zero_power',1800,power=0))
   result.append(self.case(k,'zero_policy_fallback',90000,power=0,penalty=1.0,power_min=0.0,power_max=0.0))
   result.append(self.case(k,'no_consumption',900,power=0,drain=0))
   result.append(self.case(k,'multiple',576,factories=3))
   result.append(self.case(k,'multiple_disabled',900,factories=3,multiple=0.0))
   result.append(self.case(k,'mixed_truncation',266,cost=999,speed=.7,country_mult=1.25,type_mult=.33333,power=50,factories=3))
  result.append(self.case(KINDS[0],'defense',1125,buildcat=5,country_mult=1.25))
  result.append(self.case(KINDS[0],'wall',450,wall=1))
  result.append(self.case(KINDS[1],'naval',460,naval=1,factories=4))
  ranges=[(0x6F47A0,0x1B6),(0x6F3270,8),(0x711EE0,0x1C)]
  ranges += [(k[name],n) for k in KINDS for name,n in [('getter',7),('what',6),('twhat',6)]]
  code=[dict(address=hex(a),size=n,fnv1a=hex(fnv(self.pe.get_data(a-self.base,n))),sha256=hashlib.sha256(self.pe.get_data(a-self.base,n)).hexdigest()) for a,n in ranges]
  return dict(version='1.0.46 revision',passed=True,executable_sha256=SHA,kinds=KINDS,cases=result,code=code,receiver_size=0x6C8,service_seams=[],boundary='Complete original x86 TimeToBuild and every called service run in Unicorn against synthetic House/Rules/Type state. Native virtual Size and sidebar type resolver also executed. No game process, Windows tooltip, actual production cadence, or multiplayer run. The +1 in display frames retains the existing ready-display convention and does not prove an extra simulation frame in every call order. Native Strip.Help rejects count >=75 before text; Assist accepts capacity75 only when original text is available. Private executable/code bytes are not copied into this receipt.')

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 report=Native(a.game).evidence();a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n')
 print('PASS',len(report['cases']),'complete original TimeToBuild cases; 4 native Size + sidebar type resolver cases; 0 replacement services; no game process')

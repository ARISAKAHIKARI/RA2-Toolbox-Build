"""Run supported native LaserDraw x86 on synthetic objects and audit cutout coverage.

Only terminal Surface calls, clock, RNG and atexit are intercepted; original
projection, color fading, thickness/direction offsets and quality decisions run.
This is call-boundary geometry, not an original pixel raster/live-game test.
Private PE bytes are inputs only and are never copied into the JSON result.
"""
import argparse,hashlib,json,struct,subprocess,tempfile
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *

class Probe:
 def __init__(self,path):
  raw=Path(path).read_bytes();self.sha=hashlib.sha256(raw).hexdigest()
  if self.sha not in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0','c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}:raise ValueError('Unsupported private sample SHA-256')
  pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.u.mem_map(0x400000,(len(image)+4095)&~4095);self.u.mem_write(0x400000,image)
  self.u.mem_map(0x20000000,0x30000);self.u.mem_map(0x21000000,0x20000);self.u.mem_map(0x22000000,0x1000)
  self.obj=0x20000000;self.tactical=0x20010000;self.surface=0x20011000;self.vtable=0x20012000;self.stop=0x22000f00
  self.w(0x887324,self.tactical);self.w(0x887314,self.surface);self.w(self.surface,self.vtable)
  self.w(0xa8eb78,1);self.w(0xabcd44,0);self.w(0xabc8b8,0);self.w(0xabc7c8,0)
  self.u.mem_write(0x886fa0,struct.pack('<4i',0,0,640,480));self.u.mem_write(0x8a0dd0,struct.pack('<6i',11,3,0,3,5,2));self.u.mem_write(0xb0cd48,struct.pack('<d',15/104.))
  self.w(self.tactical+0xb0,0);self.w(self.tactical+0xb4,0)
  for off in (0x34,0x3c,0x40):self.w(self.vtable+off,0x22000000+off)
  self.calls=[];self.projected=[];self.u.hook_add(UC_HOOK_CODE,self.hook)
 def w(self,a,v):self.u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def r(self,a):return struct.unpack('<I',self.u.mem_read(a,4))[0]
 def ret(self,n=0,value=1):
  sp=self.u.reg_read(UC_X86_REG_ESP);self.u.reg_write(UC_X86_REG_EAX,value);self.u.reg_write(UC_X86_REG_EIP,self.r(sp));self.u.reg_write(UC_X86_REG_ESP,sp+4+n)
 def hook(self,u,a,n,data):
  if a in (0x6d2231,0x6d2249,0x6d2258,0x6d226f):
   sp=u.reg_read(UC_X86_REG_ESP);self.projected.append(list(struct.unpack('<2i',u.mem_read(self.r(sp+8),8))));return
  if a==0x7c978a:self.ret();return
  if a==0x65c7e0:self.ret(8,0);return
  if a==0x55af60:self.ret(0,1);return
  if a in (0x22000034,0x2200003c,0x22000040):
   sp=u.reg_read(UC_X86_REG_ESP);args=[self.r(sp+4+i*4) for i in range(11 if a==0x2200003c else 7)]
   points=[list(struct.unpack('<2i',u.mem_read(args[i],8))) for i in (1,2)]
   self.calls.append({'slot':hex(a&255),'points':points});self.ret(44 if a==0x2200003c else 28);return
 def run(self,source,target,house,thickness,fl=True,supported=False,quality=False):
  self.u.mem_write(self.obj,bytes(0x60));self.w(self.obj+0x4c,15);self.w(self.obj+0x1c,thickness)
  self.u.mem_write(self.obj+0x24,struct.pack('<3i',*source));self.u.mem_write(self.obj+0x30,struct.pack('<3i',*target))
  self.w(0xabcd44,100 if quality else 0);self.u.mem_write(self.obj+0x20,bytes([house,supported]));self.u.mem_write(self.obj+0x41,b'\xff'*9);self.u.mem_write(self.obj+0x50,bytes([fl]));self.u.mem_write(self.obj+0x54,struct.pack('<2f',1,1))
  sp=0x2101e000;self.w(sp,self.stop);self.u.reg_write(UC_X86_REG_ESP,sp);self.u.reg_write(UC_X86_REG_ECX,self.obj);self.calls=[];self.projected=[]
  self.u.emu_start(0x550260,self.stop,count=1000000)
  assert self.u.reg_read(UC_X86_REG_EIP)==self.stop
  return self.calls

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('game');ap.add_argument('--output',required=True);args=ap.parse_args();p=Probe(args.game);rows=[];oracle=[]
 import itertools
 for house,supported,quality,fl,thick,delta in itertools.product((False,True),(False,True),(False,True),(False,True),(1,2,3,4,8,16,32,64),((256,0,0),(0,256,0),(256,256,0),(-256,256,0),(256,-256,0),(0,-256,0),(-256,-256,0),(-256,0,0))):
  source=(1536,1536,64);target=tuple(a+b for a,b in zip(source,delta));calls=p.run(source,target,house,thick,fl,supported,quality)
  assert len(p.projected)==2
  def project(q):return [int((q[0]-q[1])*30/256),int((q[0]+q[1])*15/256)-int(q[2]*15/104+int(q[2]>=728)+.5)]
  assert p.projected==[project(source),project(target)]
  radius=min(6,thick)+1 if house else 2
  base=p.projected
  for c in calls:
   for n in (0,1):assert max(abs(c['points'][n][i]-base[n][i]) for i in (0,1))<radius,(house,supported,quality,fl,thick,delta,radius,base,c)
  oracle.append(' '.join(map(str,[*base[0],*base[1],radius,len(calls),*[v for c in calls for point in c['points'] for v in point]])))
  rows.append({'house':house,'supported':supported,'quality':quality,'translucent':fl,'thickness':thick,'source':source,'target':target,'native_projected':base,'calls':calls})
 repo=Path(__file__).resolve().parents[3]
 with tempfile.TemporaryDirectory(prefix='beam-coverage-') as temp:
  exe=Path(temp)/'test'
  subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG',str(repo/'source/assist/tests/native_beam_effects.cpp'),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],input=str(len(oracle))+'\n'+'\n'.join(oracle)+'\n',text=True,capture_output=True,check=True)
 out={'sha256':p.sha,'scope':__doc__,'cases':rows,'original_geometry_cases':len(rows),'production_coverage_status':'pass','production_fixture_output':result.stdout.strip(),'real_game_tested':False,'native_pixel_raster_tested':False}
 Path(args.output).write_text(json.dumps(out,indent=2));print('PASS',len(rows),'native Laser geometry cases and production coverage',p.sha)

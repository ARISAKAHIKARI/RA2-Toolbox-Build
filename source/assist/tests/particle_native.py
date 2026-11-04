"""Compare supported particle SHP frame decisions with original GetCurrentFrame.
Executes only the leaf selector for BehavesLike 0/1, not native particle pixels.
"""
import argparse,hashlib,json,struct,subprocess,tempfile
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_ESP
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('games',nargs='+');ap.add_argument('--output',required=True);args=ap.parse_args()
 root=Path(__file__).resolve().parents[2];cases=[(mode,frame) for mode in (0,1) for frame in range(-128,128)]
 with tempfile.TemporaryDirectory() as folder:
  cpp=Path(folder)/'helper.cpp';exe=Path(folder)/'helper';cpp.write_text('#include "particle_visuals.h"\n#include <iostream>\nint main(){int m,f;while(std::cin>>m>>f)std::cout<<external::particles::frame(m,int8_t(f))<<"\\n";}\n')
  subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'external_reader/source'),str(cpp),'-o',str(exe)],check=True)
  wanted=list(map(int,subprocess.run([str(exe)],input=''.join(f'{m} {f}\n' for m,f in cases),text=True,capture_output=True,check=True).stdout.split()))
 rows=[]
 for path in args.games:
  raw=Path(path).read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0','c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
  pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(image)+4095)&~4095);u.mem_write(base,image);obj=0x2000000;typ=obj+0x1000;stack=obj+0x2000;stop=obj+0x3000;u.mem_map(obj,0x4000)
  def put(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
  put(obj+0xAC,typ)
  for (mode,frame),expected in zip(cases,wanted):
   put(typ+0x314,mode);u.mem_write(obj+0x12E,bytes([frame&255]));put(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,obj);u.emu_start(0x62D830,stop,count=100);got=u.reg_read(UC_X86_REG_EAX);got=got if got<0x80000000 else got-0x100000000;assert got==expected,(mode,frame,expected,got)
  rows.append({'exe_sha256':sha,'cases':len(cases),'pass':True,'selector_sha256':hashlib.sha256(image[0x62D830-base:0x62D926-base]).hexdigest()})
 Path(args.output).write_text(json.dumps({'scope':__doc__,'samples':rows,'native_pixels_tested':False,'fire_direction_supported':False,'pixel_particles_supported':False},indent=2)+'\n');print('PASS',sum(r['cases'] for r in rows),'original particle SHP frame decisions')
if __name__=='__main__':main()

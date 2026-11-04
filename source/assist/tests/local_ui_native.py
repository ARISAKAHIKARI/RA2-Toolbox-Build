"""Execute original UI anchor/gate leaf blocks on synthetic observations.
Proves font/label/timer anchor offsets and primary/archive metadata, not glyph
pixels, complete rally art bounds, Phobos UI hooks or Windows composition.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('games',nargs='+');ap.add_argument('--output',required=True);args=ap.parse_args();rows=[]
 for path in args.games:
  raw=Path(path).read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0','c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
  pe=pefile.PE(data=raw);im=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)&~4095);u.mem_write(base,im);obj=0x2000000;stack=obj+0x1000;font=obj+0x2000;surface=obj+0x3000;vt=obj+0x4000;code=obj+0x5000;stop=obj+0x6000;u.mem_map(obj,0x10000)
  def put(at,value):u.mem_write(at,struct.pack('<I',value&0xffffffff))
  def get(at):return struct.unpack('<i',u.mem_read(at,4))[0]
  put(0x89C4D0,font);put(0x887314,surface);put(0x88731C,surface);put(surface,vt);put(vt+0x7C,code);u.mem_write(code,b'\xb8'+struct.pack('<I',800)+b'\xc3')
  cases=0
  for height in (8,12,16,24,32):
   put(font+0x1C,height)
   for index in (0,1,3,20):
    for measured in (0,30,200,700):
     put(0x886FA8,800);put(0x886FAC,600);put(stack+0x1C,index);put(stack+0x10,measured);put(stack+0x18,0);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,font);u.emu_start(0x6D4CE2,0x6D4D23,count=100)
     assert get(stack+0x24)==800-measured-3 and get(stack+0x28)==600-(height+2)*(index+1);cases+=1
   for x,y,width in ((10,20,100),(300,10,700),(-3,30,40)):
    put(obj+0xC,x);put(obj+0x10,y);put(obj+0x38,width);u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x72A4F1,0x72A55F,count=100)
    assert get(stack+0x18)==x and get(stack+0x1C)==y and get(stack+0x24)==height;cases+=1
  def gate(uc,at,size,data):
   if at in (0x70ABF8,0x70AD42):uc.emu_stop()
  hook=u.hook_add(UC_HOOK_CODE,gate)
  for primary in (0,1):
   u.mem_write(obj+0x3D3,bytes([primary]));u.reg_write(UC_X86_REG_EDI,obj);u.emu_start(0x70ABEA,0x70AD43,count=10);assert u.reg_read(UC_X86_REG_EIP)==(0x70ABF8 if primary else 0x70AD42);cases+=1
  u.hook_del(hook);put(stack,stop);put(stack+4,0x12345678);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,obj);u.emu_start(0x70C610,stop,count=10);assert get(obj+0x218)==0x12345678;cases+=1
  rows.append({'exe_sha256':sha,'cases':cases,'pass':True,'scope':'original label/timer anchors; primary/archive fields; no native pixel claim'})
 Path(args.output).write_text(json.dumps({'scope':__doc__,'samples':rows,'native_pixels_tested':False,'real_game_tested':False},indent=2)+'\n');print('PASS',sum(x['cases'] for x in rows),'original UI anchor/metadata decisions')
if __name__=='__main__':main()

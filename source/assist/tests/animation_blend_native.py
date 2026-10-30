"""Run original Anim::Draw_It blend selection on synthetic observations.

Only 423061..4231F3 is executed; stop before the image/palette virtual call.
This verifies decisions, not native blitter pixels or a complete game scene.
Private EXE bytes are inputs and are never included in the output.
"""
import argparse, hashlib, json, random, struct, subprocess
from pathlib import Path
import capstone, pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_EIP

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('games',nargs='+');ap.add_argument('--helper',required=True);ap.add_argument('--output',required=True);args=ap.parse_args()
 cases=[]
 for end in range(1,101):
  for value in sorted(set([0,end,*[int(end*f)+d for f in (.2,.4,.6) for d in (0,1)]])):
   cases.append((0x600,0,0,2,0,1,value,end,0,0))
 rng=random.Random(20261030)
 for n in range(5000):cases.append((rng.choice([0x600,0x602,0x604,0x606]),rng.randrange(2),rng.randrange(2),rng.randrange(4),rng.randrange(4),rng.randrange(2),rng.randrange(100),rng.choice([-1,0,1,5,10,20,75,100]),rng.choice([0,25,50,75,30]),rng.choice([0,1,5,6,10,11,14,15,16,100])))
 data='\n'.join(' '.join(map(str,c)) for c in cases)+'\n';result=subprocess.run([args.helper],input=data,text=True,capture_output=True,check=True);expected=list(map(int,result.stdout.split()));assert len(expected)==len(cases)
 rows=[]
 for game in args.games:
  raw=Path(game).read_bytes();sha=hashlib.sha256(raw).hexdigest();assert sha in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0','c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
  pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(image)+4095)&~4095);u.mem_write(base,image)
  obj=0x2000000;typ=obj+0x1000;stack=obj+0x2000;u.mem_map(obj,0x4000)
  def w(p,v):u.mem_write(p,struct.pack('<I',v&0xffffffff))
  def stop(uc,at,size,user):
   if at in (0x4230fe,0x4238a3):uc.emu_stop()
  u.hook_add(UC_HOOK_CODE,stop)
  for case,want in zip(cases,expected):
   flags,temporal,thick,detail,threshold,fading,value,end,translucency,level=case
   u.mem_write(obj,bytes(0x200));u.mem_write(typ,bytes(0x400));w(obj+0xc8,typ);w(stack+0x14,value);w(0xa8eb78,detail);w(typ+0x2d8,threshold);w(typ+0x2c0,end);w(typ+0x2ec,translucency)
   for p,v in [(obj+0x119,temporal),(obj+0x178,level),(typ+0x368,thick),(typ+0x36a,fading)]:u.mem_write(p,bytes([v]))
   u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EAX,typ);u.reg_write(UC_X86_REG_EBX,flags);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x423061,0x4238a4,count=200)
   got=-1 if u.reg_read(UC_X86_REG_EIP)==0x4238a3 else (u.reg_read(UC_X86_REG_EBX)>>1)&3
   assert got==want,(sha,case,want,got)
  rows.append({'exe_sha256':sha,'cases':len(cases),'code_sha256':hashlib.sha256(image[0x423061-base:0x4231f3-base]).hexdigest(),'pass':True})
 Path(args.output).write_text(json.dumps({'scope':__doc__,'samples':rows,'native_pixels_tested':False,'real_game_tested':False},indent=2)+'\n');print('PASS',sum(r['cases'] for r in rows),'original native blend decisions match production helper')
if __name__=='__main__':main()

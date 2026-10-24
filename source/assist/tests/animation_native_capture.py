"""Execute original animation coordinate/frame/sort functions on private captures.
No game update, loader, renderer, Win32 display or multiplayer is executed.
"""
from pathlib import Path
import argparse,hashlib,json,struct,zipfile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EAX
p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
with zipfile.ZipFile(a.capture) as z:
 session_name=next(n for n in z.namelist() if n.endswith('/session.json'));base=session_name[:-len('session.json')];session=json.loads(z.read(session_name));results=[];hashes={}
 for stage in ['01-off','02-on','03-off']:
  u=Uc(UC_ARCH_X86,UC_MODE_32)
  def load(addr,data):
   start=addr&~4095;end=(addr+len(data)+4095)&~4095
   if end>start:u.mem_map(start,end-start)
   u.mem_write(addr,data)
  code=z.read(base+stage+'-game.mem');load(int(session['image_base'],16),code)
  for r in json.loads(z.read(base+stage+'-private-regions.json')):load(int(r['base'],16),z.read(base+r['file']))
  stack=0x70000000;out=0x71000000;stop=0x72000000
  for addr in [stack,out,stop]:load(addr,bytes(0x1000))
  def value(addr):return struct.unpack('<I',u.mem_read(addr,4))[0]
  def signed(addr):return struct.unpack('<i',u.mem_read(addr,4))[0]
  def xyz(addr):return struct.unpack('<iii',u.mem_read(addr,12))
  def run(addr,obj,args=()):
   sp=stack+0x800;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,obj);u.emu_start(addr,stop,count=10000)
   assert u.reg_read(UC_X86_REG_ESP)==sp+4+len(args)*4
   return u.reg_read(UC_X86_REG_EAX)
  n=0;paused=0;starts=0
  for i in range(value(0xA8EB40+16)):
   b=value(value(0xA8EB40+4)+i*4)
   if value(b)!=0x7e3ebc or signed(b+0x6c)<=0:continue
   run(0x459ef0,b,(out,));v=xyz(b+0x9c);assert xyz(out)==(v[0]-128,v[1]-128,v[2])
   for off in list(range(0x55c,0x5b0,4))+list(range(0x5c8,0x5e8,4)):
    obj=value(b+off)
    if not obj:continue
    assert value(obj)==0x7e3354 and value(obj+0xcc)==0
    run(0x41be00,obj,(out,));assert xyz(out)==xyz(obj+0x9c)
    expected=sum(xyz(out)[:2])+signed(obj+0x104);assert run(0x422bc0,obj)==expected
    t=value(obj+0xc8);assert run(0x424cb0,obj)==value(t+0x364)
    # Draw_It's actual instruction slice, through the displayed-frame store.
    sp=stack+0x800;u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,obj);u.emu_start(0x42303f,0x423061,count=100)
    assert value(sp+0x2c)==value(obj+0xac)+value(t+0x2b4)
    n+=1;paused+=bool(u.mem_read(obj+0x19e,1)[0]);starts+=value(t+0x2b4)>0
  results.append({'stage':stage,'native_animations':n,'power_off':paused,'nonzero_start':starts})
  for addr,length in [(0x451890,0x2a4),(0x459ef0,0x2b),(0x447ac0,0x54),(0x422be0,0x85),(0x422bc0,0x12),(0x424cb0,0x27),(0x42303f,0x22)]:
   hashes[hex(addr)]=hashlib.sha256(bytes(u.mem_read(addr,length))).hexdigest()
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'scope':__doc__,'game_exe_sha256':session['file_sha256'],'code_sha256':hashes,'results':results},indent=2)+'\n');print(json.dumps(results))

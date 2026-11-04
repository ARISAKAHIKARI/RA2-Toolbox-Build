"""Execute the original SHP Unit turret frame submission with synthetic input.

Runs 73CCAE..73CD08, including original FacingClass::Current. Only the final
DrawObject virtual boundary is observed. No original assets/bytes are exported;
this does not execute real Windows drawing or a full game.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,itertools,json,struct,subprocess
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('games',type=Path,nargs='+');parser.add_argument('--portable',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('posture',root/'assist/tests/voxel_posture_native.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Native(module.Native):
 DRAW=module.Native.OBJECT+0x6000
 SHAPE=module.Native.OBJECT+0x7000
 def __init__(self,path):
  super().__init__(path);self.submission=None
  self.put(self.OBJECT+0x6C4,'I',self.TYPE);self.put(self.VTABLE+0x50C,'I',self.DRAW)
  self.u.hook_add(UC_HOOK_CODE,self.draw_boundary)
 def draw_boundary(self,u,address,size,data):
  if address!=self.DRAW:return
  sp=u.reg_read(UC_X86_REG_ESP);self.submission=struct.unpack('<16I',u.mem_read(sp+4,64));u.emu_stop()
 def turret(self,raw,walk):
  self.put(self.OBJECT+0x3A0,'H',raw);self.put(self.OBJECT+0x3A4,'H',raw);self.put(self.OBJECT+0x3A8,'i',-1);self.put(self.OBJECT+0x3B0,'i',0);self.put(self.OBJECT+0x3B4,'h',0)
  self.put(self.TYPE+0xE5C,'b',walk);self.put(self.STACK+0x144,'I',0x123456);self.put(self.STACK+0x40,'2i',128,128);self.put(self.STACK+0x50,'4i',0,0,256,256)
  self.u.reg_write(UC_X86_REG_ESP,self.STACK);self.u.reg_write(UC_X86_REG_EBP,self.OBJECT);self.u.reg_write(UC_X86_REG_EDI,self.SHAPE);self.u.reg_write(UC_X86_REG_ESI,0x123456);self.u.reg_write(UC_X86_REG_FPCW,0xE7F)
  self.submission=None;self.u.emu_start(0x73CCAE,0x73CD0E,count=10000);assert self.submission is not None
  a=self.submission;assert a[0]==self.SHAPE and a[15]==0x2820 and a[5]==256 and a[9]==0x123456,a
  assert struct.unpack('<2i',self.u.mem_read(a[2],8))==(128,128)
  return a[1]
raws=sorted(set(range(0,65536,1024))|{1023,1025,2047,2049,32767,32769,65535});cases=list(itertools.product(raws,(0,1,2,7,31,127)))
portable=subprocess.run([str(args.portable),'--frames'],input='\n'.join(f'{raw} {walk}' for raw,walk in cases)+'\n',text=True,capture_output=True,check=True)
frames=list(map(int,portable.stdout.split()));assert len(frames)==len(cases)
results=[]
for game in args.games:
 native=Native(game)
 for (raw,walk),frame in zip(cases,frames):assert native.turret(raw,walk)==frame,(raw,walk,frame,native.submission)
 a,b=0x73CCAE,0x73CD0E
 results.append({'executable_sha256':native.sha,'cases':len(cases),'frame_mismatches':0,'same_shape':True,'same_origin':[128,128],'flags':0x2820,'code_range_sha256':{'Unit.SHP_turret_submission':hashlib.sha256(native.image[a-native.base:b-native.base]).hexdigest()}})
report={'result':'PASS','results':results,'scope':'Actual 73CCAE..73CD08 and FacingClass::Current; synthetic unit/type/SHP address; final DrawObject is an observed boundary, not a raster or Windows execution','formula':'8 * signed WalkFrames + ((round32(FacingClass::Current(TurretFacing))+4)&31)','not_certified':['full Unit.DrawAsSHP composition','native offscreen cache compositing','VXL barrels attached to SHP hull','Windows/game or multiplayer visuals']}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

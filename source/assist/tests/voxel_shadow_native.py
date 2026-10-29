"""Read-only native Drive/Ship Shadow_Matrix and Shadow_Point oracle.
Synthetic objects/ramp tables; Map::GetCell and Map::GetFloorHeight boundaries
return controlled Cell/height. No original code bytes or assets in report.
"""
from pathlib import Path
import importlib.util,struct,math,itertools,hashlib,json,subprocess,argparse
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("games",type=Path,nargs="+");parser.add_argument("--portable",type=Path,required=True);parser.add_argument("--output",type=Path,required=True);args=parser.parse_args()
oracle=[];oracle_expected=[]
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('posture',root/'assist/tests/voxel_posture_native.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
def stored(v):
 out=f32(v)
 if (v>0 and out>v) or (v<0 and out<v):
  bits=struct.unpack('<I',struct.pack('<f',out))[0];out=struct.unpack('<f',struct.pack('<I',bits-1))[0]
 return out
def sincos(a):
 n=int(stored(a)*2607.594482421875);index=int(n/2)%8192
 if n&1 and index<8191:index+=1
 sine=stored(math.sin(index*math.tau/8192));index=int(n/2)%8192+2048
 if n&1 and index<10239:index+=1
 return sine,stored(math.sin(index*math.tau/8192))
class Shadow(m.Native):
 CELL=m.Native.OBJECT+0x5000
 def __init__(self,p):
  self.floor=0;self.cell_calls=0;self.floor_calls=0;self.native_floor=False
  super().__init__(p)
  self.put(self.VTABLE+0x1c8,'I',0x5f5f40)
  self.put(self.OBJECT+0x9c,'3i',20*256+128,30*256+128,0)
  self.put(0xB0CD48,'d',.14433756729740643)
  self.put(0xAC13BC,'i',416)
 def boundary(self,u,address,size,data):
  if address not in (0x565730,0x578080) or (address==0x578080 and self.native_floor):return super().boundary(u,address,size,data)
  sp=u.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',u.mem_read(sp,4))[0]
  if address==0x565730:value=self.CELL;self.cell_calls+=1
  else:value=self.floor;self.floor_calls+=1
  u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,ret)
 def matrix(self,ramp,bucket,ship,rock,transition):
  table,address=(0x7f2d8c,0x69fb20) if ship else (0x7e7eb0,0x4b0410)
  assert struct.unpack_from('<I',self.image,table+0x28-self.base)[0]==address
  self.put(self.LOCO,'I',table);self.put(self.LOCO+0x18,'6i',(ramp+7)%21,(ramp+12)%21,100,0,8 if transition else 0,8 if transition else 0)
  self.put(self.OBJECT+0x328,'2f',.12 if rock else 0,-.18 if rock else 0)
  self.put(self.OBJECT+0x388,'H',bucket*2048);self.put(self.OBJECT+0x38c,'H',bucket*2048)
  self.put(self.CELL+0x11c,'B',ramp);self.put(0xa8ed84,'i',104)
  self.put(self.STACK,'4I',self.STOP,self.LOCO,self.OUT,0)
  self.u.reg_write(UC_X86_REG_ESP,self.STACK);self.u.reg_write(UC_X86_REG_FPCW,0xe7f)
  self.u.emu_start(address,self.STOP,count=10000)
  assert self.u.reg_read(UC_X86_REG_EIP)==self.STOP and self.u.reg_read(UC_X86_REG_ESP)==self.STACK+16
  actual=struct.unpack('<12f',self.u.mem_read(self.OUT,48))
  a=-(bucket-8)*math.tau/32;s,c=sincos(a);yaw=[c,-s,0,0,s,c,0,0,0,0,1,0];r=m.tables()[ramp][0]
  expected=[sum(r[row*4+k]*yaw[k*4+col] for k in range(3))+(r[row*4+3] if col==3 else 0) for row in range(3) for col in range(4)]
  oracle.append('M '+str(-a)+' '+' '.join(map(str,r)));oracle_expected.append(actual)
  return max(abs(x-y) for x,y in zip(actual,expected)),actual
 def point(self,z,floor,bridge):
  self.floor=floor;self.put(self.OBJECT+0xa4,'i',z);self.put(self.OBJECT+0x8c,'B',bridge)
  self.put(self.STACK,'3I',self.STOP,self.LOCO,self.OUT)
  self.u.reg_write(UC_X86_REG_ESP,self.STACK);self.u.reg_write(UC_X86_REG_FPCW,0xe7f)
  self.u.emu_start(0x55a8c0,self.STOP,count=10000)
  assert self.u.reg_read(UC_X86_REG_EIP)==self.STOP and self.u.reg_read(UC_X86_REG_ESP)==self.STACK+12
  return struct.unpack('<2i',self.u.mem_read(self.OUT,8))
def exact_floor_cases(n):
 n.native_floor=True;n.put(0x87f924,'I',n.OBJECT+0x10000);n.put(0x87f928,'I',512*512);n.put(n.OBJECT+0x10000+(30*512+20)*4,'I',n.CELL)
 n.put(0x89e770,'B',0);n.put(0x89e7c0,'i',104)
 n.put(n.CELL+0x11b,'b',0);n.put(n.CELL+0x11c,'B',0)
 # First actual floor call initializes the native 20-ramp coefficient table.
 n.point(0,0,False)
 maximum=0;count=0;levels=(-2,0,3);positions=(0,64,128,192,255)
 for ramp,level,dx,dy in itertools.product(range(21),levels,positions,positions):
  n.put(n.CELL+0x11b,'b',level);n.put(n.CELL+0x11c,'B',ramp)
  n.put(n.OBJECT+0x9c,'3i',20*256+dx,30*256+dy,2000)
  n.put(n.STACK,'2I',n.STOP,n.OBJECT+0x9c);n.u.reg_write(UC_X86_REG_ESP,n.STACK);n.u.reg_write(UC_X86_REG_FPCW,0xe7f)
  n.u.emu_start(0x578080,n.STOP,count=10000)
  actual=n.u.reg_read(UC_X86_REG_EAX);actual=actual if actual<2**31 else actual-2**32
  expected=int(level*104+.5)
  if ramp:
   a,b,c,limit,d=struct.unpack('<5d',n.u.mem_read(0x81c900+(ramp-1)*40,40));scale=struct.unpack('<d',n.u.mem_read(0x89e740,8))[0]
   extra=max(0,min(limit,dy*b*scale+dx*a*scale+c+d));expected=int(expected+extra)
  coeffs=struct.unpack('<5d',n.u.mem_read(0x81c900+(ramp-1)*40,40)) if ramp else (0,0,0,0,0)
  scale=struct.unpack('<d',n.u.mem_read(0x89e740,8))[0]
  oracle.append('F '+' '.join(map(str,(level,104,dx,dy,ramp,scale,*coeffs))));oracle_expected.append(actual)
  maximum=max(maximum,abs(actual-expected));count+=1
 assert maximum==0,maximum
 return dict(cases=count,maximum_height_error=maximum,scope='Actual Map GetFloorHeight -> Cell GetFloorHeight with native ramp table initialization, 21 ramps, signed levels and within-cell XY; synthetic map/objects; no stubbed floor height')
rows=[]
for p in args.games:
 n=Shadow(p);worst=0;count=0;distinct=set()
 for ramp,bucket,ship,rock,transition in itertools.product(range(21),(0,3,11,27),(False,True),(False,True),(False,True)):
  error,actual=n.matrix(ramp,bucket,ship,rock,transition);worst=max(worst,error);count+=1;distinct.add(tuple(round(x,5) for x in actual))
 assert worst<1e-5,(n.sha,worst)
 points=[]
 for z,floor,bridge in [(0,0,False),(52,52,False),(104,104,False),(156,156,False),(208,208,False),(104,0,False),(728,0,False),(520,104,True),(624,208,True),(832,208,True)]:
  actual=n.point(z,floor,bridge);height=z-floor-(416 if bridge else 0);expected=(0,int(height*.14433756729740643+int(height>=728)+.5));assert actual==expected,(z,floor,bridge,actual,expected)
  old_drop=max(0,int(z*.14433756729740643+int(z>=728)+.5)-(int(floor//208)+(4 if bridge else 0))*15)
  points.append(dict(world_z=z,synthetic_floor_z=floor,on_bridge=bridge,native_shadow_point=list(actual),old_level_based_drop=old_drop))
 floor_result=exact_floor_cases(n)
 ranges={'Drive.Shadow_Matrix':(0x4b0410,0x4b04c2),'Ship.Shadow_Matrix':(0x69fb20,0x69fbd2),'base.Shadow_Matrix':(0x55a7d0,0x55a8c0),'base.Shadow_Point':(0x55a8c0,0x55a8e8),'Object.GetHeight':(0x5f5f40,0x5f5f92),'screenHeight':(0x6d20e0,0x6d211c),'Map.GetFloorHeight':(0x578080,0x5780f9),'Cell.GetFloorHeight':(0x47b3a0,0x47bb5b)}
 rows.append(dict(exact_floor_height=floor_result,executable_sha256=n.sha,matrix_cases=count,maximum_matrix_coefficient_error=worst,distinct_matrices=len(distinct),synthetic_map_cell_boundary_calls=n.cell_calls,synthetic_floor_height_boundary_calls=n.floor_calls,shadow_point_cases=points,code_range_sha256={key:hashlib.sha256(n.image[a-n.base:b-n.base]).hexdigest() for key,(a,b) in ranges.items()}))
portable=subprocess.run([str(args.portable),'--oracle'],input='\n'.join(oracle)+'\n',text=True,capture_output=True,check=True)
lines=portable.stdout.splitlines();assert len(lines)==len(oracle_expected)
portable_error=0;floor_cases=matrix_cases=0
for line,expected in zip(lines,oracle_expected):
 if isinstance(expected,int):assert int(line)==expected;floor_cases+=1
 else:portable_error=max(portable_error,max(abs(a-b) for a,b in zip(map(float,line.split()),expected)));matrix_cases+=1
assert portable_error<1e-5,portable_error
report=dict(result='PASS',production_helper={'matrix_cases':matrix_cases,'floor_cases':floor_cases,'maximum_matrix_coefficient_error':portable_error,'maximum_floor_height_error':0},scope='Actual native Drive/Ship Shadow_Matrix, base Shadow_Point, Object GetHeight and screen-height functions. Synthetic ramps and objects. Only Map GetCell/GetFloorHeight boundaries substituted; no real game or visual validation.',results=rows,conclusion='Shadow matrix uses current Cell RampType, not interpolated locomotor Draw_Matrix; Shadow_Point uses exact world Z minus slope floor height and bridge height, not integer Cell Level.')
p=args.output;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','samples':len(rows),'matrix_cases_per_sample':count,'max_error':max(x['maximum_matrix_coefficient_error'] for x in rows),'point_cases_per_sample':len(points),'evidence':str(p)}))

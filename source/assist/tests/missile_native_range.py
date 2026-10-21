"""Execute the supplied EXE's original IsCloseEnough/6F7220 range arithmetic, including sqrt and integer conversion. Object getters and map/obstacle lookup use controlled fixtures. Not a live map test."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
spec=importlib.util.spec_from_file_location('n',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec);old=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=old
m=n.fixture(3);typ=m.alloc();q=m.obj(0x7e22a4,typ,m.enemy,40);p=m.unit
projectile=m.alloc();m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xb4,1500);info=m.alloc();m.w(info,m.weapon)
addresses={}
def add(at,fn):
 if at in addresses:return
 addresses[at]=fn;m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:addresses[a](),begin=at,end=at)
def self():return m.u.reg_read(UC_X86_REG_ECX)
def arg(i=0):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+i*4)
def coords():
 out=arg();m.u.mem_write(out,bytes(m.u.mem_read(self()+0x9c,12)));m.ret(out,1)
for table in (0x7f5c70,0x7e22a4):
 add(m.r(table+0x48),coords)
 add(m.r(table+0x54),lambda:m.ret(int(self()==q),0))
 add(m.r(table+0x50),lambda:m.ret(0,0))
 add(m.r(table+0x84),lambda:m.ret(m.r(self()+0x6c4),0))
 add(m.r(table+0x2c),lambda:m.ret(2 if self()==q else 40,0))
 add(m.r(table+0x400),lambda:m.ret(0,0))
 add(m.r(table+0x3f8),lambda:m.ret(info,1))
add(0x565730,lambda:m.ret(m.crateCell,1))
add(0x4cc310,lambda:m.ret(0,2))
# Native sqrt/conversion code and constants are loaded from the supplied EXE.
rows=[]
for name,dx,z,bonus,minimum,expect in [('near_low',1000,0,0,0,1),('near_high',1000,1400,0,0,0),('bonus_high',1000,1400,500,0,1),('far_low',1600,0,0,0,0),('bonus_far',1600,0,500,0,1),('min_range',100,0,0,500,0)]:
 m.u.mem_write(p+0x9c,struct.pack('<iii',10000,10000,0));m.u.mem_write(q+0x9c,struct.pack('<iii',10000+dx,10000,z));m.w(m.t+0x68c,bonus);m.w(m.weapon+0xb8,minimum)
 m.run(0x6f77b0,(q,0),p);actual=m.u.reg_read(UC_X86_REG_EAX)&255
 assert actual==expect,(name,actual,expect)
 rows.append({'case':name,'status':'PASS','result':actual});print('PASS',name,flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')

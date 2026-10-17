"""Real DLL ingress/constructors, production host planner and synthetic frame state.
Native movement/radio clearing/unloading are explicitly modeled, not executed.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',default='ore-priority-runtime.json');a=p.parse_args()
root=Path(__file__).resolve().parents[2];results=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def check(name,fn):
 fn();results.append({'name':name,'status':'PASS'});print('PASS',name,flush=True)
def fixture(mode):
 spec=importlib.util.spec_from_file_location('ore_priority_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.b(m.t+0xe0e,1);m.w(m.t+0x800,40);m.vector(m.t+0x3e8,[m.buildT]);radio(m,[m.unit])
 m.miners=[m.unit,m.obj(0x7f5c70,m.t,m.house,14),m.obj(0x7f5c70,m.t,m.house,15)]
 for i,unit in enumerate(m.miners):
  m.w(unit+0xac,10);m.w(unit+0x9c,(10-i)*256+128);m.w(unit+0xa0,10*256+128);m.u.mem_write(unit+0x33c,struct.pack('<4f',40,0,0,0))
 m.w(m.building+0x9c,30*256+128);m.w(m.building+0xa0,30*256+128)
 m.vector(0xa8ec78,m.miners+[m.building]);m.vector(0xa83d10,m.miners)
 m.testcells={}
 for y in range(27,38):
  for x in range(26,38):
   cell=m.alloc();m.w(m.cells+(y*512+x)*4,cell);m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.testcells[y*1000+x]=cell
 m.w(m.unit+0x5a4,m.building);radio(m,m.miners[:1])
 s=state(m);s.flags=32768;s.externalDisplay=1;save(m,s);s=m.hook();return m,s
# Raw native radio slots: pointer E4, slot count E8 (not a DynamicVector).
def radio(m,values):
 links=m.alloc(max(4,4*len(values)))
 for i,value in enumerate(values):m.w(links+i*4,value)
 m.w(m.building+0xe4,links);m.w(m.building+0xe8,len(values))
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def clear(m):
 for addr in (0x8b41f8,0x8b41fc,0x8b4200,0xa802c8,0xa802cc,0xa802d0):m.w(addr,0)
def submit(m,orders,stale=False):
 s=state(m);s.planRequest+=1;s.planCount=len(orders);s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame-(7 if stale else 0)
 for i,o in enumerate(orders):s.plan[i]=o
 save(m,s);return m.hook()
for mode in (0,3):
 for label in ('yield_allowed','ordinary_move_linked','near_owner','unloading','foreign_radio','disabled_mode','stale_token','empty_cargo','occupied_cell','water_cell','wrong_mission','unexpected_target','stale_frame'):
  def ingress(mode=mode,label=label):
   m,s=fixture(mode);clear(m);o=m.Order(10,0,0,33030,2,11,4,12)
   if label=='ordinary_move_linked':o.purpose=0
   elif label=='near_owner':m.w(m.unit+0x9c,31*256+128);m.w(m.unit+0xa0,33*256+128)
   elif label=='unloading':m.b(m.unit+0x6d1,1)
   elif label=='foreign_radio':radio(m,[m.miners[1]])
   elif label=='disabled_mode':s.flags=0;save(m,s)
   elif label=='stale_token':o.token=1
   elif label=='empty_cargo':m.u.mem_write(m.unit+0x33c,bytes(16))
   elif label=='occupied_cell':m.w(m.testcells[33030]+0x124,1)
   elif label=='water_cell':m.w(m.testcells[33030]+0xec,2)
   elif label=='wrong_mission':o.mission=7;o.destination=12;o.destinationTag=0x34
   elif label=='unexpected_target':o.target=14
   before=bytes(m.u.mem_read(m.unit,0x2000))+bytes(m.u.mem_read(m.building,0x2000))
   s=submit(m,[o],label=='stale_frame');assert s.planAck==s.planRequest
   expected=int(label=='yield_allowed');assert s.planAccepted==expected,(label,s.planAccepted,C.c_int32(s.planResult).value)
   assert m.r(0x8b41f8 if mode==0 else 0xa802c8)==expected
   assert m.r(0xa802c8 if mode==0 else 0x8b41f8)==0
   # Neither taking a reservation nor admitting Move directly mutates objects.
   assert before==bytes(m.u.mem_read(m.unit,0x2000))+bytes(m.u.mem_read(m.building,0x2000))
  check(str(mode)+'_'+label,ingress)

with tempfile.TemporaryDirectory(prefix='ore-host-') as temp:
 cpp=Path(temp)/'host.cpp';exe=Path(temp)/'host'
 cpp.write_text('#include "'+str(root/'assist/source/external_planner.h')+'"\n#include <cstdio>\n#include <memory>\nint main(){using namespace assist;using namespace planner;Planner p;auto w=std::make_unique<World>();uint32_t kind;while(fread(&kind,4,1,stdin)==1){if(kind==0){if(fread(w.get(),sizeof(World),1,stdin)!=1)return 2;auto out=p.step(*w,AutoOreReturn,200,300,90);uint32_t n=out.size();fwrite(&n,4,1,stdout);fwrite(out.data(),sizeof(Order),n,stdout);fflush(stdout);}else{uint32_t n;int frame;if(fread(&frame,4,1,stdin)!=1||fread(&n,4,1,stdin)!=1)return 3;for(uint32_t i=0;i<n;++i){Order o;if(fread(&o,sizeof o,1,stdin)!=1)return 4;p.accepted(o,frame);}}}}')
 subprocess.run(['g++','-O2','-std=c++17',str(cpp),'-o',str(exe)],check=True)
 for mode in (0,3):
  def pipeline(mode=mode):
   m,s=fixture(mode);radio(m,[]);s=tick(m)
   with subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE) as host:
    def plan(s):
     host.stdin.write(struct.pack('<I',0)+bytes(s.world));host.stdin.flush();n=struct.unpack('<I',host.stdout.read(4))[0];return [m.Order.from_buffer_copy(host.stdout.read(C.sizeof(m.Order))) for i in range(n)]
    def deliver(orders):
     s=submit(m,orders);assert s.planAccepted==len(orders),(s.planAccepted,len(orders),s.planRejectedMask)
     host.stdin.write(struct.pack('<IiI',1,s.data.frame,len(orders))+b''.join(bytes(o) for o in orders));host.stdin.flush();return s
    def enters(orders):return [o.unit for o in orders if o.mission==7]
    orders=plan(s);assert enters(orders)==[10];deliver(orders)
    radio(m,[m.unit]);m.w(m.unit+0xac,12)
    for i,unit in enumerate(m.miners[1:]):m.w(unit+0x9c,(31+i)*256+128);m.w(unit+0xa0,33*256+128);m.w(unit+0xac,2)
    orders=plan(tick(m));assert [o.unit for o in orders if o.purpose==4]==[10] and not enters(orders);deliver(orders)
    # Another miner arrives closer while admitted Move has not executed.
    for f in range(4):assert not enters(plan(tick(m)))
    radio(m,[]);assert not enters(plan(tick(m)))
    # Explicit model of the game executing the emitted native Move.
    m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.testcells[33030]);orders=plan(tick(m));assert enters(orders)==[14];deliver(orders)
    radio(m,[m.miners[1]]);m.w(m.miners[1]+0x5a4,m.building);m.b(m.miners[1]+0x6d1,1)
    for f in range(10):assert not enters(plan(tick(m)))
    m.u.mem_write(m.miners[1]+0x33c,bytes(16));radio(m,[]);orders=plan(tick(m));assert enters(orders)==[10] and not [o for o in orders if o.purpose==4];deliver(orders)
    host.stdin.close();assert host.wait(timeout=10)==0
  check(str(mode)+'_host_world_receipt_release_winner_protected_next_turn',pipeline)
Path(a.output).write_text(json.dumps({'inputs':{'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'RA2Toolbox.exe':hashlib.sha256((root/'RA2Toolbox.exe').read_bytes()).hexdigest()},'results':results,'limitations':'Actual DLL and original constructors/queues plus compiled production external planner. Win32, movement, radio release and unloading are modeled. No whole-game, real network or physical timing validation.'},indent=2)+'\n')

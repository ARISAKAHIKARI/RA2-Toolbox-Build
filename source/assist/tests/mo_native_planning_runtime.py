"""Isolated native planning receiver probe; no game process or private bytes emitted."""
import argparse, hashlib, importlib.util, json, struct, sys, traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--cells',action='store_true');p.add_argument('--connect-index',type=int,choices=(0,1),default=0);a=p.parse_args()
game_hash=hashlib.sha256(Path(a.game).read_bytes()).hexdigest()
# Explicitly pin both inspected images; constructors and receiver below still
# execute the original instructions and assert every stored plan/node field.
if game_hash not in {'247f72881e1a68c8fc305e3702dc0100a72d17601305999026393c340d5daeb0','268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'}:raise RuntimeError('Unsupported private PE for this native planning probe')
spec=importlib.util.spec_from_file_location('helper',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
saved=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=saved
m=n.fixture(0)
def hook(at,fn):m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:fn(),begin=at,end=at)
def reg(r):return m.u.reg_read(r)
def arg(i):return m.r(reg(UC_X86_REG_ESP)+4+4*i)
hook(0x7c8e17,lambda:m.ret(m.alloc(arg(0)),0));hook(0x7c8b3d,lambda:m.ret(0,0))
hook(0x50b6f0,lambda:m.ret(0,0));hook(0x750920,lambda:m.ret(0,2));hook(0x521b60,lambda:m.ret(0,0))
# Synthetic actors do not supply the entire virtual table. Model only the
# native waypoint eligibility query at this no-argument callsite.
hook(0x639d13,lambda:(m.u.reg_write(UC_X86_REG_EAX,1),m.u.reg_write(UC_X86_REG_EIP,0x639d19)))
objects={11:m.eng,12:m.building,14:m.obj(0x7e3ebc,m.buildT,m.enemy,14),15:m.obj(0x7e3ebc,m.buildT,m.enemy,15)}
route=(30020,30021,30022,30023) if a.cells else (12,14,15)
tag=11 if a.cells else 0x34
if a.cells:
 for cellID in route:
  cell=m.alloc();m.w(cell,0x7e4eec);m.u.mem_write(cell+0x24,struct.pack('<hh',cellID%1000,cellID//1000));objects[cellID]=cell
  cells=m.r(0x87f7e8+316);m.w(cells+4*((cellID//1000)*512+cellID%1000),cell)
for at in (0x6e6f20,0x6e6e20,0x6e6ff0):hook(at,lambda:m.ret(objects.get(m.r(reg(UC_X86_REG_ECX)),0),0))
for va,vt in ((0xac4b30,0x7efe64),(0xac4b48,0x7efe24),(0xac4c40,0x7e180c),(0xac4c78,0x7efea4),(0xac4c98,0x7efe64),(0xac4c18,0x7efe64)):
 m.vector(va,[0]*32);m.w(va,vt);m.w(va+16,0)
m.w(m.eng+0xac,2);m.w(m.eng+0xb4,0xffffffff)
def receive(ev):m.run(0x4c6cb0,(),ev)
def snapshot(stage):
 token=m.r(m.eng+0x514)
 state={'stage':stage,'token':token,'count':m.r(token+0x14) if token else 0,'pending':m.r(token+0x8c) if token else None,'loop_length':m.r(token+0x90) if token else None,'loop_start':m.r(token+0x94) if token else None}
 if token:
  nodes=[m.r(m.r(token+8)+i*4) for i in range(state['count'])];state['events']=[]
  for i,node in enumerate(nodes):
   members=[m.r(m.r(node+4)+j*4) for j in range(m.r(node+16))];pairs=[member for member in members if m.r(member)==m.eng];assert len(pairs)==1
   data=bytes(m.u.mem_read(m.r(pairs[0]+4),30))
   saved={'kind':data[0],'actor':struct.unpack_from('<I',data,7)[0],'actor_tag':data[11],'mission':data[12],'target':struct.unpack_from('<I',data,14)[0],'target_tag':data[18],'destination':struct.unpack_from('<I',data,19)[0],'destination_tag':data[23],'nearest':struct.unpack_from('<I',data,24)[0],'nearest_tag':data[28]}
   assert saved=={'kind':4,'actor':11,'actor_tag':0x34,'mission':(29 if (stage.startswith('connect_') or stage=='commit') and i>=a.connect_index else 2),'target':0,'target_tag':0,'destination':route[i],'destination_tag':tag,'nearest':route[i],'nearest_tag':tag},saved
   state['events'].append(saved)
 return state
rows=[]
try:
 for target in route:
  ev=m.alloc();m.run(0x4c6860,(0,11,0x34,2,0,0,target,tag,target,tag),ev);m.b(ev+29,1);receive(ev);rows.append(snapshot('append_'+str(target)))
 ev=m.alloc();m.run(0x4c6780,(0,42,11,0x34,route[a.connect_index],tag),ev);receive(ev);rows.append(snapshot('connect_'+str(a.connect_index)))
 assert rows[-1]['loop_length']==len(route)-a.connect_index and rows[-1]['loop_start']==a.connect_index,rows[-1]
 ev=m.alloc();m.run(0x4c66c0,(0,43),ev);receive(ev);rows.append(snapshot('commit'))
 assert rows[-1]['pending']==0xffffffff,rows[-1]
 assert not m.codeWrites
 status='PASS'
except Exception as ex:
 status='FAIL';rows.append({'error':repr(ex),'eip':hex(reg(UC_X86_REG_EIP))});traceback.print_exc()
assert hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==game_hash, 'Private PE changed during probe'
result={'status':status,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'scope':__doc__,'cells':a.cells,'connect_index':a.connect_index,'rows':rows,'boundary':'Original constructors and receiver; synthetic objects/ID resolver/heap/audio/owner/waypoint-eligibility. No pathfinding, arrival or multiplayer proof.'}
Path(a.output).write_text(json.dumps(result,indent=2)+'\n');print(status+' native planning receiver: cells='+str(a.cells)+' connect_index='+str(a.connect_index)+'; exact node fields, Move-to-AttackMove conversion, loop range and pending transition; no game process');raise SystemExit(status!='PASS')

"""Native Foot Action::Attack dispatch and original QueueMegaMission, tested separately.
Intercepts ClickedMission arguments before platform-extension trampolines. It does
not execute the complete click-to-AI chain or prove pursuit/pathfinding behavior.
"""
import argparse,importlib.util,json,hashlib,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDX
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[];packets=[]
for distance in (128,1024,8192):
 spec=importlib.util.spec_from_file_location('f',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,3);m.b(0x822cf2,0);m.w(m.unit+0x9c,10000);m.w(m.unit+0xa0,10000);m.w(m.enemyUnit+0x9c,10000+distance);m.w(m.enemyUnit+0xa0,10000)
 targetArgs=[]
 def dispatch(uc,at,size,_):
  sp=uc.reg_read(UC_X86_REG_ESP);targetArgs.append(tuple(m.r(sp+4+i*4) for i in range(4)));m.ret(1,4)
 can=m.r(0x7f5c70+0x9c);clicked=m.r(0x7f5c70+0x378)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=can,end=can)
 m.u.hook_add(UC_HOOK_CODE,dispatch,begin=clicked,end=clicked)
 m.run(0x4d74e0,(5,m.enemyUnit,0),m.unit)
 assert targetArgs==[(1,m.enemyUnit,0,0)],targetArgs
 target=m.alloc(16);destination=m.alloc(16);m.w(target,m.r(m.enemyUnit+0x10));m.b(target+4,0x34)
 m.u.reg_write(UC_X86_REG_EDX,target);m.run(0x646e90,(m.r(m.unit+0x10),0x34,destination),self=1)
 assert m.r(0xa802c8)==1
 packet=bytes(m.u.mem_read(0xa802d4,111));assert packet[0]==4 and packet[12]==1 and packet[14:19]==struct.pack('<IB',m.r(m.enemyUnit+0x10),0x34) and packet[19:24]==bytes(5),packet[:32].hex()
 packets.append(packet[7:30]);rows.append({'distance':distance,'clicked_mission_args':[1,'enemy',0,0],'payload':packet[7:30].hex(),'status':'PASS'})
assert len(set(packets))==1
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS: near/in-range/far Action::Attack dispatch has identical arguments; native queue serializes identical Attack payloads')

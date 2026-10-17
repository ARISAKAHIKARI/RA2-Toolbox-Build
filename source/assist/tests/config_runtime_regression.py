"""Actual x86 DLL ingress/constructors; synthetic world. No live network simulation."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode,flags):
 spec=importlib.util.spec_from_file_location('config_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);s=state(m);s.flags=flags;s.externalDisplay=1;save(m,s);return m
def count(m):return m.r(0x8b41f8)+m.r(0xa802c8)
def check(name,fn):
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
for mode in (0,3):
 for permission,repairable,owned in ((0,0,1),(0,1,1),(1,0,1),(1,1,1),(1,0,0)):
  def repair(permission=permission,repairable=repairable,owned=owned,mode=mode):
   m=fixture(mode,64|(134217728 if permission else 0));m.b(m.buildT+0x157a,repairable)
   if not owned:m.w(m.building+0x21c,m.enemy)
   before=bytes(m.u.mem_read(m.buildT,0x2000));s=m.hook();assert count(m)==int(owned and (permission or repairable))
   for _ in range(3):m.hook()
   assert count(m)==int(owned and (permission or repairable));assert bytes(m.u.mem_read(m.buildT,0x2000))==before
  check(f'repair_{mode}_permission{permission}_native{repairable}_own{owned}',repair)
 for case in ('default','exclude_miner','exclude_refinery','custom_miner','empty_custom','no_dock','revoke_pending','config_incomplete'):
  def ore(case=case,mode=mode):
   m=fixture(mode,32768);m.b(m.t+0xe0e,case!='custom_miner');m.w(m.t+0x800,40);m.vector(m.t+0x3e8,[] if case=='no_dock' else [m.buildT]);m.u.mem_write(m.unit+0x33c,struct.pack('<4f',40,0,0,0))
   s=state(m)
   if case in ('exclude_miner','empty_custom','custom_miner'):
    s.oreConfig.customMiners=1;s.oreConfig.minerCount=0 if case=='empty_custom' else 1;s.oreConfig.miners[0].value=b'HTNK' if case=='custom_miner' else b'OTHER'
   if case=='exclude_refinery':s.oreConfig.customRefineries=1;s.oreConfig.refineryCount=1;s.oreConfig.refineries[0].value=b'OTHER'
   s.oreConfigSeq=2;save(m,s);s=m.hook()
   if case in ('default','custom_miner'):assert s.world.refineryCount==1 and any(s.world.actors[i].flags&16 for i in range(s.world.count))
   if case in ('exclude_miner','empty_custom','exclude_refinery','no_dock'):assert s.world.refineryCount==0
   s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,0,0,12,7,0x34,0,12)
   if case=='revoke_pending':s.oreConfig.customRefineries=1;s.oreConfig.refineryCount=0;s.oreConfigSeq+=2
   if case=='config_incomplete':s.oreConfigSeq+=1
   save(m,s);s=m.hook();assert s.planAck==s.planRequest;assert s.planAccepted==int(case in ('default','custom_miner')),(case,s.planAccepted)
  check(f'ore_config_{mode}_{case}',ore)
Path(a.output).write_text(json.dumps({'inputs':{'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},'scope':__doc__,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

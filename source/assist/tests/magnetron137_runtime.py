"""Compiled sampling and native Idle packets in a synthetic world. No fall-physics or netplay claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for case in ('tree','ore_pillar','tank','aircraft','hover_low','lifted','air_layer','different_cell','crumbling','bridge'):
  m=setup(mode,True);m.w(0xAC13C8,104);m.w(m.crateCell+0xe4,0);m.w(m.crateCell+0xec,0)
  terrain=case in ('tree','ore_pillar','crumbling');vtable=0x7f522c if terrain else 0x7e22a4 if case=='aircraft' else 0x7f5c70
  q=m.obj(vtable,m.t,m.enemy,55);m.w(q+0x9c,20*256+128);m.w(q+0xa0,30*256+128);m.w(q+0xa4,1 if case=='hover_low' else 0);m.w(m.crateCell+0xe4,q)
  if case=='lifted':m.w(q+0x2b0,m.unit)
  if case=='air_layer':m.w(q+0x94,3)
  if case=='different_cell':m.w(q+0x9c,21*256+128)
  if case=='crumbling':m.b(q+0xcd,1)
  if case=='bridge':m.w(m.crateCell+0x140,0x100)
  m.vector(0xa8ec78,[m.unit,m.enemyUnit,m.building]+([] if terrain else [q]));s=state(m);s.magnetronDrop=1;save(m,s);s=tick(m)
  assert count(m)==int(case in ('tree','ore_pillar','tank')),(mode,case,count(m))
  assert not m.codeWrites;rows.append({'case':f'drop_{mode}_{case}','status':'PASS'})
 for case in ('inside','maximum','minimum','too_far','airborne'):
  m=setup(mode,True);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.unit+0xac,5);m.w(m.unit+0x5a4,0);m.w(m.enemyUnit+0xa4,0)
  m.w(m.weapon+0xb8,512);distance=4352 if case=='maximum' else 128 if case=='minimum' else 5000 if case=='too_far' else 1024
  m.w(m.enemyUnit+0x9c,m.r(m.unit+0x9c)+distance)
  if case=='airborne':m.w(m.enemyUnit+0x94,3);m.b(m.enemyUnit+0x134,1)
  s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;save(m,s);s=tick(m)
  if case in ('inside','maximum','minimum'):
   assert count(m)==1,(mode,case,count(m),s.world.count,s.world.intentCount)
   event=packets(m);assert event[12]==1 and event[14:19]==struct.pack('<IB',m.r(m.enemyUnit+0x10),0x34) and event[19:24]==bytes(5)
  else:
   # A newly enrolled global unit may enter AreaGuard, but cannot attack this candidate.
   assert count(m)<=1 and (not count(m) or packets(m)[12]==6),(case,packets(m)[:30].hex())
  assert not m.codeWrites;rows.append({'case':f'acquisition_{mode}_{case}','status':'PASS'})
 for case in ('idle','moving','attack','disabled'):
  m=setup(mode,True);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.unit+0xac,2 if case=='moving' else 1 if case=='attack' else 5)
  s=state(m);s.flags=1024|262144;s.magnetronGlobal=case!='disabled';save(m,s);s=tick(m)
  assert s.data.advancedUnits==int(case=='idle'),(case,s.data.advancedUnits,hex(m.r(m.unit+0x5a4)),m.r(m.unit+0xb4),hex(s.flags),s.magnetronGlobal,s.world.intentCount,hex(m.r(m.t+0x898)),m.r(m.unit+0x6c),m.r(m.unit+0x1c8))
  if case=='idle':
   assert any(getattr(i,"global") for i in s.world.intents[:s.world.intentCount])
   m.w(m.unit+0xac,1);m.w(m.unit+0x5a4,m.crateCell);s=tick(m);assert s.data.advancedUnits==1,'own native pursuit must not cancel global task'
   s.magnetronGlobal=0;save(m,s);s=tick(m);assert s.data.advancedUnits==0
  rows.append({'case':f'global_{mode}_{case}','status':'PASS'})
# An accepted Attack with no movement or shot must escalate to a vanilla Move.
for mode in (0,3):
 for distance in (128,4352):
  m=setup(mode,True);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.unit+0xac,5);m.w(m.unit+0x5a4,0);m.w(m.enemyUnit+0xa4,0);m.w(m.weapon+0xb8,512);m.w(m.enemyUnit+0x9c,m.r(m.unit+0x9c)+distance)
  for y in range(26,35):
   for x in range(15,42):
    cell=m.alloc(0x200);m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,cell)
  s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;save(m,s);s=tick(m)
  assert count(m)==1 and packets(m)[12]==1
  m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.enemyUnit)
  for _ in range(23):tick(m)
  assert count(m)==1,'preserve a normal pending native pursuit'
  tick(m);assert count(m)==2,(mode,distance,count(m))
  packet=packets(m)[111:222];assert packet[12]==2 and packet[14:19]==bytes(5)
  cell=struct.unpack_from('<I',packet,19)[0];assert cell%1000<20 if distance==128 else cell%1000>20
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.r(m.cells+(cell//1000*512+cell%1000)*4))
  for _ in range(4):tick(m)
  assert count(m)==2
  m.w(m.enemyUnit+0x9c,m.r(m.unit+0x9c)+1024);tick(m)
  assert count(m)==3 and packets(m)[2*111+12]==1
  assert not m.codeWrites;rows.append({'case':f'pursuit_{mode}_{distance}_attack_move_reenter','status':'PASS'})
# Ordinary LEFT object attack is handed to the native game. The mailbox only
# observes/cancels automation. Model delayed delivery to catch re-enrolment in
# the idle gap, then a long attack using either of the magnetron's weapons.
for mode in (0,3):
 for inside in (False,True):
  for target_kind in ('vehicle','building'):
   m=setup(mode,inside);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.unit+0xac,5);m.w(m.unit+0x5a4,0);m.w(m.enemyUnit+0xa4,0)
   m.w(m.building+0x21c,m.enemy);secondary=m.alloc();proj=m.alloc();wh=m.alloc();m.w(m.t+0x898+28,secondary);m.w(secondary+0xb4,6*256);m.w(secondary+0xa0,proj);m.w(secondary+0xac,wh);m.b(proj+0x2a5,1)
   for n in range(11):m.u.mem_write(wh+0xa0+n*8,struct.pack('<d',1.))
   s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;save(m,s);s=tick(m);assert s.data.advancedUnits==1
   old=count(m);m.queryObject=m.building if target_kind=='building' else m.enemyUnit
   s=pointer(m,4,65536);assert s.pointer.result==1 and count(m)==old
   for _ in range(8):
    s=tick(m);assert s.data.advancedUnits==0 and count(m)==old,('retook pending left attack',mode,inside,target_kind)
   m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.queryObject)
   m.w(0xa8ed84,m.r(0xa8ed84)+100);s=tick(m)
   assert s.data.advancedUnits==0 and count(m)==old,'ongoing ordinary Attack stays native after pending grace'
   m.w(m.unit+0xac,5);s=tick(m);assert s.data.advancedUnits==0,'idle mission with live attack target is not free'
   m.w(m.unit+0x2b4,0);s=tick(m);assert s.data.advancedUnits==1,'resume only after native attack has ended'
   assert not m.codeWrites
   rows.append({'case':f'ordinary_left_attack_{mode}_{inside}_{target_kind}_pending_active_finished','status':'PASS'})
print('PASS',len(rows),'magnetron terrain/air rejection, global enrolment and ordinary left attack ownership cases')
Path(a.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')

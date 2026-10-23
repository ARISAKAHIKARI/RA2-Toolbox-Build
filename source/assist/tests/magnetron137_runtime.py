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
 for case in ('idle','moving','attack','disabled'):
  m=setup(mode,True);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.unit+0xac,2 if case=='moving' else 1 if case=='attack' else 5)
  s=state(m);s.flags=1024|262144;s.magnetronGlobal=case!='disabled';save(m,s);s=tick(m)
  assert s.data.advancedUnits==int(case=='idle'),(case,s.data.advancedUnits,hex(m.r(m.unit+0x5a4)),m.r(m.unit+0xb4),hex(s.flags),s.magnetronGlobal,s.world.intentCount,hex(m.r(m.t+0x898)),m.r(m.unit+0x6c),m.r(m.unit+0x1c8))
  if case=='idle':
   assert any(getattr(i,"global") for i in s.world.intents[:s.world.intentCount])
   s.magnetronGlobal=0;save(m,s);s=tick(m);assert s.data.advancedUnits==0
  rows.append({'case':f'global_{mode}_{case}','status':'PASS'})
print('PASS',len(rows),'new magnetron terrain/air rejection and global enrolment cases')
Path(a.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')

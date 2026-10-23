"""Actual Runtime sampling/admission and native Idle serialization for friendly lifts.
The lift, map, Win32 and final event delivery are fixtures; no falling-physics,
damage, Windows UI or multiplayer-peer validation is claimed.
"""
from pathlib import Path
SCOPE = __doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]

def beam(mode, inside, relation='own', landing='building'):
 m=setup(mode,inside);m.w(0xac13c8,104) # native IsInAir height threshold; not a zero-initialized Rules field
 if relation=='own':m.w(m.enemyUnit+0x21c,m.house)
 elif relation=='allied':m.w(m.house+0x5788,3)
 elif relation=='neutral':m.b(m.r(m.enemy+0x34)+0x1a6,1)
 # Only the independent AutoDrop toggle is on. No selected units, no global
 # acquisition and no enhanced attack intent are needed to release this beam.
 m.vector(0xa8ecb8,[]);m.b(m.unit+0x83,0);m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.enemyUnit)
 m.w(m.crateCell+0xe4,m.building if landing=='building' else 0)
 roster=[m.unit,m.enemyUnit,m.building]
 if landing in ('tree','tank','aircraft','hover_low'):
  vt=0x7f522c if landing=='tree' else 0x7e22a4 if landing=='aircraft' else 0x7f5c70
  q=m.obj(vt,m.t,m.house,55)
  for offset in (0x9c,0xa0):m.w(q+offset,m.r(m.enemyUnit+offset))
  m.w(q+0xa4,1 if landing=='hover_low' else 0);m.w(m.crateCell+0xe4,q)
  if landing!='tree':roster.append(q)
 if landing in ('water','amphibious_water'):m.w(m.crateCell+0xec,2)
 if landing=='amphibious_water':m.w(m.t+0x5b4,3)
 if landing=='ship_land':m.w(m.t+0x67c,5)
 if landing=='cliff':m.w(0xaa073c,100);m.w(m.crateCell+0x38,100);m.b(m.crateCell+0x11a,1)
 if landing=='bridge':m.w(m.crateCell+0xe4,m.building);m.w(m.crateCell+0x140,0x100)
 m.vector(0xa8ec78,roster)
 s=state(m);s.flags=0;s.magnetronGlobal=0;s.magnetronDrop=1;save(m,s)
 return m

for mode in (0,3):
 for inside in (False,True):
  cases=[(relation,'building',True) for relation in ('own','allied','neutral','enemy')]
  cases += [('own',landing,True) for landing in ('tree','tank','water','ship_land','cliff')]
  cases += [('own',landing,False) for landing in ('clear','aircraft','hover_low','amphibious_water','bridge')]
  for relation,landing,expected in cases:
   m=beam(mode,inside,relation,landing);before=[bytes(m.u.mem_read(obj,0x800)) for obj in (m.unit,m.enemyUnit)]
   s=tick(m);source=next(x for x in s.world.actors[:s.world.count] if x.id==10)
   assert bool(source.flags&536870912)==expected,('hazard',relation,landing,mode,inside,source.flags)
   assert source.liftTarget==m.r(m.enemyUnit+0x10) and s.world.intentCount==0
   if not inside:s=submit(m)
   assert count(m)==int(expected),(relation,landing,mode,inside,count(m),s.planRejectedMask)
   if expected:
    packet=packets(m);assert packet[0]==6 and packet[7:12]==struct.pack('<IB',10,0x34),packet[:20].hex()
   for _ in range(4):tick(m)
   assert count(m)==int(expected),'same beam must not spam Idle'
   assert before==[bytes(m.u.mem_read(obj,0x800)) for obj in (m.unit,m.enemyUnit)],'queue only; no direct release-field writes'
   assert not m.codeWrites
   rows.append({'case':f'{relation}_{landing}_{mode}_{inside}','status':'PASS'})
  # Allowing an existing friendly lift does not authorize acquiring friendly
  # targets. Global acquisition must still exclude own/allied/neutral units.
  for relation in ('own','allied','neutral'):
   m=beam(mode,inside,relation,'clear');m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0)
   m.w(m.unit+0xac,5);m.w(m.unit+0x2b4,0);m.w(m.enemyUnit+0x2b4,0);m.w(m.enemyUnit+0xa4,0)
   m.w(m.enemyUnit+0x9c,m.r(m.unit+0x9c)+1024)
   s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;save(m,s);s=tick(m)
   assert not any(x.flags&536870912 for x in s.world.actors[:s.world.count])
   assert not any(x.flags&2 for x in s.world.actors[:s.world.count]),'friendly unit became enemy candidate'
   assert all(packets(m)[i*111]==4 and packets(m)[i*111+12]==5 for i in range(count(m))),'friendly auto-attack'
   assert not m.codeWrites
   rows.append({'case':f'no_friendly_acquisition_{relation}_{mode}_{inside}','status':'PASS'})
 # Revalidate the real beam/source immediately before consuming a host plan.
 for race in ('disabled','source_foreign','broken_link','gone','grounded','victim_allied','victim_enemy'):
  m=beam(mode,False);s=tick(m);assert any(x.id==10 and x.flags&536870912 for x in s.world.actors[:s.world.count])
  if race=='disabled':s.magnetronDrop=0;save(m,s)
  elif race=='source_foreign':m.w(m.unit+0x21c,m.enemy)
  elif race=='broken_link':m.w(m.enemyUnit+0x2b0,0)
  elif race=='gone':m.w(m.enemyUnit+0x6c,0)
  elif race=='grounded':m.w(m.enemyUnit+0xa4,0)
  elif race in ('victim_allied','victim_enemy'):
   m.w(m.enemyUnit+0x21c,m.enemy);m.w(m.house+0x5788,3 if race=='victim_allied' else 1)
  s=submit(m);expected=race in ('victim_allied','victim_enemy')
  assert count(m)==int(expected) and s.planAccepted==int(expected),(race,mode,count(m),s.planRejectedMask)
  assert not m.codeWrites
  rows.append({'case':f'friendly_beam_race_{race}_{mode}','status':'PASS'})
print('PASS',len(rows),'friendly/allied/neutral/enemy lifts, independent toggle, hazards, air exclusions, dedup and admission races')
Path(a.output).write_text(json.dumps({'scope':SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')

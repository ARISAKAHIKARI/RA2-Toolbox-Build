"""Compiled Runtime landing decisions: native fields and LandType/SpeedType table.
Synthetic map/Win32; tests admission and native Idle serialization, not falling physics.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
cases=[('tank_water',2,1,0,1,True),('ship_land',0,5,10,1,True),('ship_water',2,5,10,1,False),('tank_clear',0,1,0,1,False),('rough_passable',7,1,0,1,False),('cliff_impassable',3,1,0,0,True),('hover_water',2,3,0,1,False),('amphibious_water',2,1,3,1,False),('amphibious_land',0,6,5,1,False),('beach_ship_beach',6,7,11,1,False),('beach_ship_land',0,7,11,1,True)]
for mode in (0,3):
 for inside in (False,True):
  for name,land,speed,zone,passable,expected in cases:
   m=setup(mode,inside);m.w(m.crateCell+0xe4,0);m.w(m.crateCell+0xec,land);m.w(m.t+0x67c,speed);m.w(m.t+0x5b4,zone)
   m.u.mem_write(0x89ea40,struct.pack('<108f',*([1.]*108)));m.u.mem_write(0x89ea40+(land*9+speed)*4,struct.pack('<f',passable))
   s=state(m);s.flags=0;s.magnetronGlobal=0;s.magnetronDrop=1;save(m,s);s=tick(m)
   source=next(x for x in s.world.actors[:s.world.count] if x.id==10);assert bool(source.flags&536870912)==expected,(name,mode,inside,source.flags)
   if not inside:s=submit(m)
   assert count(m)==int(expected),(name,mode,inside,count(m),s.planRejectedMask)
   assert not m.codeWrites
   rows.append({'case':f'{name}_{mode}_{inside}','status':'PASS'})
print('PASS',len(rows),'landing water/land/cliff/rough/beach/hover/amphibious cases')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')

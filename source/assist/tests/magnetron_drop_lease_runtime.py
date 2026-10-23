"""Compiled manual attack -> real lease -> hazardous beam -> Idle admission.
The attack/lift is modeled; actual Runtime and native event constructors execute.
This covers the arbitration state missed by tests which only observed native clicks.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_manual_drop_runtime.py').read_text().split('\nfor mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_manual_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for inside in (False,True):
  m=setup(mode,inside);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.queryObject=m.enemyUnit
  s=state(m);s.flags=1024|262144;s.magnetronDrop=0;save(m,s)
  s=pointer(m,1,2|8192)
  assert count(m)==1 and s.pointer.accepted==1,('manual attack',mode,inside,count(m),s.pointer.result)
  # An active beam may coexist with a newly queued manual order (including
  # Sticky). Do not pretend the queued Attack was already observed/delivered.
  m.w(m.unit+0xac,6);m.w(m.unit+0x2ac,m.enemyUnit);m.w(m.enemyUnit+0x2b0,m.unit)
  s=state(m);s.magnetronDrop=1;save(m,s);s=tick(m)
  if not inside:s=current_drop(m)
  assert any(x.id==10 and x.flags&536870912 for x in s.world.actors[:s.world.count]),'hazard was detected'
  assert count(m)==2 and packets(m)[111]==6,('hazardous manual beam blocked by prior attack lease',mode,inside,count(m),s.data.commandRejected)
  for _ in range(4):tick(m)
  assert count(m)==2 and not m.codeWrites
  rows.append({'case':f'manual_attack_lease_then_drop_{mode}_{inside}','status':'PASS'})
print('PASS',len(rows),'manual attack leases do not veto explicit global hazard drop')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')

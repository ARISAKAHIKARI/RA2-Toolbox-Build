"""Native bunker arrival/linking handler 458E50 with explicit map/locomotor
boundaries. Native code writes both BunkerLinkedItem fields; no fixture writes
them. This covers arrival gates, not complete pathfinding or rendered entry.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
for stage,at_goal,moving,expected in [(0,False,False,0),(0,True,True,0),(3,False,False,3),(3,True,True,3),(3,True,False,4),(5,True,False,6)]:
 m=fixture(0);u,b=m.unit,m.building;calls=[]
 def hook(at,fn):m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:fn(),begin=at,end=at)
 def args(n):
  sp=m.u.reg_read(UC_X86_REG_ESP);return tuple(m.r(sp+4+i*4) for i in range(n))
 def capture(label,n,result=0):calls.append((label,args(n)));m.ret(result,n)
 hook(0x65ad30,lambda:m.ret(u,1))
 hook(m.r(0x7f5c70+0x2c),lambda:m.ret(1,0))
 hook(m.r(0x7f5c70+0x1bc),lambda:m.ret(m.crateCell,0))
 hook(0x47c520,lambda:m.ret(b if at_goal else 0,0))
 loco=m.alloc();table=m.alloc();stub=m.alloc();m.w(u+0x674,loco);m.w(loco,table);m.w(table+0x10,stub);hook(stub,lambda:m.ret(int(moving),1))
 hook(m.r(0x7f5c70+0x150),lambda:capture('update',0))
 hook(m.r(0x7f5c70+0x1e8),lambda:capture('mission',2))
 rules=m.alloc();m.w(0x8871e0,rules);m.w(rules+0x240,0xffffffff)
 m.w(b+0x718,stage);assert m.r(u+0x2e4)==0 and m.r(b+0x2e4)==0
 m.run(0x458e50,self=b)
 assert m.r(b+0x718)==expected,(stage,at_goal,moving,m.r(b+0x718))
 if expected==6:
  assert m.r(u+0x2e4)==b and m.r(b+0x2e4)==u and m.r(u+0x214)==0xffffffff
  assert calls==[('update',()),('mission',(5,1))],calls
 else:assert m.r(u+0x2e4)==0 and m.r(b+0x2e4)==0 and not calls
 assert not m.codeWrites
 rows.append({'case':f'native_bunker_stage{stage}_at_goal{at_goal}_moving{moving}','status':'PASS'})
print('PASS',len(rows),'native bunker arrival and final bidirectional linking cases')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')

"""Actual DLL submits explicit enhanced object-target Attack through native queues.
Synthetic actors/Win32/click query. Does not prove pathfinding or netplay.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for binding in (1,2,4):
  for click in (0,65536,1024):
   for target_kind in ('vehicle','building'):
    for distance in (128,2048,8192):
     m=setup(mode,False);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.enemyUnit+0xa4,0)
     m.w(m.building+0x21c,m.enemy);m.queryObject=m.building if target_kind=='building' else m.enemyUnit
     m.w(m.queryObject+0x9c,m.r(m.unit+0x9c)+distance)
     s=state(m);s.flags=1024|8192|262144|131072;s.magnetronGlobal=0;s.magnetronDrop=0;save(m,s)
     # Initial attack, Stop, then the same enhanced object command again.
     for repeat in range(2):
      before=count(m);s=pointer(m,1,binding|click|8192)
      if click==65536:
       for _ in range(8):
        s=tick(m);assert count(m)==before and s.pointer.ack!=s.pointer.request
       # Native event arrival is modelled; the DLL only observes these fields.
       m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.queryObject)
      for _ in range(3):
       if s.pointer.ack==s.pointer.request:break
       s=tick(m)
      assert s.pointer.accepted==1 and s.pointer.rejected==0,(mode,binding,click,target_kind,distance,s.pointer.result)
      assert count(m)==before+(click!=65536)
      if click!=65536:
       packet=packets(m)[before*111:(before+1)*111]
       assert packet[12]==1 and packet[14:19]==struct.pack('<IB',m.r(m.queryObject+0x10),0x34) and packet[19:24]==b'\0'*5
      tick(m);pointer(m,5);m.w(m.unit+0xac,5);m.w(m.unit+0x2b4,0);tick(m)
     assert not m.codeWrites
     rows.append({'case':f'attack_stop_attack_{mode}_{binding}_{click}_{target_kind}_{distance}','status':'PASS'})
print('PASS',len(rows),'enhanced object attacks including repeated orders, primary/secondary targets and range-independent submission')
Path(a.output).write_text(json.dumps({'scope':SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')

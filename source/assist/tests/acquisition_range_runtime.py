"""Real Runtime validation of configured acquisition range; native constructor, synthetic world and weapon data."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
for mode in (0,3):
 for cells in (0,2,5):
  for delta in (0,1):
   try:
    m=n.fixture(mode,1024|8192);projectile=m.alloc();warhead=m.alloc();m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,warhead);m.w(m.weapon+0xb4,2048);m.w(m.weapon+0xb8,0);m.b(projectile+0x2a5,1);m.w(m.t+0x684,-1)
    for i in range(11):m.u.mem_write(warhead+0xa0+i*8,struct.pack('<d',1))
    m.b(m.t+0x231,1);enemyType=m.r(m.enemyUnit+0x6c4);m.b(enemyType+0x231,1);m.w(m.enemy+0x34,m.alloc())
    for unit,x in ((m.unit,5000),(m.enemyUnit,5000+2048+cells*256+delta)):
     m.w(unit+0x9c,x);m.w(unit+0xa0,5000);m.w(unit+0xa4,0)
    s=n.pointer(m,1,8192|1);assert s.data.advancedUnits==1
    s.advancedRangeCells=cells;s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,1,13,0,1,0,1,0);n.save(m,s)
    s=n.tick(m);assert s.planAck==s.planRequest and s.planAccepted==int(delta==0),(cells,delta,s.planAccepted,s.planResult,s.data.lastSubmitError)
    assert not m.codeWrites
    rows.append({'case':f'{mode}_{cells}_{delta}','status':'PASS'});print('PASS',mode,cells,delta,flush=True)
   except Exception as e:rows.append({'case':f'{mode}_{cells}_{delta}','status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

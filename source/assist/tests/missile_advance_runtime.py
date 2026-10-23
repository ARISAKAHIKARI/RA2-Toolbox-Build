"""Production DLL + original event constructors, controlled world/combat queries.
Verifies bounded advance decisions and packets, not movement physics or live netplay.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('missile_runtime_fixture.py').read_text(),str(Path(__file__).with_name('missile_runtime_fixture.py')),'exec'))

original_scan=run_scan
def run_scan(m,inside):
 before=[bytes(m.u.mem_read(p,0x800)) for p in m.guns+m.targets]
 result=original_scan(m,inside)
 assert before==[bytes(m.u.mem_read(p,0x800)) for p in m.guns+m.targets],'production scan wrote gameplay fields'
 return result

def observed(m,start=0):
 if getattr(m,'draining',False):
  # Controlled event delivery consumes the real ring after observing packets.
  # Leaving 128 delivered packets queued would test OutList backpressure, not
  # fair allocation. Preserve tail/head wrap instead of lifting the safe limit.
  sp=m.r(0xa8b238) in (0,5);base=0x8b41f8 if sp else 0xa802c8;mask=16383 if sp else 127
  events=[];head=m.r(base+4)
  for j in range(n.count(m)):
   packet=bytes(m.u.mem_read(base+12+((head+j)&mask)*111,111))
   id=struct.unpack_from('<I',packet,7)[0];action=packet[12];target=struct.unpack_from('<I',packet,14)[0]
   p=next(p for p in m.guns if m.r(p+0x10)==id);m.w(p+0xac,action);m.w(p+0xb4,-1);m.w(p+0x2b4,next((q for q in m.targets if m.r(q+0x10)==target),0));events.append((id,action,target))
  m.w(base+4,m.r(base+8));m.w(base,0)
 else:events=apply(m,start)
 for id,action,target in events:
  p=next(p for p in m.guns if m.r(p+0x10)==id)
  # Model the destination owned by the engine's attack approach. This is a
  # query fixture, never a direct write performed by the production DLL.
  m.w(p+0x5a4,next((q for q in m.targets if action==1 and m.r(q+0x10)==target),0))
 return events

def make(mode,inside,kind='unit',allocation=1,only_future=False):
 m=setup(mode,inside,allocation=allocation,close2=False)
 if kind!='unit':
  table=0x7e3ebc if kind=='building' else 0x7eb058
  typ=m.buildT if kind=='building' else m.engT
  m.w(typ+0x898,m.weapon)
  for p in m.guns:
   m.w(p,table);m.w(p+(0x6c0 if kind=='infantry' else 0x6c4 if kind=='unit' else 0x520),typ)
  info=m.alloc();m.w(info,m.weapon)
  # setup already hooks the shared range and Unit/Infantry weapon getters.
  # Never attach two return stubs to one address (that would pop twice).
  funcs={0x2e4:lambda:m.ret(0,1)}
  if kind=='building':funcs.update({0x3f8:lambda:m.ret(info,1),0x350:lambda:m.ret(1,0)})
  for off,fn in funcs.items():
   address=m.r(table+off);m.u.hook_add(UC_HOOK_CODE,lambda u,at,size,_,fn=fn:fn(),begin=address,end=address)
 if only_future:m.w(m.targets[0]+0x6c,0)
 # A real surrounding band: 1750 from shooter, beyond 1500 weapon range.
 m.w(m.targets[1]+0x9c,2750)
 m.vector(0xa8ec78,m.guns+m.targets)
 return m

for mode in (0,3):
 for inside in (False,True):
  for case in ('large_army','late_approach','stagger_even','stagger_smart','future_only','timeout','leash','target_leash','gone','out_band','receding','disable','pending_disable','stop','manual','ordinary_move','sticky','building','building_enter','building_disable','building_gone','infantry','coverage_lost','urgent','minimum','wrong_weapon'):
   def test(mode=mode,inside=inside,case=case):
    kind='building' if case.startswith('building') else 'infantry' if case=='infantry' else 'unit'
    m=make(mode,inside,kind,allocation=0 if case in ('stagger_smart','coverage_lost') else 1,only_future=case in ('future_only','building','building_enter','building_disable','building_gone','infantry','timeout','leash','target_leash','gone','out_band','receding','minimum','wrong_weapon'))
    if case=='large_army':
     m.draining=True
     for num in range(6,128):
      gun=m.obj(0x7f5c70,m.t,m.house,100+num);m.w(gun+0x9c,1000);m.w(gun+0xa0,1000);m.w(gun+0xa4,0);m.guns.append(gun)
     m.vector(0xa8ec78,m.guns+m.targets);m.w(m.targets[0]+0x6c,0)
    if case=='late_approach':m.close2=True
    if case in ('ordinary_move','sticky'):
     for p in m.guns:m.w(p+0xac,2 if case=='ordinary_move' else 6);m.w(p+0x5a4,m.crateCell)
    if case=='minimum':m.w(m.weapon+0xb8,2000)
    if case=='wrong_weapon':m.w(m.weapon+0xa4,0)
    s=run_scan(m,inside);initial=[] if case=='pending_disable' else observed(m);old=n.count(m)
    assert all(action==(0 if kind=='building' else 1) for _,action,_ in initial),initial
    if case in ('ordinary_move','sticky','minimum','wrong_weapon'):
     assert not initial,(case,initial);return
    if case=='pending_disable':assert old>0
    elif case.startswith('stagger'):
     expect=3 if case=='stagger_even' else 2
     assert sum(t==40 for _,_,t in initial)==expect and sum(t==41 for _,_,t in initial)==6-expect,initial
    else:assert initial,(case,s.missileSources,s.missileTargets)
    advanced=[id for id,_,target in initial if target==41]
    if case=='large_army':
     seen={id for id,_,_ in initial}
     # Native frame budgets deliberately cap outgoing orders. Allow enough
     # batches for 128 actors, but bound the check to catch permanent starvation.
     for _ in range(16):
      if len(seen)==len(m.guns):break
      start=n.count(m);run_scan(m,inside);seen.update(id for id,action,_ in observed(m,start) if action==1)
     assert seen=={m.r(p+0x10) for p in m.guns},('starved',len(seen))
    elif case=='late_approach':
     m.closeFirst=m.close2=False;m.w(m.targets[0]+0x9c,2750);m.w(m.targets[1]+0x9c,2800)
     m.w(0xa8ed84,m.r(0xa8ed84)+31) # invalidate the old velocity sample
     run_scan(m,inside);new=observed(m,old);assert len(new)==6 and all(action==1 for _,action,_ in new),new
     start=n.count(m);m.w(0xa8ed84,m.r(0xa8ed84)+61);run_scan(m,inside);new=observed(m,start);assert len(new)==6 and all(action==5 for _,action,_ in new),new
    elif case.startswith('stagger') or case in ('future_only','infantry'):
     # Stationary target outside range is still a valid limited approach; no
     # repeated event, no pending queue flood while the original Attack moves.
     for _ in range(4):run_scan(m,inside)
     assert n.count(m)==old,(case,n.count(m),old)
    elif case in ('building_enter','building_disable','building_gone'):
     if case=='building_enter':m.close2=True
     elif case=='building_gone':m.w(m.targets[1]+0x6c,0)
     else:s=n.state(m);s.missileEnabled=0;n.save(m,s)
     run_scan(m,inside);new=observed(m,old);expected=1 if case=='building_enter' else 5
     assert len(new)==6 and all(action==expected for _,action,_ in new),(case,new)
     run_scan(m,inside)
     if expected==5:assert n.state(m).missileHeld==0
    elif case in ('timeout','building'):
     m.w(0xa8ed84,m.r(0xa8ed84)+61);run_scan(m,inside);new=observed(m,old)
     if case=='building':assert not new,new
     else:assert len(new)==6 and all(action==5 for _,action,_ in new),new
    elif case in ('leash','target_leash','gone','out_band','receding'):
     if case=='leash':
      for p in m.guns:m.w(p+0xa0,2025)
     elif case=='target_leash':
      m.w(0xa8ed84,m.r(0xa8ed84)+31)
      for p in m.guns:m.w(p+0x9c,1800)
      m.w(m.targets[1]+0x9c,3600)
     elif case=='gone':m.w(m.targets[1]+0x6c,0)
     elif case=='out_band':m.w(m.targets[1]+0x9c,6000)
     else:m.w(m.targets[1]+0x9c,2814)
     run_scan(m,inside);new=observed(m,old);assert len(new)==6 and all(action==5 for _,action,_ in new),(case,new)
     start=n.count(m);run_scan(m,inside);assert n.count(m)==start,'release grace prevents immediate reacquisition'
    elif case in ('disable','pending_disable','stop'):
     s=n.state(m);s.missileEnabled=0
     if case=='stop':s.stop=1
     n.save(m,s);run_scan(m,inside);new=observed(m,old);assert len(new)==6 and all(action==5 for _,action,_ in new),(case,new)
     run_scan(m,inside);assert n.state(m).missileHeld==0
     if case=='stop':assert n.state(m).stop==2
    elif case=='manual':
     id=advanced[0];p=next(p for p in m.guns if m.r(p+0x10)==id)
     n.pointer(m,5,mutate=lambda s,i:setattr(i.sources[0],'pointer',p) or setattr(i.sources[0],'id',id))
     m.w(p+0xac,2);m.w(p+0x2b4,0);m.w(p+0x5a4,m.crateCell);start=n.count(m)
     for _ in range(14):run_scan(m,inside)
     assert all(struct.unpack_from('<I',n.packets(m),j*111+7)[0]!=id for j in range(start,n.count(m)))
    elif case in ('coverage_lost','urgent'):
     if case=='urgent':m.w(m.targets[0]+0x2b4,m.guns[0])
     else:
      for p in m.guns:
       if m.r(p+0x10) not in advanced:m.w(p+0x2ec,m.r(0xa8ed84));m.w(p+0x2f4,60)
     run_scan(m,inside);new=observed(m,old);assert sum(action==1 and target==40 for _,action,target in new)>=(len(advanced) if case=='urgent' else 2),(case,new)
    assert not m.codeWrites
   check(f'{mode}_{inside}_{case}',test)
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

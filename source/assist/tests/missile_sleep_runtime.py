"""Compiled fire-control Sleep/Attack/Guard transitions and native constructors.

World, range and event consumption are controlled fixtures, not a real game,
projectile hit certification or network delivery test.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('missile_runtime_fixture.py').read_text(),str(Path(__file__).with_name('missile_runtime_fixture.py')),'exec'))
for mode in (0,3):
 for inside in (False,True):
  for case in ('approach','cooldown','shot_reserve','gone','disable','manual','hold_retry','burst_release','low_damage','legacy'):
   def test(mode=mode,inside=inside,case=case):
    m=setup(mode,inside,allocation=0,close2=False);m.w(m.targets[1]+0x9c,2750)
    s=n.state(m);s.missileReserve=case!='legacy';s.lowDamageFilter=case=='low_damage';n.save(m,s)
    if case=='low_damage':
     wh=m.r(m.weapon+0xac);m.u.mem_write(wh+0xa0,struct.pack('<11d',*([0.02]*11)))
    before=[bytes(m.u.mem_read(p,0x800)) for p in m.guns+m.targets]
    run_scan(m,inside)
    assert before==[bytes(m.u.mem_read(p,0x800)) for p in m.guns+m.targets]
    initial=apply(m);old=n.count(m)
    if case=='low_damage':assert not initial;return
    if case=='legacy':assert initial and all(a==1 for _,a,_ in initial);return
    attacks=[x for x in initial if x[1]==1];holds=[x for x in initial if x[1]==0]
    assert len(attacks)==2 and len(holds)==4 and all(t==0 for _,_,t in holds),initial
    # Keep incoming motion plausible for subsequent predictive samples.
    m.w(m.targets[1]+0x9c,2700)
    if case=='approach':
     m.close2=True;run_scan(m,inside);new=apply(m,old)
     assert new and all(a==1 and t==41 for _,a,t in new),new
    elif case=='cooldown':
     for id,_,_ in attacks:
      p=next(p for p in m.guns if m.r(p+0x10)==id);m.w(p+0x2ec,m.r(0xa8ed84));m.w(p+0x2f4,60)
     run_scan(m,inside);new=apply(m,old)
     assert any(a==0 and t==0 for _,a,t in new),new
    elif case=='shot_reserve':
     m.w(m.targets[1]+0x6c,0)
     for id,_,_ in attacks:
      p=next(p for p in m.guns if m.r(p+0x10)==id);m.w(p+0x2ec,m.r(0xa8ed84));m.w(p+0x2f4,60)
     run_scan(m,inside);new=apply(m,old)
     assert not any(a==1 for _,a,_ in new),new
     assert any(a==0 for _,a,_ in new),new
    elif case in ('gone','disable'):
     if case=='gone':
      for q in m.targets:m.w(q+0x6c,0)
     else:s=n.state(m);s.missileEnabled=0;n.save(m,s)
     run_scan(m,inside);new=apply(m,old);assert new and all(a==5 for _,a,_ in new),new
    elif case=='manual':
     id=holds[0][0];p=next(p for p in m.guns if m.r(p+0x10)==id)
     n.pointer(m,5,mutate=lambda s,i:(setattr(i.sources[0],'pointer',p),setattr(i.sources[0],'id',id)))
     m.w(p+0xac,2);m.w(p+0x5a4,m.crateCell);start=n.count(m)
     for _ in range(3):run_scan(m,inside)
     assert all(struct.unpack_from('<I',n.packets(m),j*111+7)[0]!=id for j in range(start,n.count(m)))
    elif case=='hold_retry':
     id=holds[0][0];p=next(p for p in m.guns if m.r(p+0x10)==id)
     # The queue accepted Sleep but the game has not observed it. Expiry of
     # the pending receipt must permit a retry, not imply actual Sleep.
     m.w(p+0xac,1);m.w(p+0x2b4,m.targets[0])
     for j in range(16):
      m.w(m.targets[1]+0x9c,2690-j*10);run_scan(m,inside)
     assert any(struct.unpack_from('<I',n.packets(m),j*111+7)[0]==id and n.packets(m)[j*111+12]==0 for j in range(old,n.count(m)))
    elif case=='burst_release':
     id=attacks[0][0];p=next(p for p in m.guns if m.r(p+0x10)==id)
     for q in m.targets:m.w(q+0x6c,0)
     if not inside:tick(m,False,'missile') # publish candidate-free snapshot
     m.w(m.weapon+0x9c,3);m.w(p+0x3b8,1)
     start=n.count(m);tick(m,inside,'missile')
     assert all(struct.unpack_from('<I',n.packets(m),j*111+7)[0]!=id for j in range(start,n.count(m)))
     m.w(p+0x3b8,0);run_scan(m,inside)
     assert any(struct.unpack_from('<I',n.packets(m),j*111+7)[0]==id and n.packets(m)[j*111+12]==5 for j in range(start,n.count(m)))
    if mode==3 and case!='low_damage':assert not m.codeWrites
   check(f'{mode}_{inside}_{case}',test)
Path(a.output).write_text(json.dumps({'scope':SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

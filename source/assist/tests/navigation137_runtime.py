"""Actual Runtime camera mailbox and SetTacticalPosition; synthetic Win32/world.
Selection endpoints are mocked. No Windows pointer/visual or multiplayer claim.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('scroll_refresh_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('scroll_refresh_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for case in ('valid','latest','duplicate','stale','odd','match','focus','chat','bounds','signature'):
  m=setup(mode);m.mousePoint=(300,300);s=state(m);s.cameraSeq=2;s.cameraMatch=s.data.match;s.cameraTick=m.clock;s.cameraTargetX=500;s.cameraTargetY=2200
  if case=='latest':s.cameraSeq=6;s.cameraTargetX=700
  if case=='duplicate':s.cameraAck=2
  if case=='stale':s.cameraTick=m.clock-300
  if case=='odd':s.cameraSeq=3
  if case=='match':s.cameraMatch+=1
  if case=='focus':s.foreground=0
  if case=='chat':m.b(0xa8bc60+25,1)
  if case=='bounds':s.cameraTargetX=100001
  if case=='signature':m.u.mem_write(0x6d6000,b'\x90')
  calls=[]
  m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:calls.append(at),begin=0x6d6000,end=0x6d6000)
  before=bytes(m.u.mem_read(m.unit,0x800));save(m,s);s=tick(m)
  assert len(calls)==int(case in ('valid','latest')),(case,len(calls),s.cameraSeq,s.cameraAck)
  if calls:
   assert m.r(m.tactical+0xd64)==(700 if case=='latest' else 500)
   assert m.r(m.tactical+0xd68)==2200
   tick(m);assert len(calls)==1,'mailbox replay'
  assert count(m)==0 and before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites
  rows.append({'case':f'camera_{mode}_{case}','status':'PASS'})
 # Selecting a fully armed plane on its airport does not call camera centering.
 m=setup(mode);t=m.alloc();plane=m.obj(0x7e22a4,t,m.house,77);m.w(t+0x684,2);m.w(plane+0x2fc,2);m.w(plane+0xac,7);m.w(plane+0x5a4,m.building)
 docks=m.alloc();m.w(docks,m.r(m.building+0x520));m.w(t+0x3ec,docks);m.w(t+0x3f0,1);m.w(t+0x3f8,1)
 m.vector(0xa8ec78,[m.unit,m.building,plane]);m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=m.r(0x7e22a4+0x13c),end=m.r(0x7e22a4+0x13c))
 s=state(m);s.selectRequest+=1;s.selectIndex=0xfffffffc;s.selectObjectID=77;s.selectMatch=s.data.match;s.selectTick=m.clock;save(m,s)
 camera=bytes(m.u.mem_read(m.tactical+0xd64,24));s=tick(m)
 assert s.selectAck==s.selectRequest and s.selectCount==1,(s.selectResult,s.selectCount)
 assert not any(c[0]==0x6d2420 for c in m.calls) and camera==bytes(m.u.mem_read(m.tactical+0xd64,24)) and count(m)==0
 rows.append({'case':f'idle_aircraft_{mode}_selection_without_camera','status':'PASS'})
print('PASS',len(rows),'camera admission, native positioning, latest-only delivery and idle-aircraft selection')
Path(a.output).write_text(json.dumps({'scope':SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')

"""Actual x86 Runtime/SEH adapter; mocked renderer exports and native resize.
Verifies transitions/ABI/restoration, not real DirectDraw, Windows or netplay.
Private EXE supplied externally solely for fingerprints and engine environment.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def setup(mode=5):
 m=fixture(mode);m.w(0xa8b230,m.alloc());m.resizes=[];m.enabled=0;m.fail=False;m.mutateWindow=False;m.exports=True
 m.w(0x8a00a4,1280);m.w(0x8a00a8,960);m.w(0xa8eb84,1280);m.w(0xa8eb88,960)
 m.w(0xb73550,1);m.b(0x89f978,0);m.w(0x887640,m.alloc());m.w(0x887308,m.alloc())
 m.w(0x886fa8,1120);m.w(0x886fac,960)
 # Large map avoids incidental small-map hook in these transition tests.
 for at,v in ((0x87f8dc,200),(0x87f8e4,2),(0x87f8e8,4),(0x87f8ec,190),(0x87f8f0,190)):m.w(at,v)
 dd=m.alloc();vt=m.alloc();m.w(0x8a0094,dd);m.w(dd,vt);m.w(vt+0x54,0x560bf0)
 m.tactical=m.r(0x887324);m.w(m.tactical+0xd64,1000);m.w(m.tactical+0xd68,2000)
 def add(fn):
  at=m.alloc(16);m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:fn(),begin=at,end=at);return at
 def arg(i=0):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def enable():m.enabled+=1;m.ret(0,0)
 enableAt=add(enable)
 def rect():m.u.mem_write(arg(1),struct.pack('<4i',0,0,1400 if m.mutateWindow and m.resizes and len(m.resizes)%2 else 1280,960));m.ret(1,2)
 rectAt=add(rect)
 cursorAt=add(lambda:(m.u.mem_write(arg(),struct.pack('<ii',640,480)),m.ret(1,1)))
 clientAt=add(lambda:m.ret(1,2))
 def resolve():m.ret({'GetClientRect':rectAt,'GetCursorPos':cursorAt,'ScreenToClient':clientAt}.get(m.string_at(arg(1)),0),2)
 resolveAt=add(resolve)
 def getproc():m.ret({'DDEnableZoom':enableAt,'DDGetProcAddress':resolveAt}.get(m.string_at(arg(1)),0) if m.exports else 0,2)
 getprocAt=add(getproc)
 for entry in m.dll.DIRECTORY_ENTRY_IMPORT:
  for imp in entry.imports:
   if imp.name==b'GetProcAddress':m.w(imp.address,getprocAt)
 def resize(uc,at,size,_):
  assert state(m).viewZoomBusy==1
  w=m.u.reg_read(UC_X86_REG_ECX);h=m.u.reg_read(UC_X86_REG_EDX);m.resizes.append((w,h))
  if m.fail and len(m.resizes)==1:m.ret(0,0);return
  m.w(0x8a00a4,w);m.w(0x8a00a8,h);m.w(0x886fa8,w-160);m.w(0x886fac,h);m.ret(1,0)
 m.u.hook_add(UC_HOOK_CODE,resize,begin=0x560bf0,end=0x560bf0)
 def center(uc,at,size,_):
  x,y=struct.unpack('<ii',m.u.mem_read(arg(),8));assert (x,y)==(1000,2000);m.ret(0,1)
 m.u.hook_add(UC_HOOK_CODE,center,begin=0x6d6000,end=0x6d6000)
 return m
def setzoom(m,p):s=state(m);s.viewZoomPercent=p;save(m,s);return m.hook()
def record(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,5,3,4):
 m=setup(mode);unit=bytes(m.u.mem_read(m.unit,0x800));frame=m.r(0xa8ed84)
 for p in (50,75,125,150,100,50,100):
  s=setzoom(m,p);expected=((128000+p//2)//p,(96000+p//2)//p)
  assert m.resizes,(s.viewZoomStatus,s.data.commandStatus,s.data.status,hex(m.r(0xa8b230)),m.mapping_names)
  assert m.resizes[-1]==expected and s.viewZoomApplied==p and s.viewZoomStatus==(0 if p==100 else 1),(p,s.viewZoomStatus,m.resizes)
  assert not s.viewZoomBusy and m.r(0xa8eb84)==1280 and m.r(0xa8eb88)==960
 assert m.enabled==len(m.resizes)==7 and count(m)==0 and m.r(0xa8ed84)==frame
 assert bytes(m.u.mem_read(m.unit,0x800))==unit and not m.codeWrites
 record(f'{mode}_all_levels_roundtrip_no_simulation_or_ini_changes')
 for boundary in ('stop','world-absent','match'):
  m=setup(mode);setzoom(m,50)
  if boundary=='stop':s=state(m);s.stop=1;save(m,s)
  elif boundary=='world-absent':m.b(0xa8e9a0,0);m.w(0xa83d4c,0);m.w(0xa8b230,0);m.w(0x87f924,0);m.w(0x87f928,0)
  else:m.w(0xa8ed84,1)
  s=m.hook()
  assert (1280,960) in m.resizes[1:],(boundary,m.resizes)
  record(f'{mode}_{boundary}_restores_base')
for case in ('renderer','signature','windowed','held','background','chat','failed','presentation','invalid'):
 m=setup()
 if case=='renderer':m.exports=False
 if case=='signature':m.u.mem_write(0x560bf0+200,b'\x90')
 if case=='windowed':m.b(0x89f978,1)
 if case=='held':m.held_keys.add(1)
 if case=='background':s=state(m);s.foreground=0;save(m,s)
 if case=='chat':m.b(0xa8bc60+25,1)
 if case=='failed':m.fail=True
 if case=='presentation':m.mutateWindow=True
 s=setzoom(m,42 if case=='invalid' else 50)
 expected={'renderer':-1,'signature':-2,'windowed':-2,'held':2,'background':2,'chat':2,'failed':-4,'presentation':-6,'invalid':-3}[case]
 assert s.viewZoomStatus==expected,(case,s.viewZoomStatus,m.resizes)
 assert m.r(0x8a00a4)==1280 and m.r(0x8a00a8)==960 and not s.viewZoomBusy
 n=len(m.resizes);m.hook();assert len(m.resizes)==n
 record(case+'_fails_closed_or_rolls_back')
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'passed':True,'checks':len(rows),'results':rows,'game_tested':False,'multiplayer_tested':False},indent=2)+'\n')


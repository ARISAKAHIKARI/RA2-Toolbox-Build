"""Compiled thirteen-site sidebar experiment with modeled Win32/DSurface/layout.

Runs actual managed patches and x86 adapters against a private original image;
models native allocation, graphics primitives and the middle of Render. Checks
register/stack replay and ownership, not real DirectDraw visual output or netplay.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from pdb_fixture_symbol import function_address
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
rows=[];sites=[(0x72ad60,5),(0x72ad67,6),(0x65386b,6),(0x654332,6),(0x653a3f,6),(0x6d06c7,6),(0x692bb4,4),(0x6a70e0,6),(0x6abd30,5),(0x4e1640,6),(0x692406,6),(0x4f4480,8),(0x4f459d,6)]
def check(name,fn):
 try:fn();rows.append({'case':name,'status':'PASS'})
 except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture():
 spec=importlib.util.spec_from_file_location('sidebar_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b230,m.alloc());m.b(0xa8ed80,1);m.w(0x886fb8,800);m.w(0x886fbc,600);m.b(0xa8eb7c,1);m.b(0xa8ed6b,0)
 m.surfaces={};m.freed=[];m.born=[];m.layout=[];m.dirty=[];m.redraw=[];m.w(0xb73550,1);m.before={at:bytes(m.u.mem_read(at,n))for at,n in sites}
 def hook(at,fn):return m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:fn(),begin=at,end=at)
 m.add_hook=hook
 def arg(i):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+i*4)
 m.arg=arg;vt=m.alloc()
 def native(at,slot,fn):m.w(vt+slot,at);hook(at,fn)
 def this():return m.u.reg_read(UC_X86_REG_ECX)
 native(0x710010,0x7c,lambda:m.ret(m.surfaces[this()]['w'],0))
 native(0x710020,0x80,lambda:m.ret(m.height_override(this(),m.surfaces[this()]['h']) if hasattr(m,'height_override') else m.surfaces[this()]['h'],0))
 native(0x710030,0x70,lambda:m.ret(2,0))
 native(0x710040,0x74,lambda:m.ret(m.surfaces[this()]['w']*2,0))
 native(0x710050,0x5c,lambda:m.ret(m.surfaces[this()]['bits'],2))
 native(0x710060,0x60,lambda:m.ret(1,0))
 native(0x710070,0,lambda:(m.freed.append(this()),m.ret(0,1)))
 def surface(w,h,p=None):
  if p is None:p=m.alloc(0x24)
  m.w(p,vt);m.surfaces[p]={'w':w,'h':h,'bits':m.alloc(w*h*2)};return p
 m.surface=surface
 m.original=[surface(632,568),surface(632,568)]
 m.w(0x88731c,m.original[0]);m.w(0x8872fc,m.original[1]);m.w(0x887314,m.original[0])
 hook(0x7c8e17,lambda:m.ret(m.alloc(arg(0)),0))
 def construct():
  ptr=this();surface(arg(0),arg(1),ptr);m.born.append(ptr);m.ret(ptr,4)
 hook(0x4ba5a0,construct)
 def geometry():
  rect=bytes(m.u.mem_read(arg(0),16));m.layout.append(struct.unpack('<4i',rect));m.u.mem_write(0x886fa0,rect);m.u.mem_write(0xb0ce28,rect);m.ret(0,1)
 hook(0x4a8960,geometry);hook(0x6d5f60,lambda:m.ret(0,1))
 def dirty():m.dirty.append((this(),tuple(arg(i) for i in range(4)),arg(4)));m.ret(0,5)
 m.dirty_hook=hook(0x6d2790,dirty)
 hook(0x4f42f0,lambda:(m.redraw.append((this(),arg(0))),m.ret(0,1)))
 # Preserve the real Render prologue and epilogue. Replace only the native
 # drawing middle with a jump to its verified final instruction boundary.
 hook(0x4f4492,lambda:m.u.reg_write(UC_X86_REG_EIP,0x4f459d))
 oldrun=m.run
 def run(address,args=(),self=0):
  if address==m.entry and args==(0,1,0):m.w(m.CONTROL+20,m.r(m.CONTROL+20)|1);args=(0,1,m.CONTROL+0x100)
  sp=m.STACK+0x1e000;m.u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),m.STOP,*args));m.u.reg_write(UC_X86_REG_ESP,sp);m.u.reg_write(UC_X86_REG_ECX,self)
  m.u.emu_start(address,m.STOP,count=20000000)
  assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP,'instruction budget'
  assert m.u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args),'stack imbalance'
 m.run=run;m.hook()
 s=state(m);s.sidebarMode=2;save(m,s);m.hook();return m
def render(m):m.run(0x4f4480,(),0x87f7e8)
def active(m):render(m);m.hook();assert state(m).sidebarStatus==1,(state(m).sidebarStatus,m.layout)
def zoom_renderer(m):
 m.zoom_resizes=[];m.zoom_enables=0
 m.w(0x887640,m.alloc());m.w(0x887308,m.original[0]);m.w(0xb73550,1);m.b(0x89f978,0)
 dd=m.alloc();vt=m.alloc();m.w(0x8a0094,dd);m.w(dd,vt);m.w(vt+0x54,0x560bf0)
 def add(fn):
  at=m.alloc(16);m.add_hook(at,fn);return at
 def enable():m.zoom_enables+=1;m.ret(0,0)
 enable_at=add(enable)
 rect_at=add(lambda:(m.u.mem_write(m.arg(1),struct.pack('<4i',0,0,800,600)),m.ret(1,2)))
 cursor_at=add(lambda:(m.u.mem_write(m.arg(0),struct.pack('<2i',100,100)),m.ret(1,1)))
 client_at=add(lambda:m.ret(1,2))
 resolve_at=add(lambda:m.ret({'GetClientRect':rect_at,'GetCursorPos':cursor_at,'ScreenToClient':client_at}.get(m.string_at(m.arg(1)),0),2))
 proc_at=add(lambda:m.ret({'DDEnableZoom':enable_at,'DDGetProcAddress':resolve_at}.get(m.string_at(m.arg(1)),0),2))
 for entry in m.dll.DIRECTORY_ENTRY_IMPORT:
  for imp in entry.imports:
   if imp.name==b'GetProcAddress':m.w(imp.address,proc_at)
 def resize():
  w=m.u.reg_read(UC_X86_REG_ECX);h=m.u.reg_read(UC_X86_REG_EDX);m.zoom_resizes.append((w,h));m.w(0x8a00a4,w);m.w(0x8a00a8,h);m.ret(1,0)
 m.add_hook(0x560bf0,resize)
def leave_at(m,start,end,regs=None):
 # Stop at the fixed continuation before it executes. This inspects actual
 # displaced-register and stack results, without pretending to run its body.
 sp=m.STACK+0x1d000;m.u.mem_write(sp,struct.pack('<8I',*[m.STOP]*8));m.u.reg_write(UC_X86_REG_ESP,sp)
 for reg,value in (regs or {}).items():m.u.reg_write(reg,value)
 flag=m.u.reg_read(UC_X86_REG_EFLAGS)
 stop=m.add_hook(end,lambda:m.u.emu_stop())
 m.u.emu_start(start,m.STOP,count=2000000);m.u.hook_del(stop)
 assert m.u.reg_read(UC_X86_REG_EIP)==end
 return sp,flag

def ownership():
 m=fixture();assert state(m).sidebarStatus==2
 assert [at for at,n in sites if bytes(m.u.mem_read(at,n))!=m.before[at]]==[0x4f4480,0x4f459d]
 m.hook();assert not m.born and state(m).sidebarStatus==2
 active(m);owned=[m.r(0x88731c),m.r(0x8872fc)];assert owned==m.born and m.layout[-1]==(0,0,800,568)
 assert m.r(0x886f90)==800
 assert all(bytes(m.u.mem_read(at,n))!=m.before[at] for at,n in sites)
 s=state(m);s.sidebarMode=0;save(m,s);m.hook()
 assert state(m).sidebarStatus==0 and m.layout[-1]==(0,0,632,568)
 assert [m.r(0x88731c),m.r(0x8872fc)]==m.original and set(m.freed)==set(owned)
 assert all(bytes(m.u.mem_read(at,n))==m.before[at]for at,n in sites)
check('complete-group-waits-render-expands-and-restores-owned-pair',ownership)

def refresh_after_geometry():
 m=fixture();assert not m.dirty and not m.redraw
 active(m);assert m.dirty==[(m.r(0x887324),(0,0,800,600),1)] and m.redraw==[(0x87f7e8,2)]
 paints=[row for row in m.posted_messages if row[1]==15];assert paints==[(1,15,0,0)],paints
 # Repeated wakes in one simulation frame do not repeat the whole-area work;
 # the next two observed frames cover the native incremental double buffers.
 m.hook();assert len(m.dirty)==1
 for _ in range(2):m.w(0xa8ed84,m.r(0xa8ed84)+1);m.hook()
 assert len(m.dirty)==3 and len(m.redraw)==3
 m.w(0xa8ed84,m.r(0xa8ed84)+1);m.hook();assert len(m.dirty)==3
 assert [row for row in m.posted_messages if row[1]==15]==paints
 s=state(m);s.sidebarMode=0;save(m,s);m.hook()
 assert m.dirty[-1]==(m.r(0x887324),(0,0,800,600),1)
 assert len([row for row in m.posted_messages if row[1]==15])==2
check('geometry-immediately-dirties-native-map-marks-redraw-and-posts-one-nonreentrant-paint',refresh_after_geometry)

def paused_render_refresh():
 m=fixture();active(m);frame=m.r(0xa8ed84);assert len(m.dirty)==1
 m.hook();assert len(m.dirty)==1
 for _ in range(2):render(m);m.hook()
 assert len(m.dirty)==3 and m.r(0xa8ed84)==frame
 render(m);m.hook();assert len(m.dirty)==3
 assert len([row for row in m.posted_messages if row[1]==15])==1
check('paused-simulation-complete-render-serial-retries-dirty-without-same-wake-work',paused_render_refresh)

def native_dirty_abi():
 m=fixture();m.u.hook_del(m.dirty_hook);storage=m.alloc(20*16)
 m.w(0xb0ce7c,storage);m.w(0xb0ce80,16);m.w(0xb0ce88,0)
 oldframe=m.r(0xa8ed84);unit=bytes(m.u.mem_read(m.unit,0x800))
 # Actual private native function, not a substitute: ECX plus Rect-by-value
 # and bool on the stack; the verified RET 0x14 must balance all five words.
 m.run(0x6d2790,(0,0,800,600,1),m.r(0x887324))
 assert m.r(0xb0ce88)==1
 assert struct.unpack('<4iB',m.u.mem_read(storage,17))==(0,0,800,600,1)
 assert m.r(0xa8ed84)==oldframe and bytes(m.u.mem_read(m.unit,0x800))==unit
check('actual-native-dirty-area-by-value-bool-and-ret20-ABI',native_dirty_abi)

def redraw_signature():
 m=fixture();m.u.mem_write(0x6d2790,b'\x90');render(m);m.hook()
 assert state(m).sidebarStatus==-1 and not m.born and not m.layout and not m.dirty and not m.redraw
check('changed-native-redraw-ABI-refused-before-surface-allocation',redraw_signature)

def deferred():
 m=fixture();active(m);owned=[m.r(0x88731c),m.r(0x8872fc)]
 leave_at(m,0x4f4480,0x4f4488,{UC_X86_REG_ECX:0x87f7e8})
 s=state(m);s.sidebarMode=0;save(m,s);m.hook()
 assert state(m).sidebarStatus==2 and not m.freed and [m.r(0x88731c),m.r(0x8872fc)]==owned
 leave_at(m,0x4f459d,0x4f45a3,{UC_X86_REG_EBP:owned[0]})
 m.hook();assert state(m).sidebarStatus==0 and set(m.freed)==set(owned)
check('native-render-context-rejects-message-mode-flip-and-retirement',deferred)

def foreign_while_enabled():
 m=fixture();active(m)
 replacement=m.surface(632,568);m.w(0x88731c,replacement)
 expected=[m.r(0x88731c),m.r(0x8872fc)]
 m.hook()
 assert state(m).sidebarStatus==-2 and not m.freed
 assert [m.r(0x88731c),m.r(0x8872fc)]==expected
 assert all(bytes(m.u.mem_read(at,n))==m.before[at]for at,n in sites)
 render(m);m.hook();assert state(m).sidebarStatus==-2 and not m.freed
 s=state(m);s.sidebarMode=0;save(m,s);m.hook();assert state(m).sidebarStatus==0
check('active-foreign-pair-retires-owned-patches-and-requires-explicit-retry',foreign_while_enabled)

for at,n in [(0x4f4480,8),(0x4f459d,6),(0x65386b,6)]:
 def lost_tracking(at=at,n=n):
  m=fixture();active(m);owned=[m.r(0x88731c),m.r(0x8872fc)];layouts=len(m.layout)
  # A platform restores its original entry in an unobserved render generation.
  m.u.mem_write(at,m.before[at]);m.hook()
  assert state(m).sidebarStatus==-2 and not m.freed
  assert len(m.layout)==layouts and [m.r(0x88731c),m.r(0x8872fc)]==owned
  zoom_renderer(m);s=state(m);s.viewZoomPercent=50;save(m,s);m.hook()
  assert not m.zoom_resizes and not m.zoom_enables and not m.freed
  s=state(m);s.sidebarMode=0;save(m,s);m.hook();assert state(m).sidebarStatus==-2 and not m.freed
  s=state(m);s.stop=1;save(m,s);m.hook()
  assert state(m).stop==1 and not m.freed and [m.r(0x88731c),m.r(0x8872fc)]==owned
  assert bytes(m.u.mem_read(at,n))==m.before[at]
 check('foreign-hook-holds-off-zoom-and-stop-cleanup-'+hex(at),lost_tracking)

for borrowed in (False,True):
 def changed_geometry(borrowed=borrowed):
  m=fixture()
  if borrowed:
   for ptr in m.original:m.surfaces[ptr]['w']=800;m.surfaces[ptr]['h']=600
  active(m);pair=[m.r(0x88731c),m.r(0x8872fc)]
  if borrowed:assert not m.born and pair==m.original
  m.w(0x886fb8,1024);m.w(0x886fa8,1024);layouts=len(m.layout)
  m.hook();assert state(m).sidebarStatus==-2 and not m.freed and len(m.layout)==layouts
  s=state(m);s.sidebarMode=0;save(m,s);m.hook()
  assert state(m).sidebarStatus==-2 and not m.freed and len(m.layout)==layouts
  assert m.r(0x886fa8)==1024 and [m.r(0x88731c),m.r(0x8872fc)]==pair
 check('foreign-window-geometry-preserved-'+str(borrowed),changed_geometry)

for boundary in ('off','reset','stop'):
 def native_failure(boundary=boundary):
  m=fixture();active(m);pair=[m.r(0x88731c),m.r(0x8872fc)];layouts=len(m.layout)
  s=state(m);s.viewZoomBusy=1;save(m,s)
  failed=function_address(m.dll,Path(a.dll).with_suffix('.pdb'),b'_AssistSidebarViewFailed')
  m.run(failed)
  assert state(m).viewZoomBusy==0 and state(m).sidebarStatus==-3
  if boundary=='off':s=state(m);s.sidebarMode=0;save(m,s)
  if boundary=='stop':s=state(m);s.stop=1;save(m,s)
  if boundary=='reset':m.w(0xa8b230,m.alloc())
  m.hook()
  assert not m.freed and len(m.layout)==layouts and [m.r(0x88731c),m.r(0x8872fc)]==pair
  assert m.r(0x886fa8)==800 and state(m).viewZoomBusy==0
 check('modeled-native-failure-clears-busy-and-holds-wide-pair-'+boundary,native_failure)

for changed in (0,1,2):
 def foreign(changed=changed):
  m=fixture();active(m);owned=[m.r(0x88731c),m.r(0x8872fc)]
  replacement=m.surface(800,600)
  if changed in (0,2):m.w(0x88731c,replacement)
  if changed in (1,2):m.w(0x8872fc,replacement)
  expected=[m.r(0x88731c),m.r(0x8872fc)]
  s=state(m);s.sidebarMode=0;save(m,s);m.hook()
  assert [m.r(0x88731c),m.r(0x8872fc)]==expected and not m.freed
 check('foreign-pair-not-freed-or-overwritten-'+str(changed),foreign)

def replay():
 m=fixture();active(m);regs={UC_X86_REG_EAX:0x11223344,UC_X86_REG_EBX:0x22334455,UC_X86_REG_ECX:0x33445566,UC_X86_REG_EDX:0x44556677,UC_X86_REG_ESI:0x55667788,UC_X86_REG_EDI:0x66778899,UC_X86_REG_EBP:0x778899aa}
 for at,end,subtract,pushes in [(0x6abd30,0x6abd35,36,[UC_X86_REG_EBX,UC_X86_REG_EBP]),(0x4e1640,0x4e1646,12,[UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI]),(0x692406,0x69240c,0,[UC_X86_REG_ECX]),(0x4f4480,0x4f4488,8,[])]:
  sp,flag=leave_at(m,at,end,regs);assert m.u.reg_read(UC_X86_REG_ESP)==sp-subtract-4*len(pushes)
  for i,r in enumerate(pushes):assert m.r(sp-subtract-4*(i+1))==regs[r]
  expected=dict(regs)
  if at==0x692406:expected[UC_X86_REG_ECX]=0x87f7e8
  if at==0x4f4480:expected[UC_X86_REG_EAX]=m.r(0x88731c)
  assert all(m.u.reg_read(r)==v for r,v in expected.items()),hex(at)
 # Finish the synthetic in-flight begin before the exit replay assertion.
 sp,flag=leave_at(m,0x4f459d,0x4f45a3,regs)
 assert m.u.reg_read(UC_X86_REG_ESP)==sp and m.r(0x887314)==regs[UC_X86_REG_EBP]
 assert all(m.u.reg_read(r)==v for r,v in regs.items()) and m.u.reg_read(UC_X86_REG_EFLAGS)==flag
 m.run(0x6a70e0,(1,));assert m.u.reg_read(UC_X86_REG_EAX)==regs[UC_X86_REG_EAX]
check('six-runtime-adapters-register-stack-and-continuation-replay',replay)

def oversized_clear():
 m=fixture();calls={}
 def height(ptr,real):
  if ptr in m.born:
   calls[ptr]=calls.get(ptr,0)+1
   if calls[ptr]>1:return 0x7fffffff
  return real
 m.height_override=height
 render(m);m.hook()
 assert state(m).sidebarStatus==-1
 assert len(m.born)==2 and set(m.freed)==set(m.born)
 assert [m.r(0x88731c),m.r(0x8872fc)]==m.original
 assert all(bytes(m.u.mem_read(at,n))==m.before[at]for at,n in sites)
check('surface-height-overflow-refused-before-clear-and-owned-allocation-rolled-back',oversized_clear)

def foreign_method():
 m=fixture();vt=m.r(m.original[0]);m.w(vt+0x7c,m.HEAP+0x100)
 render(m);m.hook()
 assert state(m).sidebarStatus==-1
 assert [m.r(0x88731c),m.r(0x8872fc)]==m.original
 assert all(bytes(m.u.mem_read(at,n))==m.before[at]for at,n in sites)
check('non-image-surface-method-never-called',foreign_method)

def constants():
 m=fixture();active(m)
 for at,n in sites[:7]:
  original=m.before[at];m.u.reg_write(UC_X86_REG_EAX,1000);m.u.reg_write(UC_X86_REG_EBP,168);m.u.reg_write(UC_X86_REG_ECX,0);m.u.reg_write(UC_X86_REG_EDX,1000);m.w(0x886f90,800);m.w(0x886fa8,632)
  sp=m.STACK+0x1d000;m.u.reg_write(UC_X86_REG_ESP,sp);m.u.emu_start(at,at+n,count=20)
  assert m.u.reg_read(UC_X86_REG_EIP)==at+n and m.u.reg_read(UC_X86_REG_ESP)==sp
  out={0x72ad60:(UC_X86_REG_ECX,0),0x72ad67:(UC_X86_REG_EDX,1000),0x65386b:(UC_X86_REG_EDX,800),0x654332:(UC_X86_REG_ECX,800),0x653a3f:(UC_X86_REG_EBX,800),0x6d06c7:(UC_X86_REG_EDI,800),0x692bb4:(UC_X86_REG_EBP,999)}[at]
  assert m.u.reg_read(out[0])==out[1],hex(at)
check('seven-constant-patches-execute-exact-instruction-semantics',constants)

def nested_pause():
 m=fixture();active(m)
 pause=function_address(m.dll,Path(a.dll).with_suffix('.pdb'),b'_GuardedPauseViewNative')
 # cdecl: stop before the caller cleanup and check the four args remain.
 sp=m.STACK+0x1e000;m.u.mem_write(sp,struct.pack('<5I',m.STOP,2,0,0,0));m.u.reg_write(UC_X86_REG_ESP,sp)
 m.u.emu_start(pause,m.STOP,count=2000000)
 assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP and m.u.reg_read(UC_X86_REG_ESP)==sp+4
 assert state(m).sidebarStatus==1
 s=state(m);s.sidebarMode=0;save(m,s);m.hook();assert state(m).sidebarStatus==0
check('pause-native-draw-capture-nests-both-render-adapters',nested_pause)

def bad_context():
 m=fixture();m.u.mem_write(0x6a70e6,b'\x90');render(m);m.hook()
 assert state(m).sidebarStatus==-2 and not m.born
 assert all(bytes(m.u.mem_read(at,n))==m.before[at]for at,n in sites)
check('surrounding-native-instruction-mismatch-rolls-back-group',bad_context)

result={'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'passed':all(r['status']=='PASS'for r in rows),'checks':len(rows),'sites':len(sites),'results':rows,'game_tested':False,'multiplayer_tested':False}
Path(a.output).write_text(json.dumps(result,indent=2)+'\n');print(('PASS'if result['passed']else'FAIL')+': thirteen-site sidebar '+str(len(rows))+' cases');raise SystemExit(not result['passed'])

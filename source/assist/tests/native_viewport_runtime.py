"""Compiled map-only viewport dispatcher with synthetic Win32/graphics primitives.

Executes actual original Render prologue/phase order/epilogue and all compiled
adapters. Native rendering, DSurface/Z/A allocation and presentation are modeled.
No game process, Windows visual output, GPU correctness or netplay is claimed.
"""
import argparse,ctypes as C,gc,hashlib,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from pdb_fixture_symbol import function_address
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--case-filter',default='');a=p.parse_args();rows=[]
initial_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();pdb_path=Path(a.dll).with_suffix('.pdb');initial_pdb_hash=hashlib.sha256(pdb_path.read_bytes()).hexdigest();initial_game_hash=hashlib.sha256(Path(a.game).read_bytes()).hexdigest()
repo=Path(__file__).resolve().parents[3]
source_paths=['source/assist/tests/native_viewport_runtime.py','source/assist/tests/sidebar_view_runtime.py','source/assist/tests/pdb_fixture_symbol.py','source/assist/source/view_zoom_native_state.h','source/assist/source/view_zoom_native_bridge.h','source/assist/source/view_zoom_native_guard.h','source/assist/source/view_zoom_bridge.h','source/assist/source/view_zoom_legacy_bridge.h','source/assist/source/native_viewport_query.h','source/assist/source/sidebar_view_bridge.h','source/assist/source/sidebar_view_guard.h','source/shared/source/native_viewport_policy.h','source/shared/source/native_viewport_projection.h','source/assist/source/protocol.h']
source_hashes={n:hashlib.sha256((repo/n).read_bytes()).hexdigest()for n in source_paths}
provider_path=Path(__file__).with_name('sidebar_view_runtime.py');provider_source=provider_path.read_text(encoding='utf-8').split('def ownership():',1)[0]
provider_source=provider_source.replace('p.parse_args()','p.parse_known_args()[0]')
provider_source=provider_source.replace('hook(0x4a8960,geometry);hook(0x6d5f60,lambda:m.ret(0,1))','m.native_geometry_hook=hook(0x4a8960,geometry);m.native_tactical_hook=hook(0x6d5f60,lambda:m.ret(0,1))')
provider_source=provider_source.replace('hook(0x4f4492,lambda:m.u.reg_write','m.middle_render_hook=hook(0x4f4492,lambda:m.u.reg_write')
provider={'__file__':str(provider_path),'__name__':'viewport_provider'};exec(compile(provider_source,str(provider_path),'exec'),provider)
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def check(name,fn):
 if a.case_filter and a.case_filter not in name:return
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()
 finally:gc.collect()
def fixture(mode=0):
 m=provider['fixture']();m.guard_native=function_address(m.dll,pdb_path,b'_GuardedNativeViewAction');m.u.mem_map(m.HEAP+0x400000,0x4000000)
 m.u.hook_del(m.middle_render_hook);m.u.hook_del(m.native_geometry_hook);m.u.hook_del(m.native_tactical_hook)
 m.buffers={};m.buffer_freed=[];m.trace=[];m.presented=0;m.skip_map=False;m.swap_raster=True;m.uniform=0x1a2b
 for at,value in ((0x87f8dc,200),(0x87f8e4,2),(0x87f8e8,4),(0x87f8ec,190),(0x87f8f0,190)):m.w(at,value)
 m.tactical=m.r(0x887324)
 for off,value in ((0xd64,-180),(0xd68,5962),(0xd74,-180),(0xd78,5962),(0xb0,-496),(0xb4,5678)):m.w(m.tactical+off,value)
 def this():return m.u.reg_read(UC_X86_REG_ECX)
 def buffer(w,h,pointer=None):
  pointer=m.alloc(0x30) if pointer is None else pointer;surface=m.surface(w,h);bits=m.surfaces[surface]['bits']
  for off,value in ((0x28,w),(0x2c,h),(0x14,surface),(0x18,bits),(0x1c,bits+w*h*2)):m.w(pointer+off,value)
  m.buffers[pointer]={'w':w,'h':h,'surface':surface};return pointer
 m.buffer=buffer;m.w(0x887644,buffer(632,568));m.w(0x87e8a4,buffer(632,568))
 def ctor():buffer(m.arg(2),m.arg(3),this());m.ret(this(),4)
 m.add_hook(0x7bc970,ctor);m.add_hook(0x410ce0,ctor)
 def dtor():m.buffer_freed.append(this());m.ret(0,0)
 m.add_hook(0x7bcae0,dtor);m.add_hook(0x410e50,dtor);m.add_hook(0x7c8b3d,lambda:m.ret(0,0))
 def geometry():
  rect=tuple(struct.unpack('<4i',m.u.mem_read(m.arg(0),16)));m.layout.append(rect);m.u.mem_write(0x886fa0,struct.pack('<4i',*rect));m.u.mem_write(0xb0ce28,struct.pack('<4i',*rect))
  m.w(0x887644,buffer(rect[2],rect[3]));m.w(0x87e8a4,buffer(rect[2],rect[3]));m.ret(0,1)
 m.add_hook(0x4a8960,geometry)
 def viewport():
  rect=tuple(struct.unpack('<4i',m.u.mem_read(m.arg(0),16)));m.u.mem_write(0xb0ce28,struct.pack('<4i',*rect))
  x=C.c_int32(m.r(m.tactical+0xd74)).value;y=C.c_int32(m.r(m.tactical+0xd78)).value
  m.w(m.tactical+0xd64,x);m.w(m.tactical+0xd68,y);m.w(m.tactical+0xb0,x-rect[2]//2);m.w(m.tactical+0xb4,y-rect[3]//2);m.ret(0,1)
 m.add_hook(0x6d5f60,viewport)
 m.mouse=m.alloc(128);m.w(m.mouse,0x7f7b2c);m.w(m.mouse+0x1c,316);m.w(m.mouse+0x20,284);m.w(0x887640,m.mouse)
 def erase():m.trace.append(('cursor-erase',m.arg(0)));m.ret(0,2)
 def mouse_draw():m.trace.append(('cursor-draw',m.arg(0)));m.ret(0,2)
 m.add_hook(0x7b92d0,erase);m.add_hook(0x7b90c0,mouse_draw)
 screen_table=m.alloc();m.w(0x87f7e8,screen_table)
 def chrome():m.trace.append(('chrome',m.r(0x88731c),m.r(0x887644)));m.ret(0,1)
 def present():m.trace.append(('present',m.r(0x88731c),m.r(0x887644)));m.presented+=1;m.ret(0,0)
 m.w(screen_table+0x40,0x7100c0);m.w(screen_table+0x44,0x7100d0);m.add_hook(0x7100c0,chrome);m.add_hook(0x7100d0,present)
 m.add_hook(0x53bae0,lambda:m.ret(int(m.skip_map),0))
 def render_map():
  surface,_,phase=(m.arg(i) for i in range(3));m.trace.append(('map',phase,surface,m.r(0x887644),m.r(0x87e8a4)))
  info=m.surfaces[surface];m.u.mem_write(info['bits'],struct.pack('<H',m.uniform)*(info['w']*info['h']))
  if phase==1 and m.swap_raster:
   first,second=m.r(0x88731c),m.r(0x8872fc);m.w(0x88731c,second);m.w(0x8872fc,first)
  m.ret(0,3)
 m.add_hook(0x6d3d10,render_map)
 # Original chat/tooltip optional singleton pointers remain absent; their
 # explicit draw stubs do not provide an alternative render scheduler.
 m.add_hook(0x5d49a0,lambda:m.ret(0,0));m.w(0xa8ef54,0);m.w(0x887368,0);m.b(0xa8b8b4,0)
 # No Sidebar surface is necessary to prove A-only destination bounds; a
 # separate sidebar fixture covers CopyFromPart and the native UI machinery.
 m.w(0x887300,0)
 s=state(m);s.sidebarMode=mode;s.viewportHostCapabilities=0;s.viewZoomPercent=100;save(m,s);m.hook();return m
def render(m):m.trace=[];m.run(0x4f4480,(),0x87f7e8);return m.trace[:]
def activate(m,percent=50):
 s=state(m);s.viewZoomPercent=percent;s.viewportHostCapabilities=3;save(m,s)
 for _ in range(5):m.hook();render(m)
 s=m.hook();assert s.viewportActive==1 and s.viewZoomStatus==1 and s.viewZoomApplied==percent,(s.viewportActive,s.viewZoomStatus,s.sidebarStatus,m.layout)
 return s
def basic():
 m=fixture();s=activate(m);assert tuple(s.viewportPhysical)==(0,0,632,568) and tuple(s.viewportLogical)==(0,0,1264,1136)
 trace=render(m);assert [row[1]for row in trace if row[0]=='map']==[0,1,2],trace
 assert [row[0]for row in trace]==['cursor-erase','map','map','chrome','map','cursor-draw','present'],trace
 windows={m.r(0x88731c),m.r(0x8872fc)}
 assert all(row[2]not in windows for row in trace if row[0]=='map')
 assert all(row[1]in windows for row in trace if row[0]in ('cursor-erase','cursor-draw','present','chrome'))
 target=m.surfaces[m.r(0x88731c)];assert struct.unpack('<H',m.u.mem_read(target['bits'],2))[0]==m.uniform
 out=m.alloc();m.run(m.r(0x7f7b60),(out,),m.mouse);assert (m.r(out),m.r(out+4))==(632,568)
 before=state(m).viewportGeneration;s=state(m);s.viewZoomPercent=100;save(m,s);m.hook();m.hook();s=state(m)
 assert not s.viewportActive and s.viewportGeneration>before and s.viewZoomStatus==0
 return {'physical_UI_native_raster_separated':True,'map_three_passes_then_window_UI_and_present':True,'native_mouse_one_transform':True,'same_thread_owned_surface_swap_and_restore':True}
check('compiled-native-map-only-render-and-roundtrip',basic)
def set_option(m,**kwargs):
 s=state(m)
 for key,value in kwargs.items():setattr(s,key,value)
 save(m,s)
def native_op(m,op):
 stub=m.alloc(32);code=bytearray(b'\x6a\x00\x68'+struct.pack('<I',op)+b'\xe8'+struct.pack('<I',(m.guard_native-(stub+12))&0xffffffff)+b'\x83\xc4\x08\xc3');m.u.mem_write(stub,bytes(code));m.run(stub);return m.u.reg_read(UC_X86_REG_EAX)
def pumps(m,count=3):
 for _ in range(count):m.hook()
 return state(m)
def getters(m):
 out=m.alloc();m.run(m.r(0x7f7b60),(out,),m.mouse);return tuple(C.c_int32(m.r(out+i*4)).value for i in range(2))
def fresh_handshake():
 m=fixture();provider['zoom_renderer'](m);m.w(0x887640,m.mouse);set_option(m,viewZoomPercent=75,viewportHostCapabilities=0x80000000)
 pumps(m);s=state(m);assert s.viewportSerial and not s.viewportSerial&1 and s.viewportGeneration and not s.viewportActive
 assert not m.zoom_resizes and not m.zoom_enables
 set_option(m,viewportHostCapabilities=0x80000003);activate(m,75)
 assert not m.zoom_resizes and not m.zoom_enables
 return {'fresh_publication_not_memset_zero':True,'no_transient_legacy_resize_before_host_ack':True}
check('fresh-host-handshake-publishes-real-inactive-and-waits-before-native',fresh_handshake)
def reconnect():
 m=fixture();s=activate(m);generation=s.viewportGeneration;physical=tuple(s.viewportPhysical);logical=tuple(s.viewportLogical)
 set_option(m,viewportSerial=0,viewportGeneration=0,viewportActive=0)
 s=m.hook();assert s.viewportSerial and not s.viewportSerial&1 and s.viewportActive==1 and s.viewportGeneration==generation
 assert tuple(s.viewportPhysical)==physical and tuple(s.viewportLogical)==logical
 render(m);assert state(m).viewportActive and len(m.trace)==7
 return {'private_active_transform_republished_without_new_generation':True}
check('active-private-state-republished-into-cleared-wire-with-same-generation',reconnect)
for percent in (67,75,80,125,150,200,300,400):
 def level(percent=percent):
  m=fixture();s=activate(m,percent);a=tuple(s.viewportPhysical);r=tuple(s.viewportLogical)
  expected=((a[2]*3+1)//2,(a[3]*3+1)//2)if percent==67 else ((a[2]*100+percent//2)//percent,(a[3]*100+percent//2)//percent)
  assert r==(0,0,*expected)
  assert getters(m)==(r[2]//2,r[3]//2)
  assert [x[1]for x in render(m)if x[0]=='map']==[0,1,2]
 check('native-percent-'+str(percent),level)
for mode,left in ((0,True),(1,False),(1,True),(2,False),(2,True)):
 def physical_layout(mode=mode,left=left):
  m=fixture(mode);m.b(0xa8eb7c,0 if left else 1);s=activate(m,75)
  assert tuple(s.viewportPhysical)==((0,0,800,568)if mode else (168,0,632,568))
  assert s.sidebarStatus==(3 if mode==1 else 1 if mode==2 else 0)
  assert tuple(s.viewportLogical)[:2]==(0,0)
  assert [x[1]for x in render(m)if x[0]=='map']==[0,1,2]
  set_option(m,viewZoomPercent=100,sidebarMode=0);pumps(m);assert not state(m).viewportActive and state(m).sidebarStatus==0
 check('sidebar-mode-'+str(mode)+'-left-'+str(left),physical_layout)
def fit_map():
 m=fixture();m.w(0x87f8ec,20);m.w(0x87f8f0,10);s=activate(m,0)
 assert tuple(s.viewportLogical)==(0,0,1200,1078)
 assert s.smallMapClampStatus==1 # shorter map axis is automatically pinned
 assert [x[1]for x in render(m)if x[0]=='map']==[0,1,2]
 set_option(m,viewZoomPercent=100);pumps(m);assert not state(m).viewportActive
 return {'dynamic_map_fit':True,'clamp_dependency_without_independent_option':True}
check('MAX-current-map-fit-and-small-axis-dependency',fit_map)
def retained_clamp():
 m=fixture();m.w(0x87f8ec,20);m.w(0x87f8f0,10);activate(m,50)
 assert bytes(m.u.mem_read(0x6d8640,1))==b'\xe9'
 m.u.mem_write(0x4f44cb,m.game.get_data(0x4f44cb-0x400000,5));set_option(m,stop=1);pumps(m,3)
 assert state(m).viewportActive and bytes(m.u.mem_read(0x6d8640,1))==b'\xe9'
 point=m.alloc();m.u.mem_write(point,struct.pack('<2i',100000,100000));m.run(0x6d8640,(point,))
 x,y=struct.unpack('<2i',m.u.mem_read(point,8));assert x==-5280 and y==3262,(x,y)
 return {'retained_raster_retains_camera_dependency':True,'denied_capture_keeps_pure_degenerate_axis_fix':True}
check('clamp-dependency-survives-stop-with-foreign-render-owner',retained_clamp)
for mode in (0,1):
 def native_widgets(mode=mode):
  m=fixture(mode);m.w(0x887300,m.surface(168,568));m.w(0x886f94,158);s=activate(m,50)
  m.w(m.mouse+0x1c,750);m.w(m.mouse+0x20,60);assert getters(m)==(750,60)
  m.w(m.mouse+0x20,300)
  expected=(750,300)if mode==0 else (int(750*s.viewportLogical[2]/s.viewportPhysical[2]),int(300*s.viewportLogical[3]/s.viewportPhysical[3]))
  assert getters(m)==expected
  return {'native_widget_coordinates_stay_physical':True,'radar_only_lower_column_is_map':mode==1}
 check('native-sidebar-widget-coordinate-mode-'+str(mode),native_widgets)
for reason in ('focus','paused','observer'):
 def suspended(reason=reason):
  m=fixture();s=activate(m);generation=s.viewportGeneration;pair=(m.r(0x88731c),m.r(0x8872fc));layouts=len(m.layout)
  if reason=='focus':set_option(m,foreground=0)
  elif reason=='paused':m.b(0xa8e9a0,0)
  else:m.b(m.house+0x1f5,1)
  pumps(m,6);assert state(m).viewportActive and state(m).viewportGeneration==generation and not m.freed and len(m.layout)==layouts
  assert (m.r(0x88731c),m.r(0x8872fc))==pair
  assert [x[1]for x in render(m)if x[0]=='map']==[0,1,2]
  set_option(m,viewZoomPercent=100);native_op(m,21);pumps(m,6);assert not state(m).viewportActive
 check('native-keeps-shared-render-owner-during-'+reason,suspended)
def cleanup_during_render():
 m=fixture();activate(m);start=m.STACK+0x1d000;m.u.mem_write(start,struct.pack('<I',m.STOP));m.u.reg_write(UC_X86_REG_ESP,start);m.u.reg_write(UC_X86_REG_ECX,0x87f7e8)
 stop=m.add_hook(0x4f451b,lambda:m.u.emu_stop());m.u.emu_start(0x4f4480,m.STOP,count=20000000);m.u.hook_del(stop)
 # Finish map completion (which resamples and swaps out), but stop BEFORE
 # the true Render leave. A Windows callback is still inside native Render.
 stop=m.add_hook(0x4f4520,lambda:m.u.emu_stop());m.u.emu_start(0x4f451b,m.STOP,count=20000000);m.u.hook_del(stop)
 context=m.u.context_save();set_option(m,viewZoomPercent=100);native_op(m,21);assert state(m).viewportActive and not m.freed
 m.u.context_restore(context);m.u.emu_start(0x4f4520,m.STOP,count=20000000)
 native_op(m,21);pumps(m);assert not state(m).viewportActive and m.freed
 return {'map_completion_does_not_end_render_lease':True}
check('off-callback-after-map-before-true-render-leave-retains-all-allocations',cleanup_during_render)
def loading_wait():
 m=fixture();s=activate(m);gen=s.viewportGeneration;pair=(m.r(0x88731c),m.r(0x8872fc));m.w(0xa9fab0,1)
 assert render(m)==[] and state(m).viewportSerial&1 and state(m).viewZoomBusy==1
 native_op(m,21);assert state(m).viewportActive and not m.freed and (m.r(0x88731c),m.r(0x8872fc))==pair
 m.w(0xa9fab0,0);m.hook();assert len(render(m))==7 and state(m).viewportGeneration==gen and not state(m).viewportSerial&1
 return {'loading_is_recoverable_wait_not_permanent_foreign_conflict':True}
check('loading-render-before-next-tick-waits-and-recovers-same-world',loading_wait)
for change in ('scenario','rewind'):
 def reused_generation(change=change):
  m=fixture();activate(m);old_windows={m.r(0x88731c),m.r(0x8872fc)};old_layouts=len(m.layout)
  if change=='scenario':m.w(0xa8b230,m.alloc())
  else:m.w(0xa8ed84,50)
  current=(-90,5800)
  for off,value in ((0xd64,current[0]),(0xd68,current[1]),(0xd74,current[0]),(0xd78,current[1])):m.w(m.tactical+off,value)
  assert render(m)==[]
  assert native_op(m,21)==1 and not state(m).viewportActive
  set_option(m,viewZoomPercent=100);pumps(m)
  assert state(m).sidebarStatus==0 and not (old_windows&set(m.freed)),(m.freed,m.layout,state(m).sidebarStatus)
  assert (C.c_int32(m.r(m.tactical+0xd74)).value,C.c_int32(m.r(m.tactical+0xd78)).value)==current
  assert all(rect==(0,0,632,568)for rect in m.layout[old_layouts:])
  return {'same_addresses_do_not_authorize_old_sidebar_destructor_or_saved_pair_replay':True,'current_centres_preserved':True}
 check('cross-'+change+'-same-pair-addresses-retire-old-journal-with-fresh-current-layout',reused_generation)
def install_new_native_pair(m,width=800,height=600):
 pair=[m.surface(width,height),m.surface(width,height)]
 for at,value in zip((0x88731c,0x8872fc,0x887314),(pair[0],pair[1],pair[0])):m.w(at,value)
 m.w(0x887644,m.buffer(width,height));m.w(0x87e8a4,m.buffer(width,height));area=(0,0,width-168,height-32)
 for at in (0x886fa0,0xb0ce28):m.u.mem_write(at,struct.pack('<4i',*area))
 return set(pair)
def rebuilt_generation():
 m=fixture();activate(m);old={m.r(0x88731c),m.r(0x8872fc)};m.w(0xa8b230,m.alloc());fresh=install_new_native_pair(m)
 assert render(m)==[];set_option(m,viewZoomPercent=100);pumps(m,5)
 assert not state(m).viewportActive and state(m).sidebarStatus==0 and not ((old|fresh)&set(m.freed))
 assert {m.r(0x88731c),m.r(0x8872fc)}==fresh
 return {'complete_new_pair_and_buffers_adopt_only_current_geometry':True}
check('full-new-native-generation-cleans-both-owners-without-old-pair-replay',rebuilt_generation)
def window_generation():
 m=fixture();activate(m);old={m.r(0x88731c),m.r(0x8872fc)};layouts=len(m.layout)
 m.w(0x886fb8,1200);m.w(0x886fbc,800);m.w(0x8a00a4,1200);m.w(0x8a00a8,800)
 assert render(m)==[];assert native_op(m,21)==0 and not m.freed and len(m.layout)==layouts
 fresh=install_new_native_pair(m,1200,800);set_option(m,viewZoomPercent=100);pumps(m,5)
 assert not state(m).viewportActive and state(m).sidebarStatus==0 and not ((old|fresh)&set(m.freed))
 assert all(rect==(0,0,1032,768)for rect in m.layout[layouts:]),m.layout
 return {'no_replay_of_old_A_after_window_changes':True}
check('window-resize-waits-for-adequate-new-native-pair-and-never-replays-old-A',window_generation)
for which in ('surface','buffer','tracking','phase'):
 def foreign(which=which):
  m=fixture();activate(m);layouts=len(m.layout);old={m.r(0x88731c),m.r(0x8872fc)}
  if which=='surface':m.w(0x88731c,m.surface(800,600))
  elif which=='buffer':m.w(0x887644,m.buffer(800,600))
  else:
   site=0x4f4480 if which=='tracking' else 0x4f44cb
   m.u.mem_write(site,m.game.get_data(site-0x400000,8 if which=='tracking' else 5))
  expected=[m.r(at)for at in (0x88731c,0x8872fc,0x887644,0x87e8a4)]
  native_op(m,21);set_option(m,viewZoomPercent=100);pumps(m)
  assert state(m).viewZoomStatus<0 and not m.freed and len(m.layout)==layouts
  assert [m.r(at)for at in (0x88731c,0x8872fc,0x887644,0x87e8a4)]==expected
  return {'foreign_memory_preserved_and_cleanup_held':True}
 check('foreign-'+which+'-holds-cleanup-without-free-or-geometry-write',foreign)

def invalid_target():
 m=fixture();activate(m,125);a=tuple(state(m).viewportPhysical);r=tuple(state(m).viewportLogical);gen=state(m).viewportGeneration
 # Exercise the >8192 R branch without allocating a giant synthetic raster.
 # Window transition is temporary, so the previously valid transform must
 # remain usable when that request is rejected and native geometry is stable.
 m.w(0x886fb8,7680);m.w(0x886fbc,4320);set_option(m,viewZoomPercent=50);native_op(m,20)
 assert state(m).viewportActive and state(m).viewZoomStatus==-3 and tuple(state(m).viewportLogical)==r
 m.w(0x886fb8,800);m.w(0x886fbc,600);assert len(render(m))==7 and getters(m)==(r[2]//2,r[3]//2)
 assert state(m).viewportGeneration==gen and tuple(state(m).viewportPhysical)==a
 return {'oversized_request_does_not_poison_previous_active_Render_or_input':True,'giant_raster_allocation_not_tested':True}
check('invalid-new-raster-target-retains-healthy-previous-render-and-input',invalid_target)
for oldmode in (0,2):
 for original_flag in (0,1):
  def new_gadget(oldmode=oldmode,original_flag=original_flag):
   m=fixture(oldmode);activate(m);g=0xb07e80
   # The previous hide2 journal owns a saved flag from the old Scene. The
   # new Scene reuses the same static Gadget object with its own native flag.
   m.w(0xa8b230,m.alloc());m.b(g+0x1e,original_flag);m.w(0xa8ed84,m.r(0xa8ed84)+1)
   assert render(m)==[];set_option(m,viewZoomPercent=100,sidebarMode=0);native_op(m,21);pumps(m,5)
   assert m.u.mem_read(g+0x1e,1)==bytes([original_flag]),(oldmode,original_flag,m.u.mem_read(g+0x1e,1),state(m).sidebarStatus)
   assert not state(m).viewportActive and state(m).sidebarStatus==0
   return {'new_scene_native_gadget_flag_preserved':original_flag,'old_hidden_mode':oldmode}
  check('new-scene-gadget-flag-'+str(original_flag)+'-old-mode-'+str(oldmode),new_gadget)
GPRS=(UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
FXREGS=(UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_MXCSR,UC_X86_REG_XMM0,UC_X86_REG_XMM1,UC_X86_REG_XMM2,UC_X86_REG_XMM3,UC_X86_REG_XMM4,UC_X86_REG_XMM5,UC_X86_REG_XMM6,UC_X86_REG_XMM7)
def fx(m):return tuple(m.u.reg_read(reg)for reg in FXREGS)
def seed_fx(m):
 m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_MXCSR,0x5f80)
 for i,reg in enumerate(FXREGS[4:]):m.u.reg_write(reg,0x123456789abcdef0fedcba9876543210+i)
def adapter_render_replay():
 m=fixture();activate(m);observed={};pending={};fns=[];seed_fx(m)
 spans=[(0x4f4480,0x4f4488),(0x4f4492,0x4f4497),(0x4f44cb,0x4f44d0),(0x4f44fd,0x4f4502),(0x4f4505,0x4f450b),(0x4f451b,0x4f4520),(0x4f459d,0x4f45a3)]
 for at,end in spans:
  def before(at=at):pending[at]=({reg:m.u.reg_read(reg)for reg in GPRS},fx(m),m.u.reg_read(UC_X86_REG_EFLAGS))
  def after(at=at):
   before,fp,flags=pending[at];after={reg:m.u.reg_read(reg)for reg in GPRS};expected=before.copy()
   if at==0x4f4480:expected[UC_X86_REG_ESP]-=8;expected[UC_X86_REG_EAX]=m.r(0x88731c)
   elif at==0x4f4492:expected[UC_X86_REG_EAX]=m.r(0x887314)
   elif at==0x4f44cb:expected[UC_X86_REG_EAX]=m.r(0x88731c)
   elif at==0x4f44fd:expected[UC_X86_REG_ESP]-=4;expected[UC_X86_REG_EDX]=m.r(before[UC_X86_REG_ESI]);expected[UC_X86_REG_ECX]=before[UC_X86_REG_ESI];assert m.r(after[UC_X86_REG_ESP])==before[UC_X86_REG_EAX]
   elif at==0x4f4505:expected[UC_X86_REG_ECX]=m.r(0x88731c)
   elif at==0x4f451b:expected[UC_X86_REG_EAX]=(before[UC_X86_REG_EAX]&0xffffff00)|m.u.mem_read(0xb0b519,1)[0]
   assert after==expected,(hex(at),before,after,expected)
   assert fx(m)==fp,hex(at)
   if at!=0x4f4480:assert m.u.reg_read(UC_X86_REG_EFLAGS)==flags,hex(at)
   observed[hex(at)]={'GPR_stack_displaced_replay':True,'FX_state_preserved':True}
  fns.extend((m.add_hook(at,before),m.add_hook(end,after)))
 render(m)
 assert len(observed)==7,observed
 return {'compiled_sites':observed,'shared_true_Render_leave_also_verified':True}
check('compiled-seven-render-adapters-register-stack-and-FX-replay',adapter_render_replay)
def cdecl(m,name,args=()):
 address=function_address(m.dll,pdb_path,name);stub=m.alloc(128);code=bytearray()
 for value in reversed(args):code+=b'\x68'+struct.pack('<I',value&0xffffffff)
 code+=b'\xe8'+struct.pack('<I',(address-(stub+len(code)+5))&0xffffffff)
 if args:code+=b'\x83\xc4'+bytes([len(args)*4])
 code+=b'\xc3';m.u.mem_write(stub,bytes(code));m.run(stub);return m.u.reg_read(UC_X86_REG_EAX)
def bounded_wnd_stack():
 m=fixture();activate(m);packed=(284<<16)|316;point=m.alloc();m.w(point,packed);mapped=[]
 for i in range(16):
  result=cdecl(m,b'_GuardedNativeViewWndEnter',(point,0x1000+i));assert result not in (0,0xffffffff);assert m.r(result)==((568<<16)|632);mapped.append(result)
 assert len(set(mapped))==16 and cdecl(m,b'_GuardedNativeViewWndEnter',(point,0x9999))==0xffffffff
 assert m.r(point)==packed
 assert native_op(m,21)==0 and not m.freed
 for i in reversed(range(16)):assert cdecl(m,b'_GuardedNativeViewWndLeave')==0x1000+i
 assert native_op(m,21)==1
 return {'bounded_16_frames_nested_LIFO':True,'caller_LPARAM_never_mutated':True}
check('native-Wnd-mapping-nested-depth-limit-LIFO-and-retirement-denial',bounded_wnd_stack)
def wnd_return_adapter():
 m=fixture();activate(m);point=m.alloc();m.w(point,(284<<16)|316);seed_fx(m);initial=fx(m);seen=[]
 def body():
  sp=m.u.reg_read(UC_X86_REG_ESP);slot=sp+0x1c+16;shadow=m.r(slot);assert shadow!=point and m.r(shadow)==((568<<16)|632);assert m.r(point)==((284<<16)|316);seen.append(shadow)
  # Model ONLY native body, retain the actual original prologue and RET16
  # convention, then execute the compiled Runtime return thunk.
  m.u.reg_write(UC_X86_REG_EAX,0xa55a7b23);m.u.reg_write(UC_X86_REG_ESP,sp+0x1c);m.ret(0xa55a7b23,4)
 h=m.add_hook(0x6930a8,body);m.run(0x6930a0,(0x201,0,0,point),0x87f7e8);m.u.hook_del(h)
 assert len(seen)==1 and m.u.reg_read(UC_X86_REG_EAX)==0xa55a7b23 and fx(m)==initial
 assert native_op(m,21)==1
 return {'actual_compiled_Wnd_entry_and_return_thunk_RET16':True,'native_body_modeled':True,'native_return_value_and_FX_preserved':True}
check('compiled-Wnd-entry-and-return-thunk-shadow-LPARAM-and-RET16',wnd_return_adapter)
def manual_adapter_sites():
 m=fixture();activate(m);observed={};point=m.alloc();m.w(point,123);m.w(point+4,456)
 for at,end,prologue in ((0x692b60,0x692b67,40),(0x692300,0x692307,24),(0x6d7840,0x6d7845,80)):
  if at==0x6d7840:assert native_op(m,0)==1
  seed_fx(m);initial=fx(m);sp=m.STACK+0x1c000;m.u.mem_write(sp,struct.pack('<8I',m.STOP,point,0x123,0x234,0x345,0x456,0x567,0x678))
  before={UC_X86_REG_EAX:0x11223344,UC_X86_REG_EBX:0x22334455,UC_X86_REG_ECX:0x87f7e8,UC_X86_REG_EDX:0x33445566,UC_X86_REG_ESI:0x44556677,UC_X86_REG_EDI:0x55667788,UC_X86_REG_EBP:0x66778899,UC_X86_REG_ESP:sp}
  for reg,value in before.items():m.u.reg_write(reg,value)
  m.u.reg_write(UC_X86_REG_EFLAGS,0x602);h=m.add_hook(end,lambda:m.u.emu_stop());m.u.emu_start(at,m.STOP,count=2000000);m.u.hook_del(h)
  assert m.u.reg_read(UC_X86_REG_EIP)==end and m.u.reg_read(UC_X86_REG_ESP)==sp-prologue
  for reg,value in before.items():
   if reg not in (UC_X86_REG_ESP,UC_X86_REG_EAX):assert m.u.reg_read(reg)==value,(hex(at),reg)
  if at==0x692b60:assert (m.r(point),m.r(point+4))==(316,284)
  if at==0x6d7840:assert m.r(0x887314)==m.r(0x88731c)
  if at==0x692300:assert m.u.reg_read(UC_X86_REG_EAX)==0x345
  else:assert m.u.reg_read(UC_X86_REG_EAX)==before[UC_X86_REG_EAX]
  assert m.u.reg_read(UC_X86_REG_EFLAGS)&0x400 and fx(m)==initial
  observed[hex(at)]={'complete_displaced_prologue_stack':prologue,'FX_and_DF_preserved':True}
 # The five immediate operand patches intentionally read physical A, never R.
 for at,end,reg in ((0x478bd1,0x478bd7,UC_X86_REG_EDX),(0x478bd7,0x478bdc,UC_X86_REG_EAX),(0x478c17,0x478c1c,UC_X86_REG_EDI),(0x478e4a,0x478e50,UC_X86_REG_ESI),(0x478e50,0x478e56,UC_X86_REG_EBX)):
  provider['leave_at'](m,at,end);value=m.u.reg_read(reg)
  if at==0x478c17:assert tuple(struct.unpack('<4i',m.u.mem_read(value,16)))==tuple(state(m).viewportPhysical)
  else:assert value==(632 if at in (0x478bd7,0x478e50)else 0)
  observed[hex(at)]={'operand_points_to_physical_A':True}
 # Restore the synthetic in-Render shadow before testing local UI adapters.
 native_op(m,6)
 tooltip=m.alloc();m.w(0x887368,tooltip);processed=[];m.add_hook(0x724ad0,lambda:(processed.append(m.u.reg_read(UC_X86_REG_ECX)),m.ret(0,0)))
 seed_fx(m);initial=fx(m);provider['leave_at'](m,0x6a7089,0x6a7090,{UC_X86_REG_EAX:0x11223344,UC_X86_REG_ECX:tooltip,UC_X86_REG_EDX:0x55667788})
 assert processed==[tooltip] and m.u.reg_read(UC_X86_REG_EAX)==0x11223344 and m.u.reg_read(UC_X86_REG_ECX)==tooltip and m.u.reg_read(UC_X86_REG_EDX)==0x55667788 and fx(m)==initial
 observed['0x6a7089']={'process_tooltip_without_duplicate_native_draw':True,'GPR_FX_preserved':True}
 seed_fx(m);initial=fx(m);out=m.alloc();m.run(m.r(0x7f7b60),(out,),m.mouse);assert (m.r(out),m.r(out+4))==(632,568) and m.u.reg_read(UC_X86_REG_EAX)==out and fx(m)==initial
 observed['0x7f7b60']={'virtual_pointer_returns_mapped_point_RET4_FX_preserved':True}
 return {'compiled_sites':observed}
check('compiled-edge-click-shroud-and-five-physical-A-data-operand-sites',manual_adapter_sites)

def programmatic_query_capture():
 m=fixture();activate(m);point=m.alloc();m.u.mem_write(point,struct.pack('<2i',632,568));outputs=[m.alloc()for _ in range(5)];calls=[]
 def body():
  sp=m.u.reg_read(UC_X86_REG_ESP);calls.append(tuple(m.r(sp+24+4+i*4)for i in range(6)));m.u.reg_write(UC_X86_REG_ESP,sp+24);m.ret(0,6)
 m.add_hook(0x692307,body);m.w(m.mouse+0x1c,799) # Physical UI sidebar, not map.
 token=cdecl(m,b'_BeginAssistCapture');assert token
 try:
  result=cdecl(m,b'_GuardedNativeViewQuery',(point,*outputs));assert result==1 and calls==[(point,*outputs)]
  assert native_op(m,21)==1 # Query depth was finally released.
 finally:cdecl(m,b'_EndAssistCapture',(token,))
 return {'WM_style_Capture_lease_admits_query':True,'no_second_mouse_classification_for_already_mapped_query':True,'native_body_modeled':True}
check('active-programmatic-692300-query-accepts-Capture-lease-and-finally-releases-depth',programmatic_query_capture)
def cap0_retiring():
 m=fixture();provider['zoom_renderer'](m);m.w(0x887640,m.mouse);activate(m);start=m.STACK+0x1d000;m.u.mem_write(start,struct.pack('<I',m.STOP));m.u.reg_write(UC_X86_REG_ESP,start);m.u.reg_write(UC_X86_REG_ECX,0x87f7e8)
 stop=m.add_hook(0x4f4520,lambda:m.u.emu_stop());m.u.emu_start(0x4f4480,m.STOP,count=20000000);m.u.hook_del(stop);context=m.u.context_save()
 set_option(m,viewportHostCapabilities=0);m.hook();assert state(m).viewportActive and not m.zoom_resizes and not m.zoom_enables
 m.u.context_restore(context);m.u.emu_start(0x4f4520,m.STOP,count=20000000);pumps(m,5)
 assert not state(m).viewportActive and m.zoom_resizes and m.zoom_enables==1
 for at,n in ((0x4f4492,5),(0x4f44cb,5),(0x4f44fd,5),(0x4f4505,6),(0x4f451b,5),(0x692b60,7),(0x692300,7),(0x6930a0,8),(0x6d7840,5),(0x6a7089,7),(0x7f7b60,4),(0x478bd1,6),(0x478bd7,5),(0x478c17,5),(0x478e4a,6),(0x478e50,6)):
  assert bytes(m.u.mem_read(at,n))==m.game.get_data(at-0x400000,n),hex(at)
 return {'legacy_starts_only_after_native_true_render_retirement':True}
check('capability-loss-during-native-Render-does-not-start-legacy-until-retired',cap0_retiring)
def scroll_mutex():
 m=fixture();set_option(m,viewportHostCapabilities=3,viewZoomPercent=50,scrollRefreshMode=1,scrollRefreshHz=144);m.hook();assert state(m).scrollRefreshStatus==-2
 activate(m);m.hook();assert state(m).scrollRefreshStatus==-2 and not state(m).scrollRefreshDraws
 token=cdecl(m,b'_GuardedNativeViewWndEnter',(m.mouse+0x1c,0x1234));assert token not in (0,0xffffffff)
 set_option(m,viewZoomPercent=100);m.hook();assert state(m).scrollRefreshStatus==-2 and state(m).viewportActive and not state(m).scrollRefreshDraws
 assert cdecl(m,b'_GuardedNativeViewWndLeave')==0x1234
 return {'SceneRefresh_extra_Render_rejected_during_demand_active_and_retirement':True}
check('native-viewport-explicitly-excludes-extra-scroll-refresh-in-all-ownership-phases',scroll_mutex)
final_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();assert final_hash==initial_hash,'DLL changed during fixture'
assert hashlib.sha256(pdb_path.read_bytes()).hexdigest()==initial_pdb_hash and hashlib.sha256(Path(a.game).read_bytes()).hexdigest()==initial_game_hash
assert source_hashes=={n:hashlib.sha256((repo/n).read_bytes()).hexdigest()for n in source_paths},'source changed during fixture'
result={'scope':__doc__,'dll_sha256':final_hash,'pdb_sha256':initial_pdb_hash,'game_sha256':initial_game_hash,'source_sha256':source_hashes,'hash_stable_during_test':True,'checks':len(rows),'results':rows,'pass':all(r['status']=='PASS'for r in rows),'game_process_tested':False,'multiplayer_tested':False}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');raise SystemExit(not result['pass'])

"""Compiled Runtime small-map opt-in and real camera-entry interception.

The private native image, managed patch writer and new Runtime leaf execute
in x86 emulation; Win32/world inputs are explicit fixtures. No running game
is accessed, and this does not prove visual output or multiplayer behavior.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
FIELDS=(0x87F8DC,0x87F8E4,0x87F8E8,0x87F8EC,0x87F8F0,0x886FA8,0x886FAC)

def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def geometry(m,g):
 for at,v in zip(FIELDS,g):m.w(at,v)
def tick(m):m.w(0xA8ED84,m.r(0xA8ED84)+1);return m.hook()
def fixture():
 spec=importlib.util.spec_from_file_location('camera_runtime_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xA8B230,m.alloc());m.original=bytes(m.u.mem_read(0x6D8640,5));m.hook();m.hook();return m
def invoke(m,point):
 at=m.alloc(8);m.u.mem_write(at,struct.pack('<2i',*point));m.run(0x6D8640,(at,),0x12345678);return struct.unpack('<2i',m.u.mem_read(at,8))
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn()or{})});print('PASS',name,flush=True)
 except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()

def optin():
 m=fixture();g=(200,2,3,20,40,1920,900);geometry(m,g);s=tick(m);assert s.smallMapClamp==0 and s.smallMapClampStatus==0 and bytes(m.u.mem_read(0x6D8640,5))==m.original
 s.smallMapClamp=1;save(m,s);s=tick(m);assert s.smallMapClampStatus==1 and m.u.mem_read(0x6D8640,1)==b'\xe9',(s.smallMapClampStatus,s.data.status,s.data.commandStatus)
 queued=(m.r(0xA802C8),m.r(0x8B41F8));centerx=30*(2*g[1]-g[0])+30*g[3]
 for point in ((-100000,-100000),(100000,100000),(0,0),(centerx,3500)):
  fixed=invoke(m,point);assert fixed[0]==centerx and fixed[1]==min(max(point[1],3465),3900),(point,fixed)
 assert (m.r(0xA802C8),m.r(0x8B41F8))==queued
 s=state(m);s.smallMapClamp=0;save(m,s);s=tick(m);assert s.smallMapClampStatus==0 and bytes(m.u.mem_read(0x6D8640,5))==m.original
 return {'default_no_patch':True,'explicit_ordinary_product_patch':True,'horizontal_axis_centered':True,'other_axis_scrolls':True,'no_unit_events':True,'off_restores_owned_entry':True}
check('ordinary-product-optin-horizontal-only-and-off',optin)

def dimensions():
 m=fixture();s=state(m);s.smallMapClamp=1;save(m,s)
 for g,expected in [((200,2,3,40,10,800,1080),1),((200,2,3,20,10,1921,1081),1),((200,2,3,20,40,1152,900),2),((200,2,3,20,40,1280,900),1),((200,2,3,90,80,1920,1080),2)]:
  geometry(m,g);s=tick(m);assert s.smallMapClampStatus==expected,(g,s.smallMapClampStatus)
  if expected==1:
   fixed=invoke(m,(100000,100000));mw,mh=g[3]*60,g[4]*30+135
   center=(30*(2*g[1]-g[0])+mw//2,15*(2*g[2]+g[0]-5)+mh//2)
   if mw<g[5]:assert fixed[0]==center[0],(g,fixed,center)
   if mh<g[6]:assert fixed[1]==center[1],(g,fixed,center)
  else:assert bytes(m.u.mem_read(0x6D8640,5))==m.original
 return {'vertical_only_and_both_axes':True,'odd_viewport_map_center':True,'sidebar_width_threshold_1152_to_1280':True,'zoom_geometry_changes_rechecked':True,'large_map_restores_native':True}
check('axis-and-zoom-sidebar-live-geometry-transitions',dimensions)

def foreign():
 m=fixture();geometry(m,(200,2,3,20,40,1920,900));foreign=b'\xe9\x01\x02\x03\x04';m.u.mem_write(0x6D8640,foreign);s=state(m);s.smallMapClamp=1;save(m,s);s=tick(m)
 assert s.smallMapClampStatus==-2 and bytes(m.u.mem_read(0x6D8640,5))==foreign
 return {'preexisting_platform_hook_preserved':True,'conflict_status':-2}
check('preexisting-platform-camera-hook-not-overwritten',foreign)

report={'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'passed':all(r['status']=='PASS'for r in rows),'results':rows}
Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(not report['passed'])

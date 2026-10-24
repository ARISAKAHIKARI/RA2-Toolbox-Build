"""Actual compiled Runtime + manager with a supplied game image; synthetic Win32/world.
Tests patch lifecycle and type writes, not visual reveal, production or netplay.
"""
import argparse,ctypes as C,importlib.util,json,struct,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
MAP={0x508E60:('7d25','eb25'),0x508EBA:('744c','9090'),0x508F8F:('7465','9090'),0x577B3A:('7439','eb39'),0x6D88FC:('7505','eb05')}
PLACE={0x4A8EC3:('0f8593010000','90e993010000'),0x4A918E:('750c','eb0c')}
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode=0):
 spec=importlib.util.spec_from_file_location('direct_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.w(0xa8ed84,10);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.hook();return m
def verify(m,sites,on):
 for at,pair in sites.items():assert bytes(m.u.mem_read(at,len(bytes.fromhex(pair[0]))))==bytes.fromhex(pair[int(on)]),hex(at)
def pass_case(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,3,4,5):
 m=fixture(mode);s=state(m);s.renderReveal=3;s.flags=65536;s.placementImplementation=1;save(m,s);s=m.hook()
 assert (s.data.renderRevealStatus,s.data.placementStatus)==(1,1),(s.data.renderRevealStatus,s.data.placementStatus)
 verify(m,MAP,True);verify(m,PLACE,True);assert s.data.hookInstalled==7
 assert bytes(m.u.mem_read(0x6378e0,5))==m.game.get_data(0x6378e0-0x400000,5)
 s.renderReveal=1;s.placementImplementation=0;save(m,s);s=m.hook();verify(m,MAP,False);verify(m,PLACE,False);assert s.data.hookInstalled==3
 s.renderReveal=3;s.placementImplementation=1;save(m,s);s=m.hook();verify(m,MAP,True);verify(m,PLACE,True)
 s.stop=1;save(m,s);m.hook();verify(m,MAP,False);verify(m,PLACE,False)
 pass_case(f'{mode}_direct_controlled_switch_stop_restoration')
for address in (0x577b3a,0x508eba,0x4a918e):
 m=fixture();m.u.mem_write(address,b'\x75' if address==0x577b3a else b'\xCC');s=state(m);s.renderReveal=3;s.flags=65536;s.placementImplementation=1;save(m,s);s=m.hook()
 rejected=MAP if address in MAP else PLACE
 assert (s.data.renderRevealStatus if address in MAP else s.data.placementStatus)==-2
 for at,pair in rejected.items():
  if at!=address:verify(m,{at:pair},False)
 assert m.u.mem_read(address,1)==(b'\x75' if address==0x577b3a else b'\xCC')
 pass_case(f'foreign_{address:x}_whole_group_refused')
def cow(m):
 obj=m.alloc(0x2000);m.w(obj,0x7eb610);m.u.mem_write(obj+0x24,b'COW\0'+b'\0'*20);m.w(obj+0x634,-1);m.w(obj+0x6cc,1)
 m.vector(0xa8e348,[m.engT,obj]);return obj
for case in ('off','stop','foreign','new_round','stale_request'):
 m=fixture();obj=cow(m);s=state(m);s.techEnhance=1;save(m,s);s=m.hook()
 assert m.r(obj+0x634)==1 and s.techEnhanceCount==1 and s.techEnhanceStatus==1,(case,s.techEnhanceStatus,s.techEnhanceCount,m.r(obj+0x634))
 if case=='stale_request':
  for cmd in (10,12,16,20,22,27):
   s.command=cmd;s.arg=1;s.flags=4096;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;s.requestTypeHash=1;save(m,s);s=m.hook();assert C.c_int32(s.result).value==-7 and not s.data.techOpen
 elif case=='new_round':
  m.w(obj+0x634,7);m.w(0xa8ed84,0);s.techEnhance=0;save(m,s);m.hook();assert m.r(obj+0x634)==7
 else:
  if case=='foreign':m.w(obj+0x634,7)
  if case=='stop':s.stop=1
  else:s.techEnhance=0
  save(m,s);m.hook();assert m.r(obj+0x634)==(7 if case=='foreign' else 0xffffffff)
 pass_case('enhancement_'+case)
m=fixture();s=state(m);s.scrollRefreshMode=3;s.scrollRefreshHz=144;s.viewZoomPercent=50;save(m,s);s=m.hook()
assert s.scrollRefreshStatus==0 and s.viewZoomApplied==100 and s.data.hookInstalled==0
pass_case('stale_shared_refresh_zoom_remain_disabled')
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')

"""Independent compiled Runtime camera dependency retirement probe.
Only private PE/DLL bytes in Unicorn; no game process operation.
"""
import argparse,ast,hashlib,json,sys,traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3]
provider=repo/'source/assist/tests/native_viewport_runtime.py';src=provider.read_text(encoding='utf-8-sig')
ns={'__file__':str(provider),'__name__':'independent_clamp_provider'};exec(compile(src.split('def basic():',1)[0],str(provider),'exec'),ns)
module=ast.parse(src);need={'set_option','pumps','cdecl'}
for f in module.body:
 if isinstance(f,ast.FunctionDef) and f.name in need:exec(compile(ast.Module(body=[f],type_ignores=[]),str(provider),'exec'),ns)
fixture,activate,cdecl,state,pumps,set_option,struct=[ns[n]for n in ('fixture','activate','cdecl','state','pumps','set_option','struct')]
tracked=[repo/'source/assist/source/small_map_camera_bridge.h',repo/'source/assist/source/view_zoom_native_state.h',repo/'source/assist/source/view_zoom_native_bridge.h',repo/'source/assist/source/bridge.cpp',provider,Path(__file__)]
before={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in tracked}
rows=[]
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**fn()})
 except Exception as ex:traceback.print_exc();rows.append({'case':name,'status':'FAIL','error':repr(ex)})
def normal_stop():
 m=fixture();m.w(0x87f8ec,20);m.w(0x87f8f0,10);activate(m,50);assert bytes(m.u.mem_read(0x6d8640,1))==b'\xe9'
 token=cdecl(m,b'_GuardedNativeViewWndEnter',(m.mouse+0x1c,0x12345678));assert token not in (0,0xffffffff)
 set_option(m,stop=1);pumps(m,2);first=state(m);assert first.viewportActive and first.stop==1
 retained=bytes(m.u.mem_read(0x6d8640,5));point=m.alloc();m.u.mem_write(point,struct.pack('<2i',100000,100000));m.run(0x6d8640,(point,));during=struct.unpack('<2i',m.u.mem_read(point,8))
 assert retained[0]==0xe9 and during==(-5280,3262),(retained.hex(),during)
 assert cdecl(m,b'_GuardedNativeViewWndLeave')==0x12345678
 pumps(m,3);final=state(m);restored=bytes(m.u.mem_read(0x6d8640,5));original=m.game.get_data(0x6d8640-0x400000,5)
 assert not final.viewportActive and final.stop==2 and restored==original,(final.viewportActive,final.stop,restored.hex())
 return {'retained_while_native_wnd_busy':True,'denied_capture_point':during,'later_reset_retires_camera_and_acknowledges_stop':True}
def stop_without_ownership():
 m=fixture();m.w(0x87f8ec,20);m.w(0x87f8f0,10);set_option(m,stop=1,viewZoomPercent=50,viewportHostCapabilities=3);pumps(m,2)
 assert state(m).stop==2 and bytes(m.u.mem_read(0x6d8640,5))==m.game.get_data(0x6d8640-0x400000,5)
 return {'stop_does_not_install_new_dependency':True}
def foreign_hold():
 m=fixture();m.w(0x87f8ec,20);m.w(0x87f8f0,10);activate(m,50);births=len(m.born);freed=len(m.freed)
 m.u.mem_write(0x4f44cb,m.game.get_data(0x4f44cb-0x400000,5));set_option(m,stop=1);pumps(m,5)
 assert state(m).viewportActive and state(m).stop==1 and len(m.freed)==freed and len(m.born)==births
 assert bytes(m.u.mem_read(0x6d8640,1))==b'\xe9'
 point=m.alloc();m.u.mem_write(point,struct.pack('<2i',100000,100000));m.run(0x6d8640,(point,));first=struct.unpack('<2i',m.u.mem_read(point,8));m.run(0x6d8640,(point,));second=struct.unpack('<2i',m.u.mem_read(point,8))
 assert first==second==(-5280,3262),(first,second)
 return {'foreign_phase_hold_does_not_acknowledge_stop_or_free_referenced_resources':True,'camera_dependency_retained_without_new_allocations':True,'denied_capture_clamp_is_idempotent':True}
check('normal-deferred-stop-keeps-clamp-then-retires-on-next-reset',normal_stop)
check('foreign-render-owner-hold-keeps-clamp-until-process-teardown',foreign_hold)
check('stop-with-no-native-ownership-never-installs-camera',stop_without_ownership)
after={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in tracked};assert after==before,'source changed during independent probe'
r={'source_sha256':before,'changed_during_run':[],'schema':'ra2.clamp-retirement-independent.v1','scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'pdb_sha256':hashlib.sha256(Path(a.dll).with_suffix('.pdb').read_bytes()).hexdigest(),'provider_sha256':hashlib.sha256(provider.read_bytes()).hexdigest(),'pass':all(x['status']=='PASS'for x in rows),'results':rows,'game_process_accessed':False}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(not r['pass'])

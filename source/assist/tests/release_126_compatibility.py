"""Execute compiled Runtime against supplied EXE code with synthetic Win32/world.
Negative controls change ABI dependencies, not patch entry bytes. Baseline mode
records 1.0.25 accepting them; it does not claim a real platform uses these edits.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--baseline',action='store_true');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture():
 spec=importlib.util.spec_from_file_location('compat126',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,3);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.hook();return m
def case(name,fn):
 try:detail=fn() or {};rows.append({'case':name,'status':'PASS',**detail});print('PASS',name,detail,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def tech(m):
 s=state(m);s.flags=4096;s.techAllowed[1]=1;s.command=10;s.arg=1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook()
 assert s.data.techOpen==1 and s.data.hookInstalled==2
 return 0x6aa786+struct.unpack('<i',m.u.mem_read(0x6aa782,4))[0]
def gate(boundary):
 m=fixture();entry=tech(m)
 if boundary=='frame_reset':m.w(0xa8ed84,0)
 elif boundary=='mode_change':m.w(0xa8b238,4)
 elif boundary=='observer':m.w(0xac1198,m.house)
 elif boundary in ('defeated','loser','spectator'):m.b(m.house+{'defeated':0x1f5,'loser':0x1f6,'spectator':0x1f8}[boundary],1)
 elif boundary=='frame_ahead':m.w(0xa8ed84,m.r(0xa8ed84)+1)
 elif boundary=='menu':m.w(0xa8eda0,1)
 elif boundary=='host_stall':m.clock+=5000;m.w(m.SH+m.Shared.heartbeat.offset,1)
 elif boundary=='stopped':m.w(m.SH+m.Shared.stop.offset,1)
 allowed=boundary in ('frame_ahead','menu','host_stall') or (a.baseline and boundary!='stopped')
 before=bytes(m.u.mem_read(m.t,0x2000)),m.r(0xa802c8),m.r(0x8b41f8)
 m.run(entry,(m.t,0,1),m.house)
 assert m.canbuild_calls[-1]==(m.house,m.t,int(allowed),1),m.canbuild_calls[-1]
 assert before==(bytes(m.u.mem_read(m.t,0x2000)),m.r(0xa802c8),m.r(0x8b41f8))
 return {'ui_permission':allowed,'no_event_or_type_write':True}
for b in ('frame_reset','mode_change','observer','defeated','loser','spectator','frame_ahead','menu','host_stall','stopped'):case('sidebar_'+b,lambda b=b:gate(b))

# Same patch bytes, different stack arguments/continuations. Exercise the sole
# writer directly, so the feature's own preflight cannot hide a manager gap.
hazards=[('placement_stack',0x4a8eb0,0x4a8ebb,0x24),
 ('shroud_stack',0x47f038,0x47efe5,0x38),
 ('fog_stack',0x47f274,0x47f257,0x30),
 ('arrival_stack',0x416ab6,0x416acb,0x48),
 ('tech_arguments',0x6a97d2,0x6a97cb,0x50),
 ('tech_callee',0x6aa781,0x4f7870,0xe9)]
def manager(name,site,dependency,value,changed=True):
 m=fixture();length=10 if site==0x416ab6 else 5
 original=bytes(m.u.mem_read(site,length))
 if changed:m.b(dependency,value)
 req=m.request(0);result,lease=m.manager_call(req);assert result==1
 q=m.request(2);q.token=lease.token;q.address=site;q.length=length
 patch=bytes([0xe8 if site in (0x6a97d2,0x6aa781) else 0xe9])+struct.pack('<i',m.manager_export-site-5)+b'\x90'*(length-5)
 for i in range(length):q.before[i]=original[i];q.after[i]=patch[i]
 result,_=m.manager_call(q);accepted=not changed or a.baseline
 assert result==(1 if accepted else -5),(name,result)
 assert bytes(m.u.mem_read(site,length))==(patch if accepted else original)
 if accepted:
  # Dependency drift must not strand an owned patch during cleanup.
  m.b(dependency,value)
  for i in range(length):q.before[i]=patch[i];q.after[i]=original[i]
  assert m.manager_call(q)[0]==1
  assert bytes(m.u.mem_read(site,length))==original
 lease.operation=1;assert m.manager_call(lease)[0]==1
 assert m.u.mem_read(dependency,1)==bytes([value])
 return {'incompatible_dependency':changed,'installation_accepted':accepted,'foreign_bytes_preserved':True}
for h in hazards:
 case('manager_reject_'+h[0],lambda h=h:manager(*h))
 if not a.baseline:case('manager_valid_then_restore_'+h[0],lambda h=h:manager(*h,changed=False))
def late(kind):
 m=fixture()
 if kind=='tech':tech(m);site=0x6aa777;sites=(0x6a97d2,0x6aa781)
 else:
  s=state(m);s.renderReveal=1;save(m,s);s=m.hook();assert s.data.renderRevealStatus==1
  site=0x47f257;sites=(0x47f038,0x47f274)
 m.b(site,0x90);m.clock+=1100;m.w(0xa8ed84,101);m.w(m.SH+m.Shared.heartbeat.offset,m.clock);s=m.hook()
 assert s.data.hookInstalled==(2 if a.baseline else 0),s.data.hookInstalled
 assert m.u.mem_read(site,1)==b'\x90'
 if not a.baseline:
  for at in sites:assert bytes(m.u.mem_read(at,5))==m.game.get_data(at-0x400000,5)
 # An unrelated ordinary repair still uses its original event channel.
 s=state(m);s.command=1;s.arg=0;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook()
 assert m.r(0xa802c8)==1 and bytes(m.u.mem_read(0xa802d4,1))==b'\x15'
 return {'owned_pair_restored':not a.baseline,'ordinary_repair_preserved':True}
for k in ('tech','render'):case('late_dependency_'+k,lambda k=k:late(k))
Path(a.output).write_text(json.dumps({'scope':__doc__,'baseline':a.baseline,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows,'real_netplay':False},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))

"""Actual x86 Runtime backend routing, host calculation and native constructors.
Win32/world/native legality are mocked. No real Windows/game/netplay claim.
"""
from pathlib import Path
import subprocess,tempfile
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
root=Path(__file__).resolve().parents[2]
rows=[]
def record(name):rows.append({'name':name,'status':'PASS'});print('PASS',name,flush=True)
def clear(m):
 for at in (0x8b41f8,0x8b41fc,0x8b4200,0xa802c8,0xa802cc,0xa802d0):m.w(at,0)
def configure(m,mask,flags=0):
 m.clock+=16;m.b(0xa8e9a0,0);m.hook();s=state(m);s.computeRequested=mask;s.flags=flags;save(m,s)
 m.clock+=16;m.b(0xa8e9a0,1);m.w(0xa8ed84,1);s=m.hook();assert s.computeApplied==mask;return s
def issue(m,command):
 s=state(m);s.command=command;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);return m.hook()
with tempfile.TemporaryDirectory(prefix='compute-locations-') as tmp:
 cpp=Path(tmp)/'host.cpp';host=Path(tmp)/'host'
 cpp.write_text('#include "'+str(root/'assist/source/compute_tasks.h')+'"\n#include "'+str(root/'assist/source/external_planner.h')+'"\n#include <memory>\n#include <cstdio>\nint main(int argc,char**argv){int mode=argc>1?atoi(argv[1]):0;if(mode==0){auto in=std::make_unique<assist::compute::LoadInput>();auto work=std::make_unique<assist::loading::Workspace>();if(fread(in.get(),sizeof(*in),1,stdin)!=1)return 2;auto r=assist::compute::load(*in,*work);fwrite(&r,sizeof r,1,stdout);}else if(mode==1){auto in=std::make_unique<assist::compute::RepairInput>();if(fread(in.get(),sizeof(*in),1,stdin)!=1)return 2;auto r=assist::compute::repair(*in);fwrite(&r,sizeof r,1,stdout);}else{auto in=std::make_unique<assist::planner::World>();if(fread(in.get(),sizeof(*in),1,stdin)!=1)return 2;assist::planner::Planner p;auto out=p.step(*in,argc>2?uint32_t(strtoul(argv[2],nullptr,10)):assist::AutoOreReturn,200,300,90);uint32_t n=out.size();fwrite(&n,4,1,stdout);fwrite(out.data(),sizeof(assist::planner::Order),n,stdout);}}')
 subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
 for mode in (0,3):
  for inside in (False,True):
   for stale in ((False,True) if not inside else (False,)):
    m=fixture(mode);configure(m,(192 if inside else 64));clear(m)
    m.u.mem_write(m.t+0x388,struct.pack('<d',4.));m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.vector(0xa8ecb8,[m.unit,m.eng]);s=state(m);s.loadRadius=30;save(m,s)
    before=bytes(m.u.mem_read(m.eng,0x2000));s=issue(m,15)
    if not inside:
     assert s.loadTask!=s.loadComputed and s.request!=s.ack and count(m)==0,('no external task',s.loadTask,s.loadComputed,s.result)
     r=subprocess.run([str(host),'0'],input=bytes(s.loadInput),stdout=subprocess.PIPE,check=True).stdout
     assert len(r)==C.sizeof(m.LoadResult);s.loadResult=m.LoadResult.from_buffer_copy(r);s.loadComputed=s.loadTask
     if stale:m.clock+=300
     save(m,s);s=tick(m)
    assert s.request==s.ack and count(m)==int(not stale),(mode,inside,stale,s.result,s.count,count(m))
    assert before==bytes(m.u.mem_read(m.eng,0x2000)) and not m.codeWrites
    if not stale:assert packets(m)[12]==7
    else:assert C.c_int32(s.result).value==-7
    record(f'load_{mode}_{inside}_{stale}')
  for inside in (False,True):
   for stale in ((False,True) if not inside else (False,)):
    m=fixture(mode);configure(m,192 if inside else 128);clear(m);s=issue(m,1)
    if not inside:
     assert s.repairTask!=s.repairComputed and s.request!=s.ack and count(m)==0
     r=subprocess.run([str(host),'1'],input=bytes(s.repairInput),stdout=subprocess.PIPE,check=True).stdout
     assert len(r)==C.sizeof(m.RepairResult);s.repairResult=m.RepairResult.from_buffer_copy(r);s.repairComputed=s.repairTask
     if stale:m.clock+=300
     save(m,s);s=tick(m)
    assert s.request==s.ack and count(m)==int(not stale),(mode,inside,stale,s.result,count(m))
    if not stale:assert packets(m)[0]==0x15
    else:assert C.c_int32(s.result).value==-7
    assert not m.codeWrites;record(f'repair_{mode}_{inside}_{stale}')
  for inside in (False,True):
   m=fixture(mode);m.b(m.t+0xe0e,1);m.w(m.t+0x800,40);m.vector(m.t+0x3e8,[m.buildT]);m.u.mem_write(m.unit+0x33c,struct.pack('<4f',40,0,0,0));m.w(m.unit+0xac,10)
   m.w(m.building+0x9c,18*256+128);m.w(m.building+0xa0,28*256+128);m.w(m.unit+0x9c,19*256+128);m.w(m.unit+0xa0,30*256+128)
   clear(m);s=configure(m,192|(4 if inside else 0),32768)
   if not inside:
    assert count(m)==0
    data=subprocess.run([str(host),'2'],input=bytes(s.world),stdout=subprocess.PIPE,check=True).stdout;n=struct.unpack_from('<I',data)[0];assert n>0
    s.planCount=n;s.planRequest+=1;s.planTick=s.world.tick;s.planFrame=s.world.frame;s.planMatch=s.world.match
    for i in range(n):s.plan[i]=m.Order.from_buffer_copy(data[4+i*C.sizeof(m.Order):4+(i+1)*C.sizeof(m.Order)])
    save(m,s);s=m.hook()
   assert count(m)>=1,(mode,inside,s.world.count,s.world.refineryCount,s.runtimeTracking)
   assert any(packets(m)[i*111+12]==7 for i in range(count(m))) and not m.codeWrites
   record(f'ore_{mode}_{inside}')
 # New persistent input has already used its actor's frame allowance.
 # Internal planning must defer to the next frame, never blacklist the target.
 for mode in (0,3):
  for family,flag,bit in (('advanced',8192,1),('magnetron',262144,2),('defense',1073741824,8)):
   flag|=1024 if family!='defense' else 0
   m=fixture(mode,flag);projectile=m.alloc();warhead=m.alloc();m.w(m.weapon+0xb4,4096);m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,warhead);m.b(projectile+0x2a5,1);m.w(m.enemy+0x34,m.alloc());m.w(m.unit+0x18c,0)
   for n in range(11):m.u.mem_write(warhead+0xa0+n*8,struct.pack('<d',1.))
   if family=='magnetron':m.b(m.weapon+0x15c,1);m.b(warhead+0x15b,1)
   if family=='defense':
    m.u.mem_write(m.buildT+0x24,b'NALASR\0');m.w(m.buildT+0x898,m.weapon);m.w(m.eng+0x21c,m.enemy);objects=[m.building,m.eng];source=m.building
   else:objects=[m.unit,m.enemyUnit];source=m.unit
   m.vector(0xa8ec78,objects)
   for n,obj in enumerate(objects):m.w(obj+0x9c,(10+n)*256+128);m.w(obj+0xa0,10*256+128)
   clear(m);s=configure(m,192|bit,flag)
   if family!='defense':
    s=pointer(m,1,2 if family=='magnetron' else 1);assert s.pointer.accepted==1,(family,s.pointer.accepted)
    assert count(m)==1,(family,'same-frame duplicate',count(m));s=tick(m)
   assert count(m)>=1 and any(packets(m)[n*111+12]==1 for n in range(count(m))),(family,mode,count(m),s.world.count,s.world.intentCount)
   assert not m.codeWrites;record(f'internal_{family}_{mode}_first_frame_admission')
 # Cooperative allocation uses the same pure reader/ranker, and the same
 # per-assignment admission in either location.
 for mode in (0,3):
  for air in (False,True):
   for inside in (False,True):
    flags=2097152 if air else 1048576;m=fixture(mode,flags)
    if air:
     sources=[m.unit];targets=[m.enemyUnit];m.b(m.unit+0x134,1);m.w(m.enemy+0x34,m.alloc());m.w(m.unit+0x18c,0)
     projectile=m.alloc();warhead=m.alloc();m.w(m.weapon+0xb4,4096);m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,warhead);m.b(projectile+0x2a5,1)
     for n in range(11):m.u.mem_write(warhead+0xa0+n*8,struct.pack('<d',1.))
    else:
     m.b(m.t+0xd2e,1);m.u.mem_write(m.t+0x380,struct.pack('<d',3.));m.b(m.buildT+0x16ab,1)
     sources=[m.unit,m.obj(0x7f5c70,m.t,m.house,20,selected=True)];targets=[m.building,m.obj(0x7e3ebc,m.buildT,m.house,30)]
     for target in targets:m.w(target+0xe8,1);m.w(target+0xe4,m.alloc(4))
     for source in sources:
      for target in targets:m.action_overrides[(source,target)]=3
    m.vector(0xa8ec78,sources+targets);m.vector(0xa8ecb8,sources)
    for n,obj in enumerate(sources+targets):m.w(obj+0x9c,(10+n)*256+128);m.w(obj+0xa0,10*256+128)
    configure(m,192|((32 if air else 16) if inside else 0),flags);clear(m);m.queryObject=targets[0]
    def select(s,i):
     i.count=len(sources)
     for n,source in enumerate(sources):i.sources[n]=m.PointerSource(m.r(source+0x10),source)
    s=pointer(m,1,16 if air else 8,select)
    assert s.pointer.accepted==len(sources),(mode,air,inside,s.pointer.accepted,s.pointer.result)
    if inside:
     assert s.runtimeCoopDone==s.cooperativeRequest.serial and s.runtimeCoopAccepted==len(sources),(mode,air,inside,s.runtimeCoopDone,s.runtimeCoopAccepted,s.runtimeCoopRejected)
    else:
     assert count(m)==0;s.cooperativePlan+=1;s.cooperativeSerial=s.cooperativeRequest.serial;s.cooperativeTick=m.clock;s.cooperativeFrame=s.data.frame;s.cooperativeMatch=s.data.match;s.cooperativeCount=len(sources);s.cooperativeMode=2 if air else 1
     for n,source in enumerate(sources):s.cooperativeOrders[n]=m.CooperativeAssignment(s.cooperativeRequest.sources[n].id,s.cooperativeRequest.sources[n].token,m.r(targets[n]+0x10),m.t if air else m.buildT)
     save(m,s);s=m.hook();assert s.cooperativeAccepted==len(sources),(mode,air,inside,s.cooperativeAccepted,s.cooperativeRejected)
    assert count(m)==len(sources) and not m.codeWrites
    record(f'cooperative_{mode}_{air}_{inside}')
 # Actual Runtime latches all 1024 per-feature combinations; changing settings
 # mid-match cannot transfer live jobs, then the next match applies the mask.
 m=fixture(0)
 for mask in range(1024):
  s=configure(m,mask);s.computeRequested=mask^1023;save(m,s);s=tick(m)
  assert s.computeApplied==mask and count(m)==0
 record('1024_mixed_masks_and_no_live_handoff')
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')

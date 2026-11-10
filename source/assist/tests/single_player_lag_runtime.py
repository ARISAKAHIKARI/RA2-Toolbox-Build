"""Compiled DLL vs private original x86 routines, in Unicorn (not a game run).

Win32 time and final Surface drawing are fixture IO. Actual native projection,
clip, RGB interpolation, rectangle intersection and Clear execute unchanged.
Tests compare ordered draw arguments and A* memory, not screenshots or FPS.
"""
import argparse, ctypes as C, hashlib, json, random, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
assert hashlib.sha256(a.game.read_bytes()).hexdigest()=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
fixture=Path(__file__).with_name('entry_runtime_regression.py')
ns={'__name__':'lag_fixture','__file__':str(fixture),'fixture_paths':[str(a.game.resolve()),str(a.dll.resolve())]}
exec(compile(fixture.read_text(encoding='utf-8-sig'),str(fixture),'exec'),ns)
u,w,r,b,run,hook=(ns[k] for k in ('u','w','r','b','run','hook'))
Shared,SH=ns['Shared'],ns['SH'];game=ns['game'];base=game.OPTIONAL_HEADER.ImageBase
sites=(0x42a5b0,0x70508a);sizes=(6,7);original=[game.get_data(at-base,n) for at,n in zip(sites,sizes)]
def state():return Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))
def configure(mask,session=5):
 w(0xa8b238,session);s=state();s.flags=0;s.singlePlayerLagFix=mask;s.foreground=1;u.mem_write(SH,bytes(s));return hook()
w(0xa8b230,ns['alloc']());w(0xa8b24c,0)

s=configure(3);assert (s.lagPathStatus,s.lagMindStatus)==(1,1),(s.lagPathStatus,s.lagMindStatus)
patched=[bytes(u.mem_read(at,n)) for at,n in zip(sites,sizes)]
assert all(x[0]==0xe9 for x in patched)

# Strict matching is deliberately not bypassed by the debug-control settings.
results=[]
def record(name,**info):results.append({'case':name,'status':'PASS',**info})
record('actual managed install, both native sites, SP 5')
for mode in (3,4,1,2,6):
 s=configure(3,mode);assert (s.lagPathStatus,s.lagMindStatus)==(-1,-1)
 assert [bytes(u.mem_read(at,n)) for at,n in zip(sites,sizes)]==original
 record('retire in unsupported or multiplayer session',session=mode)
s=configure(3,0);assert (s.lagPathStatus,s.lagMindStatus)==(1,1);record('reinstall in SP campaign 0')
s=configure(0);assert (s.lagPathStatus,s.lagMindStatus)==(0,0)
assert [bytes(u.mem_read(at,n)) for at,n in zip(sites,sizes)]==original
record('explicit disable restores exact original bytes')
# Alter a dependency, not our patch bytes. The other independent optimization
# may still install; never claim that both are enabled if only one is supported.
at=0x6d2140;old=bytes(u.mem_read(at,1));u.mem_write(at,b'\x90');s=configure(3)
assert s.lagPathStatus==1 and s.lagMindStatus==-2
assert bytes(u.mem_read(sites[1],sizes[1]))==original[1]
configure(0);u.mem_write(at,old);s=configure(3);assert s.lagMindStatus==1
record('changed native projection rejected, path remains independent')

# Map separate fixed state: pool counters, both queues, visited generations.
A=0x50000000;u.mem_map(A,0x500000);astar=A;pool=A+0x1000;opens=A+0x190000
queue=A+0x2a0000;hier=queue+0x100;heap=A+0x2b0000;hheap=A+0x300000
visited=A+0x350000;hvisited=A+0x351000
rng=random.Random(0x1048)
regions=[(astar,0x100),(pool+0x180000-4,12),(opens+0x100000-4,12),
 (queue,0x120),(heap-16,0x40030),(hheap-16,0x40030),(visited,0x300),(hvisited,0x1000)]
def setup_path(n,m,generation):
 for at,length in regions:u.mem_write(at,b'\xa5'*length)
 for off,value in ((0xc,pool),(0x10,opens),(0x14,queue),(0x68,hier),(0x18,visited),(0x1c,visited+0x100),(0x28,generation)):w(astar+off,value)
 for q,h,count in ((queue,heap,n),(hier,hheap,m)):
  w(q,count);w(q+4,65536);w(q+8,h)
 for level,count in enumerate((0,5,9)):
  w(0x87f85c+level*4,count)
  for off,j in ((0x40,0),(0x4c,1),(0x58,2)):w(astar+off+level*4,hvisited+(level*3+j)*0x100)
 w(0x89c2dc,8)
def capture():return [bytes(u.mem_read(at,length)) for at,length in regions]
cases=[(n,m,g) for n,m in [(-1,-1),(0,0),(1,1),(32,0),(0,32),(129,17),(1024,63),(4096,2048),(65535,65535)] for g in (0,0xfffffffe,0xffffffff)]
cases += [(rng.randrange(4096),rng.randrange(4096),rng.randrange(0xffffffff)) for _ in range(12)]
for n,m,g in cases:
 setup_path(n,m,g);u.mem_write(sites[0],original[0]);u.ctl_remove_cache(0x42a5b0,0x42a68e);run(sites[0],self=astar);expected=capture()
 setup_path(n,m,g);u.mem_write(sites[0],patched[0]);u.ctl_remove_cache(0x42a5b0,0x42a68e);run(sites[0],self=astar);actual=capture()
 assert actual==expected,('A* difference',n,m,hex(g),[i for i,(x,y) in enumerate(zip(actual,expected)) if x!=y])
record('A* exact memory including canaries, empty/negative/full queues and generation wrap',fixtures=len(cases))
# The hot handler must bypass immediately on a mode change, before maintenance.
setup_path(128,64,17);u.mem_write(sites[0],original[0]);u.ctl_remove_cache(0x42a5b0,0x42a68e);run(sites[0],self=astar);expected=capture()
setup_path(128,64,17);u.mem_write(sites[0],patched[0]);u.ctl_remove_cache(0x42a5b0,0x42a68e);w(0xa8b238,3);run(sites[0],self=astar);assert capture()==expected
w(0xa8b238,5);record('native path handler immediately bypasses multiplayer before next tick')

# Use the real native projection rather than the command fixture's simplified
# coordinate stub. Final Surface methods are sinks recording their exact args.
u.hook_del(ns['native_stub_hooks'][0x6d2140])
surface=ns['alloc']();table=ns['alloc']();w(surface,table);w(0x88731c,surface)
fill=ns['STUB']+0xe000;line=fill+16;w(table+0x14,fill);w(table+0x30,line)
trace=[];counts={'projection':0,'instruction':0}
def draw(uc,at,size,_):
 sp=u.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 if at==fill:trace.append(('rect',tuple(struct.unpack('<4i',u.mem_read(arg(0),16))),arg(1)))
 else:trace.append(('line',tuple(struct.unpack('<2i',u.mem_read(arg(0),8))),tuple(struct.unpack('<2i',u.mem_read(arg(1),8))),arg(2)))
 ns['ret'](1,2 if at==fill else 3)
u.hook_add(UC_HOOK_CODE,draw,begin=fill,end=fill);u.hook_add(UC_HOOK_CODE,draw,begin=line,end=line)
u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:counts.__setitem__('projection',counts['projection']+1),begin=0x6d2140,end=0x6d2140)
def insn(uc,at,size,_):counts['instruction']+=1
counter=u.hook_add(UC_HOOK_CODE,insn)
tactical=r(0x887324)
u.mem_write(0xb0cd48,struct.pack('<d',0.14433756729740643))
projection_pairs=set();instruction_pairs=[]
def render(from_,to,rgb,view,camera,clock,fast,mode=5):
 trace.clear();counts.update(projection=0,instruction=0)
 u.mem_write(sites[1],patched[1] if fast else original[1]);u.ctl_remove_cache(0x704e40,0x705263);w(0xa8b238,mode)
 u.mem_write(0x886fa0,struct.pack('<4i',*view));w(tactical+0xb0,camera[0]);w(tactical+0xb4,camera[1]);ns['clock']=clock
 u.reg_write(UC_X86_REG_FPCW,0x37f);u.reg_write(UC_X86_REG_FPSW,0);u.reg_write(UC_X86_REG_FPTAG,0xffff)
 run(0x704e40,tuple(v&0xffffffff for v in (*from_,*to,rgb)))
 return list(trace),dict(counts)
fixtures=[]
for rgb in (0,0xffffff,0x00ff00,0x113bd9):
 for clk in (0,16,496,512,10000,0xffffffff):
  fixtures.append(((10000,10000,0),(15000,12000,1000),rgb,(0,0,800,600),(0,800),clk))
for _ in range(160):
 start=tuple(rng.randrange(-40000,50000) for _ in range(3));end=tuple(rng.randrange(-40000,50000) for _ in range(3))
 fixtures.append((start,end,rng.randrange(0x1000000),(rng.randrange(-100,100),rng.randrange(-100,100),rng.randrange(1,3000),rng.randrange(1,2000)),(rng.randrange(-1000,1000),rng.randrange(-1000,1000)),rng.randrange(0x100000000)))
for masks in ((11,3,5,2,0,3),(10,3,5,3,0,3)):
 for at,v in zip((0x8a0dd0,0x8a0dd4,0x8a0de0,0x8a0de4,0x8a0dd8,0x8a0ddc),masks):w(at,v)
 for f in fixtures:
  expected,c1=render(*f,False);actual,c2=render(*f,True)
  assert expected==actual,('ordered native draw differs',f,expected[:5],actual[:5])
  assert c1['projection']==66 and c2['projection']==35,(c1,c2)
  projection_pairs.add((c1['projection'],c2['projection']));instruction_pairs.append((c1['instruction'],c2['instruction']))
record('mind links: native coordinates, clipped segments, ordered colors and endpoint rectangles',fixtures=len(fixtures)*2,formats=['RGB565','RGB555'],projection_calls=[66,35],instructions_total=[sum(x[0] for x in instruction_pairs),sum(x[1] for x in instruction_pairs)])
f=fixtures[0];expected,_=render(*f,False);actual,count=render(*f,True,mode=3);assert actual==expected and count['projection']==66
record('mind handler immediately bypasses multiplayer before maintenance')
u.hook_del(counter)
# Restore fixture clock for Runtime heartbeat checks and verify actual off path.
ns['clock']=100000;configure(0)
assert [bytes(u.mem_read(at,n)) for at,n in zip(sites,sizes)]==original
record('post-workload managed retirement restores both native sites')
a.out.parent.mkdir(parents=True,exist_ok=True)
a.out.write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(a.game.read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(a.dll.read_bytes()).hexdigest(),'results':results,'live_game_tested':False,'gpu_takeover':False},ensure_ascii=False,indent=2)+'\n')
print('PASS',len(results),'runtime groups;',len(cases),'A* states;',len(fixtures)*2,'native draw comparisons')

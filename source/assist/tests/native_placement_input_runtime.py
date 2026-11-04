"""Replay compiled Runtime placement clicks without a Host-ready pulse.

The real DLL manager/filter/dispatcher/placement admission/ctor/queue execute.
The established harness supplies Win32/world services; ProcessClickCoords is
an explicit flat-cell query sink. Audio worker creation fails as a Win32 seam.
Placement reception/effects are not executed.
"""
import argparse, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game',type=Path);p.add_argument('dll',type=Path)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expect-stale',action='store_true',help='Reproduce the pre-fix cross-frame refusal.')
a=p.parse_args()
spec=importlib.util.spec_from_file_location('placement_runtime_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m)
u=m.u;w=m.w;r=m.r;b=m.b
state=lambda:m.Shared.from_buffer_copy(u.mem_read(m.SH,m.C.sizeof(m.Shared)))
save=lambda s:u.mem_write(m.SH,bytes(s))
rows=[]
# Acknowledgement audio must not spawn a worker in this isolated replay.
for at,name in m.imports.items():
    if name=='CreateThread':
        u.hook_del(m.native_stub_hooks[at])
        u.hook_add(UC_HOOK_CODE,lambda cpu,pc,size,user:m.ret(0,6),begin=at,end=at)
def check(name,condition):
    assert condition,name
    rows.append(dict(case=name,status='PASS'))

# Ordinary completed factory/product, exact local House and a flat indexed map.
w(0xA8B230,m.alloc());w(0xB73550,1);w(0x8809A0,-1)
u.mem_write(0x886FA0,struct.pack('<4i',0,0,632,600))
factory=m.alloc();product=m.obj(0x7E3EBC,m.buildT,m.house,101)
w(m.house+0x10,201);w(m.buildT+0x10,301)
w(m.house+0x53BC,factory);w(factory+0x10,401);w(factory+0x6C,m.house)
w(factory+0x24,54);w(factory+0x58,product)
b(product+0x74,0);b(product+0x81,1);w(m.buildT+0xEF0,0)
w(0x8192B8,1);w(0x819310,1)
cells=m.alloc(512*100*4);cell=m.alloc()
w(0x87F924,cells);w(0x87F928,512*100);w(cells+(70*512+70)*4,cell)
u.mem_write(cell+0x24,struct.pack('<2h',70,70))
u.mem_write(0x87F7E8+0xF4,struct.pack('<6i',100,100,0,0,100,100))
for offset in (0x1190,0x11A4):u.mem_write(0x87F7E8+offset,struct.pack('<3I',0,0,0xFFFFFFFF))
u.mem_write(0x87F7E8+0x11B0,bytes(5))

queries=[]
def query(cpu,pc,size,user):
    sp=cpu.reg_read(m.UC_X86_REG_ESP);args=[r(sp+4+i*4)for i in range(6)]
    assert cpu.reg_read(m.UC_X86_REG_ECX)==0x87F7E8
    queries.append(struct.unpack('<2i',u.mem_read(args[0],8)))
    u.mem_write(args[1],struct.pack('<2h',70,70));u.mem_write(args[2],struct.pack('<3i',70*256+128,70*256+128,0))
    w(args[3],0);b(args[4],0);b(args[5],0);m.ret(1,6)
u.hook_add(UC_HOOK_CODE,query,begin=0x692300,end=0x692300)

s=state();s.flags=65536;s.placementImplementation=2;save(s);s=m.hook()
check('completed_product_published',s.data.commandStatus==1 and s.data.placementOfferCount==1)
s.command=33;s.arg2=1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(s);s=m.hook()
check('arm_without_event',s.ack==s.request and s.result==2 and s.placementPreview.active==1 and r(0x8B41F8)==0)

message=m.CONTROL+0x200
def physical(kind,frame,key=0):
    w(0xA8ED84,frame);w(m.CONTROL+16,1);w(m.CONTROL+20,0)
    u.mem_write(message,struct.pack('<7I',1,kind,key,(100<<16)|100,0,0,0))
    m.run(m.entry,(0,1,message))
    return state()
frame=r(0xA8ED84)
w(0xB0FE5C,product);w(0xB0FE60,product)
s=physical(0x201,frame+1)
check('new_frame_down_consumed_without_event',r(message+4)==0 and s.placementPreview.active==1 and r(0x8B41F8)==0)
s=physical(0x202,frame+2)
if a.expect_stale:
    check('baseline_stale_release_refused',r(message+4)==0 and s.placementPreview.active==1 and r(0x8B41F8)==0)
else:
    check('new_frame_up_queues_once_without_host_ready',r(message+4)==0 and s.placementPreview.active==0 and r(0x8B41F8)==1 and r(m.CONTROL+20)==0)
    check('queue_acceptance_clears_both_exact_quick_caches',not r(0xB0FE5C) and not r(0xB0FE60) and r(factory+0x58)==product and not u.mem_read(product+0x74,1)[0])
    packet=bytes(u.mem_read(0x8B4204,111))
    check('original_place_payload_and_sp_schedule',packet[0]==0x0B and struct.unpack_from('<I',packet,3)[0]==frame+3 and struct.unpack_from('<3i2h',packet,7)==(7,0,0,70,70))
    # Recreate the sidebar cache between samples. Native clear runs before Q
    # reaches the game, even while the exact product is still Limbo.
    w(0xB0FE5C,product);w(0xB0FE60,product)
    s=physical(0x100,frame+3,ord('Q'))
    check('q_before_host_sample_clears_both_exact_caches',r(message+4)==0x100 and not r(0xB0FE5C) and not r(0xB0FE60) and r(0x8B41F8)==1)
    physical(0x202,frame+3)
    check('extra_release_cannot_duplicate_event',r(0x8B41F8)==1)

report=dict(scope=__doc__,pass_=True,checks=len(rows),cases=rows,query_sink_calls=len(queries),
            baseline_refusal_expected=a.expect_stale,game_sha256=hashlib.sha256(a.game.read_bytes()).hexdigest(),
            dll_sha256=hashlib.sha256(a.dll.read_bytes()).hexdigest(),game_process_accessed=False,placement_reception_executed=False,multiplayer_tested=False)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print('PASS:',len(rows),'compiled Runtime placement input cases; Host-ready=0; no game reception/netplay')

"""Run captured native CRC, Ctrl+Alt action and Sticky transition prefixes.

Requires the owner's private .mem/.valid capture, never shipped with this test.
CRC executes the original full computation; only its logging sink is mocked.
Input and mission probes use synthetic object/map/keyboard queries. They do not
prove complete pathfinding, arrival effects, Windows input or live netplay.
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--capture',required=True)
p.add_argument('--output',required=True)
a=p.parse_args()
with zipfile.ZipFile(a.capture) as z:
    image=z.read(next(n for n in z.namelist() if n.endswith('/00-clean-game.mem')))
    valid=z.read(next(n for n in z.namelist() if n.endswith('/00-clean-game.valid')))
assert len(image)==len(valid)
BASE=0x400000
S=0x20000000
STOP=0x30000000
STACK=0x30010000
MASK=0xffffffff

class Probe:
    def __init__(self):
        self.u=Uc(UC_ARCH_X86,UC_MODE_32)
        self.u.mem_map(BASE,(len(image)+4095)&~4095)
        self.u.mem_write(BASE,image)
        self.u.mem_map(S,0x100000)
        self.u.mem_map(STOP,0x20000)
        self.stubs={}
        self.pcs=set()
        self.u.hook_add(UC_HOOK_CODE,self.hook)
    def w(self,at,value):self.u.mem_write(at,struct.pack('<I',value&MASK))
    def r(self,at):return struct.unpack('<I',self.u.mem_read(at,4))[0]
    def b(self,at,value):self.u.mem_write(at,bytes([value]))
    def ret(self,value=0,pop=0):
        sp=self.u.reg_read(UC_X86_REG_ESP)
        self.u.reg_write(UC_X86_REG_EAX,value&MASK)
        self.u.reg_write(UC_X86_REG_EIP,self.r(sp))
        self.u.reg_write(UC_X86_REG_ESP,sp+4+pop)
    def hook(self,u,at,size,_):
        if at in self.stubs:
            self.stubs[at]()
            return
        assert BASE<=at<BASE+len(image),hex(at)
        assert all(valid[at-BASE:at-BASE+size]),'invalid captured instruction '+hex(at)
        self.pcs.add(at)
    def run(self,at,this=0,args=()):
        self.w(STACK,STOP)
        for i,v in enumerate(args):self.w(STACK+4+i*4,v)
        self.u.reg_write(UC_X86_REG_ESP,STACK)
        self.u.reg_write(UC_X86_REG_ECX,this)
        self.u.emu_start(at,STOP,count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP)==STOP,('budget',hex(at))
        assert self.u.reg_read(UC_X86_REG_ESP)==STACK+4+4*len(args),('stack',hex(at))
        return self.u.reg_read(UC_X86_REG_EAX)
    def vector(self,at,values,storage):
        self.w(at+4,storage)
        self.w(at+16,len(values))
        for i,v in enumerate(values):self.w(storage+i*4,v)
    def vt(self,obj,original,slots):
        table=S+0x80000+(obj-S)//0x2000*0x1000
        self.u.mem_write(table,bytes(self.u.mem_read(original,0x600)))
        self.w(obj,table)
        for offset,(value,pop) in slots.items():
            stub=table+0x800+offset
            self.w(table+offset,stub)
            self.stubs[stub]=lambda v=value,n=pop:self.ret(v,n)

def mix(h,v):return (((h<<1)|(h>>31))+v)&MASK
def coord(x,y):return int(x/10)+(int(y/10)<<16)
def facing(raw):return (((raw>>7)+1)>>1)&255
def crc_case(change):
    q=Probe()
    infantry,unit,building,air,house,scenario=[S+i*0x2000 for i in range(6)]
    objects=[infantry,unit,building,air]
    vtables=[0x7eb058,0x7f5c70,0x7e3ebc,0x7e22a4]
    xy=[(12340,23450),(34560,45670),(56780,67890),(78900,89010)]
    for obj,vt,(x,y) in zip(objects,vtables,xy):
        q.w(obj,vt)
        q.w(obj+0x520,S+0x40000);q.w(obj+0x6c0,S+0x40000);q.w(obj+0x6c4,S+0x40000)
        q.w(obj+0x9c,x);q.w(obj+0xa0,y);q.w(obj+0xa4,512)
        q.w(obj+0x388,0x3200);q.w(obj+0x3a0,0x6700)
    q.w(0xa8ed84,100)
    q.vector(0xa83de8,[infantry],S+0x20000)
    q.vector(0x8b4108,[unit],S+0x21000)
    q.vector(0xa8eb40,[building],S+0x22000)
    q.vector(0xa80228,[house],S+0x23000)
    q.b(house+0x241,0)
    for i in range(5):q.vector(0x8a0360+i*24,[],S+0x24000+i*0x100)
    q.vector(0x8a0360,[infantry,unit,building,air],S+0x24000)
    q.vector(0x87f778,[air,unit],S+0x25000)
    q.w(0xa8b230,scenario)
    rng=scenario+0x218
    q.w(rng+4,0);q.w(rng+8,103)
    for i in range(250):q.w(rng+12+4*i,((i+1)*0x01020304)&MASK)
    q.stubs[0x650a90]=lambda:q.ret() # log only; computation and RNG stay native
    if change=='x_small':q.w(unit+0x9c,34561)
    if change=='x_step':q.w(unit+0x9c,34570)
    if change=='z':q.w(unit+0xa4,2048)
    if change=='health':q.w(unit+0x6c,999)
    if change=='money':q.w(house+0x30c,999999)
    if change=='mission':q.w(unit+0xac,6)
    if change=='techlevel':q.w(S+0x40000+0x634,1)
    if change=='map_clear':q.b(house+0x241,1)
    if change=='direction':q.w(unit+0x388,0x3300)
    if change=='random':q.w(rng+12,123)
    if change=='order':q.vector(0x8a0360,[unit,infantry,building,air],S+0x24000)
    if change=='count':q.vector(0x87f778,[air],S+0x25000)
    stream=[]
    def loc(obj):return coord(q.r(obj+0x9c),q.r(obj+0xa0))
    stream += [loc(infantry)+facing(q.r(infantry+0x388))]
    stream += [loc(unit)+facing(q.r(unit+0x388))+facing(q.r(unit+0x3a0))]
    stream += [loc(building)+facing(q.r(building+0x388)),q.u.mem_read(house+0x241,1)[0]]
    rtti={infantry:15,unit:1,building:6,air:2}
    layer=[unit,infantry,building,air] if change=='order' else objects
    logic=[air] if change=='count' else [air,unit]
    stream += [loc(o)+rtti[o] for o in layer+logic]
    stream += [q.r(rng+12)^q.r(rng+12+103*4)]
    expected=0
    for v in stream:expected=mix(expected,v)
    q.run(0x64dab0)
    actual=q.r(0xac51fc)
    assert actual==expected,(change,hex(actual),hex(expected))
    assert q.r(rng+4)==1 and q.r(rng+8)==104
    return {'case':change,'native_crc':actual,'oracle_crc':expected,'status':'PASS'}

rows=[crc_case(c) for c in ('baseline','x_small','x_step','z','health','money','mission','techlevel','map_clear','direction','random','order','count')]
baseline=rows[0]['native_crc']
for row in rows:
    equal=row['case'] in ('baseline','x_small','z','health','money','mission','techlevel')
    assert (row['native_crc']==baseline)==equal,row

# Original QueueMission(start=true included) still asks Unit readiness and
# therefore cannot move an ordinary vehicle out of Sticky.
for requested in (1,2,5,7,11,13,28,29):
    for start in (0,1):
        q=Probe();q.w(S,0x7f5c70);q.w(S+0xac,6);q.w(S+0xb4,-1)
        q.run(0x5b35e0,S,(requested,start))
        assert q.r(S+0xac)==6 and q.r(S+0xb4)==requested
        assert q.run(0x744270,S)==0
        rows.append({'case':'sticky_native_queue','requested':requested,'start':start,'current':6,'queued':requested,'ready':False,'status':'PASS'})

# Actual destination prefix has a ForceMission branch, but only with a type
# flag and an already matching destination/target. This is not a generic fix.
for flag in (0,1):
    q=Probe();unit=S;typ=S+0x2000;target=S+0x4000
    q.w(unit,0x7f5c70);q.w(unit+0x6c4,typ);q.b(typ+0xd6a,flag)
    q.w(unit+0xac,6);q.w(unit+0xb4,2)
    q.w(unit+0x5a4,target);q.w(unit+0x2b4,target)
    reached=[]
    def remaining_body():
        reached.append('remaining_destination_body')
        q.u.reg_write(UC_X86_REG_EIP,0x74317a) # original common epilogue
    q.stubs[0x741a80]=remaining_body
    q.run(0x741970,unit,(0,1))
    assert q.r(unit+0xac)==(1 if flag else 6)
    assert (0x5b2fd0 in q.pcs)==bool(flag)
    rows.append({'case':'sticky_destination_prefix','BalloonHover':flag,'current_mission':q.r(unit+0xac),'force_mission_called':0x5b2fd0 in q.pcs,'unexecuted_body':reached,'status':'PASS'})

# Native planning completion drains to local DoList even in an MP session.
q=Probe();event=S;items=S+0x2000;timer=S+0xf0000
packet=bytearray(111);packet[0]=4;packet[29]=2
q.u.mem_write(event,bytes(packet))
q.w(0xac4b4c,items);q.w(items,event);q.w(0xac4b58,1)
q.w(0xa8ed84,100);q.w(0xa8b238,3);q.w(0xa8b24c,0)
q.w(0x8b41f8,0);q.w(0x8b41fc,0);q.w(0xa802c8,0)
q.w(0x7e1530,timer);q.stubs[timer]=lambda:q.ret(123)
q.stubs[0x7c8b3d]=lambda:q.ret() # allocator free only
q.run(0x637550)
assert q.r(0x8b41f8)==1 and q.r(0xa802c8)==0 and q.r(0xac4b58)==0
assert q.r(0x8b41fc)==0x3fff
assert q.r(0x8b4204+0x3fff*111+3)==101
rows.append({'case':'native_planning_drain_MP','DoList_added':1,'OutList_added':0,'scheduled_frame':101,'status':'PASS'})

# Actual FrameInfo comparison chooses the ring slot (Frame - Delay) & 255.
for mismatch in (False,True):
    q=Probe();q.w(0x8b41fc,0);q.w(0x8b4204+3,101);q.w(0x8b4204+7,0x12345678+int(mismatch));q.b(0x8b4204+13,5)
    q.w(0xb04474+96*4,0x12345678)
    ends=[]
    for at in (0x64caa8,0x64cc61):q.stubs[at]=lambda at=at:(ends.append(at),q.u.reg_write(UC_X86_REG_EIP,STOP))
    q.u.reg_write(UC_X86_REG_EBP,0);q.u.reg_write(UC_X86_REG_EDX,5)
    q.u.emu_start(0x64c6b8,STOP,count=1000)
    assert ends==[0x64cc61 if mismatch else 0x64caa8]
    rows.append({'case':'native_FrameInfo_compare','Frame':101,'Delay':5,'compared_ring_index':96,'mismatch_branch':mismatch,'status':'PASS'})

# Ctrl+Alt is resolved at the native cell-action gate. Force the two keyboard
# predicate values, not the resulting Action. Exclude map extension/hazards.
for explored in (0,1):
    q=Probe();unit=S;cell=S+0x2000;house=S+0x4000;scenario=S+0x6000
    q.vt(unit,0x7eb058,{0x1d4:(0,0),0x1d8:(0,0),0x330:(1,0),0xa0:(1,0),0x9c:(1,0)})
    q.w(unit+0x21c,house);q.w(unit+0x6c0,S+0xa000);q.w(0xa83d4c,house);q.w(0xa8b230,scenario)
    q.w(cell+0x44,-1)
    # Native first modifier group (force fire), third group (force move).
    q.w(0xa8ec00,17);q.w(0xa8ec04,17);q.w(0xa8ec08,16);q.w(0xa8ec0c,16);q.w(0xa8ebf8,18);q.w(0xa8ebfc,18)
    q.stubs[0x5657a0]=lambda:q.ret(cell,4)
    q.stubs[0x565730]=lambda:q.ret(cell,4)
    q.stubs[0x578080]=lambda:q.ret(0,4)
    q.stubs[0x586360]=lambda:q.ret(0,4)
    q.stubs[0x54f5c0]=lambda:q.ret(int(q.r(q.u.reg_read(UC_X86_REG_ESP)+4) in (17,18)),4)
    q.stubs[0x50b6f0]=lambda:q.ret(1)
    q.stubs[0x5023b0]=lambda:q.ret(explored,4)
    q.stubs[0x6f3970]=lambda:q.ret(0,4) # ordinary infantry has a weapon
    action=q.run(0x51f800,unit,(S+0x8000,0,0))
    assert action==(26 if explored==0 else 51),(explored,action)
    rows.append({'case':'ctrl_alt_native_cell_action','cell_query_result':explored,'action':action,'name':'GuardArea' if action==26 else 'PatrolWaypoint','status':'PASS'})

    if action==26:
        # Execute the native click dispatcher, observing its ClickedMission
        # arguments at the delivery boundary rather than inventing a mission.
        sent=[]
        table=q.r(unit)
        def clicked():
            sp=q.u.reg_read(UC_X86_REG_ESP)
            sent.append([q.r(sp+4+i*4) for i in range(4)])
            q.ret(1,16)
        q.stubs[q.r(table+0x378)]=clicked
        q.stubs[q.r(table+0x70)]=lambda:q.ret(action,12) # already verified native classifier
        q.stubs[0x578460]=lambda:q.ret(1,8)
        for _ in range(32):q.run(0x4d7d50,unit,(action,S+0x8000,0,0))
        assert sent==[[11,cell,0,0]]*32,sent
        rows.append({'case':'ctrl_alt_native_click_dispatch','repetitions':32,'mission':11,'target':'cell','destination':'none','status':'PASS'})
        # GuardArea isn't one of the normal infantry's allowed building-entry
        # missions. Verify the exact gate 32 times without changing abilities.
        for engineer in (0,1):
            q.b(S+0xa000+0xec3,engineer);q.w(unit+0xac,11)
            ends=[]
            for at in (0x51c37d,0x51c41c):
                q.stubs[at]=lambda at=at:(ends.append(at),q.u.reg_write(UC_X86_REG_EIP,STOP))
            for _ in range(32):
                q.u.reg_write(UC_X86_REG_EBP,unit);q.u.reg_write(UC_X86_REG_ESI,S+0xc000)
                q.u.reg_write(UC_X86_REG_ESP,STACK)
                q.u.emu_start(0x51c2e1,STOP,count=1000)
            assert ends==[0x51c37d if engineer else 0x51c41c]*32
            rows.append({'case':'ctrl_alt_repeated_building_gate','mission':11,'engineer':engineer,'repetitions':32,'admitted_at_this_gate':bool(engineer),'status':'PASS'})

for mission in (0,23):
    q=Probe();q.w(S,0x7f5c70);q.w(S+0xac,mission);q.w(S+0xb4,-1);q.w(S+0x6c,100);q.b(S+0x90,1)
    q.stubs[0x5f3e70]=lambda:q.ret() # unrelated object bookkeeping
    for frame in (100,10000):
        q.w(0xa8ed84,frame);q.run(0x5b3060,S)
        assert q.r(S+0xac)==mission and q.r(S+0xd0)==450
    flags=list(q.u.mem_read(0xa8e3a8+mission*32+4,6))
    rows.append({'case':'passive_native_mission_tick','mission':mission,'flags_NoThreat_Zombie_Recruitable_Paralyzed_Retaliate_Scatter':flags,'timer_duration':450,'after_time_current_unchanged':True,'status':'PASS'})
    for requested in (1,2,5):
        q=Probe();q.w(S,0x7f5c70);q.w(S+0xac,mission);q.w(S+0xb4,-1)
        q.w(S+0x674,S+0x2000);q.w(S+0x2000,S+0x3000);q.w(S+0x3000+0x80,S+0xf0000)
        q.stubs[S+0xf0000]=lambda:q.ret(0,4)
        q.stubs[0x4a51d0]=lambda:q.ret(1)
        q.stubs[0x65ad40]=lambda:q.ret(0,4)
        q.stubs[0x565730]=lambda:q.ret(S+0x4000,4)
        q.stubs[0x47c520]=lambda:q.ret(0)
        q.w(0xa8ed84,100);q.run(0x5b35e0,S,(requested,1))
        assert q.r(S+0xac)==requested and q.r(S+0xb4)==MASK,(mission,requested,q.r(S+0xac))
        rows.append({'case':'passive_native_order_exit','before':mission,'requested':requested,'after':q.r(S+0xac),'status':'PASS'})

out={'scope':__doc__,'supported_executable_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6',
     'capture_image_sha256':hashlib.sha256(image).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows,
     'native_code_sha256':{f'{lo:08X}-{hi:08X}':hashlib.sha256(image[lo-BASE:hi-BASE]).hexdigest() for lo,hi in [(0x64dab0,0x64de7d),(0x65c780,0x65c7d1),(0x741970,0x741a80),(0x700600,0x7008a6)]}}
Path(a.output).write_text(json.dumps(out,indent=2)+'\n')
print('PASS',len(rows),'captured native CRC/input/mission cases; no live gameplay or multiplayer validation')

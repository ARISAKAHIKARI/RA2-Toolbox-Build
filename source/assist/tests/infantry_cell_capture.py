"""Captured native infantry click classification, event send and cell-entry gates.

Synthetic keyboard/map/world and network-ID packing queries. Native action
selection, click dispatch, ClickedMission, QueueMegaMission and Event constructor
execute unchanged. Geometry-driven selection of object versus cell is NOT
reproduced; forced action-boundary cases prove dispatch only. No full pathfinding,
arrival, building capture/destruction or live multiplayer validation.
"""
from pathlib import Path
SCOPE=__doc__
exec(compile(Path(__file__).with_name('network_state_capture.py').read_text().split('rows=[crc_case',1)[0],str(Path(__file__).with_name('network_state_capture.py')),'exec'))

def setup(engineer=True,c4=False,mods=(17,18)):
    q=Probe();unit=S;typ=S+0x2000;building=S+0x4000;bt=S+0x6000;house=S+0x8000;enemy=S+0xa000;rules=S+0xc000;scenario=S+0xe000;cell=S+0x10000;coords=S+0x12000
    q.vt(unit,0x7eb058,{0x1d4:(0,0),0x1d8:(0,0),0x330:(1,0),0xa0:(1,0),0x9c:(1,0),0x48:(coords,4),0x84:(typ,0),0x2ac:(0,0)})
    q.vt(building,0x7e3ebc,{0x48:(coords,4),0x80:(0,0),0x1d4:(0,0),0x88:(bt,0)})
    q.w(unit+0x6c0,typ);q.w(unit+0x21c,house);q.b(typ+0xec3,engineer);q.b(typ+0xec2,c4)
    q.w(building+0x520,bt);q.w(building+0x21c,enemy);q.w(building+0x6c,25);q.w(bt+0xa0,100);q.b(bt+0xccc,1);q.b(bt+0x1572,1)
    q.w(enemy+0x34,S+0x14000);q.w(0xa83d4c,house);q.w(0x8871e0,rules);q.w(0xa8b230,scenario);q.w(cell+0x44,-1);q.w(cell+0xe4,building)
    q.u.mem_write(rules+0x17f8,struct.pack('<f',0.5));q.u.mem_write(rules+0x16f8,struct.pack('<d',1.0));q.u.mem_write(coords,struct.pack('<iii',20*256+128,30*256+128,0))
    # Preserve the sample's actual default key mapping: Alt, Ctrl, Shift.
    assert [q.r(at) for at in range(0xa8ebf8,0xa8ec10,4)]==[18,18,17,17,16,16]
    for addr,pop,value in ((0x5657a0,4,cell),(0x565730,4,cell),(0x578080,4,0),(0x586360,4,0),(0x50b6f0,0,1),(0x4f9a90,4,0),(0x6f3970,4,-1 if engineer else 0),(0x457ce0,4,0),(0x5023b0,4,0),(0x578460,8,1)):
        q.stubs[addr]=lambda value=value,pop=pop:q.ret(value,pop)
    q.stubs[0x54f5c0]=lambda:q.ret(int(q.r(q.u.reg_read(UC_X86_REG_ESP)+4) in mods),4)
    return q,unit,building,cell

def sender(q,unit,building,cell):
    # Identity resolution only; event construction and OutList append stay native.
    ids={0:(0,0),unit:(10,0x34),building:(12,0x34),cell:(30020,11)}
    def pack():
        sp=q.u.reg_read(UC_X86_REG_ESP);obj=q.r(sp+4);dest=q.u.reg_read(UC_X86_REG_ECX)
        assert obj in ids,hex(obj)
        value,tag=ids[obj];q.w(dest,value);q.b(dest+4,tag);q.ret(dest,4)
    q.stubs[0x6e6ab0]=pack
    q.stubs[0x731bf0]=lambda:q.ret(0)
    q.stubs[0x637aa0]=lambda:q.ret(0)
    q.b(0x822cf2,0);q.w(0xa8ed84,100);q.w(0xa8b238,3)
    for at in (0xa802c8,0xa802cc,0xa802d0):q.w(at,0)
    timer=S+0xf0000;q.w(0x7e1530,timer);q.stubs[timer]=lambda:q.ret(123)

rows=[]
# Complete original object classification and click dispatch for an engineer.
for mods in ((),(17,),(18,),(17,18)):
    q,unit,building,cell=setup(mods=mods);sender(q,unit,building,cell)
    action=q.run(0x51e3b0,unit,(building,0));assert action==9
    q.run(0x51f190,unit,(action,building,0))
    b=bytes(q.u.mem_read(0xa802d4,111))
    assert q.r(0xa802c8)==1 and b[0]==4 and b[12]==8
    assert b[14:19]==bytes(5) and b[19:24]==struct.pack('<IB',12,0x34)
    rows.append({'case':'native_engineer_object_click','keys':list(mods),'action':9,'event_type':4,'mission':8,'target':'none','destination':'building_network_ID','status':'PASS'})
# When the native input is a cell click, Ctrl+Alt produces GuardArea in this
# ordinary non-extension fixture, even for an engineer. No geometry claim.
for engineer in (False,True):
    q,unit,building,cell=setup(engineer=engineer);sender(q,unit,building,cell)
    coord=S+0x16000;q.u.mem_write(coord,struct.pack('<hh',20,30))
    action=q.run(0x51f800,unit,(coord,0,0));assert action==26
    q.run(0x51f250,unit,(action,coord,0,0))
    b=bytes(q.u.mem_read(0xa802d4,111))
    assert q.r(0xa802c8)==1 and b[12]==11 and b[14:19]==struct.pack('<IB',30020,11) and b[19:24]==bytes(5)
    rows.append({'case':'native_ctrl_alt_cell_click','engineer':engineer,'action':26,'mission':11,'target':'cell','destination':'none','status':'PASS'})
# Observe the real cell-dispatch branches, with ONLY the already-selected
# Action substituted. This does not prove an edge/overlap selects that Action.
for action,mission in ((9,8),(16,17)):
    q,unit,building,cell=setup();sender(q,unit,building,cell)
    slot=q.r(q.r(unit)+0x70);q.stubs[slot]=lambda action=action:q.ret(action,12)
    coord=S+0x16000;q.u.mem_write(coord,struct.pack('<hh',20,30))
    q.run(0x4d7d50,unit,(action,coord,0,0))
    b=bytes(q.u.mem_read(0xa802d4,111))
    assert b[0]==4 and b[12]==mission and b[14:19]==bytes(5) and b[19:24]==struct.pack('<IB',30020,11)
    rows.append({'case':'selected_action_cell_dispatch_boundary','injected_action':action,'mission':mission,'target':'none','destination':'cell','native_sender_and_constructor':True,'status':'PASS'})
# Execute both the mission gate AND the subsequent destination comparisons.
for mission in (8,17,11):
    for engineer,c4 in ((False,False),(True,False),(False,True)):
        for dest in ('building','building_cell','other_cell'):
            q,unit,building,cell=setup(engineer,c4);q.w(unit+0xac,mission)
            q.w(unit+0x5a4,building if dest=='building' else cell if dest=='building_cell' else S+0x18000)
            q.stubs[0x47c520]=lambda:q.ret(building)
            ends=[]
            for at in (0x51c71b,0x51c41c):q.stubs[at]=lambda at=at:(ends.append(at),q.u.reg_write(UC_X86_REG_EIP,STOP))
            q.u.reg_write(UC_X86_REG_EBP,unit);q.u.reg_write(UC_X86_REG_ESI,building);q.u.reg_write(UC_X86_REG_ESP,STACK)
            q.u.emu_start(0x51c2e1,STOP,count=10000)
            admitted=(mission==8 or mission==17 and c4 or mission==11 and engineer) and dest!='other_cell'
            assert ends==[0x51c71b if admitted else 0x51c41c],(mission,engineer,c4,dest,ends)
            rows.append({'case':'native_mission_and_destination_gate','mission':mission,'engineer':engineer,'C4':c4,'destination':dest,'passed_this_gate':admitted,'status':'PASS'})
for mission in (8,17):
    q,unit,building,cell=setup(False,False);q.w(unit+0xac,mission);q.w(unit+0x5a4,cell);q.w(unit+0x2b4,0)
    q.stubs[0x50b730]=lambda:q.ret(1);q.stubs[0x65c7e0]=lambda:q.ret(0,8)
    q.run(0x4d4b20,unit)
    assert q.r(unit+0xac)==mission and q.r(unit+0x5a4)==cell and q.r(unit+0x2b4)==0
    rows.append({'case':'native_capture_sabotage_tick_keeps_cell_destination','mission':mission,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':SCOPE,'supported_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6','game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'native classification/dispatch/serialization/destination-gate cases')

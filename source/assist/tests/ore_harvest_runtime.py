"""Compiled Runtime native-unload observation before cadence and real Harvest queue.

The matching shared ABI comes from entry_runtime_regression. The Runtime,
original event constructor and queue run in x86 emulation; world/Win32 inputs
are controlled fixtures. This is not a live game or multiplayer test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]

def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def wake(m,delta=16):
    m.clock+=delta;m.w(0xA8ED84,m.r(0xA8ED84)+1);m.w(m.SH+m.Shared.heartbeat.offset,m.clock);m.run(m.entry,(0,1,0));return state(m)
def queued(m):return m.r(0xA802C8)+m.r(0x8B41F8)
def resource(m,items,x,y,overlay=0):
    cell=m.alloc(0x200);m.u.mem_write(cell+0x24,struct.pack('<2h',x,y));m.w(cell+0x44,overlay);m.w(cell+0xEC,0);m.w(items+(y*512+x)*4,cell);return cell
def fixture():
    spec=importlib.util.spec_from_file_location('ore_runtime_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
    m.w(0xA8B230,m.alloc());m.w(0xA8B238,3);m.w(m.house+0x10,101);m.w(m.t+0x10,301);m.w(m.buildT+0x10,501)
    m.b(m.t+0xE0E,1);m.w(m.t+0x800,20);m.w(m.t+0x67C,1);m.vector(m.t+0x3E8,[m.buildT]);m.vector(0x8B4108,[m.unit]);m.vector(0xA8EB40,[m.building]);m.vector(0xA8ECB8,[m.unit])
    m.w(m.unit+0xAC,10);m.w(m.unit+0xB4,0xffffffff);m.w(m.unit+0x5A4,0);m.b(m.unit+0x6D1,0);m.u.mem_write(m.unit+0x33C,struct.pack('<4f',10,0,0,0))
    m.u.mem_write(m.unit+0x9C,struct.pack('<3i',101*256,102*256,0));m.u.mem_write(m.building+0x9C,struct.pack('<3i',100*256,100*256,0))
    m.links=m.alloc(4);m.w(m.links,m.unit);m.w(m.building+0xE4,m.links);m.w(m.building+0xE8,0)
    m.u.mem_write(0x87F7E8+0xF4,struct.pack('<6i',100,100,0,0,100,100));m.ore_cells=m.alloc(512*512*4);m.w(0x87F924,m.ore_cells);m.w(0x87F928,512*512)
    overlay=m.alloc(0x400);m.b(overlay+0x2A9,1);m.w(overlay+0x294,0);m.vector(0xA83D80,[overlay]);tib=m.alloc(0x100);m.w(tib+0xE0,overlay);m.w(tib+0xE8,1);m.w(tib+0xEC,0);m.w(tib+0xB8,25);m.vector(0xB0F4E8,[tib])
    m.near_resource=resource(m,m.ore_cells,106,102);m.far_resource=resource(m,m.ore_cells,130,102)
    for land in range(12):m.u.mem_write(0x89EA40+(land*9+1)*4,struct.pack('<f',0 if land==2 else 1))
    s=state(m);s.flags=32768;s.computeRequested=s.computeApplied=32767;s.scanMs=1000;s.externalDisplay=1;s.megaQueueMode=1;save(m,s)
    s=wake(m);assert s.data.status==1 and s.data.commandStatus==1,(s.data.status,s.data.commandStatus);assert queued(m)==0
    return m
def start(m):
    m.b(m.unit+0x6D1,1);m.w(m.unit+0xAC,16);m.w(m.unit+0x5A4,m.building);m.w(m.building+0xE8,1);return wake(m)
def finish(m):
    m.u.mem_write(m.unit+0x33C,bytes(16));m.b(m.unit+0x6D1,0);m.w(m.unit+0xAC,10);m.w(m.unit+0x5A4,m.far_resource);m.w(m.building+0xE8,0)
    for n in range(8):
        s=wake(m)
        if queued(m):return s
    return s
def check(name,fn):
    try:rows.append({'case':name,'status':'PASS',**(fn()or{})});print('PASS',name,flush=True)
    except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()
def native_handoff():
    m=fixture();s=state(m);seq=s.worldSeq;serial=s.computeFrameSerial
    s=wake(m);assert s.worldSeq==seq and s.computeFrameSerial==serial and queued(m)==0
    s=start(m);assert s.worldSeq==seq and s.computeFrameSerial==serial and queued(m)==0
    m.u.mem_write(m.unit+0x33C,bytes(16));s=wake(m);assert s.worldSeq==seq and queued(m)==0
    s=finish(m);assert queued(m)==1 and s.worldSeq!=seq,(queued(m),s.worldSeq,seq,s.data.lastSubmitError)
    packet=bytes(m.u.mem_read(0xA802D4,111));assert packet[0]==4 and struct.unpack_from('<I',packet,7)[0]==10 and packet[0xC]==10
    assert struct.unpack_from('<I',packet,0xE)[0]==0 and packet[0x12]==0
    assert struct.unpack_from('<I',packet,0x13)[0]==102106 and packet[0x17]==11
    # Native TargetClass keeps an opaque ID when the tag is None. The tag,
    # rather than unrelated payload bytes, is the native reference contract.
    assert packet[0x1C]==0
    for n in range(10):wake(m)
    assert queued(m)==1
    return {'all_runtime_mask':32767,'scan_ms':1000,'no_heavy_world_resample_during_native_unload':True,'native_unload_without_tool_enter_observed':True,'empty_hold_while_still_unloading_no_order':True,'one_original_harvest_event':True,'nearest_from_refinery_dock':102106,'target_none_destination_cell_nearest_none':True,'native_none_tag_opaque_id':struct.unpack_from('<I',packet,0x18)[0]}
check('native-unload-before-cadence-to-one-real-Harvest-event',native_handoff)
def manual_wins():
    m=fixture();start(m);s=state(m);s.command=30;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);s=wake(m)
    assert s.ack==s.request and C.c_int32(s.result).value==1 and queued(m)==1,(s.ack,s.request,s.result,queued(m))
    packet=bytes(m.u.mem_read(0xA802D4,111));assert packet[0xC]==5
    finish(m)
    for n in range(6):wake(m)
    assert queued(m)==1
    return {'actual_manual_SetGuard_command_queued':True,'original_unload_episode_cancelled':True,'no_extra_harvest_after_manual_command':True}
check('later-explicit-player-command-wins',manual_wins)
def replaced_world():
    m=fixture();start(m);m.w(0xA8B230,m.alloc());finish(m);assert queued(m)==0
    return {'scenario_replacement_discards_old_unload_evidence':True}
check('world-replacement-no-old-Harvest',replaced_world)
def queued_mission_wins():
    m=fixture();start(m);m.w(m.unit+0xB4,5);finish(m)
    assert queued(m)==0,'a native queued Guard must retain precedence even without an input token change'
    return {'native_queued_Guard_preserved_without_input_capture':True,'no_extra_harvest':True}
check('uncaptured-native-queued-mission-wins',queued_mission_wins)
report={'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'passed':all(r['status']=='PASS'for r in rows),'game_process_operated':False,'multiplayer_tested':False,'results':rows}
Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(not report['passed'])

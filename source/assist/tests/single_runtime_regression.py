"""Actual unified DLL ingress and client coexistence, with mocked OS transport."""
import argparse,importlib.util,ctypes as C,json,hashlib,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def fixture():
 spec=importlib.util.spec_from_file_location('single_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m);return m
m=fixture();ctrl=m.SH+0x3e0000;msg=m.SH+0x3e0100;cs=0x22000000
m.u.mem_map(cs,0x500000);m.cursor_mapping_enabled=True
m.u.mem_write(cs,struct.pack('<7I',0x4355523a,123,456,3,99,0,m.clock))
def state():return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(s):m.u.mem_write(m.SH,bytes(s))
def dispatch(mask,wp=1):m.w(ctrl+20,mask);m.run(m.entry,(0,wp,msg))
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
m.hook();m.w(ctrl+16,3)
s=state();s.command=1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(s)
dispatch(3);s=state();assert s.ack==s.request and m.r(cs+12)==0 and m.r(cs+16)==0
rc,q=m.manager_call(m.request(3));assert rc==1 and q.activeModule==0 and q.reentrant==0
passed('cursor_and_tactical_execute_same_wake_same_module_without_contention')
s=state();s.command=-1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;before=s.ack;save(s)
dispatch(0);assert state().ack==before
passed('no_host_ready_bit_no_command_dispatch')
dispatch(3,0);assert m.r(ctrl+20)==3 and state().ack==before
passed('peek_without_remove_retains_ready_work')
rc,lease=m.manager_call(m.request(0));assert rc==1
dispatch(3);assert m.r(ctrl+20)==3 and state().ack==before
lease.operation=1;assert m.manager_call(lease)[0]==1
m.run(m.entry,(0,1,msg));assert state().ack==state().request and m.r(ctrl+20)==0
passed('nested_wake_retains_work_then_commits_once_after_release')
m.current_thread=778;dispatch(3);assert m.r(ctrl+20)==3;m.current_thread=777
m.run(m.entry,(0,1,msg));assert m.r(ctrl+20)==0
passed('wrong_thread_does_not_consume_host_work')
m.w(ctrl+16,2);m.w(cs+12,3);m.w(cs+16,99);dispatch(3);assert m.r(cs+16)==0
passed('cursor_subscription_survives_tactical_disable')
m.w(ctrl+16,1);m.w(cs+12,3);dispatch(3);assert m.r(cs+12)==3
passed('disabled_cursor_is_not_dispatched')
s=state();s.command=-1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;before=s.ack;save(s)
assert m.watchdog_callback;m.run(m.watchdog_callback,(0,0,1,m.clock));assert state().ack==before
passed('watchdog_does_not_execute_live_pending_commands')
s=state();s.stop=1;save(s);m.w(cs+12,2);m.run(m.watchdog_callback,(0,0,1,m.clock));assert state().stop==2 and m.r(cs+12)==0 and m.r(cs+16)==0
passed('watchdog_restores_stopped_clients_without_host_wake')
# Reopen the same cursor mapping after a completed stop, without old state.
m.w(cs+12,3);m.w(cs+16,99);m.w(ctrl+16,2);dispatch(2);assert m.r(cs+16)==0 and m.r(cs+12)==0
passed('cursor_stop_then_reattach_reopens_mapping')
# Actual cursor service calls the guarded original Hide/Show ABI. Only the
# game drawing functions are counted stubs; ownership/restoration is real DLL code.
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX
mouse=m.alloc();vt=m.alloc();m.w(mouse,vt);m.w(mouse+12,0);m.w(vt+4,0x7b8a00);m.w(vt+12,0x7b9930);m.w(vt+16,0x7b9750);m.w(0x887640,mouse)
visibility=[]
def visibility_call(u,at,size,unused):
 assert u.reg_read(UC_X86_REG_ECX)==mouse
 delta=-1 if at==0x7b9930 else 1;visibility.append(delta);m.w(mouse+12,m.r(mouse+12)+delta);m.ret(0,0)
m.u.hook_add(UC_HOOK_CODE,visibility_call,begin=0x7b9930,end=0x7b9930)
m.u.hook_add(UC_HOOK_CODE,visibility_call,begin=0x7b9750,end=0x7b9750)
m.w(cs+12,1);dispatch(2);assert visibility==[-1] and m.r(mouse+12)==0xffffffff and m.r(cs+16)==1
m.w(cs+12,2);m.run(m.watchdog_callback,(0,0,1,m.clock));assert visibility==[-1,1] and m.r(mouse+12)==0 and m.r(cs+16)==0
m.run(m.watchdog_callback,(0,0,1,m.clock));assert visibility==[-1,1]
passed('hide_and_stop_restore_are_paired_once_through_shared_execution_token')
m.w(cs+12,1);dispatch(2);assert visibility==[-1,1,-1]
m.process_dead=True;m.run(m.watchdog_callback,(0,0,1,m.clock));assert visibility==[-1,1,-1,1] and m.r(mouse+12)==0
passed('dead_host_watchdog_restores_cursor_without_new_work')
n=fixture();n.hook();assert n.watchdog_threads==[777]
n.current_thread=778;n.dead_threads.add(777);n.hook();assert n.watchdog_threads==[777,778]
n.hook();assert n.watchdog_threads==[777,778]
passed('watchdog_rearmed_once_after_confirmed_game_thread_migration')
n=fixture();n.unique_process_handles=True;n.w(n.SH+0x3e0000+16,0);n.hook()
assert len(n.opened_process_handles)==1
n.hook();assert len(n.opened_process_handles)==1 # same live process is reused
n.dead_process_handles.add(n.opened_process_handles[0]);n.hook()
assert len(n.opened_process_handles)==2 # same PID, new process object
n.w(n.SH+0x3e0000+16,1);assert n.hook().data.status==1
passed('recycled_host_pid_reconnects_instead_of_reusing_dead_controller_handle')
# Exercise the actual frame callback installed by this DLL. Extract its direct
# call target from the naked observer's CLD/CALL sequence, not a source model.
n=fixture();s=n.hook();s.driverMode=1;s.flags=0;s.externalDisplay=1;n.u.mem_write(n.SH,bytes(s));n.hook()
site=0x55d360;assert n.u.mem_read(site,1)==b'\xe9'
gate=site+5+struct.unpack('<i',n.u.mem_read(site+1,4))[0]
code=bytes(n.u.mem_read(gate,96));at=code.index(b'\xfc\xe8');frameCall=gate+at+6+struct.unpack('<i',code[at+2:at+6])[0]
for i in range(3):n.w(0xa8ed84,n.r(0xa8ed84)+1);n.run(frameCall)
s=n.Shared.from_buffer_copy(n.u.mem_read(n.SH,C.sizeof(n.Shared)));assert s.driverStatus==2,s.driverStatus
s.command=-1;s.request+=1;s.requestTick=n.clock;s.requestMatch=s.data.match;before=s.ack;n.u.mem_write(n.SH,bytes(s))
n.w(0xa8ed84,n.r(0xa8ed84)+1);n.run(frameCall)
assert n.r(n.SH+n.Shared.ack.offset)==before
passed('frame_callback_without_host_grant_cannot_execute_pending_command')
n.hook();assert n.r(n.SH+n.Shared.ack.offset)==before
n.w(0xa8ed84,n.r(0xa8ed84)+1);n.run(frameCall)
assert n.r(n.SH+n.Shared.ack.offset)==s.request
s=n.Shared.from_buffer_copy(n.u.mem_read(n.SH,C.sizeof(n.Shared)));s.request+=1;s.requestTick=n.clock;n.u.mem_write(n.SH,bytes(s))
n.w(0xa8ed84,n.r(0xa8ed84)+1);n.run(frameCall);assert n.r(n.SH+n.Shared.ack.offset)!=s.request
passed('frame_callback_consumes_host_grant_once_and_waits_for_next')
n=fixture();n.legacy_loaded=True;n.hook();assert n.r(n.SH+n.Shared.seq.offset)==0
passed('legacy_injected_runtime_blocks_new_generation')
a.output.write_text(json.dumps({'scope':'actual single DLL; mocked OS messages, cursor resource-only session and synthetic game objects; not native Windows or netplay','inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')

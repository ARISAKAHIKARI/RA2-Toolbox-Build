"""Actual DLL entry/handshake diagnostics with synthetic Win32 transport; not native loader validation."""
import argparse,importlib.util,ctypes as C,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def fixture():
 spec=importlib.util.spec_from_file_location('diagnostics_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m);return m
class Diagnostic(C.Structure):
 _fields_=[('seq',C.c_uint32),('callbacks',C.c_uint32),('tick',C.c_uint32),('thread',C.c_uint32),('stage',C.c_int32),('error',C.c_uint32),('base',C.c_uint32),('rva',C.c_uint32)]
def diagnostic(m):return Diagnostic.from_buffer_copy(m.u.mem_read(m.SH+0x3e0000+40,32))
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
m=fixture();s=m.hook();d=diagnostic(m)
assert d.callbacks==1 and d.seq%2==0 and d.thread==777 and d.tick==m.clock and d.stage==2 and not d.error
assert d.base==m.dll.OPTIONAL_HEADER.ImageBase and d.base+d.rva==m.entry and s.data.status==1
assert set(m.mapping_names)=={'Local\\RA2Runtime_v3_123_456_','Local\\RA2Assist_v042_123'}
passed('actual_export_RVA_thread_namespace_and_first_callback_match')
m.hook();d=diagnostic(m);assert d.callbacks==2 and d.stage==2
passed('subsequent_wake_reuses_runtime_and_keeps_reporting')
for knob,stage,error in [('legacy_loaded',-2,0),('fail_owner_open',-4,5),('process_dead',-5,0),('fail_watchdog_pin',-6,8),('fail_watchdog_timer',-7,8)]:
 m=fixture();setattr(m,knob,True);s=m.hook();d=diagnostic(m)
 assert d.callbacks==1 and d.stage==stage and d.error==error and d.thread==777,(knob,d.stage,d.error)
 assert m.r(m.SH+0x3e0000+20)==1 and not s.seq and not s.ack and not m.r(m.SH+0x3e0000+32)
 passed(knob+'_explicit_rejection_preserves_ready_work')
 # The same mailbox must be usable after a transient failure clears.
 if knob!='process_dead':
  setattr(m,knob,False);s=m.hook();assert diagnostic(m).stage==2 and s.data.status==1
passed('transient_failure_retry_connects_without_diagnostic_consuming_work')
m=fixture();m.hook();m.current_thread=778;m.hook();d=diagnostic(m)
assert d.stage==-1 and d.thread==778 and m.r(m.SH+0x3e0000+20)==1
m.current_thread=777;m.hook();assert diagnostic(m).stage==2 and m.r(m.SH+0x3e0000+20)==0
passed('wrong_thread_reported_then_bound_thread_resumes')
m=fixture();msg=m.SH+0x3e0100;m.run(m.entry,(0,0,msg));assert diagnostic(m).callbacks==0 and m.r(m.SH+0x3e0000+20)==1
passed('peek_without_remove_does_not_report_or_consume_wake')
m=fixture();m.w(m.SH+0x3e0000+20,0);m.run(m.entry,(0,1,m.SH+0x3e0100));d=diagnostic(m)
assert d.stage==2 and d.callbacks==1 and m.r(m.SH+0x3e0000+32)==0 and not m.r(m.SH+m.Shared.seq.offset)
passed('diagnostic_handshake_does_not_create_host_work')
m=fixture();m.w(m.SH+0x3e0000+4,1);s=m.hook();assert diagnostic(m).callbacks==0 and not s.seq
passed('wrong_protocol_mapping_is_not_written_or_accepted')
m=fixture();m.unique_process_handles=True;m.w(m.SH+0x3e0000+16,0);m.hook();m.dead_process_handles.add(m.opened_process_handles[0]);m.hook()
assert diagnostic(m).stage==2 and diagnostic(m).callbacks==2 and len(m.opened_process_handles)==2
passed('dead_retained_controller_reconnect_keeps_diagnostic_view_valid')
a.output.write_text(json.dumps({'scope':__doc__,'inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')

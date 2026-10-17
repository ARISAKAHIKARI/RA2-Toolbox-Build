"""Actual DLL SP queue/pause/chat tests; OS, game world and time are synthetic."""
import argparse,importlib.util,ctypes as C,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,5):
 spec=importlib.util.spec_from_file_location('console_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.w(0x8b41f8,0);m.w(0x8b41fc,0);m.w(0x8b4200,0);m.w(0xa802c8,0)
 s=m.hook();assert s.data.status==s.data.commandStatus==1 and s.data.match;epoch=s.data.match
 passed(f'sp_{mode}_message_ingress_authenticates_match')
 s.command=1;s.request+=1;s.requestMatch=epoch;s.requestTick=m.clock;before=s.ack;m.u.mem_write(m.SH,bytes(s));m.b(0xa8e9a0,0)
 s=m.hook();assert s.data.status==1 and s.data.commandStatus==0 and s.data.match==epoch and s.ack==before
 passed(f'sp_{mode}_pause_preserves_identity_and_pending_request')
 m.b(0xa8e9a0,1);s=m.hook();assert s.ack==s.request and s.result==1 and s.count==1 and s.data.match==epoch
 assert m.r(0x8b41f8)==1 and m.r(0xa802c8)==0
 packet=bytes(m.u.mem_read(0x8b4204,111));assert packet[0]==0x15 and int.from_bytes(packet[3:7],'little')==m.r(0xa8ed84)+1
 assert s.driverMode==0 and bytes(m.u.mem_read(0x55d360,1))!=b'\xe9'
 passed(f'sp_{mode}_resume_commits_once_to_DoList_without_frame_hook')
 m.b(0xa8bc60+25,1);s.command=1;s.request+=1;s.requestMatch=epoch;s.requestTick=m.clock;m.u.mem_write(m.SH,bytes(s));s=m.hook()
 assert s.ack==s.request and C.c_int32(s.result).value==-8 and m.r(0x8b41f8)==1,(s.ack,s.request,s.result,m.r(0x8b41f8))
 passed(f'sp_{mode}_chat_has_explicit_rejection_no_duplicate_event')
a.output.write_text(json.dumps({'scope':__doc__,'inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')

"""Actual x86 GameRuntime + original Produce constructor; mocked world/CanBuild. Not live netplay."""
import argparse,ctypes as C,hashlib,importlib.util,json,traceback
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture(mode):
 spec=importlib.util.spec_from_file_location('construction_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=268435456;s.externalDisplay=1;m.canbuild_full_result=1;m.u.mem_write(m.SH,bytes(s))
 strip=0x87f7e8+0x1544;m.w(strip+84,1);m.w(strip+88,0);m.w(strip+92,7);s=m.hook()
 return m,s
for mode in (0,3,4,5):
 for case in ('ready','no_cameo','no_prerequisite','in_limbo','canceled_parent','not_owner','occupied','disabled','old_round'):
  name=f'{mode}_{case}'
  try:
   m,s=fixture(mode);slot=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==7)
   if case=='no_cameo':m.w(0x87f7e8+0x1544+84,0)
   if case=='no_prerequisite':m.canbuild_full_result=0
   if case=='in_limbo':m.b(m.building+0x81,1);m.b(m.building+0x90,0)
   if case=='canceled_parent':m.w(m.building+0x6c,0)
   if case=='not_owner':m.w(m.building+0x21c,m.enemy)
   if case=='occupied':
    f=m.alloc();m.w(m.house+0x53bc,f);m.w(f+0x6c,m.house);m.w(f+0x58,m.building)
   if case=='disabled':s.flags=0
   before=bytes(m.u.mem_read(m.buildT,0x2000));s.command=20;s.arg=slot;s.arg2=12;s.requestToken=73;s.request+=1;s.requestMatch=s.data.match+(case=='old_round');s.requestTick=m.clock;m.u.mem_write(m.SH,bytes(s));s=m.hook()
   assert s.ack==s.request,(s.ack,s.request)
   count=m.r(0x8b41f8)+m.r(0xa802c8);assert count==int(case=='ready'),(count,C.c_int32(s.result).value,s.data.lastSubmitError)
   assert bytes(m.u.mem_read(m.buildT,0x2000))==before
   if case=='ready':
    assert s.result==1 and s.count==1 and m.canbuild_calls[-1][2:]==(0,1)
    at=0x8b4204 if mode in (0,5) else 0xa802d4;packet=bytes(m.u.mem_read(at,111));assert packet[0]==0x0e,packet.hex()
    for _ in range(3):m.hook()
    assert m.r(0x8b41f8)+m.r(0xa802c8)==1
   rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
  except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

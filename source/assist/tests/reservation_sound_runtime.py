"""Actual x86 GameRuntime feedback dispatch; native audio device call is observed/stubbed.
Not an audible Windows or real multiplayer test. Checks no added event/code writes.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
for mode in (0,3):
 for case in ('fresh','deadline','expired','old_round','background','disabled','chat','menu','observer','no_rules','silent_rule','signature','with_produce','wrong_thread'):
  name=f'{mode}_{case}'
  try:
   spec=importlib.util.spec_from_file_location('sound_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
   m.w(0xa8b238,mode);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=268435456;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s));s=m.hook()
   played=[];writes=[]
   def play(uc,at,size,_):
    sp=m.u.reg_read(UC_X86_REG_ESP);played.append((m.u.reg_read(UC_X86_REG_ECX),m.u.reg_read(UC_X86_REG_EDX),m.r(sp+4),m.r(sp+8),m.current_thread));m.ret(0,2)
   m.u.hook_add(UC_HOOK_CODE,play,begin=0x750920,end=0x750920)
   rules=m.alloc(0x2000);m.w(0x8871e0,rules);m.w(rules+0x18c,37)
   s.uiSoundRequest+=1;s.uiSoundTick=m.clock;s.uiSoundMatch=s.data.match
   if case=='deadline':s.uiSoundTick-=190
   if case=='expired':s.uiSoundTick-=191
   if case=='old_round':s.uiSoundMatch+=1
   if case=='background':s.foreground=0
   if case=='disabled':s.flags=0
   if case=='chat':m.b(0xa8bc60+25,1)
   if case=='menu':m.w(0xa8eda0,1)
   if case=='observer':m.w(0xac1198,m.house)
   if case=='no_rules':m.w(0x8871e0,0)
   if case=='silent_rule':m.w(rules+0x18c,0xffffffff)
   if case=='signature':m.u.mem_write(0x750920,b'\xe9')
   if case=='wrong_thread':m.current_thread=778
   if case=='with_produce':
    strip=0x87f7e8+0x1544;m.w(strip+84,1);m.w(strip+88,0);m.w(strip+92,7);m.canbuild_full_result=1
    s.command=20;s.arg=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==7);s.arg2=12;s.requestToken=73;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock
   section=next(x for x in m.game.sections if x.Name.rstrip(b'\0')==b'.text');base=m.game.OPTIONAL_HEADER.ImageBase+section.VirtualAddress
   m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,size,value,_:writes.append((at,size)),begin=base,end=base+section.Misc_VirtualSize-1)
   before=(bytes(m.u.mem_read(m.house,0x6000)),bytes(m.u.mem_read(m.buildT,0x2000)))
   m.u.mem_write(m.SH,bytes(s));s=m.hook()
   if case in ('menu','wrong_thread'):
    assert s.uiSoundAck!=s.uiSoundRequest and not played
    m.w(0xa8eda0,0);m.current_thread=777;m.clock+=400;s=m.hook()
   assert s.uiSoundAck==s.uiSoundRequest,(s.uiSoundAck,s.uiSoundRequest)
   expected=int(case in ('fresh','deadline','with_produce'))
   assert len(played)==expected,played
   if expected:assert played==[(37,0x2000,0x3f800000,0,777)],played
   for _ in range(3):s=m.hook()
   assert len(played)==expected and not writes
   events=m.r(0x8b41f8)+m.r(0xa802c8);assert events==int(case=='with_produce'),events
   if case=='with_produce':assert s.ack==s.request and s.result==1 and s.count==1
   assert before==(bytes(m.u.mem_read(m.house,0x6000)),bytes(m.u.mem_read(m.buildT,0x2000)))
   rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
  except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

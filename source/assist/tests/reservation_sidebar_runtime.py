"""Actual DLL common message capture and native sidebar busy-state branch.
World, Win32 and audio device are mocked. No real Windows/LBW/netplay test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode=0,tab=0):
 spec=importlib.util.spec_from_file_location('sidebar_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);s=state(m);s.flags=268435456;s.externalDisplay=1;save(m,s);s=m.hook()
 m.tab=tab;m.strip=0x87f7e8+0x1544+tab*3988;m.factory=m.alloc();m.parent=m.alloc(0x2000);m.w(m.parent,0x7e3ebc);m.w(m.parent+0x10,90);m.w(m.parent+0x520,m.buildT);m.w(m.factory+0x58,m.parent);m.w(m.factory+0x6c,m.house);m.w(m.factory+0x24,12);m.w(m.house+(0x53cc if tab else 0x53bc),m.factory)
 m.w(m.buildT+0xe08,5 if tab else 0);m.w(0x87f7e8+0x539c,tab);m.b(m.strip+0x1c,1);m.w(m.strip+0x38,tab);m.w(m.strip+0x54,2)
 for i in range(2):m.w(m.strip+88+i*52,i);m.w(m.strip+92+i*52,7)
 m.w(0x886f94,0);m.w(0x886f9c,600);m.w(0xb0b4f8,200);session=m.alloc(0x4000);m.w(0xa8b230,session)
 for i in range(14):
  g=0xb07e80+tab*60*56+i*56;m.w(g+12,640+i%2*64);m.w(g+16,201+i//2*50);m.w(g+20,60);m.w(g+24,48)
 m.msg=m.alloc();m.w(m.SH+0x3e0000+20,0);s=state(m);s.reservePolicy=m.ReservationPolicy(s.data.match,m.house,m.parent,90,m.clock,tab,5);save(m,s)
 m.code_writes=[];sec=next(x for x in m.game.sections if x.Name.rstrip(b'\0')==b'.text');base=m.game.OPTIONAL_HEADER.ImageBase+sec.VirtualAddress
 m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,size,value,_:m.code_writes.append((at,size)),begin=base,end=base+sec.Misc_VirtualSize-1)
 return m

def message(m,kind,x=660,y=220,keys=0):
 m.u.mem_write(m.msg,struct.pack('<7I',1,kind,keys,(x&65535)|((y&65535)<<16),m.clock,0,0));m.run(m.entry,(0,1,m.msg));return m.r(m.msg+4),state(m)
def event_count(m):return m.r(0x8b41f8)+m.r(0xa802c8)
for mode in (0,3):
 for case in ('normal','defense','double_click','scaled','idle','completed','completed_other','manual_pause','ui_pause','zero_slots','busy_input','policy_odd','old_policy','old_round','parent_reused','menu','chat','observer','disabled','outside','unsupported','right','drag','scroll','changed_parent','lost_focus','expired_hold','full_outlist','frame_advanced','frame_rollback','engine_paused','cancel_right','cancel_shift','cancel_full','cancel_completed','cancel_notqueued'):
  name=f'input_{mode}_{case}'
  try:
   m=fixture(mode,case=='defense');s=state(m)
   if case=='scaled':m.client_width=1600;m.client_height=1200
   if case=='idle':m.w(m.factory+0x58,0)
   if case in ('completed','completed_other'):m.w(m.factory+0x24,54)
   if case=='completed_other':
    other=m.alloc(0x2000);m.w(other+0xdf8,1);m.w(m.parent+0x520,other)
   if case=='manual_pause':m.b(m.factory+0x70,1);m.b(m.factory+0x71,1)
   if case=='ui_pause':m.w(m.strip+88+0x10,2)
   if case=='zero_slots':s.reservePolicy.slots=0
   if case=='busy_input':s.reserveInput=2;s.reserveConsumed=1
   if case=='policy_odd':s.reservePolicySeq=1
   if case=='old_policy':s.reservePolicy.tick-=151
   if case=='old_round':s.reservePolicy.match+=1
   if case=='parent_reused':m.w(m.parent+0x10,91)
   if case=='menu':m.w(0xa8eda0,1)
   if case=='chat':m.b(0xa8bc60+25,1)
   if case=='observer':m.w(0xac1198,m.house)
   if case=='disabled':s.flags=0
   if case=='frame_advanced':m.w(0xa8ed84,s.data.frame+1)
   if case=='frame_rollback':m.w(0xa8ed84,s.data.frame-1)
   if case=='engine_paused':m.b(0xa8e9a0,0)
   if case=='full_outlist':m.w(0x8b41f8 if mode==0 else 0xa802c8,128)
   if case.startswith('cancel_'):
    s.reservePolicy.count=2;s.reservePolicy.queued[0]=s.reservePolicy.queued[1]=0;s.reservePolicy.slots=3
    if case=='cancel_full':s.reservePolicy.slots=0;s.reservePolicy.count=5
    if case=='cancel_completed':m.w(m.factory+0x24,54)
    if case=='cancel_notqueued':s.reservePolicy.queued[0]=s.reservePolicy.queued[1]=1
   save(m,s);before_count=event_count(m)
   x,y=(1320,440) if case=='scaled' else (100,100) if case=='outside' else (724,220) if case=='unsupported' else (660,220)
   down=0x203 if case=='double_click' else 0x204 if case=='right' or case.startswith('cancel_') else 0x201
   kind,s=message(m,down,x,y,4 if case=='cancel_shift' else 0)
   capture=case in ('normal','defense','double_click','scaled','drag','scroll','changed_parent','lost_focus','expired_hold','full_outlist','frame_advanced','completed_other','cancel_right','cancel_shift','cancel_full','cancel_completed')
   assert kind==(0 if capture else down),(hex(kind),case)
   assert s.reserveInput==(2 if case=='busy_input' else 0)
   if case=='drag':x+=12
   if case=='scroll':m.w(m.strip+0x44,1)
   if case=='changed_parent':m.w(m.factory+0x58,0)
   if case=='lost_focus':s.foreground=0;save(m,s)
   if case=='expired_hold':
    m.clock+=1501;s=state(m);s.heartbeat=m.clock;s.reservePolicy.tick=m.clock;save(m,s)
   up=0x205 if case=='right' or case.startswith('cancel_') else 0x202;kind,s=message(m,up,x,y)
   assert kind==(0 if capture else up),(hex(kind),case)
   accepted=case in ('normal','defense','double_click','scaled','full_outlist','frame_advanced','completed_other','cancel_right','cancel_shift','cancel_full','cancel_completed')
   assert s.reserveInput==(1 if accepted else 2 if case=='busy_input' else 0)
   if accepted:
    assert (s.reserveClick.index,s.reserveClick.parentID,s.reserveClick.tab)==(0,90,m.tab)
    assert s.reserveClick.operation==(2 if case.startswith('cancel_') else 1) and s.reserveClick.all==int(case=='cancel_shift')
   assert event_count(m)==before_count and not m.code_writes
   kind,s=message(m,0x202,x,y);assert kind==0x202 # no dangling suppression on unrelated UP
   rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
  except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()

for mode in (0,3):
 for case in ('observed','paused','completed','canceled','factory_removed','replaced','unobserved'):
  name=f'presentation_{mode}_{case}'
  try:
   m=fixture(mode);m.w(m.factory+0x58,0);m.canbuild_full_result=1;s=state(m)
   s.command=20;s.arg=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==7);s.arg2=12;s.requestToken=73;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);s=m.hook();assert s.result==1 and s.count==1 and event_count(m)==1
   assert m.r(m.strip+88+0x10)==0 # never marks busy just because an event was queued
   if case!='unobserved':
    product=m.obj(0x7e3ebc,m.buildT,m.house,92);m.b(product+0x81,1);m.w(m.factory+0x58,product);s=m.hook()
    assert m.r(m.strip+88+0xc)==m.factory and m.r(m.strip+88+0x10)==1
    assert m.r(m.strip+88+52+0x10)==0 # only current cameo changed; native draw shades all others
    busy=[]
    def busy_branch(uc,at,size,_):busy.append(m.u.mem_read(m.u.reg_read(UC_X86_REG_ESP)+0x17,1)[0]);m.ret(0,0)
    m.u.hook_add(UC_HOOK_CODE,busy_branch,begin=0x6a95ee,end=0x6a95ee)
    def draw_test():
     m.u.reg_write(UC_X86_REG_EAX,0);m.u.reg_write(UC_X86_REG_EDI,2);m.u.reg_write(UC_X86_REG_ESI,m.strip)
     m.u.mem_write(m.STACK+0x1e000+0x17,b'\0');m.run(0x6a95c8);return busy[-1]
    assert draw_test()==1 # original executable's busy/shading decision
    if case=='paused':m.w(m.strip+88+0x10,2);m.b(m.factory+0x70,1);m.b(m.factory+0x71,1);m.hook();assert m.r(m.strip+88+0x10)==2 and draw_test()==1
    if case=='completed':m.w(m.factory+0x24,54);m.hook();assert draw_test()==1
    if case in ('canceled','factory_removed','replaced'):
     m.w(m.factory+0x58,0)
     if case=='factory_removed':m.w(m.house+0x53bc,0)
     if case=='replaced':m.w(m.factory+0x58,m.parent) # replaced with an unrelated product identity
     m.hook()
     if case=='replaced':assert draw_test()==1 # same type now owned by native producer, preserve valid display
     else:assert m.r(m.strip+88+0x10)==0 and m.r(m.strip+88+0xc)==0 and draw_test()==0
   else:
    for _ in range(50):m.hook()
    assert m.r(m.strip+88+0x10)==0
   assert event_count(m)==1 and not m.code_writes
   rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
  except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))

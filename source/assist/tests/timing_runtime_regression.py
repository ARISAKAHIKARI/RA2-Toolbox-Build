"""Run compiled central queue with ABI-legal volatile register clobbers.
Also replay the exact 0.7.8 null-write instruction when --baseline is supplied.
Game clock/OS are synthetic; constructors and DLL instructions are real.
"""
import argparse,ctypes as C,importlib.util,hashlib,json,struct
from pathlib import Path
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--baseline',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def fixture(dll):
 spec=importlib.util.spec_from_file_location('clock_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(dll)];spec.loader.exec_module(m)
 m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.u.mem_write(m.t+0x388,struct.pack('<d',3.));m.w(m.unit+0x114,0);m.w(m.unit+0x118,0);return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def issue(m,command,mode):
 m.w(0xa8b238,mode);m.w(0x8b41f8,0);m.w(0x8b41fc,0);m.w(0x8b4200,0);m.w(0xa802c8,0);m.w(0xa802d0,0)
 s=m.hook();s.command=command;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;m.u.mem_write(m.SH,bytes(s));return m.hook()
baseline=None
if a.baseline:
 m=fixture(a.baseline);caught={}
 class Captured(Exception):pass
 def zero_write(uc,access,address,size,value,_):
  if address==0 and value==m.clock:
   caught.update(address=address,instruction=uc.reg_read(UC_X86_REG_EIP),value=value);raise Captured()
 m.u.hook_add(UC_HOOK_MEM_WRITE,zero_write,begin=0,end=3)
 try:issue(m,15,0)
 except Captured:pass
 assert caught and caught['instruction']-m.dll.OPTIONAL_HEADER.ImageBase==0x11148,caught
 baseline={'sha256':hashlib.sha256(a.baseline.read_bytes()).hexdigest(),'reproduced_null_write_rva':'0x11148',**caught}
 print('PASS baseline 0.7.8 reproduces exact onsite fault RVA',flush=True)
for mode in (0,3):
 for command in (1,15):
  for pattern in ('zero','poison','legacy_nonvolatile'):
   m=fixture(a.dll)
   m.timer_clobber={UC_X86_REG_ECX:0 if pattern=='zero' else 0xcafebabe,UC_X86_REG_EDX:0 if pattern=='zero' else 0xdead0000}
   if pattern=='legacy_nonvolatile':m.timer_clobber.update({UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0xdead1234,UC_X86_REG_EDI:0x99887766,UC_X86_REG_EBP:0})
   s=issue(m,command,mode)
   assert s.ack==s.request and s.result==1 and s.count==1 and s.data.commandStatus==1,(mode,command,pattern,s.result,s.count)
   count=m.r(0x8b41f8 if mode==0 else 0xa802c8);assert count==1
   par=(0x8b41f8+12+16384*111) if mode==0 else (0xa802c8+12+128*111)
   # Layout constants from event_delivery.h: DoList entries begin at +12.
   assert m.r(par)==m.clock,(hex(par),m.r(par),m.clock)
   assert m.r(m.SH+0x3e0000+72)==0
   name=f'session_{mode}_command_{command}_{pattern}'
   rows.append({'case':name,'status':'PASS','queue_timing':m.clock});print('PASS',name,flush=True)
a.output.write_text(json.dumps({'inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest(),'game':hashlib.sha256(a.game.read_bytes()).hexdigest()},'baseline_reproduction':baseline,'results':rows,'scope':'Actual DLL/constructors, synthetic OS/game clock. Not onsite Windows run.'},indent=2)+'\n')

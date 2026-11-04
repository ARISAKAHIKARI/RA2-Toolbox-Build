"""Offline native sender experiment; no network and no simulation receiver.

Compare unmodified sender, unconditional NOP of two House writes, and a
Unicorn-only candidate that retains House only for Sell/Destruct and splits
compressed batches at changes in effective House. This is not product code.
Protocol 0 wire events are parsed as 111-byte records; protocol 1/2 use the
original 64BDD0 decoder. No full transport receive, scheduler or effects run.
"""
import argparse, hashlib, json, struct, traceback
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
raw=Path(a.game).read_bytes();pe=pefile.PE(data=raw);image=pe.get_memory_mapped_image()
HOUSE_WRITES=(0x649e1e,0x64b9d4);EXPECTED=bytes.fromhex('888c40d602a800')
rows=[]

class Probe:
 def __init__(self,protocol,variant,events,local=0,head=0,frame=1234,delay=5,dolist_count=0,stop_send=False):
  self.u=Uc(UC_ARCH_X86,UC_MODE_32);self.u.mem_map(0x400000,0x1000000);self.u.mem_write(0x400000,image)
  self.u.mem_map(0x20000000,0x100000);self.houses=[0x20000000,0x20020000]
  self.items=0x20040000;self.iface=0x20042000;self.vt=0x20043000;self.buf=0x20044000
  self.stack=0x200f0000;self.stop=0x200f8000;self.clock=self.stop+16;self.send=self.stop+32;self.flush=self.stop+48
  self.protocol=protocol;self.variant=variant;self.local=local;self.frame=frame;self.delay=delay
  self.sent=[];self.stop_send=stop_send;self.stopped=False;self.events=events;self.head=head
  for at in HOUSE_WRITES:
   assert bytes(self.u.mem_read(at,7))==EXPECTED
   if variant=='nop':self.u.mem_write(at,b'\x90'*7)
  for index,house in enumerate(self.houses):self.w(self.items+index*4,house);self.w(house+0x30,index)
  self.w(0xa8022c,self.items);self.w(0xa83d4c,self.houses[local]);self.w(0xa8ed84,frame)
  self.w(0xa8b24c,protocol);self.w(0xa8b554,3);self.w(0xa8b54c,2);self.w(0xa8b238,3)
  self.w(0xac51fc,0x12345678);self.b(0xa8ed74,0);self.w(0xb0487c,0)
  self.w(0x7e1530,self.clock);self.w(self.iface,self.vt);self.w(self.vt+4,self.flush);self.w(self.vt+8,self.send)
  self.w(0xa802c8,len(events));self.w(0xa802cc,head);self.w(0xa802d0,(head+len(events))&127)
  self.w(0x8b41f8,dolist_count);self.w(0x8b41fc,0);self.w(0x8b4200,0)
  for at in (0x4068e0,self.clock,self.send,self.flush,0x649d49,0x64b660,*HOUSE_WRITES):
   self.u.hook_add(UC_HOOK_CODE,self.hook,begin=at,end=at)
  for index,(kind,house,ident) in enumerate(events):
   dest=0xa802d4+((head+index)&127)*111;self.u.mem_write(dest,bytes(111))
   self.w(0xa83a54+((head+index)&127)*4,432100+index)
   if kind==0x16:self.run(0x4c65e0,dest,(house,kind,ident,0x34))
   elif kind==4:self.run(0x4c6860,dest,(house,ident,0x34,2,0,0,0,0,0,0))
   else:self.run(0x4c66c0,dest,(house,kind))
  self.before=[self.out(index) for index in range(len(events))]
  self.house_before=[bytes(self.u.mem_read(h,0x20000)) for h in self.houses]
 def w(self,at,x):self.u.mem_write(at,struct.pack('<I',x&0xffffffff))
 def r(self,at):return struct.unpack('<I',self.u.mem_read(at,4))[0]
 def b(self,at,x):self.u.mem_write(at,bytes([x]))
 def out(self,index):return bytes(self.u.mem_read(0xa802d4+((self.head+index)&127)*111,111))
 def effective(self,slot):
  event=bytes(self.u.mem_read(0xa802d4+slot*111,3))
  return event[2] if event[0] in (8,0x16) else self.local
 def ret(self,value,argc=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);self.u.reg_write(UC_X86_REG_EAX,value)
  self.u.reg_write(UC_X86_REG_EIP,self.r(sp));self.u.reg_write(UC_X86_REG_ESP,sp+4+4*argc)
 def hook(self,u,at,size,_):
  sp=u.reg_read(UC_X86_REG_ESP)
  if at==0x4068e0:self.ret(0)
  elif at==self.clock:self.ret(5678)
  elif at==self.flush:self.ret(0)
  elif at==self.send:
   data=bytes(u.mem_read(self.r(sp+4),self.r(sp+8)))
   self.sent.append(data)
   if self.stop_send:self.stopped=True;u.emu_stop()
   else:self.ret(0,4)
  elif self.variant=='candidate':
   if at in HOUSE_WRITES:
    slot=self.r(0xa802cc);kind=bytes(u.mem_read(0xa802d4+slot*111,1))[0]
    if kind in (8,0x16):u.reg_write(UC_X86_REG_EIP,at+7)
   elif at==0x649d49 and self.protocol in (1,2) and self.r(0xa802c8):
    eax=u.reg_read(UC_X86_REG_EAX);u.reg_write(UC_X86_REG_EAX,(eax&~255)|self.effective(self.r(0xa802cc)))
   elif at==0x64b660:
    count=min(self.r(0xa802c8),self.r(sp+12));head=self.r(0xa802cc)
    if count:
     house=self.effective(head);prefix=1
     while prefix<count and self.effective((head+prefix)&127)==house:prefix+=1
     self.w(sp+12,prefix)
 def run(self,address,selfptr,args=(),edx=0):
  self.w(self.stack,self.stop)
  for i,x in enumerate(args):self.w(self.stack+4+4*i,x)
  self.u.reg_write(UC_X86_REG_ESP,self.stack);self.u.reg_write(UC_X86_REG_ECX,selfptr);self.u.reg_write(UC_X86_REG_EDX,edx)
  self.u.emu_start(address,self.stop,count=200000)
  if not self.stopped:
   assert self.u.reg_read(UC_X86_REG_EIP)==self.stop,'instruction budget exhausted'
   assert self.u.reg_read(UC_X86_REG_ESP)==self.stack+4+4*len(args),'ABI stack imbalance'
 def execute(self,capacity=4096):
  self.run(0x649ca0,self.iface,(capacity,self.delay,1),self.buf)
  count=self.r(0x8b41f8)
  self.local_records=[bytes(self.u.mem_read(0x8b4204+i*111,111)) for i in range(min(count,len(self.events)))]
  assert self.r(0xa83d4c)==self.houses[self.local]
  assert self.house_before==[bytes(self.u.mem_read(h,0x20000)) for h in self.houses]
  return self
 def decoded(self):
  if self.protocol==0:
   assert all(len(packet)%111==0 for packet in self.sent)
   return [packet[i:i+111] for packet in self.sent for i in range(0,len(packet),111)]
  self.w(0x8b41f8,0);self.w(0x8b41fc,0);self.w(0x8b4200,0)
  for packet in self.sent:
   self.u.mem_write(self.buf,packet);self.run(0x64bdd0,self.buf,edx=len(packet))
  return [bytes(self.u.mem_read(0x8b4204+i*111,111)) for i in range(self.r(0x8b41f8))]
 def summary(self):
  return {'packet_lengths':[len(p) for p in self.sent],
          'headers':[{'house':p[2],'frame':int.from_bytes(p[3:7],'little'),'crc':p[7:11].hex(),'word_0b':int.from_bytes(p[11:13],'little'),'byte_0d':p[13]} for p in self.sent],
          'out_remaining':self.r(0xa802c8),'out_head':self.r(0xa802cc),'local_records':[brief(b) for b in self.local_records]}

def brief(b):return {'kind':b[0],'house':b[2],'frame':int.from_bytes(b[3:7],'little'),'id':int.from_bytes(b[7:11],'little') if b[0] in (4,0x16) else None}
def check(name,fn):
 try:
  result=fn();rows.append({'case':name,'status':'PASS',**(result or {})});print('PASS',name,flush=True)
 except Exception as error:
  traceback.print_exc();rows.append({'case':name,'status':'FAIL','error':repr(error)})

mixed=[(0x16,0,101),(8,1,0),(0x16,1,102),(8,0,0),(0x16,1,103),(8,1,0)]
own=[(kind,0,ident) for kind,house,ident in mixed]
for protocol in (0,1,2):
 for variant in ('vanilla','nop','candidate'):
  for head in (0,126):
   def trial(protocol=protocol,variant=variant,head=head):
    m=Probe(protocol,variant,mixed,head=head).execute();data=m.summary();local=m.local_records
    decoded=m.decoded();remote=[b for b in decoded if b[0]!=0x1c]
    assert len(local)==len(remote)==len(mixed)
    expected_local=[0 if variant=='vanilla' else h for k,h,i in mixed]
    expected_remote=[0 if variant=='vanilla' or (variant=='nop' and protocol) else h for k,h,i in mixed]
    assert [b[2] for b in local]==expected_local
    assert [b[2] for b in remote]==expected_remote
    expected_frame=((1234+5+2)//3)*3 if protocol==2 else 1239
    for records in (local,remote):
     assert [b[0] for b in records]==[k for k,h,i in mixed]
     assert [int.from_bytes(b[7:11],'little') for b in records if b[0]==0x16]==[101,102,103]
     assert all(int.from_bytes(b[3:7],'little')==expected_frame for b in records)
    if variant=='candidate':assert all(a[:12]==b[:12] for a,b in zip(local,remote))
    if variant=='candidate' and protocol:
     assert [p[2] for p in m.sent]==[0,1,0,1] and len(decoded)==len(mixed)+4
    else:assert len(m.sent)==1
    assert m.r(0xa802c8)==0 and m.r(0xa802cc)==((head+len(mixed))&127)
    data['decoded_records']=[brief(b) for b in decoded];data['local_remote_house_match']=expected_local==expected_remote
    return data
   check(f'mixed_protocol_{protocol}_{variant}_head_{head}',trial)
 for variant in ('nop','candidate'):
  def equivalent(protocol=protocol,variant=variant):
   a=Probe(protocol,'vanilla',own).execute();b=Probe(protocol,variant,own).execute()
   assert a.sent==b.sent and a.local_records==b.local_records
   assert a.summary()==b.summary() and a.decoded()==b.decoded()
   return {'exact_wire_local_and_decode_equivalence':True}
  check(f'own_equivalence_protocol_{protocol}_{variant}',equivalent)
 for variant in ('vanilla','nop','candidate'):
  def no_capacity(protocol=protocol,variant=variant):
   minimum=222 if protocol==0 else 20 # FrameInfo + Sell's 1-byte type and 5-byte Target.
   m=Probe(protocol,variant,[(0x16,1,101)],stop_send=True).execute(minimum-1)
   assert m.stopped and m.r(0xa802c8)==1 and m.r(0xa802cc)==0 and m.r(0x8b41f8)==0
   assert m.out(0)==m.before[0]
   return {'stopped_at_first_send':True,**m.summary()}
  check(f'insufficient_capacity_protocol_{protocol}_{variant}',no_capacity)
 for variant in ('vanilla','candidate'):
  def pressure(protocol=protocol,variant=variant):
   m=Probe(protocol,variant,[(0x16,1,101)],dolist_count=8192).execute()
   assert m.r(0xa802c8)==1 and m.r(0x8b41f8)==8192 and m.out(0)==m.before[0]
   return {'no_outlist_consumption':True,'packet_lengths':[len(p) for p in m.sent]}
  check(f'local_dolist_pressure_protocol_{protocol}_{variant}',pressure)

for protocol in (1,2):
 def unsupported(protocol=protocol):
  events=[(8,1,0),(0x2d,1,0),(0x16,1,104),(0x2d,0,0)]
  m=Probe(protocol,'candidate',events).execute();remote=[x for x in m.decoded() if x[0]!=0x1c]
  assert [x[2] for x in m.local_records]==[1,0,1,0]==[x[2] for x in remote]
  return {'only_sell_destruct_retain_foreign_house':True,'packet_houses':[x[2] for x in m.sent]}
 check(f'non_whitelisted_event_protocol_{protocol}',unsupported)

Path(a.output).write_text(json.dumps({'scope':__doc__,'game_fixture_sha256':hashlib.sha256(raw).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']!='PASS' for row in rows))

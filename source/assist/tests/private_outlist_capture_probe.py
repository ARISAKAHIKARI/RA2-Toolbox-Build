"""Execute production x86 interception stubs and sender with a private engine
fixture. Fixed Win32/object boundaries are replaced; no live platform claim."""
import argparse,json,struct,hashlib,re
from pathlib import Path
import pefile
from unicorn import *
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('--memory',required=True);p.add_argument('--dll',required=True);p.add_argument('--output',required=True);a=p.parse_args()
b=Path(a.memory).read_bytes();dll=pefile.PE(a.dll);base=dll.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,b);u.mem_map(0x20000000,0x100000);u.mem_map(base,(dll.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,dll.get_memory_mapped_image())
w=lambda at,x:u.mem_write(at,struct.pack('<I',x&0xffffffff))
r=lambda at:struct.unpack('<I',u.mem_read(at,4))[0]
exports={e.name.decode():base+e.address for e in dll.DIRECTORY_ENTRY_EXPORT.symbols}
imports={};stub=0x200e0000
for mod in dll.DIRECTORY_ENTRY_IMPORT:
 for imp in mod.imports:
  imports[stub]=imp.name.decode() if imp.name else str(imp.ordinal);w(imp.address,stub);u.mem_write(stub,b'\xc3');stub+=16
stop=0x200ff000;stack=0x200f0000;clock=0x200ff100;packet=0x20050000;cell=0x20070000
u.mem_write(clock,b'\xb8'+struct.pack('<I',5678)+b'\xc3');w(0x7e1530,clock)
stopAt=set();observed=[];sent=[];sendOK=True;flushes=0

def ret(value,n=0):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(uc,at,size,_):
 global flushes
 if at in stopAt:observed.append(at);uc.emu_stop();return
 if at in imports:
  name=imports[at];sp=uc.reg_read(UC_X86_REG_ESP)
  if name=='GetCurrentThreadId':ret(42)
  elif name in ('memcpy','memmove'):dst,src,n=r(sp+4),r(sp+8),r(sp+12);uc.mem_write(dst,bytes(uc.mem_read(src,n)));ret(dst)
  elif name=='memset':dst,c,n=r(sp+4),r(sp+8),r(sp+12);uc.mem_write(dst,bytes([c&255])*n);ret(dst)
  elif name=='memcmp':x,y,n=r(sp+4),r(sp+8),r(sp+12);v=bytes(uc.mem_read(x,n));z=bytes(uc.mem_read(y,n));ret(0 if v==z else 1)
  else:raise AssertionError(('unexpected import',name,hex(at)))
 elif at==0x5414c0:
  sp=uc.reg_read(UC_X86_REG_ESP);assert r(sp+12)==1 and r(sp+16)==0xffffffff
  sent.append(bytes(uc.mem_read(r(sp+4),r(sp+8))));ret(int(sendOK),16)
 elif at==0x541820:flushes+=1;ret(0)
u.hook_add(UC_HOOK_CODE,hook)
def run(name,*args):
 stopAt.clear();w(stack,stop)
 for i,x in enumerate(args):w(stack+4+i*4,x)
 u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(exports[name],stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop,(name,hex(u.reg_read(UC_X86_REG_EIP)))
 return u.reg_read(UC_X86_REG_EAX)
for tag,at,n in [(7,0x4068e0,5),(8,0x4c6860,6),(9,0x6521c0,5)]:
 dest=run('Entry',tag);u.mem_write(at,b'\xe9'+struct.pack('<I',(dest-at-5)&0xffffffff)+b'\x90'*(n-5))
w(0xa83d4c,0x20001000);w(0x20001030,0);w(0x20001010,77);w(0x20002010,0x1234);w(0x2000221c,0x20001000);w(0xa8b230,0x20003000);w(0xa8ed84,1230);w(0xa8b238,3);w(0xac4cf4,0);w(cell,0x00050004)
# Synthetic current/human Houses for House-only command identity checks.
w(0xa8022c,0x20060000);w(0xa80230,2);w(0xa80238,2)
w(0x20060000,0x20001000);w(0x20060004,0x20010000)
w(0x20010010,78);w(0x20010030,1)
u.mem_write(0x200011ec,b'\x01\x01');u.mem_write(0x200101ec,b'\x00\x01')
for h in (0x20001000,0x20010000):u.mem_write(h+0x1f5,bytes(4))
argsByCtor={0x4c65e0:[0,6,0x1234,0x34],0x4c6650:[0,0x17,cell],0x4c66c0:[0,8],0x4c6720:[0,3,1],0x4c6780:[0,0x2a,0x1234,0x34,1001,11],0x4c6860:[0,0x1234,0x34,2,0,0,1001,11,0,0],0x4c68e0:[0,0x1234,0x34,2,0,0,1001,11,3,4],0x4c6970:[0,0xe,7,1,0],0x4c6ae0:[0,0xb,7,1,0,cell],0x4c6b60:[0,0x12,1,cell]}
header=Path(__file__).parents[1]/'source/private_outlist_sites.h'
sites=[]
for row in header.read_text().split('inline constexpr CaptureSite CaptureSites[]={')[1].split('};')[0].splitlines():
 vals=re.findall(r'-?0x[0-9A-F]+|-?\d+',row)
 if vals:sites.append([int(v,0) for v in vals])
assert run("Compatible")==1
checks=[]
for active in (0,1):
 for caller,ctor,success,failure,delta,esi,edi,ebp,result in sites:
  run('Reset',active);w(0xa802c8,0);w(0xa802d0,0);u.mem_write(packet,bytes(111));args=argsByCtor[ctor]
  w(stack,caller)
  for i,x in enumerate(args):w(stack+4+4*i,x)
  sp=stack+4+4*len(args)
  for off in range(0,256,4):w(sp+off,0x900000+off)
  # Native Mega's copied success epilogue returns to this parent boundary.
  w(sp+0x78,stop)
  u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,packet)
  for reg,x in [(UC_X86_REG_ESI,0x11111111),(UC_X86_REG_EDI,0x22222222),(UC_X86_REG_EBP,0x33333333)]:u.reg_write(reg,x)
  stopAt.clear();stopAt.add(caller if not active else success);observed.clear()
  u.emu_start(ctor,stop,count=1000000)
  if active and caller==0x646ef5:assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+0x88
  else:assert observed==[caller if not active else success],(active,hex(caller),observed,hex(u.reg_read(UC_X86_REG_EIP)))
  if not(active and caller==0x646ef5):assert u.reg_read(UC_X86_REG_ESP)==sp+(delta if active else 0),(hex(caller),'stack')
  assert r(0xa802c8)==0 and r(0xa802d0)==0,(hex(caller),'native queue changed')
  assert run('Depth')==0 and run('Count')==active,(active,hex(caller),'capture count')
  if active:
   e=bytes(u.mem_read(run('Packet',0),111));native=bytes(u.mem_read(packet,111));assert e==native,(hex(caller),'constructor payload',e.hex(),native.hex())
  checks.append({'active':bool(active),'caller':hex(caller),'constructor':hex(ctor)})
# By-value planning submit and original fallback, including its ret 112 ABI.
for active,kind in [(1,4),(1,0x2b),(0,4),(1,0x1d)]:
 run('Reset',active);w(0xa802c8,0);w(0xa802d0,0);e=bytearray(111);e[0]=kind;struct.pack_into('<I',e,3,1230)
 if kind==4:struct.pack_into('<IBBBIBIBIBB',e,7,0x1234,0x34,2,0,0,0,1001,11,0,0,1)
 w(stack,stop);u.mem_write(stack+4,bytes(e)+b'\0');u.reg_write(UC_X86_REG_ESP,stack);stopAt.clear();u.emu_start(0x6521c0,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_ESP)==stack+116
 captured=int(active and kind!=0x1d);assert run('Count')==captured and r(0xa802c8)==1-captured
 if captured:assert bytes(u.mem_read(run('Packet',0),111))==bytes(e)
# Reliable-send rejection must leave both queues/counters untouched; retry
# acceptance commits the same events to DoList once before Flush.
run('Reset',1);w(0xa802c8,0);w(0x8b41f8,0);w(0x8b4200,16383);w(0xa8b24c,1);w(0xa8b550,15);w(0xa8b554,5);w(0xac51fc,0x12345678);w(0xb04474+(1230&255)*4,0x12345678);u.mem_write(0xafa400,struct.pack('<H',65535));w(0xb0487c,40)
w(0xa8e9c0,0x7ec16c);u.mem_write(0xa8e9c4,b'\x01\x01');w(0xa8ea04,2)
for kind in (0xe,0x2b,0xe):
 e=bytearray(111);e[0]=kind;struct.pack_into('<I',e,3,1230);u.mem_write(packet,bytes(e));assert run('Submit',packet)==1
sendOK=False;run('Send');assert run('Count')==3 and run('Sent')==0 and r(0x8b41f8)==0 and r(0xb0487c)==40
assert run('Status')==0xfffffff8 and flushes==0
sendOK=True;run('Advance',50);run('Send');assert run('Count')==0 and run('Sent')==3 and r(0x8b41f8)==3 and r(0x8b4200)==2 and r(0xb0487c)==41
assert len(sent)==2 and sent[0]==sent[1] and flushes==1
run('Send');assert len(sent)==2 and flushes==1
run('Submit',packet);run('Stop');assert run('Count')==0
# Destruct: actual production queue -> encoder -> native decoder -> original
# Event::Respond and House end-state setter. Transport and Houses are synthetic.
destruct=[]
def native(at,self,*args):
 stopAt.clear();w(stack,stop)
 for i,x in enumerate(args):w(stack+4+i*4,x)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,self);u.emu_start(at,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4+4*len(args)
def prepare_destruct(proto):
 run('Reset',1);w(0xa8b24c,proto);w(0xa802c8,0);w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
 for h in (0x20001000,0x20010000):u.mem_write(h+0x1f5,bytes(4))
 w(0x20010010,78);w(0x20010030,1);w(0x20060004,0x20010000);u.mem_write(0x200101ed,b'\x01')
 e=bytearray(111);e[0]=8;e[2]=1;struct.pack_into('<I',e,3,1230);u.mem_write(packet,bytes(e))
for proto in (0,1,2):
 prepare_destruct(proto)
 if 'Target' in exports:
  run('Legacy',0);assert run('Target',0x20010000,78,1)==1
 assert run('Submit',packet)==1
 run('Advance',4000) # Explicit accepted Destruct is not an automation timeout.
 before=len(sent);sendOK=False;run('Send');assert run('Count')==1 and r(0x8b41f8)==0
 sendOK=True;run('Advance',50);run('Send');assert run('Count')==0 and run('Sent')==1
 assert len(sent)==before+2 and sent[-1]==sent[-2]
 local=bytes(u.mem_read(0x8b4204,111));assert local[:3]==b'\x08\x00\x01'
 wire=sent[-1];u.mem_write(packet,wire)
 if proto:
  w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
  u.reg_write(UC_X86_REG_EDX,len(wire));native(0x64bdd0,packet)
  assert r(0x8b41f8)==2;decoded=bytes(u.mem_read(0x8b4204+111,111))
 else:
  assert len(wire)==222;decoded=wire[111:]
 assert decoded[:7]==local[:7],('Destruct decoder target/frame',proto)
 u.mem_write(packet,decoded);native(0x4c6cb0,packet)
 assert bytes(u.mem_read(0x200101f6,1))==b'\x01' and bytes(u.mem_read(0x200011f6,1))==b'\x00'
 assert r(0x20010298)==1230 and r(0x200102a0)==0
 destruct.append({'protocol':proto,'target_house':1,'local_house':0,'wire_bytes':len(wire),'retry_once':True,'receiver_target_only':True})
# Identity recycling between admission and send must cancel, never retarget.
identity_cases=0
for address,value in ((0x20010010,79),(0x20010030,2),(0x20060004,0x20012000)):
 prepare_destruct(1);assert run('Submit',packet)==1;w(address,value);before=len(sent);run('Send')
 assert run('Count')==0 and len(sent)==before and r(0x8b41f8)==0;identity_cases+=1
prepare_destruct(1);u.mem_write(0x200101ed,b'\x00');assert run('Submit',packet)==0
if 'Target' in exports:
 assert run('Target',0x20010000,78,1)==0xfffffffc
 prepare_destruct(1);run('Reset',0);run('Legacy',0)
 assert run('Target',0x20010000,78,1)==0xffffffd6
 run('Legacy',1);assert run('Target',0x20010000,78,1)==1
 assert run('Target',0x20010000,79,1)==0xfffffff9
prepare_destruct(1);assert run('Submit',packet)==1;run('Stop');before=len(sent);run('Send');assert run('Count')==0 and len(sent)==before
Path(a.output).write_text(json.dumps({'memory_sha256':hashlib.sha256(b).hexdigest(),'fixture_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'capture_cases':checks,'planning_cases':4,'transport_retry_atomicity':True,'destruct_pipeline':destruct,'destruct_identity_cancellations':identity_cases,'multiplayer_AI_rejected':True,'target_route_cases':5 if 'Target' in exports else 0,'live_game_tested':False,'live_platform_tested':False},indent=2))
print('PASS',len(checks),'capture/fallback cases, planning, retry, 3 Destruct wire/receiver paths, target identity/cancellation')

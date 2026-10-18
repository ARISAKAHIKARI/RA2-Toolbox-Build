"""Render-only experiment: actual compiled stubs + original Cell draw caller.
The mask blitters and GetOcclusion are mocked; this is not a screenshot/netplay test.
"""
from pathlib import Path
import argparse, hashlib, json, struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);args=p.parse_args()
root=Path(__file__).resolve().parent.parent
dll=pefile.PE(str(root/'GameRuntime.dll'));game=pefile.PE(str(args.reference))
base=dll.OPTIONAL_HEADER.ImageBase;mapped=dll.get_memory_mapped_image()
prefix=bytes.fromhex('9C 60 89 E3 81 EC 10 02 00 00 83 E4 F0 0F AE 04 24 FC')
sites={0x47EFE0:bytes.fromhex('55 8B EC 83 EC 34'),0x47F250:bytes.fromhex('A0 C6 E7 89 00')}
stubs={};offset=0
while (offset:=mapped.find(prefix,offset))>=0:
 block=mapped[offset:offset+160]
 if bytes.fromhex('6A 00 6A 00 6A 03 E8') in block[:48]:
  call=block.index(b'\xe8',len(prefix))
  guard=base+offset+call+5+struct.unpack_from('<i',block,call+1)[0]
  site=0x47EFE0 if bytes.fromhex('55 89 E5 83 EC 34') in block else 0x47F250
  assert site not in stubs
  stubs[site]=(base+offset,guard)
 offset+=len(prefix)
assert set(stubs)==set(sites),stubs
for at,sig in sites.items():assert game.get_data(at-game.OPTIONAL_HEADER.ImageBase,len(sig))==sig

registers=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
stack=0x300f000;stop=0x300000;cell=0x2002000;rules=0x2005000;house=0x2006000
values=[0x12345678,0xABABABAB,cell,0xCDCDCDCD,0x2004000,0xEFEFEFEF,0x30303030]
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32)
 for pe in (game,dll):
  at=pe.OPTIONAL_HEADER.ImageBase;u.mem_map(at,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(at,pe.get_memory_mapped_image())
 u.mem_map(0x2000000,0x20000);u.mem_map(0x3000000,0x10000);u.mem_map(stop,0x1000)
 u.mem_write(stack,struct.pack('<5I',stop,0x2000100,0x2000200,15,0x3333))
 u.mem_write(0x89e7c6,b'\x01')
 for reg,value in zip(registers,values):u.reg_write(reg,value)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EFLAGS,0x202)
 u.reg_write(UC_X86_REG_MXCSR,0x1F80);u.reg_write(UC_X86_REG_FPCW,0x37F)
 for n in range(8):u.reg_write(UC_X86_REG_XMM0+n,int.from_bytes(bytes([n+1])*16,'little'))
 return u
def snapshot(u):
 return [u.reg_read(r) for r in registers+[UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_MXCSR,UC_X86_REG_FPCW]+list(range(UC_X86_REG_XMM0,UC_X86_REG_XMM0+8))]
def ret(u,value,argc):
 sp=u.reg_read(UC_X86_REG_ESP);ip=struct.unpack('<I',u.mem_read(sp,4))[0]
 u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+argc*4);u.reg_write(UC_X86_REG_EIP,ip)
results=[]
for at,(start,guard) in stubs.items():
 for on in (False,True):
  for direction in (0,0x400):
   u=machine();u.reg_write(UC_X86_REG_EFLAGS,0x202|direction);initial=snapshot(u)
   end=stop if on else at+len(sites[at]);calls=[];finished=[]
   def hook(emu,ip,size,data):
    if ip==guard:
     sp=emu.reg_read(UC_X86_REG_ESP);assert struct.unpack('<III',emu.mem_read(sp+4,12))==(3,0,0)
     assert not emu.reg_read(UC_X86_REG_EFLAGS)&0x400
     calls.append(ip);emu.reg_write(UC_X86_REG_ECX,0xBAD);emu.reg_write(UC_X86_REG_EDX,0xBAD)
     for n in range(8):emu.reg_write(UC_X86_REG_XMM0+n,0)
     emu.reg_write(UC_X86_REG_MXCSR,0x3F80);emu.reg_write(UC_X86_REG_FPCW,0x27F);ret(emu,int(on),0)
    elif ip==end:finished.append(ip);emu.emu_stop()
   u.hook_add(UC_HOOK_CODE,hook);u.emu_start(start,0,count=200)
   assert finished==[end] and len(calls)==1
   if on:
    initial[7]=stack+16;assert snapshot(u)==initial
   else:
    original=machine();original.reg_write(UC_X86_REG_EFLAGS,0x202|direction)
    original.emu_start(at,at+len(sites[at]),count=5)
    assert snapshot(u)==snapshot(original),(hex(at),'displaced instructions')
   results.append({'case':'stub','site':hex(at),'enabled':on,'DF':bool(direction),'pass':True})

# Original caller always performs the same two occlusion queries/cache updates.
# Only mask drawing changes. Verify entire Cell and House bytes, not just one flag.
for fog in (False,True):
 for shroudValue in (-2,-1,7):
  for fogValue in (-2,-1,8):
   outcomes=[]
   for enabled in (False,True):
    u=machine();u.mem_write(cell,bytes((n*13+37)&255 for n in range(0x200)))
    u.mem_write(house,bytes(0x200));u.mem_write(0xA83D4C,struct.pack('<I',house))
    u.mem_write(0xA8B230,struct.pack('<I',rules));u.mem_write(rules,struct.pack('<I',0x1000 if fog else 0))
    for at,(start,guard) in stubs.items():u.mem_write(at,b'\xe9'+struct.pack('<i',start-at-5)+b'\x90'*(len(sites[at])-5))
    guards={v[1] for v in stubs.values()};continuations={at+len(sites[at]):at for at in stubs}
    queries=[];draws=[];finished=[]
    def caller(emu,ip,size,data):
     sp=emu.reg_read(UC_X86_REG_ESP)
     if ip==0x6D8700:
      _,ptr,isfog=struct.unpack('<III',emu.mem_read(sp,12));assert ptr==cell+0x24
      queries.append(isfog);ret(emu,fogValue if isfog else shroudValue,2)
     elif ip in guards:ret(emu,int(enabled),0)
     elif ip in continuations:
      at=continuations[ip];draws.append(at)
      # Restore the leaf prologue's local stack before a mocked blit returns.
      if at==0x47EFE0:
       ebp=emu.reg_read(UC_X86_REG_EBP);saved=struct.unpack('<I',emu.mem_read(ebp,4))[0]
       emu.reg_write(UC_X86_REG_EBP,saved);emu.reg_write(UC_X86_REG_ESP,ebp+4)
      ret(emu,0,3)
     elif ip==stop:finished.append(ip);emu.emu_stop()
    u.hook_add(UC_HOOK_CODE,caller);u.emu_start(0x4801F0,0,count=1500)
    assert finished==[stop] and queries==[0,1]
    assert u.reg_read(UC_X86_REG_ESP)==stack+12
    assert draws==([] if enabled else [0x47EFE0]+([0x47F250] if fog else []))
    outcomes.append((bytes(u.mem_read(cell,0x200)),bytes(u.mem_read(house,0x200))))
   assert outcomes[0]==outcomes[1]
   results.append({'case':'original_cell_caller','fog':fog,'shroud':shroudValue,'fog_value':fogValue,'pass':True})
report={'version':'1.0.6','dll_sha256':hashlib.sha256((root/'GameRuntime.dll').read_bytes()).hexdigest(),
 'reference_sha256':hashlib.sha256(args.reference.read_bytes()).hexdigest(),'stubs':{hex(k):hex(v[0]) for k,v in stubs.items()},
 'cases':results,'occlusion_and_blitters_mocked':True,'real_rendering_tested':False,'real_netplay_tested':False}
(root/'RENDER_REVEAL_VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: 8 compiled render stub cases and 18 original Cell caller comparisons; vision/cache parity, stack/flags/register/FPU preservation')

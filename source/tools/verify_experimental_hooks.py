"""Verify reference signatures and emulate actual compiled x86 callback stubs.
This does not run the Windows loader, original planning implementation or netplay.
Requires pefile and unicorn; the reference EXE is an explicit local input.
"""
from pathlib import Path
import argparse,hashlib,json,struct
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *

parser=argparse.ArgumentParser()
parser.add_argument('--reference',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parent.parent
path=root/'GameRuntime.dll';pe=pefile.PE(str(path));base=pe.OPTIONAL_HEADER.ImageBase
mapped=pe.get_memory_mapped_image();game=pefile.PE(str(args.reference))
signatures={0x4A8EB0:bytes.fromhex('A1 4C 3D A8 00'),
            0x7E2908:bytes.fromhex('50 CB 41 00'),
            0x416AB6:bytes.fromhex('C7 86 BC 00 00 00 03 00 00 00'),
            0x6385C0:bytes.fromhex('81 EC E0 00 00 00 53 56 8B F1')}
for at,expected in signatures.items():
    assert game.get_data(at-game.OPTIONAL_HEADER.ImageBase,len(expected))==expected,hex(at)
# Unique prologue belonging to our three naked stubs. Locate the actual DLL
# machine code without relying on source names or optional debug symbols.
prefix=bytes.fromhex('9C 60 89 E3 81 EC 10 02 00 00 83 E4 F0 0F AE 04 24 FC')
starts=[];offset=0
while (offset:=mapped.find(prefix,offset))>=0:
    starts.append(offset);offset+=len(prefix)
assert len(starts)==5,starts
stubs={}
for start in starts:
    block=mapped[start:start+48]
    if bytes.fromhex('6A 00 6A 00 6A 03 E8') in block:continue # covered by verify_render_reveal.py
    if bytes.fromhex('6A 00 51 6A 01 E8') in block:op=1
    elif bytes.fromhex('6A 00 56 6A 02 E8') in block:op=2
    else:op=0
    call=block.index(b'\xe8',len(prefix))
    target=base+start+call+5+struct.unpack_from('<i',block,call+1)[0]
    stubs[op]=(base+start,target)
assert set(stubs)=={0,1,2}
registers=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,
           UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
results=[]
for op in range(3):
 for result in (0,1):
  for direction in (0,0x400):
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,mapped)
    u.mem_map(0x400000,0x800000)
    u.mem_map(0x2000000,0x20000);u.mem_map(0x3000000,0x10000)
    u.mem_map(0x300000,0x1000)
    object=0x2002000;house=7;frame=0x2004000;esp=0x300F000;stop=0x300000
    u.mem_write(esp,struct.pack('<5I',stop,object,house,0x2222,0x3333))
    u.mem_write(0xA83D4C,struct.pack('<I',0x2006000))
    u.mem_write(frame+0xBC,struct.pack('<I',2))
    values=[0x12345678,0xABABABAB,object,0xCDCDCDCD,frame,0xEFEFEFEF,0x30303030]
    for reg,value in zip(registers,values):u.reg_write(reg,value)
    u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_EFLAGS,0x202|direction)
    u.reg_write(UC_X86_REG_MXCSR,0x1F80);u.reg_write(UC_X86_REG_FPCW,0x37F)
    xmm=[int.from_bytes(bytes([n+1])*16,'little') for n in range(8)]
    for n,v in enumerate(xmm):u.reg_write(UC_X86_REG_XMM0+n,v)
    start,guard=stubs[op];calls=[];finished=[]
    expected_stop=stop if op==1 or (op==0 and result) else 0x4A8EB5 if op==0 else 0x416AC0
    def hook(emu,at,size,data):
      if at==guard:
        stack=emu.reg_read(UC_X86_REG_ESP)
        returned,a,b,c=struct.unpack('<4I',emu.mem_read(stack,16))
        assert (a,b,c)==(op,frame if op==2 else object,house if op==0 else 0),(a,b,c)
        assert not emu.reg_read(UC_X86_REG_EFLAGS)&0x400,'DF must be clear inside C++ callback'
        calls.append((a,b,c))
        emu.reg_write(UC_X86_REG_EAX,result);emu.reg_write(UC_X86_REG_ECX,0xBAD);emu.reg_write(UC_X86_REG_EDX,0xBAD)
        for n in range(8):emu.reg_write(UC_X86_REG_XMM0+n,0)
        emu.reg_write(UC_X86_REG_MXCSR,0x3F80);emu.reg_write(UC_X86_REG_FPCW,0x27F)
        emu.reg_write(UC_X86_REG_ESP,stack+4);emu.reg_write(UC_X86_REG_EIP,returned)
      elif at==expected_stop:finished.append(at);emu.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook);u.emu_start(start,0,count=200)
    assert finished==[expected_stop] and len(calls)==1,(op,result,finished,calls)
    expected=values[:]
    if op==0:expected[0]=(values[0]&~255)|1 if result else 0x2006000
    elif op==1:expected[0]=result
    assert [u.reg_read(r) for r in registers]==expected,(op,result,'registers')
    assert u.reg_read(UC_X86_REG_ESP)==esp+(20 if op==0 and result else 4 if op==1 else 0)
    assert u.reg_read(UC_X86_REG_EFLAGS)==0x202|direction
    assert u.reg_read(UC_X86_REG_MXCSR)==0x1F80 and u.reg_read(UC_X86_REG_FPCW)==0x37F
    assert [u.reg_read(UC_X86_REG_XMM0+n) for n in range(8)]==xmm
    assert struct.unpack('<I',u.mem_read(frame+0xBC,4))[0]==(3 if op==2 and not result else 2)
    results.append({'op':op,'callback_result':result,'incoming_DF':bool(direction),'pass':True})
report={'version':'1.0.9','dll_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'reference_sha256':hashlib.sha256(args.reference.read_bytes()).hexdigest(),
        'signature_addresses':[hex(a) for a in signatures],
        'stub_addresses':{str(op):hex(value[0]) for op,value in stubs.items()},'cases':results,
        'native_game_functions_executed':False,'real_netplay_tested':False}
(root/'EXPERIMENTAL_HOOK_VERIFICATION.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: reference signatures; 12 compiled x86 stub cases: arguments, stack, branches, flags, registers and x87/SSE state')

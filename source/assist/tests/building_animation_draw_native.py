"""Original x86 DrawIfVisible/Draw_It guard probe; no game/Windows/rasterizer.

Uses a private supported executable, never writes its bytes. The image getter,
projection, map/cell, tint/height and final DrawSHP API are fixture boundaries.
The original visibility, fade, frame selection and draw-submission instructions
run unchanged. This tests draw eligibility, not a live terminal-frame sequence.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe',dest='game',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--unicorn-path',type=Path)
a=p.parse_args()
if a.unicorn_path:sys.path.insert(0,str(a.unicorn_path))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP

raw=a.game.read_bytes();sha=hashlib.sha256(raw).hexdigest()
assert sha=='7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6'
pe=struct.unpack_from('<I',raw,0x3c)[0];number=struct.unpack_from('<H',raw,pe+6)[0]
optional=pe+24;sections=optional+struct.unpack_from('<H',raw,pe+20)[0]
image=bytearray(struct.unpack_from('<I',raw,optional+56)[0]);image[:0x1000]=raw[:0x1000]
for n in range(number):
    size,rva,rawsize,offset=struct.unpack_from('<IIII',raw,sections+n*40+8)
    image[rva:rva+rawsize]=raw[offset:offset+rawsize]

OBJ=0x10000000;TYPE=OBJ+0x1000;SHAPE=OBJ+0x2000;CELL=OBJ+0x3000
STACK=0x70000000;STOP=STACK+0xF000;POINT=OBJ+0x4000;RECT=POINT+0x100
def run(alive,limbo,invisible,dying,value=19,start=0,shadow=False):
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x800000);u.mem_write(0x400000,bytes(image))
    u.mem_map(OBJ,0x10000);u.mem_map(STACK,0x10000)
    def put(p,v):u.mem_write(p,struct.pack('<I',v&0xffffffff))
    def byte(p,v):u.mem_write(p,bytes([v]))
    def word(p):return struct.unpack('<I',u.mem_read(p,4))[0]
    def ret(pop=0,value=0):
        sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value)
        u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
    put(OBJ,0x7E3354);put(OBJ+0xC8,TYPE);put(OBJ+0xAC,value);put(OBJ+0xD4,OBJ+0x6000)
    byte(OBJ+0x80,1);byte(OBJ+0x81,limbo);byte(OBJ+0x90,alive);byte(OBJ+0x179,dying);byte(OBJ+0x19D,invisible)
    put(TYPE+0x2B4,start);put(TYPE+0x2C0,20);put(TYPE+0x2BC,20);byte(TYPE+0x35D,1);byte(TYPE+0x372,int(shadow))
    put(0xA8EB78,2);put(0xABCD44,0);put(0xB73550,1);byte(0xA8ED6B,0);byte(0x8A0DF0,0)
    put(0x886FA0,0);put(0x886FA4,0);put(0x886FA8,640);put(0x886FAC,512)
    put(RECT,0);put(RECT+4,0);put(RECT+8,640);put(RECT+12,512)
    put(SHAPE+4,0);u.mem_write(SHAPE,struct.pack('<HHHH',0,64,64,80))
    put(POINT,320);put(POINT+4,256)
    draws=[];frontiers=[]
    def hook(uc,address,size,user):
        sp=u.reg_read(UC_X86_REG_ESP)
        if address==0x5F3E30:frontiers.append('image');ret(0,SHAPE)
        elif address==0x6D2140:
            out=word(sp+8);u.mem_write(out,struct.pack('<ii',320,256));ret(8,1)
        elif address==0x55AF60:ret(0,0)
        elif address==0x5F5F40:ret(0,0)
        elif address==0x5657A0:ret(4,CELL)
        elif address==0x487950:ret(0,0)
        elif address==0x425630:ret(0,0)
        elif address==0x6D20E0:ret(0,0)
        elif address==0x4AED70:
            draws.append({'shape':word(sp+4)==SHAPE,'frame':word(sp+8)})
            ret(56,1)
    u.hook_add(UC_HOOK_CODE,hook)
    sp=STACK+0x8000;u.mem_write(sp,struct.pack('<IIII',STOP,RECT,1,0))
    u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,OBJ)
    u.emu_start(0x422C70,STOP,count=20000)
    assert u.reg_read(UC_X86_REG_EIP)==STOP
    assert u.reg_read(UC_X86_REG_ESP)==sp+16
    return draws

cases=[]
for alive in [0,1]:
    for start in [0,20]:
        for value in [0,18,19]:
            draws=run(alive,0,0,0,value,start)
            assert draws==[{'shape':True,'frame':start+value}],draws
            cases.append({'alive':alive,'value':value,'start':start,'draw_frame':draws[0]['frame']})
for state in [(0,1,0,0),(0,0,1,0),(1,1,0,0),(1,0,1,0)]:
    assert not run(*state),state
for alive in [0,1]:assert run(alive,0,0,1)==[{'shape':True,'frame':19}]
for repeat in range(20):assert run(0,0,0,0)==[{'shape':True,'frame':19}]
report={'passed':True,'game_exe_sha256':sha,'scope':__doc__,'native_cases':len(cases)+6+20,'cases':cases,
        'code_sha256':{hex(at):hashlib.sha256(image[at-0x400000:at-0x400000+n]).hexdigest() for at,n in [(0x422C70,0x26),(0x5F4B10,0x1FF),(0x422CA0,0xC0D)]},
        'conclusion':'Alive=0 does not suppress original DrawIfVisible or Draw_It; limbo and Invisible do. TimeToDie is not a Draw_It guard; the reader deliberately keeps its existing exclusion as a conservative lifecycle constraint. No live terminal-state causality is established.'}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['scope','cases','code_sha256']}))

"""Compare read-only Drive/Ship posture with the original Draw_Matrix code.

Requires explicit authorized PE32 engine input and Unicorn/pefile. Only the
GetTechnoType virtual boundary returns a synthetic type; the native draw,
timers, quaternion interpolation, rocking and matrix math execute unchanged.
Ramp/quaternion tables and all objects are synthetic. No game code bytes,
assets, private executable or framebuffer are written to the report.
"""
from pathlib import Path
import argparse
import hashlib
import itertools
import json
import math
import pefile
import struct
import subprocess
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EIP,
                              UC_X86_REG_ESP, UC_X86_REG_FPCW)

SUPPORTED={
 '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0',
 'c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a',
}
PIN='b22f1860dd7b100cad57123d4ca5d459b9020d88'
RANGES={'Drive.Draw_Matrix':(0x4AFF60,0x4B0405),
        'Ship.Draw_Matrix':(0x69F670,0x69FB15),
        'base.Draw_Matrix':(0x55A730,0x55A7CF),
        'ramp.copy':(0x7559B0,0x7559CC),
        'ramp.interpolate':(0x755A40,0x755AB4),
        'quaternion.slerp':(0x646590,0x646729)}


def tables():
    result=[]
    for i in range(21):
        angle=0 if i==0 else .08+.02*(i%10)
        azimuth=i*math.tau/20
        x=math.cos(azimuth)*math.sin(angle/2)
        y=math.sin(azimuth)*math.sin(angle/2)
        z=0.;w=math.cos(angle/2)
        q=[x,y,z,w]
        matrix=[1-2*(y*y+z*z),2*(x*y-w*z),2*(x*z+w*y),0,
                2*(x*y+w*z),1-2*(x*x+z*z),2*(y*z-w*x),0,
                2*(x*z-w*y),2*(y*z+w*x),1-2*(x*x+y*y),0]
        # Both readers receive the same stored f32 values, not an ideal table.
        f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
        result.append(([f32(v) for v in matrix],[f32(v) for v in q]))
    return result


class Native:
    OBJECT=0x70000000
    LOCO=OBJECT+0x1000
    OUT=OBJECT+0x2000
    TYPE=OBJECT+0x3000
    VTABLE=OBJECT+0x4000
    STOP=OBJECT+0x20000
    STACK=OBJECT+0x2F000

    def __init__(self,path):
        data=path.read_bytes();self.sha=hashlib.sha256(data).hexdigest()
        assert self.sha in SUPPORTED,'unsupported executable'
        pe=pefile.PE(data=data);self.base=pe.OPTIONAL_HEADER.ImageBase
        assert self.base==0x400000 and pe.FILE_HEADER.Machine==0x14C
        self.image=pe.get_memory_mapped_image();self.u=Uc(UC_ARCH_X86,UC_MODE_32)
        self.u.mem_map(self.base,(len(self.image)+4095)&~4095)
        self.u.mem_write(self.base,self.image);self.u.mem_map(self.OBJECT,0x30000)
        self.put(self.OBJECT,'I',self.VTABLE)
        self.put(self.VTABLE+0x84,'I',self.STOP+0x100)
        self.put(self.LOCO+8,'I',self.OBJECT)
        self.put(self.OBJECT+0x390,'i',-1)
        for i,(matrix,q) in enumerate(tables()):
            self.put(0xB45188+i*48,'12f',*matrix)
            self.put(0xB43188+i*16,'4f',*q)
        self.u.hook_add(UC_HOOK_CODE,self.boundary)
        self.boundaries=0
        self.hashes={n:hashlib.sha256(self.image[a-self.base:b-self.base]).hexdigest()
                     for n,(a,b) in RANGES.items()}

    def put(self,address,fmt,*values):
        self.u.mem_write(address,struct.pack('<'+fmt,*values))

    def boundary(self,u,address,size,data):
        if address!=self.STOP+0x100:return
        self.boundaries+=1
        sp=u.reg_read(UC_X86_REG_ESP)
        ret=struct.unpack('<I',u.mem_read(sp,4))[0]
        u.reg_write(UC_X86_REG_EAX,self.TYPE)
        u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)

    def draw(self,values):
        current,previous,start,duration,rate,frame,bucket,ship,side,forward,sx,sy=values
        address=0x69F670 if ship else 0x4AFF60
        table=0x7F2D8C if ship else 0x7E7EB0
        fn=struct.unpack_from('<I',self.image,table+0x24-self.base)[0]
        assert fn==address,'unexpected locomotor draw entry'
        self.put(self.LOCO,'I',table)
        self.put(self.LOCO+0x18,'6i',current,previous,start,0,duration,rate)
        self.put(self.OBJECT+0x328,'2f',side,forward)
        self.put(self.TYPE+0x360,'2d',sx,sy)
        self.put(self.OBJECT+0x388,'H',bucket*2048)
        self.put(self.OBJECT+0x38C,'H',bucket*2048)
        self.put(0xA8ED84,'i',frame)
        self.put(self.STACK,'4I',self.STOP,self.LOCO,self.OUT,0)
        self.u.reg_write(UC_X86_REG_ESP,self.STACK)
        self.u.reg_write(UC_X86_REG_FPCW,0xE7F)
        self.u.emu_start(address,self.STOP,count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP)==self.STOP
        assert self.u.reg_read(UC_X86_REG_ESP)==self.STACK+16
        return struct.unpack('<12f',self.u.mem_read(self.OUT,48))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('games',type=Path,nargs='+')
    parser.add_argument('--portable',type=Path,required=True,
                        help='compiled assist/tests/voxel_posture.cpp executable')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();table=tables();results=[]
    cases=[]
    for current,bucket,elapsed,rock,ship in itertools.product(
            range(21),(0,8,16,24),(0,4,8),((0.,0.),(.1,-.15)),(0,1)):
        previous=(current+7)%21
        cases.append((current,previous,100,8,8,100+elapsed,bucket,ship,*rock,1.,1.))
    # The steady path copies the current matrix directly; rocking also uses
    # native shape-scale pivot offsets rather than a generic center rotation.
    for current,bucket,scale,ship in itertools.product(range(21),(3,11,19,27),
            ((1.,1.),(1.5,2.25)),(0,1)):
        cases.append((current,current,100,0,0,100,bucket,ship,-.12,.18,*scale))
    lines=[]
    for values in cases:
        current,previous=values[:2]
        lines.append(' '.join(map(str,(*values,*table[current][0],*table[previous][0],
                                      *table[current][1],*table[previous][1]))))
    proc=subprocess.run([str(args.portable),'--matrix'],input='\n'.join(lines)+'\n',
                        text=True,capture_output=True,check=True)
    portable=[]
    for line in proc.stdout.splitlines():
        fields=line.split();assert fields[0]=='1',line
        portable.append(tuple(map(float,fields[1:])))
    assert len(portable)==len(cases)
    for path in args.games:
        native=Native(path);maximum=0.;worst=None
        for values,actual in zip(cases,portable):
            expected=native.draw(values)
            error=max(abs(a-b) for a,b in zip(actual,expected))
            if error>maximum:maximum=error;worst=values
        results.append({'executable_sha256':native.sha,'cases':len(cases),
                        'maximum_matrix_coefficient_error':maximum,
                        'tolerance':.001,'passed':maximum<.001,
                        'worst_synthetic_case':list(worst),
                        'type_virtual_boundary_calls':native.boundaries,
                        'code_range_sha256':native.hashes})
    report={'result':'PASS' if all(r['passed'] for r in results) else 'FAIL',
            'pinned_yrpp_commit':PIN,'results':results,
            'scope':'actual Drive/Ship Draw_Matrix, native timer, ramp quaternion and rocking arithmetic; synthetic objects/types/ramp tables; no game loop or renderer',
            'not_certified':['custom/other locomotors','native shadow matrix','live-game or multiplayer rendering']}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0 if report['result']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())

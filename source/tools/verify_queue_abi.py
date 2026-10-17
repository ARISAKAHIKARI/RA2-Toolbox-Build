#!/usr/bin/env python3
"""Read-only PE32 static fingerprint for the game's QueueMegaMission wrapper.

Does NOT modify the EXE. Static matching is NOT an in-memory Hook or netplay test.
Usage: python tools/verify_queue_abi.py gamemd-spawn.exe [another.exe]
"""
import hashlib
from pathlib import Path
import struct
import sys

def image_reader(buf):
    if buf[:2] != b'MZ': raise ValueError('not MZ')
    nt=struct.unpack_from('<I',buf,0x3C)[0]
    if buf[nt:nt+4] != b'PE\0\0':raise ValueError('not PE')
    machine,sections=struct.unpack_from('<HH',buf,nt+4)
    opt=nt+24
    if machine!=0x14c or struct.unpack_from('<H',buf,opt)[0]!=0x10b:raise ValueError('not x86 PE32')
    base=struct.unpack_from('<I',buf,opt+28)[0]
    sectoff=opt+struct.unpack_from('<H',buf,nt+20)[0]
    def read_va(address,count):
        rva=address-base
        for i in range(sections):
            j=sectoff+i*40
            vsize,va,rawsize,rawptr=struct.unpack_from('<IIII',buf,j+8)
            if va<=rva<va+min(rawsize,max(vsize,rawsize)) and rva-va+count<=rawsize:
                return bytes(buf[rawptr+rva-va:rawptr+rva-va+count])
        raise ValueError(f'VA {address:#x} unmapped')
    return read_va

signatures={
    0x646E90: bytes.fromhex('83 ec 70 32 c0 56 57'),
    0x646E97: bytes.fromhex('8b 7c 24 7c 83 ec 08 8b f4 83 ec 08 89 3e 88 46 04'),
    0x646EF0: bytes.fromhex('e8 6b f9 e7 ff'),
    0x646F16: bytes.fromhex('b9 1b 00 00 00 f3 a5 66 a5 a4'),
    0x646F20: bytes.fromhex('ff 15 30 15 7e 00'),
    0x646F43: bytes.fromhex('a3 c8 02 a8 00'),
    0x646F55: bytes.fromhex('c2 0c 00'),
}

def check(path):
    b=Path(path).read_bytes()
    f=image_reader(b)
    errors=[]
    for a,exp in signatures.items():
        if (actual:=f(a,len(exp)))!=exp:errors.append(f'{a:#x}: {actual.hex()} != {exp.hex()}')
    if errors:raise ValueError('; '.join(errors))
    return hashlib.sha256(b).hexdigest(),hashlib.sha256(f(0x646E90,0xD0)).hexdigest()

if __name__=='__main__':
    if len(sys.argv)<2:
        sys.exit('usage: verify_queue_abi.py gamemd-spawn.exe [...]')
    ok=True
    for name in sys.argv[1:]:
        try:
            sha,part=check(name)
            print(f'PASS {Path(name).name}: SHA256={sha} queue_prologue_sha256={part}')
        except Exception as e:
            ok=False;print(f'FAIL {Path(name).name}: {e}')
    print('Static ABI only: not a multiplayer/runtime signature certification')
    sys.exit(0 if ok else 1)

"""Resolve fixture entry points from the matching MSF7 PDB, never fixed RVAs."""
import struct
from pathlib import Path

def function_address(pe,path,name):
    raw=Path(path).read_bytes()
    assert raw.startswith(b'Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0')
    block_size,_,_,directory_size,_,block_map=struct.unpack_from('<6I',raw,32)
    count=lambda size:(size+block_size-1)//block_size
    def read(blocks,size):return b''.join(raw[n*block_size:(n+1)*block_size] for n in blocks)[:size]
    blocks=struct.unpack_from('<%dI'%count(directory_size),raw,block_map*block_size)
    directory=read(blocks,directory_size);n=struct.unpack_from('<I',directory)[0]
    sizes=struct.unpack_from('<%dI'%n,directory,4);at=4+4*n;streams=[]
    for size in sizes:
        if size==0xffffffff:streams.append(b'');continue
        length=count(size);blocks=struct.unpack_from('<%dI'%length,directory,at);at+=4*length
        streams.append(read(blocks,size))
    debug=[pe.get_data(e.struct.AddressOfRawData,e.struct.SizeOfData) for e in pe.DIRECTORY_ENTRY_DEBUG if e.struct.Type==2]
    assert len(debug)==1 and debug[0][:4]==b'RSDS'
    assert streams[1][8:12]==debug[0][20:24] and streams[1][12:28]==debug[0][4:20],'PDB/DLL identity mismatch'
    found=set();needle=name+b'\0'
    for stream in streams:
        offset=0
        while True:
            at=stream.find(needle,offset)
            if at<0:break
            offset=at+len(needle)
            if at>=14 and struct.unpack_from('<H',stream,at-12)[0]==0x110e:
                _,_,flags,value,segment=struct.unpack_from('<HHIIH',stream,at-14)
                if flags&2 and 0<segment<=len(pe.sections):found.add(pe.OPTIONAL_HEADER.ImageBase+pe.sections[segment-1].VirtualAddress+value)
            if at>=39 and struct.unpack_from('<H',stream,at-37)[0] in (0x110f,0x1110):
                value,segment=struct.unpack_from('<IH',stream,at-7)
                if 0<segment<=len(pe.sections):found.add(pe.OPTIONAL_HEADER.ImageBase+pe.sections[segment-1].VirtualAddress+value)
    assert len(found)==1,(name,found)
    return found.pop()

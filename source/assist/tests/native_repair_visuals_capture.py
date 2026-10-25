"""Original repair-wrench DrawExtras selection, animation and status draw ABI.

Input is a private supported capture. The original Building center, Map lookup,
GameOptions::GetAnimSpeed and Tactical projection execute unchanged. Only the
IsShrouded result and final DrawSHP boundary are controlled. Output contains
counts, hashes, frame choices and aggregate properties, never game art or dumps.
Repair flags are controlled: no repairing building was present in this capture.
This does not prove live Windows visuals, multiplayer or every DrawExtras path.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, SUPPORTED_SHA
from native_lightconvert_capture import Native
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                               UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW)

CPP = r'''
#include "source/external_reader/source/repair_visuals.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
int main(){
 auto word=[](){uint32_t n=0;std::cin.read(reinterpret_cast<char*>(&n),4);return n;};
 uint32_t object=word(),id=word(),count=word();std::map<uint32_t,std::vector<uint8_t>> memory;
 for(unsigned i=0;i<count;++i){auto at=word(),n=word();auto&bytes=memory[at];bytes.resize(n);std::cin.read(reinterpret_cast<char*>(bytes.data()),n);}assert(std::cin);
 auto read=[&](uint32_t at,void*out,size_t n){auto i=memory.upper_bound(at);if(i==memory.begin())return false;--i;
  if(uint64_t(at)-i->first+n>i->second.size())return false;memcpy(out,i->second.data()+at-i->first,n);return true;};
 for(int speed=0;speed<7;++speed){memcpy(memory.at(0xA8EB60).data(),&speed,4);int period=std::max(2,(112/(speed+1))/4);
  for(int tick=0;tick<period*2;++tick){memcpy(memory.at(0xA8ED84).data(),&tick,4);external::repairvisuals::Sprite out;
   assert(external::repairvisuals::sample(read,object,id,out));
   std::cout<<"{\"speed\":"<<speed<<",\"tick\":"<<tick<<",\"frame\":"<<out.frame<<",\"world\":["<<out.x<<","<<out.y<<","<<out.z<<"]}\n";
  }
 }
}
'''


def inspect(capture, executable):
    n = Native(capture)
    assert capture.name(0x819404, 24) == 'WRENCH.SHP'
    assert capture.word(0x7E3EBC+0x110) == 0x6F5190
    shape, converter = capture.word(0x89DDC8), capture.word(0x87F6C8)
    assert capture.word(converter) == 0x7E5358 and capture.word(converter+4) == 2
    assert capture.word(converter+0x16C) == 1
    rows, draws, controlled = [], [], {'shrouded': False}

    def boundary(u, address, _size, _data):
        if address in (0x6F534E, 0x6F5EE3):
            u.emu_stop()
            return
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x487950:
            u.reg_write(UC_X86_REG_EAX, int(controlled['shrouded']))
            cleanup = 0
        else:
            assert address == 0x4AED70
            args = struct.unpack('<14I', u.mem_read(sp+4, 56))
            position = struct.unpack('<2i', u.mem_read(args[2], 8))
            assert args[0] == shape and u.reg_read(UC_X86_REG_EDX) == converter
            assert args[3] == 0x71009000 and args[4] == 0xE00
            assert args[8] == 1000 and all(args[i] == 0 for i in (5, 6, 7, 9, 10, 11, 12, 13))
            draws.append({'frame': args[1], 'screen': position})
            cleanup = 56
        u.reg_write(UC_X86_REG_EIP, n.read(sp))
        u.reg_write(UC_X86_REG_ESP, sp+4+cleanup)

    hooks = [n.u.hook_add(UC_HOOK_CODE, boundary, begin=fn, end=fn)
             for fn in (0x487950, 0x4AED70, 0x6F534E, 0x6F5EE3)]

    def draw(building, tick, speed, repair=True, sink=False, shroud=False):
        draws.clear()
        n.write(0xA8ED84, tick)
        n.write(0xA8EB60, speed)
        n.u.mem_write(building+0x6E8, bytes([repair]))
        n.u.mem_write(building+0x3CD, bytes([sink]))
        n.u.mem_write(building+0x68, b'\0')  # unrelated Ivan-bomb branch excluded
        n.u.mem_write(0x70008000, struct.pack('<3I', 0x72000000, 0x71009100, 0x71009000))
        n.u.mem_write(0x71009000, struct.pack('<4i', 0, 0, 640, 480))
        controlled['shrouded'] = shroud
        for reg, value in ((UC_X86_REG_ECX, building), (UC_X86_REG_ESP, 0x70008000), (UC_X86_REG_FPCW, 0xE7F)):
            n.u.reg_write(reg, value)
        n.u.emu_start(0x6F5190, 0x72000000, count=100000)
        assert n.u.reg_read(UC_X86_REG_EIP) in (0x6F534E, 0x6F5EE3)
        return list(draws)

    try:
        buildings = []
        types = set()
        for building in capture.vector(0xA8EB40):
            if capture.word(building) != 0x7E3EBC or capture.signed(building+0x6C) <= 0:
                continue
            typ = capture.word(building+0x520)
            if typ not in types:
                buildings.append(building)
                types.add(typ)
            if len(buildings) == 3:
                break
        for building in buildings:
            typ, identity = capture.word(building+0x520), capture.word(building+0x10)
            assert capture.word(capture.word(building)+0xAC) == 0x459EF0
            xyz = struct.unpack('<3i', capture.read(building+0x9C, 12))
            native_center = n.call(0x459EF0, building, [0x71009200])
            world = struct.unpack('<3i', n.u.mem_read(native_center, 12))
            assert world == (xyz[0]-128, xyz[1]-128, xyz[2])
            n.call(0x6D2140, capture.word(0x887324), [native_center, 0x71009300])
            projected = struct.unpack('<2i', n.u.mem_read(0x71009300, 8))
            spans = [(building, 0x6E9), (0x89DDC8, 4), (0x87F6C8, 4), (0xA8ED84, 4), (0xA8EB60, 4)]
            data = struct.pack('<3I', building, identity, len(spans))
            for address, size in spans:
                value = bytearray(capture.read(address, size))
                if address == building:
                    value[0x6E8], value[0x3CD] = 1, 0
                data += struct.pack('<2I', address, size)+value
            result = subprocess.run([str(executable)], input=data, capture_output=True, check=True)
            portable = [json.loads(line) for line in result.stdout.splitlines()]
            for case in portable:
                actual = draw(building, case['tick'], case['speed'])
                assert len(actual) == 1 and actual[0]['frame'] == case['frame']
                assert actual[0]['screen'] == projected and case['world'] == list(world)
            gates = 0
            for repair in (False, True):
                for sink in (False, True):
                    for shroud in (False, True):
                        assert bool(draw(building, 100, 1, repair, sink, shroud)) == (repair and not sink and not shroud)
                        gates += 1
            rows.append({'type': capture.name(typ+0x24, 24), 'original_frame_position_draw_cases': len(portable),
                         'controlled_repair_sinking_shroud_gates': gates,
                         'native_center_delta': [-128, -128, 0], 'frames_seen': sorted({r['frame'] for r in portable})})
    finally:
        for hook in hooks:
            n.u.hook_del(hook)
    return {'capture_actual_repairing_buildings': sum(bool(capture.read(b+0x6E8, 1)[0]) for b in capture.vector(0xA8EB40) if capture.word(b) == 0x7E3EBC),
            'resource': 'WRENCH.SHP', 'shape_capture': capture.shape(shape), 'draw_flags': '0E00', 'draw_intensity': 1000,
            'palette': 'MOUSE_PAL', 'palette_shades': 1, 'tested_buildings': rows,
            'code_sha256': {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a, b-a)).hexdigest()
                            for a, b in ((0x6F5190, 0x6F51B0), (0x6F5253, 0x6F5347),
                                         (0x459EF0, 0x459F1B), (0x5FB2E0, 0x5FB306),
                                         (0x45E98D, 0x45E9A3))}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    with tempfile.TemporaryDirectory(prefix='ra2-native-repair-') as directory:
        source, executable = Path(directory)/'probe.cpp', Path(directory)/'probe'
        source.write_text(CPP)
        subprocess.run(['g++', '-std=c++17', '-O2', '-I'+str(root), str(source), '-o', str(executable)], check=True)
        stages = {stage: inspect(Capture(args.capture, stage), executable) for stage in ('01-off', '02-on', '03-off')}
    result = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__, 'stages': stages,
              'game_tested': False, 'windows_rendering_tested': False, 'multiplayer_tested': False}
    Path(args.output).write_text(json.dumps(result, indent=2)+'\n')
    print('PASS original repair DrawExtras: '+str(sum(r['original_frame_position_draw_cases'] for s in stages.values() for r in s['tested_buildings']))+
          ' frame/center/draw ABI cases; '+str(sum(r['controlled_repair_sinking_shroud_gates'] for s in stages.values() for r in s['tested_buildings']))+' controlled gates')


if __name__ == '__main__':
    main()

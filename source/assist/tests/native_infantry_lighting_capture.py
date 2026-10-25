"""Verify ExtraInfantryLight using captured original x86, then production C++.

Input is a private supported-game capture. Only type names, aggregate brightness
values and code hashes are emitted. Ground Infantry::Draw selection and push
slices execute unchanged. INI tests seed ReadDouble's x87 return, retaining the
original multiply/_ftol/store. This is not a live game, complete drawing path,
Windows visual comparison or multiplayer test. Airborne/bridge/cloak/flash
special cases are not claimed by this ordinary-ground fixture.
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
from unicorn.x86_const import (UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_EIP,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW)

CPP = r'''
#include "source/external_reader/source/scene_lighting.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
int main(){
 auto word=[](){uint32_t n=0;std::cin.read(reinterpret_cast<char*>(&n),4);return n;};
 uint32_t type=word(),owner=word(),x=word(),y=word(),z=word(),rules=word(),count=word();
 std::map<uint32_t,std::vector<uint8_t>> memory;
 for(unsigned i=0;i<count;++i){auto at=word(),size=word();auto&bytes=memory[at];bytes.resize(size);std::cin.read(reinterpret_cast<char*>(bytes.data()),size);}assert(std::cin);
 auto read=[&](uint32_t at,void*out,size_t n){auto i=memory.upper_bound(at);if(i==memory.begin())return false;--i;
  if(uint64_t(at)-i->first+n>i->second.size())return false;memcpy(out,i->second.data()+at-i->first,n);return true;};
 for(int extra:{0,200,-200,600,-2000,4000}){
  memcpy(memory.at(rules+0x17D8).data(),&extra,4);
  auto light=external::lighting::object(read,type,owner,1,x,y,z,{});
  std::cout<<"{\"extra\":"<<extra<<",\"production_intensity\":"<<light.intensity<<"}\n";
 }
}
'''


def inspect(capture, executable):
    n, rows, skipped_bridge_or_structure = Native(capture), [], 0
    rules = capture.word(0x8871E0)
    assert capture.name(0x83A23C) == 'ExtraInfantryLight'
    assert struct.unpack('<d', capture.read(0x7E4658, 8))[0] == 1000
    seed, number = 0x72000200, 0x71009010
    n.u.mem_write(seed, b'\xdb\xe3\xdd\x05'+struct.pack('<I', number))
    scaling = []
    for config in (0.2, 0, -0.2, 0.75, -2, 4):
        n.u.mem_write(number, struct.pack('<d', config))
        n.u.emu_start(seed, seed+8, count=2)
        n.u.reg_write(UC_X86_REG_ESP, 0x70008000)
        n.u.reg_write(UC_X86_REG_ESI, rules)
        n.u.reg_write(UC_X86_REG_FPCW, 0xE7F)
        n.u.emu_start(0x66B6F4, 0x66B705, count=1000)
        assert n.u.reg_read(UC_X86_REG_EIP) == 0x66B705
        stored = struct.unpack('<i', n.u.mem_read(rules+0x17D8, 4))[0]
        assert stored == int(config*1000)
        scaling.append({'config_double': config, 'stored_signed_integer': stored})
    for infantry in capture.vector(0xA8EC78):
        if capture.word(infantry) != 0x7EB058 or capture.signed(infantry+0x6C) <= 0:
            continue
        x, y, z = struct.unpack('<3i', capture.read(infantry+0x9C, 12))
        if x <= 0 or y <= 0 or capture.read(infantry+0x8C, 1) != b'\0':
            continue
        typ, owner = capture.word(infantry+0x6C0), capture.word(infantry+0x21C)
        items = capture.word(0x87F7E8+0x13C)
        cell_slot = items+(y//256*512+x//256)*4
        cell = capture.word(cell_slot)
        if capture.word(cell+0x140) & 0x10080:
            skipped_bridge_or_structure += 1
            continue
        base = struct.unpack('<h', capture.read(cell+0x10A, 2))[0]
        vector = capture.word(typ+0xDF0) or 0xB054D0
        scheme_slot = capture.word(vector+4)+capture.word(owner+0x16054)*4
        scheme = capture.word(scheme_slot)
        convert = capture.word(scheme+0x30C)
        spans = [(typ, 0xE00), (owner+0x16054, 4), (0x87F7E8+0x13C, 8),
                 (cell_slot, 4), (cell, 0x150), (0x89E7C0, 4), (vector, 20),
                 (scheme_slot, 4), (scheme+0x30C, 4), (convert, 0x1B4),
                 (0x8871E0, 4), (rules+0x17D8, 4)]
        data = struct.pack('<7I', typ, owner, x, y, z, rules, len(spans))
        for address, size in spans:
            data += struct.pack('<2I', address, size)+capture.read(address, size)
        portable = subprocess.run([str(executable)], input=data, check=True, capture_output=True)
        cases = [json.loads(line) for line in portable.stdout.splitlines()]
        # Retain the native Infantry GetCell ABI and MapClass lookups.
        coord = n.call(capture.word(capture.word(infantry)+0x1C0), infantry, [0x71008000])
        coord = bytes(n.u.mem_read(coord, 4))
        for case in cases:
            n.write(rules+0x17D8, case['extra'])
            n.u.mem_write(0x70008058, coord)
            for reg, value in ((UC_X86_REG_ESP, 0x70008000), (UC_X86_REG_EBP, infantry),
                               (UC_X86_REG_EBX, capture.signed(capture.word(0xA8B230)+0x3544))):
                n.u.reg_write(reg, value)
            n.u.emu_start(0x51933B, 0x51945D, count=100000)
            assert n.u.reg_read(UC_X86_REG_EIP) == 0x51945D
            native = struct.unpack('<i', struct.pack('<I', n.u.reg_read(UC_X86_REG_ESI)))[0]
            assert native == base+case['extra']
            assert case['production_intensity'] == max(0, min(2000, native))
            # Execute the actual brightness push; stop before the frame/draw calls.
            n.u.emu_start(0x51958A, 0x5195B5, count=100)
            assert n.u.reg_read(UC_X86_REG_EIP) == 0x5195B5
            pushed = struct.unpack('<i', n.u.mem_read(n.u.reg_read(UC_X86_REG_ESP), 4))[0]
            assert pushed == native
            case.update(native_unclamped_intensity=native, native_pushed_intensity=pushed)
        rows.append({'type': capture.name(typ+0x24, 24), 'cell_object_intensity': base,
                     'captured_extra_infantry_light': capture.signed(rules+0x17D8), 'cases': cases})
    assert rows
    return {'runtime_signed_integer_offset': '17D8', 'rules_scaling_cases': scaling,
            'native_ground_infantry_cases': sum(len(r['cases']) for r in rows),
            'skipped_bridge_or_structure_infantry': skipped_bridge_or_structure,
            'infantry': rows,
            'code_sha256': {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a, b-a)).hexdigest()
                            for a, b in ((0x66B6E7, 0x66B705), (0x51933B, 0x51945D),
                                         (0x51958A, 0x5195E8))}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    with tempfile.TemporaryDirectory(prefix='ra2-infantry-light-') as directory:
        source, executable = Path(directory)/'probe.cpp', Path(directory)/'probe'
        source.write_text(CPP)
        subprocess.run(['g++', '-std=c++17', '-O2', '-I'+str(root), str(source), '-o', str(executable)], check=True)
        stages = {stage: inspect(Capture(args.capture, stage), executable)
                  for stage in ('01-off', '02-on', '03-off')}
    result = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__,
              'stages': stages, 'game_tested': False, 'windows_rendering_tested': False,
              'multiplayer_tested': False}
    Path(args.output).write_text(json.dumps(result, indent=2)+'\n')
    print('PASS 18 original Rules scaling cases; '+str(sum(s['native_ground_infantry_cases'] for s in stages.values()))+
          ' original ground Infantry brightness/push and production cases')


if __name__ == '__main__':
    main()

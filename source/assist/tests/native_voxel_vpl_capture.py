"""Audit loaded VPL, original indexed voxel raster and fresh/cached blitters.

The input is a private supported-game capture. Only counts, hashes, bounds and
mean RGB are emitted. Captured model/palette/table bytes travel to a temporary
production C++ probe through stdin and are never written as output artifacts.
The raster comparison uses body models and the same raw owner palette; it is
before scene LightConvert, ABuffer, clipping and shadows. Separate one-pixel
tests execute both original scalar blitter paths against the captured LUT.
Unit brightness/base/dispatch uses captured ground units. Aircraft tests cover
only its separate additive-rule and push slices with a controlled altitude term.
No live game, Windows display, complete scene compositor or multiplayer runs.
"""
from collections import Counter
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, SUPPORTED_SHA
from native_lightconvert_capture import Native
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP,
                               UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI,
                               UC_X86_REG_ESI, UC_X86_REG_EIP, UC_X86_REG_ESP,
                               UC_X86_REG_FPCW)


CPP = r'''
#include "source/external_reader/source/voxel_preview.h"
#include <cassert>
#include <iostream>
#include <map>
using namespace external::voxel;
int main(){
 auto word=[](){uint32_t n=0;std::cin.read(reinterpret_cast<char*>(&n),4);return n;};
 uint32_t lib=word(),hva=word(),count=word();std::map<uint32_t,std::vector<uint8_t>> memory;
 for(unsigned i=0;i<count;++i){auto at=word(),size=word();auto&bytes=memory[at];bytes.resize(size);std::cin.read(reinterpret_cast<char*>(bytes.data()),size);}
 uint8_t rgb[768];std::cin.read(reinterpret_cast<char*>(rgb),768);assert(std::cin);std::array<uint32_t,256> palette{};
 for(int i=1;i<256;++i)palette[i]=0xff000000u|(rgb[i*3]<<16)|(rgb[i*3+1]<<8)|rgb[i*3+2];
 auto read=[&](uint32_t at,void*out,size_t n){auto i=memory.upper_bound(at);if(i==memory.begin())return false;--i;
  if(uint64_t(at)-i->first+n>i->second.size())return false;memcpy(out,i->second.data()+at-i->first,n);return true;};
 Vpl vpl;assert(vpl.capture(read));auto&pages=memory.at(0xB41178);
 for(size_t i=0;i<pages.size();++i)assert(vpl.map(unsigned(i/256),uint8_t(i))==pages[i]);
 std::vector<uint8_t> file(784+pages.size());memcpy(file.data()+8,memory.at(0xB2FB58).data(),4);memcpy(file.data()+784,pages.data(),pages.size());
 Vpl disk;assert(disk.load(file)&&disk.signature()==vpl.signature());
 Model model;assert(capturePart(read,lib,hva,model));
 for(int facing:{0,8,16,24})for(bool useVpl:{true,false}){
  Image image;assert(render(model,(facing-8)*(6.28318530717958647692/32),palette,image,0,0,useVpl?&vpl:nullptr,255));
  double sums[3]{};unsigned pixels=0,zero=0;
  for(size_t i=0;i<image.pixels.size();++i){auto color=image.pixels[i];if(!(color>>24))continue;++pixels;zero+=image.indices[i]==0;for(int c=0;c<3;++c)sums[c]+=(color>>(16-c*8))&255;}
  assert(pixels);std::cout<<"{\"facing\":"<<facing<<",\"vpl\":"<<(useVpl?"true":"false")<<",\"opaque_pixels\":"<<pixels<<",\"opaque_zero_index\":"<<zero
   <<",\"bounds\":["<<image.x<<","<<image.y<<","<<image.width<<","<<image.height<<"],\"mean_rgb\":["<<sums[0]/pixels<<","<<sums[1]/pixels<<","<<sums[2]/pixels<<"]}\n";
 }
}
'''

CPP_LIGHT = r'''
#include "source/external_reader/source/scene_lighting.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
int main(){
 auto word=[](){uint32_t n=0;std::cin.read(reinterpret_cast<char*>(&n),4);return n;};
 uint32_t type=word(),owner=word(),x=word(),y=word(),z=word(),rules=word(),count=word();std::map<uint32_t,std::vector<uint8_t>> memory;
 for(unsigned i=0;i<count;++i){auto at=word(),size=word();auto&bytes=memory[at];bytes.resize(size);std::cin.read(reinterpret_cast<char*>(bytes.data()),size);}assert(std::cin);
 auto read=[&](uint32_t at,void*out,size_t n){auto i=memory.upper_bound(at);if(i==memory.begin())return false;--i;
  if(uint64_t(at)-i->first+n>i->second.size())return false;memcpy(out,i->second.data()+at-i->first,n);return true;};
 for(int kind:{2,0})for(int extra:{0,200,-200,600,-2000,4000}){
  memcpy(memory.at(rules+(kind==2?0x17D4:0x17DC)).data(),&extra,4);
  auto light=external::lighting::object(read,type,owner,kind,x,y,z,{});
  std::cout<<"{\"kind\":"<<kind<<",\"extra\":"<<extra<<",\"production_intensity\":"<<light.intensity<<"}\n";
 }
}
'''


def code_hashes(capture):
    ranges = ((0x753B70, 0x753BCB), (0x758A30, 0x758AC3),
              (0x753D00, 0x754610), (0x494B60, 0x494C3A),
              (0x4958D0, 0x4959B1), (0x497FD0, 0x498150),
              (0x4990E0, 0x49927F), (0x490B90, 0x490E50),
              (0x66B69E, 0x66B6BC), (0x73CF70, 0x73D0DB),
              (0x73D30E, 0x73D395), (0x70676F, 0x7067E4),
              (0x66B730, 0x66B754), (0x414925, 0x41493E),
              (0x414987, 0x4149DD))
    return {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a, b-a)).hexdigest()
            for a, b in ranges}


def loader(capture):
    """Original wrapper and loader; only FileClass open/read/close are doubled."""
    n = Native(capture)
    file_at, vtable = 0x71008000, 0x71008100
    n.write(file_at, vtable)
    for slot, target in ((0x1C, 0x72000100), (0x24, 0x72000110), (0x34, 0x72000120)):
        n.write(vtable+slot, target)
    checks = []
    for count in (1, 16, 32):
        palette = bytes((i*7+5) & 255 for i in range(768))
        pages = bytes((i*13+i//256) & 255 for i in range(count*256))
        data = struct.pack('<4I', 16, 31, count, 0xA5A55A5A) + palette + pages
        offset, requests = 0, []
        n.write(0xB2FB58, 0)

        def boundary(u, address, _size, _data):
            nonlocal offset
            assert u.reg_read(UC_X86_REG_ECX) == file_at
            sp = u.reg_read(UC_X86_REG_ESP)
            cleanup, result = 0, 0
            if address == 0x72000100:
                assert n.read(sp+4) == 1
                cleanup, result = 4, 1
            elif address == 0x72000110:
                destination, size = struct.unpack('<II', u.mem_read(sp+4, 8))
                assert offset+size <= len(data) and n.read(0xB2FB58) == 0
                u.mem_write(destination, data[offset:offset+size])
                offset += size
                requests.append(size)
                cleanup, result = 8, size
            else:
                assert address == 0x72000120
            u.reg_write(UC_X86_REG_EAX, result)
            u.reg_write(UC_X86_REG_ESP, sp+4+cleanup)
            u.reg_write(UC_X86_REG_EIP, n.read(sp))

        hook = n.u.hook_add(UC_HOOK_CODE, boundary, begin=0x72000100, end=0x72000120)
        try:
            assert n.call(0x753B70, file_at, []) == 0
            assert n.u.reg_read(UC_X86_REG_ESP) == 0x70008000+4
        finally:
            n.u.hook_del(hook)
        assert offset == len(data) and requests == [16, 768, count*256]
        assert n.read(0xB2FB58) == count
        assert bytes(n.u.mem_read(0xB41178, len(pages))) == pages
        assert bytes(n.u.mem_read(0xB2FB78, 768)) == palette
        checks.append({'pages': count, 'read_lengths': requests, 'mapping_unchanged': True})
    return checks


def blitters(capture):
    n = Native(capture)
    scheme = next(s for s in capture.vector(0xB054D0) if capture.word(s+0x310) > 1)
    converter = capture.word(scheme+0x30C)
    cases, selected = 0, []
    for flags in (0x2800, 0x6800):
        plain = n.call(0x490B90, converter, [flags])
        rle = n.call(0x490E50, converter, [flags])
        fn = n.read(n.read(plain)+4)
        assert n.read(plain+4) == n.read(rle+4) and n.read(plain+8) == n.read(rle+8)
        selected.append({'flags': f'{flags:04X}', 'fresh': f'{fn:08X}',
                         'cached': f'{n.read(n.read(rle)+4):08X}'})
        for brightness in (0, 500, 1000, 1056, 1140, 1168, 1500, 2000):
            shade = min(254, 261*brightness >> 11)
            for alpha in (0, 32, 64, 100, 127):
                row = struct.unpack('<H', n.u.mem_read(n.read(plain+8)+shade*512+alpha*2, 2))[0]
                for index in (1, 7, 16, 23, 48, 127, 239, 240, 254, 255):
                    expected = struct.unpack('<H', n.u.mem_read(n.read(plain+4)+(row|index)*2, 2))[0]
                    cached = n.pixel(rle, brightness, alpha, index)
                    assert n.u.reg_read(UC_X86_REG_ESP) == 0x70008000+4+10*4
                    n.u.mem_write(0x71004000, b'\xff\xff')
                    n.u.mem_write(0x71005000, b'\xad\xde')
                    n.call(fn, plain, [0x71005000, 0x71006000, 1, 100,
                                      0x71004000, 0x71003000, brightness, 0])
                    assert n.u.reg_read(UC_X86_REG_ESP) == 0x70008000+4+8*4
                    fresh = struct.unpack('<H', n.u.mem_read(0x71005000, 2))[0]
                    assert cached == fresh == expected
                    cases += 2
    return {'scalar_pixel_cases': cases, 'mismatches': 0, 'selected': selected}


class Raster:
    def __init__(self, capture):
        self.capture = capture
        self.u = capture.emulator()
        self.u.mem_map(0x70000000, 0x10000)

    def put(self, address, fmt, *values):
        self.u.mem_write(address, struct.pack('<'+fmt, *values))

    def call(self, fn, ecx=0, edx=0, args=()):
        stack, stop = 0x7000F000, 0x7000FFF0
        self.put(stack, 'I'*(len(args)+1), stop, *args)
        for reg, value in ((UC_X86_REG_ECX, ecx), (UC_X86_REG_EDX, edx),
                           (UC_X86_REG_ESP, stack), (UC_X86_REG_FPCW, 0xE7F)):
            self.u.reg_write(reg, value)
        self.u.emu_start(fn, stop, count=30000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == stop
        assert self.u.reg_read(UC_X86_REG_ESP) == stack+4+len(args)*4

    def render(self, lib, hva, facing, palette):
        rotation, draw, limb, rect = 0x70000100, 0x70000200, 0x70000300, 0x70000400
        angle = (facing-8)*math.tau/32
        fbits = lambda value: struct.unpack('<I', struct.pack('<f', value))[0]
        self.put(rotation, '12f', 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)
        self.call(0x5AF1A0, rotation, args=(fbits(-angle),))
        self.call(0x5AF980, draw, 0xB44318, args=(rotation,))
        self.call(0x753D00, lib, args=(0, draw, 0x887430, 0x887470, fbits(3)))
        self.call(0x753E00)
        for section in range(self.capture.word(lib+4)):
            matrix = self.capture.word(hva+12)+section*48
            self.call(0x5AF980, limb, draw, args=(matrix,))
            self.call(0x7540F0, lib, section, args=(0, limb))
        self.call(0x754510, rect)
        x, y, bx, by, w, h = struct.unpack('<6i', self.u.mem_read(rect, 24))
        assert min(bx, by) >= 0 and bx+w <= 256 and by+h <= 256
        data = bytes(self.u.mem_read(0xB2FF78, 256*256))
        pixels = b''.join(data[(by+row)*256+bx:(by+row)*256+bx+w] for row in range(h))
        counts = Counter(pixels)
        counts.pop(0, None)
        total = sum(counts.values())
        assert total
        return {'bounds': [x, y, w, h], 'opaque_pixels': total,
                'mean_rgb': [sum(n*palette[i*3+c] for i, n in counts.items())/total for c in range(3)]}


def raster_comparison(capture, executable):
    raster, rows = Raster(capture), []
    for unit in capture.vector(0xA8EC78):
        if capture.word(unit) != 0x7F5C70:
            continue
        typ = capture.word(unit+0x6C4)
        lib, hva = capture.word(typ+0xB0), capture.word(typ+0xB4)
        owner = capture.word(unit+0x21C)
        scheme = capture.vector(0xB054D0)[capture.word(owner+0x16054)]
        palette = capture.read(scheme+4, 768)
        spans = [(lib, 28), (hva, 16), (capture.word(lib+24), capture.word(lib+12)),
                 (capture.word(lib+20), capture.word(lib+8)*164),
                 (capture.word(hva+12), capture.word(hva+4)*48),
                 (0xB2FB58, 4), (0xB41178, capture.word(0xB2FB58)*256)]
        data = struct.pack('<3I', lib, hva, len(spans))
        for address, size in spans:
            data += struct.pack('<2I', address, size)+capture.read(address, size)
        portable = subprocess.run([str(executable)], input=data+palette,
                                  check=True, capture_output=True)
        measurements = [json.loads(line) for line in portable.stdout.splitlines()]
        for facing in (0, 8, 16, 24):
            row = {'type': capture.name(typ+0x24, 24), 'facing': facing,
                   'native_indexed_raw_palette': raster.render(lib, hva, facing, palette)}
            for value in measurements:
                if value['facing'] == facing:
                    row['production_vpl' if value['vpl'] else 'production_fallback'] = value
            rows.append(row)
    return rows


def unit_lighting(capture, executable):
    """Original Unit brightness/dispatch, with final draw boundaries stopped."""
    n, rows = Native(capture), []
    rules = capture.word(0x8871E0)
    assert capture.name(0x83A250, 32) == 'ExtraUnitLight'
    multiplier = struct.unpack('<d', capture.read(0x7E4658, 8))[0]
    assert multiplier == 1000
    scaling = []
    # Seed the return value of INI ReadDouble on x87, then run unchanged
    # multiply/_ftol/store instructions. This does not emulate an INI parser.
    seed, number = 0x72000200, 0x71009010
    n.u.mem_write(seed, b'\xdb\xe3\xdd\x05'+struct.pack('<I', number))
    for name, offset, start, end in (('ExtraUnitLight', 0x17D4, 0x66B6AB, 0x66B6BC),
                                      ('ExtraAircraftLight', 0x17DC, 0x66B73D, 0x66B754)):
        for config in (0.2, 0, -0.2, 0.75, -2, 4):
            n.u.mem_write(number, struct.pack('<d', config))
            n.u.emu_start(seed, seed+8, count=2)
            n.u.reg_write(UC_X86_REG_ESP, 0x70008000)
            n.u.reg_write(UC_X86_REG_ESI, rules)
            n.u.reg_write(UC_X86_REG_FPCW, 0xE7F)
            n.u.emu_start(start, end, count=1000)
            stored = struct.unpack('<i', n.u.mem_read(rules+offset, 4))[0]
            assert stored == int(config*1000)
            scaling.append({'field': name, 'config_double': config, 'stored_signed_integer': stored})
    for unit in capture.vector(0xA8EC78):
        if capture.word(unit) != 0x7F5C70:
            continue
        typ, owner = capture.word(unit+0x6C4), capture.word(unit+0x21C)
        x, y, z = struct.unpack('<3i', capture.read(unit+0x9C, 12))
        items = capture.word(0x87F7E8+0x13C)
        cell_slot = items+(y//256*512+x//256)*4
        cell = capture.word(cell_slot)
        vector = capture.word(typ+0xDF0) or 0xB054D0
        scheme_slot = capture.word(vector+4)+capture.word(owner+0x16054)*4
        scheme = capture.word(scheme_slot)
        convert = capture.word(scheme+0x30C)
        spans = [(typ, 0xE00), (owner+0x16054, 4), (0x87F7E8+0x13C, 8),
                 (cell_slot, 4), (cell, 0x150), (0x89E7C0, 4), (vector, 20),
                 (scheme_slot, 4), (scheme+0x30C, 4), (convert, 0x1B4),
                 (0x8871E0, 4), (rules+0x17D4, 4), (rules+0x17DC, 4)]
        data = struct.pack('<7I', typ, owner, x, y, z, rules, len(spans))
        for address, size in spans:
            data += struct.pack('<2I', address, size)+capture.read(address, size)
        result = subprocess.run([str(executable)], input=data, check=True, capture_output=True)
        portable_rows = [json.loads(line) for line in result.stdout.splitlines()]
        production = {r['extra']: r['production_intensity'] for r in portable_rows if r['kind'] == 2}
        coords = n.call(capture.word(capture.word(unit)+0x1B8), unit, [0x71008000])
        coords = bytes(n.u.mem_read(coords, 4))
        assert capture.read(unit+0x684, 1) == b'\xff'
        original_voxel = capture.read(typ+0x236, 1)
        captured_extra = capture.signed(rules+0x17D4)
        row = {'type': capture.name(typ+0x24, 24), 'cell_object_intensity':
               struct.unpack('<h', capture.read(cell+0x10A, 2))[0],
               'captured_extra_unit_light': captured_extra, 'cases': [], 'aircraft_additive_cases': []}
        for extra, portable in production.items():
            n.write(rules+0x17D4, extra)
            n.u.mem_write(0x70008010, coords)
            n.u.reg_write(UC_X86_REG_ESP, 0x70008000)
            n.u.reg_write(UC_X86_REG_ESI, unit)
            n.u.reg_write(UC_X86_REG_FPCW, 0xE7F)
            n.u.emu_start(0x73CF70, 0x73D0DB, count=100000)
            native = struct.unpack('<i', struct.pack('<I', n.u.reg_read(UC_X86_REG_EBP)))[0]
            assert portable == max(0, min(2000, native))
            # The actual voxel wrapper's normal-unit modifier preserves the
            # incoming value; flash/cloak states are not synthesized here.
            n.u.reg_write(UC_X86_REG_ESP, 0x70008000)
            n.u.reg_write(UC_X86_REG_EBP, unit)
            n.u.reg_write(UC_X86_REG_EAX, 0x71009000)
            n.write(0x70008070, native)
            n.u.emu_start(0x70676F, 0x7067E4, count=100000)
            assert n.u.reg_read(UC_X86_REG_EDI) == native & 0xFFFFFFFF
            dispatched = []

            def boundary(u, address, _size, _data):
                sp = u.reg_read(UC_X86_REG_ESP)
                actual = struct.unpack('<i', u.mem_read(sp+0x1C, 4))[0]
                assert actual == native and u.reg_read(UC_X86_REG_ECX) == unit
                dispatched.append({'kind': 'voxel' if address == 0x73B470 else 'shp_unit',
                                   'native_intensity': actual})
                u.reg_write(UC_X86_REG_ESP, sp+4+0x20)
                u.reg_write(UC_X86_REG_EIP, n.read(sp))

            hooks = [n.u.hook_add(UC_HOOK_CODE, boundary, begin=fn, end=fn)
                     for fn in (0x73B470, 0x73C5F0)]
            try:
                for voxel in (0, 1):
                    n.u.mem_write(typ+0x236, bytes([voxel]))
                    for reg, value in ((UC_X86_REG_ESP, 0x70008000), (UC_X86_REG_ESI, unit),
                                       (UC_X86_REG_EDI, typ), (UC_X86_REG_EBP, native & 0xFFFFFFFF),
                                       (UC_X86_REG_EBX, 0x71009100)):
                        n.u.reg_write(reg, value)
                    n.u.emu_start(0x73D30E, 0x73D395, count=10000)
            finally:
                for hook in hooks:
                    n.u.hook_del(hook)
                n.u.mem_write(typ+0x236, original_voxel)
            assert [r['kind'] for r in dispatched] == ['shp_unit', 'voxel']
            row['cases'].append({'extra': extra, 'native_unclamped_intensity': native,
                                 'production_intensity': portable, 'dispatch': dispatched})
        # A synthetic object supplies only the layout read by these slices.
        # No aircraft locomotor, height calculation or full draw is executed.
        aircraft, aircraft_type, aircraft_vtable = 0x7100B000, 0x7100C000, 0x7100D000
        n.write(aircraft, aircraft_vtable)
        n.write(aircraft+0x6C4, aircraft_type)
        n.write(aircraft_vtable+0x510, 0x4DAF10)
        for portable in portable_rows:
            if portable['kind'] != 0:
                continue
            extra, base = portable['extra'], row['cell_object_intensity']
            n.write(rules+0x17DC, extra)
            for reg, value in ((UC_X86_REG_ESP, 0x70008000), (UC_X86_REG_EBP, aircraft),
                               (UC_X86_REG_EBX, base), (UC_X86_REG_ESI, 0)):
                n.u.reg_write(reg, value)
            n.u.emu_start(0x414925, 0x41493E, count=100)
            native = struct.unpack('<i', struct.pack('<I', n.u.reg_read(UC_X86_REG_EBX)))[0]
            assert native == base+extra
            assert portable['production_intensity'] == max(0, min(2000, native))
            n.u.emu_start(0x414987, 0x414997, count=100)
            pushed = struct.unpack('<i', n.u.mem_read(n.u.reg_read(UC_X86_REG_ESP), 4))[0]
            assert pushed == native
            row['aircraft_additive_cases'].append({'controlled_base_intensity': base,
                    'controlled_altitude_term': 0, 'extra': extra,
                    'native_unclamped_intensity': native, 'native_pushed_intensity': pushed,
                    'production_intensity': portable['production_intensity']})
        rows.append(row)
    return {'rules_global': '008871E0', 'runtime_signed_integer_offsets': {'unit': '17D4', 'aircraft': '17DC'},
            'multiplier': multiplier, 'native_scaling_cases': scaling, 'units': rows,
            'native_brightness_cases': sum(len(r['cases']) for r in rows),
            'native_dispatch_cases': sum(len(r['cases'])*2 for r in rows),
            'native_aircraft_additive_cases': sum(len(r['aircraft_additive_cases']) for r in rows)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    result = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__,
              'stages': {}, 'game_tested': False, 'windows_rendering_tested': False,
              'multiplayer_tested': False}
    for stage in ('00-clean', '01-off', '02-on', '03-off'):
        capture = Capture(args.capture, stage)
        count = capture.word(0xB2FB58)
        assert 1 <= count <= 32
        table = capture.read(0xB41178, count*256)
        stable = capture.file(stage+'-game.stable')
        base = int(capture.session['image_base'], 16)
        for address, size in ((0xB2FB58, 4), (0xB41178, len(table))):
            checked = stable[address-base:address-base+size]
            assert len(checked) == size and 0 not in checked
        result['stages'][stage] = {'pages': count, 'bytes_valid_and_stable': len(table),
                                  'table_sha256': hashlib.sha256(table).hexdigest()}
        if stage != '00-clean':
            result['stages'][stage]['scalar_blitters'] = blitters(capture)
        if stage == '01-off':
            result['code_sha256'] = code_hashes(capture)
            result['synthetic_loader'] = loader(capture)
            with tempfile.TemporaryDirectory(prefix='ra2-voxel-vpl-') as directory:
                path = Path(directory)
                source = path/'probe.cpp'
                source.write_text(CPP)
                executable = path/'probe'
                root = Path(__file__).resolve().parents[3]
                subprocess.run(['g++', '-std=c++17', '-O2', '-I'+str(root),
                                str(source), '-o', str(executable)], check=True)
                result['body_raster_comparisons'] = raster_comparison(capture, executable)
                light_source, light_executable = path/'light.cpp', path/'light'
                light_source.write_text(CPP_LIGHT)
                subprocess.run(['g++', '-std=c++17', '-O2', '-I'+str(root),
                                str(light_source), '-o', str(light_executable)], check=True)
                result['unit_lighting'] = unit_lighting(capture, light_executable)
    result['scalar_pixel_cases'] = sum(s.get('scalar_blitters', {}).get('scalar_pixel_cases', 0)
                                       for s in result['stages'].values())
    Path(args.output).write_text(json.dumps(result, indent=2)+'\n')
    print(f"PASS 4 stable loaded VPL snapshots; 3 original synthetic loader cases; "
          f"{result['scalar_pixel_cases']} fresh/cached scalar pixel cases; "
          f"{len(result['body_raster_comparisons'])} body raster aggregate comparisons; "
          f"{result['unit_lighting']['native_brightness_cases']} Unit brightness / "
          f"{result['unit_lighting']['native_dispatch_cases']} voxel-SHP dispatch / "
          f"{result['unit_lighting']['native_aircraft_additive_cases']} Aircraft additive cases")


if __name__ == '__main__':
    main()

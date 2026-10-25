"""Audit original Bullet Draw_It selection, frame, transform and palette paths.

The private supported-game capture contains no live bullets. Constructed Bullet
and BulletType layouts run the captured original x86 and retain native math,
MapClass ground queries and virtual GetImage/GetAnimFrame/GetHeight/GetZ calls.
Only the terminal SHP/VXL draw boundaries are intercepted. No live game,
framebuffer, projectile physics or multiplayer test is performed. Output is
sanitized decisions, counters and code hashes; no art or executable bytes.
"""
import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, SUPPORTED_SHA
from native_lightconvert_capture import Native
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                               UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW)


def fixed_direction(x, y):
    q = math.trunc((math.atan2(-y, x)-math.pi/2)*(-65536/math.tau))
    return ((((q & 65535) >> 10)+1) >> 1) & 31


def stored(value):
    out = struct.unpack('<f', struct.pack('<f', value))[0]
    if (value > 0 and out > value) or (value < 0 and out < value):
        bits = struct.unpack('<I', struct.pack('<f', out))[0]-1
        out = struct.unpack('<f', struct.pack('<I', bits))[0]
    return out


def native_sc(angle):
    n = math.trunc(stored(angle)*2607.594482421875)
    index = math.trunc(n/2) % 8192
    if n & 1 and index < 8191:
        index += 1
    ci = math.trunc(n/2) % 8192 + 2048
    if n & 1 and ci < 10239:
        ci += 1
    return stored(math.sin(index*math.tau/8192)), stored(math.sin(ci*math.tau/8192))


CPP = r'''
#include "source/external_reader/source/voxel_lighting.h"
#include <array>
#include <cassert>
#include <iostream>
using namespace external::voxel;
int main(){
 uint32_t count=0;std::cin.read(reinterpret_cast<char*>(&count),4);
 for(uint32_t j=0;j<count;++j){double angles[2]{};std::cin.read(reinterpret_cast<char*>(angles),16);assert(std::cin);
  auto light=nativeVoxelLight(angles[0],angles[1]);std::array<uint8_t,256> pages{};pages.fill(16);
  for(unsigned i=0;i<253;++i){std::array<float,3> normal{};if(normalVector(4,i,normal))pages[i]=uint8_t(nativeProjectilePage(normal,light));}
  std::cout.write(reinterpret_cast<const char*>(pages.data()),pages.size());
 }
}
'''


class Probe(Native):
    BULLET, TYPE, POINT, RECT, SHP = 0x71008000, 0x71008400, 0x71008800, 0x71008900, 0x71008C00

    def __init__(self, capture):
        super().__init__(capture)
        self.capture, self.draws, self.angles, self.orientation = capture, [], [], None
        self.u.mem_write(self.TYPE, capture.read(capture.vector(0xA83C80)[0], 0x300))
        self.write(self.BULLET, 0x7E46E4)
        self.write(self.BULLET+0xAC, self.TYPE)
        self.write(self.TYPE+0xA4, self.SHP)
        self.u.mem_write(self.BULLET+0x9C, struct.pack('<3i', 36288, 61248, 416))
        self.u.mem_write(self.BULLET+0xE8, struct.pack('<3d', 1, 0, 0))
        self.u.mem_write(self.POINT, struct.pack('<2i', 100, 100))
        self.u.mem_write(self.RECT, struct.pack('<4i', 0, 0, 640, 480))
        # The scene's screen/shroud gate is controlled separately from the
        # projectile-specific visibility gates being tested here.
        self.write(capture.word(0xA8B230), 0)
        for offset in (0x236, 0x29A, 0x29E, 0x2A8, 0x2A9, 0x2F4, 0x2F5):
            self.byte(self.TYPE+offset, 0)
        self.byte(self.TYPE+0x2A1, 1)
        self.hooks = [self.u.hook_add(UC_HOOK_CODE, self.boundary, begin=fn, end=fn)
                      for fn in (0x4AED70, 0x46B0C0, 0x5AF1A0, 0x5AF080, 0x5AF980)]

    def byte(self, p, value):
        self.u.mem_write(p, bytes([value]))

    def boundary(self, u, address, _size, _data):
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x5AF980:
            self.orientation = struct.unpack('<12f', u.mem_read(self.read(sp+4), 48))
            return
        if address in (0x5AF1A0, 0x5AF080):
            self.angles.append((address, struct.unpack('<f', u.mem_read(sp+4, 4))[0]))
            return
        if address == 0x4AED70:
            args = struct.unpack('<14i', u.mem_read(sp+4, 56))
            self.draws.append({'kind': 'shp', 'converter': u.reg_read(UC_X86_REG_EDX),
                               'shape_matches': args[0] == self.SHP, 'frame': args[1],
                               'point': list(struct.unpack('<2i', u.mem_read(args[2], 8))),
                               'flags': args[4], 'remap': args[5], 'depth': args[6],
                               'gradient': args[7], 'brightness': args[8]})
            cleanup = 56
        else:
            args = struct.unpack('<7I', u.mem_read(sp+4, 28))
            self.draws.append({'kind': 'vxl', 'model_pair_matches': args[0] == self.TYPE+0xB0,
                               'matrix': list(struct.unpack('<12f', u.mem_read(args[1], 48))),
                               'hva_frame': args[4], 'flags': args[5], 'brightness': args[6]})
            cleanup = 28
        u.reg_write(UC_X86_REG_EIP, self.read(sp))
        u.reg_write(UC_X86_REG_ESP, sp+4+cleanup)

    def draw(self):
        self.draws, self.angles, self.orientation = [], [], None
        self.call(0x468090, self.BULLET, [self.POINT, self.RECT])
        assert self.u.reg_read(UC_X86_REG_ESP) == 0x70008000+12
        return self.draws


def inspect(capture, production):
    p = Probe(capture)
    assert capture.word(0x7E46E4+0x114) == 0x468090
    assert capture.word(0x7E46E4+0x1E8) == 0x468000
    lut = struct.unpack('<32i', capture.read(0x7F4890, 128))
    assert tuple((28-i) & 31 for i in range(32)) == lut
    frames = []
    for rotates, low, high, runtime_frame, direction in itertools.product((False, True), (0, 1), (0, 9), (0, 11, 255), range(32)):
        angle = direction*math.tau/32
        vx, vy = math.cos(angle), math.sin(angle)
        p.byte(p.TYPE+0x2A1, not rotates)
        p.byte(p.TYPE+0x2F4, low)
        p.byte(p.TYPE+0x2F5, high)
        p.byte(p.BULLET+0x12C, runtime_frame)
        p.u.mem_write(p.BULLET+0xE8, struct.pack('<3d', vx, vy, 0))
        actual = p.call(0x468000, p.BULLET, [])
        expected = runtime_frame if low or high else lut[fixed_direction(vx, vy)] if rotates else 0
        assert actual == expected
        frames.append(actual)
    p.byte(p.TYPE+0x2A1, 1)
    p.byte(p.TYPE+0x2F4, 1)
    p.byte(p.TYPE+0x2F5, 0)
    p.byte(p.BULLET+0x12C, 11)
    palette_cases, damage_cases = [], []
    schemes = capture.vector(0xB054D0)
    for anim, firer, inherited, shadow, bright in itertools.product((False, True), (False, True), (-1, 0, 1), (False, True), (False, True)):
        p.byte(p.TYPE+0x2A8, anim)
        p.byte(p.TYPE+0x2A9, firer)
        p.byte(p.TYPE+0x29A, shadow)
        p.byte(p.BULLET+0xE0, bright)
        p.write(p.BULLET+0x114, inherited)
        draws = p.draw()
        assert len(draws) == (2 if shadow else 1)
        expected = capture.word(0x87F6C0 if anim else 0x87F6C4)
        if not anim and firer:
            index = inherited if inherited >= 0 else capture.signed(capture.word(0xA83D4C)+0x16054)
            expected = capture.word(schemes[index]+0x30C)
        body = draws[-1]
        assert body == {'kind': 'shp', 'converter': expected, 'shape_matches': True, 'frame': 11,
                        'point': [100, 100], 'flags': 0x2E00, 'remap': 0, 'depth': -90,
                        'gradient': 0, 'brightness': 1000}
        if shadow:
            assert draws[0] == {'kind': 'shp', 'converter': capture.word(0x87F6C4), 'shape_matches': True,
                                'frame': 11, 'point': [100, 130], 'flags': 0x2601, 'remap': 0,
                                'depth': -40, 'gradient': 0, 'brightness': 1000}
        palette_cases.append({'anim_palette': anim, 'firers_palette': firer, 'inherited_color': inherited,
                              'shadow': shadow, 'bright': bright, 'draws': len(draws),
                              'body_gradient': body['gradient'], 'body_remap': body['remap'],
                              'shadow_gradient': draws[0]['gradient'] if shadow else None,
                              'shadow_remap': draws[0]['remap'] if shadow else None})
    p.byte(p.TYPE+0x29A, 0)
    for damage, on_map, alive in itertools.product((-200, 0, 100), (0, 1), (0, 1)):
        p.write(p.BULLET+0x6C, damage)
        p.byte(p.BULLET+0x74, on_map)
        p.byte(p.BULLET+0x90, alive)
        assert len(p.draw()) == 1
        damage_cases.append({'damage': damage, 'on_map': on_map, 'alive': alive})
    visibility = []
    for inviso, next_anim, voxel in itertools.product((False, True), (False, True), (False, True)):
        p.byte(p.TYPE+0x29E, inviso)
        p.byte(p.BULLET+0x158, next_anim)
        p.byte(p.TYPE+0x236, voxel)
        draws = p.draw()
        assert bool(draws) == (not inviso and not next_anim)
        visibility.append({'invisible_type': inviso, 'spawn_next_anim': next_anim, 'voxel': voxel,
                           'draw': bool(draws)})
    p.byte(p.TYPE+0x29E, 0)
    p.byte(p.BULLET+0x158, 0)
    p.byte(p.TYPE+0x236, 1)
    orientation = []
    matrices = []
    for direction, slope in itertools.product(range(32), (-3, -1, 0, 1, 3)):
        a = direction*math.tau/32
        vx, vy, vz = math.cos(a), math.sin(a), slope
        p.u.mem_write(p.BULLET+0xE8, struct.pack('<3d', vx, vy, vz))
        draws = p.draw()
        assert len(draws) == 1 and draws[0]['kind'] == 'vxl' and draws[0]['model_pair_matches']
        assert draws[0]['hva_frame'] == 0 and draws[0]['flags'] == 0x800 and draws[0]['brightness'] == 1000
        yaw = (fixed_direction(vx, vy)-8)*(-math.tau/32)
        pitch = (fixed_direction(math.hypot(vx, vy), -vz)-8)*(math.tau/32)
        assert len(p.angles) == 2
        assert abs(p.angles[0][1]-yaw) < 2e-6 and abs(p.angles[1][1]-pitch) < 2e-6
        sy, cy = native_sc(p.angles[0][1])
        sp, cp = native_sc(p.angles[1][1])
        expected_matrix = (stored(cy*cp), -sy, stored(cy*sp), 0,
                           stored(sy*cp), cy, stored(sy*sp), 0, -sp, 0, cp, 0)
        assert p.orientation is not None
        assert max(abs(a-b) for a, b in zip(p.orientation, expected_matrix)) < 1e-6
        # CapturePart already mirrors local Y. M*Rz(yaw)*Ry(pitch)*M
        # equals Rz(-yaw)*Ry(pitch): pitch first, then reversed yaw.
        for x, y, z in ((1, 0, 0), (0, 1, 0), (0, 0, 1), (3, -7, 11)):
            native_point = (x, -y, z)
            transformed = [sum(p.orientation[r*4+c]*native_point[c] for c in range(3)) for r in range(3)]
            transformed[1] = -transformed[1]
            a, b, c = cp*x+sp*z, y, -sp*x+cp*z
            preview_point = (cy*a+sy*b, -sy*a+cy*b, c)
            assert max(abs(a-b) for a, b in zip(transformed, preview_point)) < 5e-6
        orientation.append({'direction': direction, 'slope': slope, 'native_yaw': p.angles[0][1],
                            'native_pitch': p.angles[1][1], 'hva_frame': 0})
        matrices.append((draws[0]['matrix'], -yaw, pitch))
    diffuse = diffuse_pages(capture, matrices, production)
    vxl_palettes = vxl_palette_selection(capture)
    layers = []
    for cached in (0, 1, 2, 3, 4, 0xFFFFFFFF, 0x7FFFFFFF):
        p.write(p.BULLET+0x94, cached)
        for flat in range(256):
            p.byte(p.TYPE+0x2F7, flat)
            assert p.call(0x468B90, p.BULLET, []) == (1 if flat else 3)
        layers.append({'cached_layer': cached, 'flat_values_checked': 256})
    return {'captured_live_bullets': len(capture.vector(0xA8ED40)), 'fixture': 'synthetic Bullet and copied BulletType; terminal draw intercepted',
            'frame_cases': len(frames), 'palette_shadow_bright_cases': len(palette_cases),
            'shp_draw_parameter_cases': palette_cases,
            'damage_is_not_lifecycle_cases': len(damage_cases), 'visibility_cases': visibility,
            'vxl_orientation_cases': orientation,
            'native_projectile_diffuse': diffuse, 'vxl_palette_flat_cases': vxl_palettes,
            'native_layer_cases': len(layers)*256, 'cached_layer_ignored': layers,
            'native_z_test': z_pixels(capture),
            'code_sha256': {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a, b-a)).hexdigest()
                            for a, b in ((0x4664C0, 0x46655E), (0x468000, 0x468089),
                                         (0x468090, 0x468429), (0x46B0C0, 0x46B258),
                                         (0x753C80, 0x753CFB), (0x758670, 0x7586E8),
                                         (0x468B90, 0x468BA6), (0x493DF0, 0x493E90),
                                         (0x494B60, 0x494C3A), (0x497FD0, 0x498150))}}


def diffuse_pages(capture, matrices, production):
    n = Native(capture)
    unit = next(u for u in capture.vector(0xA8EC78) if capture.word(u) == 0x7F5C70)
    lib = capture.word(capture.word(unit+0x6C4)+0xB0)
    normal_type = capture.read(capture.word(lib+20)+0xA3, 1)[0]
    normal_count = capture.word(0x8469F4+normal_type*4)
    normal_table = capture.word(0x8469E0+normal_type*4)
    normals = [struct.unpack('<3f', capture.read(normal_table+i*12, 12)) for i in range(normal_count)]
    local_lights = []
    data = struct.pack('<I', len(matrices))+b''.join(struct.pack('<2d', yaw, pitch) for _, yaw, pitch in matrices)
    portable = subprocess.run([str(production)], input=data, check=True, capture_output=True).stdout
    assert len(portable) == len(matrices)*256
    def boundary(u, _address, _size, _data):
        assert u.reg_read(UC_X86_REG_EDX) == normal_type
        local_lights.append(struct.unpack('<3f', u.mem_read(u.reg_read(UC_X86_REG_ECX), 12)))
    hook = n.u.hook_add(UC_HOOK_CODE, boundary, begin=0x758670, end=0x758670)
    try:
        for index, (matrix, _yaw, _pitch) in enumerate(matrices):
            n.u.mem_write(0x71009000, struct.pack('<12f', *matrix))
            n.u.reg_write(UC_X86_REG_EDX, 0)
            n.call(0x753C80, lib, [0, 0x71009000, 0x887470])
            actual = bytes(n.u.mem_read(0xB45990, 256))
            light = local_lights[-1]
            expected = [math.trunc(max(0, sum(a*b for a, b in zip(normal, light)))*16) & 255 for normal in normals]
            assert actual[:normal_count] == bytes(expected)
            assert actual[253:256] == bytes([16]*3)
            generated = portable[index*256:(index+1)*256]
            assert generated[:normal_count] == actual[:normal_count], (index, sum(a != b for a, b in zip(generated[:normal_count], actual[:normal_count])))
            assert generated[253:256] == actual[253:256]
    finally:
        n.u.hook_del(hook)
    return {'original_setup_calls': len(matrices), 'normal_type': normal_type,
            'normal_page_checks': len(matrices)*(normal_count+3), 'special_indices_page': 16,
            'production_normal_page_checks': len(matrices)*(normal_count+3),
            'production_page_mismatches': 0, 'diffuse_only': True, 'specular_term': False}


def vxl_palette_selection(capture):
    n, rows = Native(capture), []
    b, t, rect, point = 0x71008000, 0x71008400, 0x71008800, 0x71008900
    n.write(b+0xAC, t)
    n.u.mem_write(rect, struct.pack('<6i', 0, 0, 0, 0, 2, 2))
    n.u.mem_write(point, struct.pack('<2i', 100, 100))
    schemes = capture.vector(0xB054D0)
    selected = []
    def boundary(u, _address, _size, _data):
        sp = u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<6I', u.mem_read(sp+4, 24))
        selected.append((u.reg_read(UC_X86_REG_ECX), args))
        u.reg_write(UC_X86_REG_EAX, 0x71008C00)
        u.reg_write(UC_X86_REG_EIP, n.read(sp))
        u.reg_write(UC_X86_REG_ESP, sp+8)
    hook = n.u.hook_add(UC_HOOK_CODE, boundary, begin=0x490B90, end=0x490B90)
    try:
        for color, flat in itertools.product((0, 1, 2), (False, True)):
            n.write(t+0x2D4, color)
            n.u.mem_write(t+0x2F7, bytes([flat]))
            sp = 0x70008000
            for offset, value in ((0x10, b), (0x100, point), (0x10C, 0x800), (0x110, 1000)):
                n.write(sp+offset, value)
            n.u.reg_write(UC_X86_REG_ESP, sp)
            n.u.reg_write(UC_X86_REG_EAX, rect)
            n.u.reg_write(UC_X86_REG_FPCW, 0xE7F)
            n.u.emu_start(0x46B18C, 0x46B21F, count=1000)
            assert n.u.reg_read(UC_X86_REG_EIP) == 0x46B21F
            converter, args = selected[-1]
            assert converter == capture.word(schemes[color]+0x30C)
            assert args[0] == (0x2800 if flat else 0x800) and args[2] == (0 if flat else 2) and args[3] == 1000
            rows.append({'color_scheme_index': color, 'flat': flat, 'flags': args[0], 'mode': args[2], 'brightness': args[3]})
    finally:
        n.u.hook_del(hook)
    return rows


def z_pixels(capture):
    n, rows = Native(capture), []
    cv = capture.word(capture.vector(0xB054D0)[1]+0x30C)
    for kind, flags in (('plain_vxl_nonflat', 0x800), ('plain_vxl_flat', 0x2800), ('rle_shp', 0x2E00)):
        rle = kind == 'rle_shp'
        blitter = n.call(0x490E50 if rle else 0x490B90, cv, [flags])
        fn = n.read(n.read(blitter)+4)
        assert fn == (0x497FD0 if rle else 0x493DF0 if flags == 0x800 else 0x494B60)
        pixels = []
        for z in (0, 99, 100, 101, 65535):
            n.u.mem_write(0x71003000, struct.pack('<H', 127))
            n.u.mem_write(0x71004000, struct.pack('<H', z))
            n.u.mem_write(0x71005000, b'\xad\xde')
            n.u.mem_write(0x71006000, bytes([48]))
            n.u.mem_write(0x71007000, b'\0')
            args = [0x71005000, 0x71006000, 1, 100, 0x71004000, 0x71003000, 1000, 0]
            if rle:
                args = [0x71005000, 0x71006000, 1, 0, 100, 0x71004000, 0x71003000, 1000, 0, 0x71007000]
            n.call(fn, blitter, args)
            actual = struct.unpack('<H', n.u.mem_read(0x71005000, 2))[0]
            assert struct.unpack('<H', n.u.mem_read(0x71004000, 2))[0] == z
            expected_draw = flags == 0x800 or z > 100
            assert (actual != 0xDEAD) == expected_draw
            pixels.append(actual)
        assert pixels[-1] != 0xDEAD
        if flags == 0x800:
            assert len(set(pixels)) == 1
        rows.append({'kind': kind, 'flags': flags, 'native_pixel_function': f'{fn:08X}',
                     'z_test': flags != 0x800, 'strict_z_compare': flags != 0x800,
                     'z_write': False, 'cases': len(pixels)})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    with tempfile.TemporaryDirectory(prefix='ra2-projectile-native-') as directory:
        source, executable = Path(directory)/'probe.cpp', Path(directory)/'probe'
        source.write_text(CPP)
        subprocess.run(['g++', '-std=c++17', '-O2', '-I'+str(root), str(source), '-o', str(executable)], check=True)
        stages = {stage: inspect(Capture(args.capture, stage), executable) for stage in ('01-off', '02-on', '03-off')}
    report = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__, 'stages': stages,
              'game_tested': False, 'windows_rendering_tested': False, 'multiplayer_tested': False}
    Path(args.output).write_text(json.dumps(report, indent=2)+'\n')
    print('PASS 3 original Bullet Draw_It fixtures: '+str(sum(s['frame_cases'] for s in stages.values()))+
          ' frame cases; '+str(sum(s['palette_shadow_bright_cases'] for s in stages.values()))+
          ' palette/shadow/bright cases; '+str(sum(len(s['vxl_orientation_cases']) for s in stages.values()))+' VXL orientations')


if __name__ == '__main__':
    main()

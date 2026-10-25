"""Replay original Building::GetCurrentFrame for occupied buildings.

Uses the private, validity-checked supported capture, original x86 frame logic,
original virtual occupant-count/type accessors and health-ratio function. Only
synthetic object/type fields and rule thresholds change. No game update, SHP
rasterizer, Windows composition or multiplayer runs; no private bytes escape.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, Probe, SUPPORTED_SHA
from unicorn.x86_const import UC_X86_REG_ECX


def requests():
    values = []
    for count, health, strength, tech, thresholds, stage in itertools.product(
            (0, 1, 3, -1), (0, 1, 24, 25, 26, 49, 50, 51, 99, 100),
            (100, 200), (-1, 0, 1, 10), ((.5, .25), (.6, .3), (.2, .7)), (0, 7)):
        values.append((count, health, strength, tech, *thresholds, stage))
    return values


def inspect(capture, cases, production):
    p = Probe(capture)
    p.u.mem_write(p.T + 0x157B, b'\1')
    p.u.mem_write(p.B + 0x6E4, b'\1')
    p.write(p.B + 0x534, 1)
    rules = p.read(0x8871E0)
    for n, (count, health, strength, tech, yellow, red, stage) in enumerate(cases):
        p.write(p.B + 0x694, count)
        p.write(p.B + 0x6C, health)
        p.write(p.B + 0xF8, stage)
        p.write(p.T + 0xA0, strength)
        p.write(p.T + 0x634, tech)
        p.u.mem_write(rules + 0x1700, struct.pack('<dd', yellow, red))
        frame = p.function(0x43EF90, p.B)
        assert frame == production[n], (cases[n], frame, production[n])
    # Confirm the actual image-selection entry still dispatches to the normal
    # body for both empty and occupied, regardless of the current frame.
    body = p.read(p.read(p.T) + 0x9C)
    make = p.read(p.read(p.T) + 0xC0)
    for count in (0, 1, 3):
        p.write(p.B + 0x694, count)
        p.reset()
        p.u.reg_write(UC_X86_REG_ECX, p.B)
        assert p.execute_to_one(0x4513D0, {body, make}) == body
    inventory = []
    for b in capture.vector(0xA8EB40):
        t = capture.word(b + 0x520)
        if capture.read(t + 0x157B, 1)[0]:
            inventory.append({'type': capture.name(t + 0x24, 24),
                'occupants': capture.signed(b + 0x694), 'health': capture.signed(b + 0x6C),
                'tech_level': capture.signed(t + 0x634)})
    ranges = ((0x43EF90, 0x43F174), (0x4581F0, 0x4581F7), (0x5F5C60, 0x5F5C80),
              (0x6F3270, 0x6F3278), (0x4513D0, 0x451400))
    return {'native_frame_cases': len(cases), 'mismatches': 0,
        'native_normal_image_dispatch_cases': 3, 'captured_occupiable_buildings': inventory,
        'code_sha256': {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a,b-a)).hexdigest() for a,b in ranges}}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    cases = requests()
    with tempfile.TemporaryDirectory(prefix='ra2-garrison-') as tmp:
        binary = Path(tmp) / 'production'
        subprocess.run(['g++','-std=c++17','-O2',str(Path(__file__).with_name('garrison_frame.cpp')), '-o',str(binary)], check=True)
        data = ''.join(' '.join(map(str, row))+'\n' for row in cases).encode()
        production = list(map(int,subprocess.check_output([str(binary),'--probe'],input=data).split()))
    assert len(production) == len(cases)
    stages = {stage:inspect(Capture(args.capture,stage),cases,production) for stage in ('01-off','02-on','03-off')}
    result = {'scope':__doc__,'supported_executable_sha256':SUPPORTED_SHA.lower(),
        'native_frame_cases': len(cases)*3, 'mismatches':0,
        'native_normal_image_dispatch_cases':9, 'stages':stages,
        'game_tested':False,'windows_rendering_tested':False,'multiplayer_tested':False}
    Path(args.output).parent.mkdir(parents=True,exist_ok=True)
    Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
    print(f"PASS {result['native_frame_cases']} native/production garrison frames; 9 native normal-image dispatches")

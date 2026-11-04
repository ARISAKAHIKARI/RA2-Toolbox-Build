"""Execute original Object::Deselect with synthetic objects and selection UI.

Requires the owner's supported private EXE; the EXE and original function bytes
are never part of this repository. The complete Deselect, native vector FindIndex
and native current-object getters/setters execute without stubs. This establishes
local selection semantics and ABI, not complete Windows input or multiplayer.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

SUPPORTED = {
    '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0',
    'c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a',
}
VTABLES = (0x7E22A4, 0x7F5C70, 0x7EB058, 0x7E3EBC)
REGIONS = ((0x5F44A0, 123), (0x40CF00, 38), (0x4AEB10, 23), (0x4AEB30, 24))
BASE, OBJECTS, ITEMS, STACK, STOP = 0x400000, 0x20000000, 0x20200000, 0x30100000, 0x30000000


def fnv(data):
    h = 2166136261
    for value in data:
        h = ((h ^ value) * 16777619) & 0xFFFFFFFF
    return h


def run(image, vtable, count, chosen, listed=True, selected=True, current='chosen', twice=False):
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(BASE, (len(image) + 4095) & ~4095)
    u.mem_write(BASE, image)
    u.mem_map(OBJECTS, 0x300000)
    u.mem_map(STOP, 0x200000)
    read = lambda at: struct.unpack('<I', u.mem_read(at, 4))[0]
    write = lambda at, value: u.mem_write(at, struct.pack('<I', value & 0xFFFFFFFF))
    objects = [OBJECTS + i * 0x1000 for i in range(count)]
    actor = objects[chosen] if listed else OBJECTS + count * 0x1000
    for i, obj in enumerate(objects + ([] if listed else [actor])):
        u.mem_write(obj, bytes([0x50 + i % 32]) * 0x800)
        write(obj, vtable)
        u.mem_write(obj + 0x83, bytes([int(selected if obj == actor else True)]))
        write(ITEMS + i * 4, obj)
    write(0xA8ECB8, 0x7E4F64)
    write(0xA8ECBC, ITEMS)
    write(0xA8ECC0, count)
    write(0xA8ECC4, 1)
    write(0xA8ECC8, count)
    hovered = actor if current == 'chosen' else objects[(chosen + 1) % count] if current == 'other' else 0
    write(0x87F7E8 + 0x11A0, hovered)
    u.mem_write(0x87F7E8 + 0x119C, bytes([int(bool(hovered))]))
    before = {obj: bytes(u.mem_read(obj, 0x800)) for obj in objects + ([] if listed else [actor])}
    writes, pcs = set(), set()
    u.hook_add(UC_HOOK_MEM_WRITE, lambda uc, access, at, size, value, extra: writes.update(range(at, at + size)))
    u.hook_add(UC_HOOK_CODE, lambda uc, at, size, extra: pcs.add(at))
    for invocation in range(2 if twice else 1):
        write(STACK, STOP)
        for reg, value in ((UC_X86_REG_EBX, 0x11223344), (UC_X86_REG_ESI, 0x22334455), (UC_X86_REG_EDI, 0x33445566), (UC_X86_REG_EBP, 0x44556677)):
            u.reg_write(reg, value)
        u.reg_write(UC_X86_REG_ESP, STACK)
        u.reg_write(UC_X86_REG_ECX, actor)
        method = read(read(actor) + 0x150)
        assert method == 0x5F44A0
        u.emu_start(method, STOP, count=200000)
        assert u.reg_read(UC_X86_REG_EIP) == STOP
        assert u.reg_read(UC_X86_REG_ESP) == STACK + 4
        for reg, value in ((UC_X86_REG_EBX, 0x11223344), (UC_X86_REG_ESI, 0x22334455), (UC_X86_REG_EDI, 0x33445566), (UC_X86_REG_EBP, 0x44556677)):
            assert u.reg_read(reg) == value
    expected = [obj for obj in objects if not (selected and listed and obj == actor)]
    assert read(0xA8ECC8) == len(expected), (hex(vtable), count, chosen, listed, selected, current, read(0xA8ECC8), expected)
    assert [read(ITEMS + i * 4) for i in range(len(expected))] == expected
    for obj, original in before.items():
        after = bytes(u.mem_read(obj, 0x800))
        if obj == actor and selected:
            assert after == original[:0x83] + bytes(1) + original[0x84:]
        else:
            assert after == original
    expected_hover = 0 if selected and hovered == actor else hovered
    assert read(0x87F7E8 + 0x11A0) == expected_hover
    assert bytes(u.mem_read(0x87F7E8 + 0x119C, 1)) == bytes([int(bool(expected_hover))])
    allowed = set(range(STACK - 32, STACK + 8)) | set(range(ITEMS, ITEMS + count * 4)) | set(range(0xA8ECC8, 0xA8ECCC)) | {actor + 0x83} | set(range(0x87F7E8 + 0x11A0, 0x87F7E8 + 0x11A4)) | {0x87F7E8 + 0x119C}
    assert writes <= allowed, [hex(at) for at in sorted(writes - allowed)[:10]]
    assert all(any(start <= pc < start + length for start, length in REGIONS) for pc in pcs)
    return {'vtable': hex(vtable), 'count': count, 'chosen': chosen, 'listed': listed, 'selected': selected, 'current': current, 'twice': twice, 'status': 'PASS'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', action='append', required=True)
    parser.add_argument('--output')
    args = parser.parse_args()
    results, signatures = [], None
    for path in args.game:
        data = Path(path).read_bytes()
        sha = hashlib.sha256(data).hexdigest()
        assert sha in SUPPORTED, ('unsupported private executable', sha)
        pe = pefile.PE(data=data)
        image = pe.get_memory_mapped_image()
        assert pe.OPTIONAL_HEADER.ImageBase == BASE
        current = [{'address': hex(at), 'length': size, 'fnv1a32': hex(fnv(image[at - BASE:at - BASE + size]))} for at, size in REGIONS]
        assert signatures is None or signatures == current
        signatures = current
        rows = []
        for vtable in VTABLES:
            assert struct.unpack_from('<I', image, vtable + 0x150 - BASE)[0] == 0x5F44A0
            for count, chosen in ((1, 0), (3, 0), (3, 1), (3, 2), (512, 255)):
                for hover in ('chosen', 'other', 'none'):
                    rows.append(run(image, vtable, count, chosen, current=hover, twice=True))
            rows.append(run(image, vtable, 3, 1, listed=False))
            rows.append(run(image, vtable, 3, 1, selected=False))
        results.append({'game_sha256': sha, 'cases': len(rows), 'results': rows})
    report = {'scope': __doc__, 'game_tested': False, 'multiplayer_tested': False, 'signatures': signatures, 'samples': results}
    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
    print('PASS', sum(sample['cases'] for sample in results), 'original native per-object deselection cases')
    print(json.dumps(signatures))


if __name__ == '__main__':
    main()

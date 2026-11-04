"""Compiled production deselection-helper admission and native execution.

The helper fixture has raw read/call adapters, not a fake Deselect. All native
selection edits execute the owner's private supported EXE. No Windows UI or
live multiplayer claim is made.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import *

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game', action='append', required=True)
parser.add_argument('--helper', required=True)
parser.add_argument('--output')
args = parser.parse_args()
helper = pefile.PE(args.helper)
helper_base = helper.OPTIONAL_HEADER.ImageBase
helper_image = helper.get_memory_mapped_image()
entry = helper_base + next(export.address for export in helper.DIRECTORY_ENTRY_EXPORT.symbols if export.name == b'selectionFixture')
rows = []
for game in args.game:
    raw = Path(game).read_bytes()
    sha = hashlib.sha256(raw).hexdigest()
    assert sha in {'268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0', 'c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a'}
    native = pefile.PE(data=raw).get_memory_mapped_image()
    for case in ('ready', 'dispatch_aircraft', 'dispatch_unit', 'outside_entry', 'unselected', 'wrong_owner', 'dead', 'infantry', 'wrong_method', 'wrong_vector_method', 'not_listed', 'duplicate', 'bad_count', 'bad_capacity', 'unreadable', 'patched_deselect', 'patched_find', 'patched_get', 'patched_set'):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        for base, image in ((0x400000, native), (helper_base, helper_image)):
            u.mem_map(base, (len(image) + 4095) & ~4095)
            u.mem_write(base, image)
        u.mem_map(0x20000000, 0x500000)
        u.mem_map(0x30000000, 0x200000)
        write = lambda at, value: u.mem_write(at, struct.pack('<I', value & 0xFFFFFFFF))
        read = lambda at: struct.unpack('<I', u.mem_read(at, 4))[0]
        actor, other, third, items, stack, stop = 0x20000000, 0x20002000, 0x20004000, 0x20200000, 0x30100000, 0x30000000
        objects = [other, actor, third]
        for i, obj in enumerate(objects):
            write(obj, 0x7F5C70 if case == 'dispatch_unit' and obj == actor else 0x7E22A4)
            write(obj + 0x10, i + 1)
            write(obj + 0x21C, 0x20400000)
            write(obj + 0x6C, 100)
            for off in (0x74, 0x83, 0x90):
                u.mem_write(obj + off, bytes([1]))
            write(items + i * 4, obj)
        write(0xA8ECB8, 0x7E4F64)
        write(0xA8ECBC, items)
        write(0xA8ECC0, 3)
        write(0xA8ECC8, 3)
        write(0xA8ECC4, 1)
        write(0x87F7E8 + 0x11A0, actor)
        u.mem_write(0x87F7E8 + 0x119C, bytes([1]))
        operation, deny = 1, 0
        if case == 'ready': operation = 0
        if case == 'outside_entry': operation = 2
        if case == 'unselected': u.mem_write(actor + 0x83, bytes(1))
        if case == 'wrong_owner': write(actor + 0x21C, 0x20410000)
        if case == 'dead': write(actor + 0x6C, 0)
        if case == 'infantry': write(actor, 0x7EB058)
        if case == 'wrong_method': write(0x7E22A4 + 0x150, 0x5F44A1)
        if case == 'wrong_vector_method': write(0x7E4F64 + 0x10, 0x40CF01)
        if case == 'not_listed': write(items + 4, other)
        if case == 'duplicate': write(items + 8, actor)
        if case == 'bad_count': write(0xA8ECC8, 4097)
        if case == 'bad_capacity': write(0xA8ECC0, 2)
        if case == 'unreadable': deny = 0xA8ECC8
        patched = {'patched_deselect': 0x5F44A0, 'patched_find': 0x40CF00, 'patched_get': 0x4AEB10, 'patched_set': 0x4AEB30}.get(case)
        if patched: u.mem_write(patched, bytes([u.mem_read(patched, 1)[0] ^ 1]))
        before = [bytes(u.mem_read(obj, 0x800)) for obj in objects]
        vector_before = bytes(u.mem_read(items, 12))
        count_before = read(0xA8ECC8)
        u.mem_write(stack, struct.pack('<4I', stop, actor, operation, deny))
        u.reg_write(UC_X86_REG_ESP, stack)
        u.emu_start(entry, stop, count=1000000)
        assert u.reg_read(UC_X86_REG_EIP) == stop
        assert u.reg_read(UC_X86_REG_ESP) == stack + 4
        result = u.reg_read(UC_X86_REG_EAX)
        success = case in ('ready', 'dispatch_aircraft', 'dispatch_unit')
        assert bool(result) == success, (case, result)
        if case.startswith('dispatch_'):
            assert read(0xA8ECC8) == 2 and [read(items + i * 4) for i in range(2)] == [other, third]
            assert bytes(u.mem_read(actor + 0x83, 1)) == bytes(1)
            assert read(0x87F7E8 + 0x11A0) == 0
            for i, obj in enumerate(objects):
                expected = before[i] if obj != actor else before[i][:0x83] + bytes(1) + before[i][0x84:]
                assert bytes(u.mem_read(obj, 0x800)) == expected
        else:
            assert read(0xA8ECC8) == count_before and bytes(u.mem_read(items, 12)) == vector_before
            assert all(bytes(u.mem_read(obj, 0x800)) == original for obj, original in zip(objects, before))
        rows.append({'game_sha256': sha, 'case': case, 'status': 'PASS'})
if args.output:
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps({'scope': __doc__, 'game_tested': False, 'multiplayer_tested': False, 'helper_sha256': hashlib.sha256(Path(args.helper).read_bytes()).hexdigest(), 'results': rows}, indent=2) + '\n')
print('PASS', len(rows), 'compiled production helper admission and original native selection cases')

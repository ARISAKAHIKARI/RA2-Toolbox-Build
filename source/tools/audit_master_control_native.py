"""Private-PE, emulator-only control predicate audit; never starts a game.

The native-unpacked variant uses the previously captured nine-byte prefix.
The supported PE variant executes its actual forwarder. Only the emulator's
copy is patched. Output is metadata/results, never a binary or raw dump.
Requires pefile, capstone, unicorn.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
import capstone
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

SUPPORTED = '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
NATIVE = bytes.fromhex('A1 38 B2 A8 00 85 C0 75 1D')
FORWARD = bytes.fromhex('E9 FB 2A 67 00 CC CC CC CC')
STRICT = bytes.fromhex('8B 15 4C 3D A8 00 33 C0 3B CA 0F 94 C0 C3')
LOCAL, FOREIGN, OBJECT = 0x20010000, 0x20020000, 0x20030000
STACK, STOP = 0x300F0000, 0x20101000
CALLS = [(0x6339BA, 'E8 31 7D ED FF', 'E8 57 7D ED FF'),
         (0x637E40, 'E8 AB 38 ED FF', 'E8 D1 38 ED FF')]

class Harness:
    def __init__(self, pe, variant, style):
        self.mu = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        self.mu.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        self.mu.mem_write(base, pe.get_memory_mapped_image())
        self.mu.mem_map(0x20000000, 0x200000)
        self.mu.mem_map(0x30000000, 0x100000)
        self.put(LOCAL + 0x30, 0)
        self.put(FOREIGN + 0x30, 1)
        if variant == 'captured_native_prefix_composite':
            self.mu.mem_write(0x50B6F0, NATIVE)
        if style != 'original':
            if variant == 'supported_PE_forwarder' or style == 'master_unified_entry':
                self.mu.mem_write(0x50B6F0, bytes.fromhex('E9 1B 00 00 00'))
            else:
                self.mu.mem_write(0x50B6F7, bytes.fromhex('EB 17'))
        if style in ('master_complete', 'master_unified_entry'):
            for address, before, after in CALLS:
                self.mu.mem_write(address, bytes.fromhex(after))
        self.writes = []
        self.stop = STOP
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self.on_write)
        self.mu.hook_add(UC_HOOK_CODE, self.on_code)

    def put(self, at, value, fmt='I'):
        self.mu.mem_write(at, struct.pack('<' + fmt, value))

    def on_write(self, uc, _access, address, size, value, _):
        if not 0x30000000 <= address < 0x30100000:
            self.writes.append([hex(address), size])

    def on_code(self, uc, address, _size, _):
        if address == self.stop:
            uc.emu_stop()

    def run(self, entry, house, planning=False):
        self.writes = []
        self.put(STACK, STOP)
        for reg, value in [(UC_X86_REG_ESP, STACK), (UC_X86_REG_ECX, house),
                           (UC_X86_REG_EAX, 0x12345678), (UC_X86_REG_EDX, 0xFEDCBA98),
                           (UC_X86_REG_EBX, 0x12340001), (UC_X86_REG_EBP, 0x12340002),
                           (UC_X86_REG_ESI, 0x12340003), (UC_X86_REG_EDI, OBJECT),
                           (UC_X86_REG_EFLAGS, 0x202)]:
            self.mu.reg_write(reg, value)
        self.stop = 0x637E55 if planning else STOP
        self.mu.emu_start(entry, STOP, count=1000)
        assert self.mu.reg_read(UC_X86_REG_EIP) == self.stop
        assert self.mu.reg_read(UC_X86_REG_ESP) == STACK + (0 if planning else 4)
        assert not self.writes, self.writes
        for reg, value in [(UC_X86_REG_EBX, 0x12340001), (UC_X86_REG_EBP, 0x12340002),
                           (UC_X86_REG_EDI, OBJECT)]:
            assert self.mu.reg_read(reg) == value
        if not planning:
            assert self.mu.reg_read(UC_X86_REG_ESI) == 0x12340003
        return self.mu.reg_read(UC_X86_REG_ESI if planning else UC_X86_REG_EAX)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == SUPPORTED, 'Unverified executable'
    pe = pefile.PE(data=raw)
    def data(va, size): return pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, size)
    assert data(0x50B6F0, 9) == FORWARD
    assert data(0x50B710, 6) == bytes.fromhex('B8 01 00 00 00 C3')
    assert data(0x50B716, len(STRICT)) == STRICT
    dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for address, code, mnemonic, target in [
        (0x50B6F0, 'E9 1B 00 00 00', 'jmp', 0x50B710),
        (0x50B6F7, '75 1D', 'jne', 0x50B716),
        (0x50B6F7, 'EB 17', 'jmp', 0x50B710),
    ]:
        instructions = list(dis.disasm(bytes.fromhex(code), address))
        assert len(instructions) == 1 and instructions[0].size == len(bytes.fromhex(code))
        assert instructions[0].mnemonic == mnemonic and int(instructions[0].op_str, 16) == target
    sites = []
    for address, before, after in CALLS:
        assert data(address, 5) == bytes.fromhex(before)
        for byte_string, target in [(before, 0x50B6F0), (after, 0x50B716)]:
            instructions = list(dis.disasm(bytes.fromhex(byte_string), address))
            assert len(instructions) == 1 and instructions[0].size == 5
            assert instructions[0].mnemonic == 'call' and int(instructions[0].op_str, 16) == target
        sites.append(dict(address=hex(address), before=before, after=after, length=5))
    # The historic suspect site lies inside MOV's absolute-address operand.
    suspect = list(dis.disasm(data(0x6378DD, 8), 0x6378DD))
    assert suspect[0].size == 6 and suspect[0].mnemonic == 'mov'
    assert 0x6378E0 not in {x.address for x in suspect}
    variants = ['supported_PE_forwarder', 'captured_native_prefix_composite']
    rows, checks = [], 0
    for variant in variants:
        for style in ['original', 'predicate_only', 'master_complete', 'master_unified_entry']:
            h = Harness(pe, variant, style)
            count = 0
            for mode in (0, 3, 4, 5):
                for flags in range(4):
                    for owner in (LOCAL, FOREIGN):
                        h.put(0xA8B238, mode)
                        h.put(0xA83D4C, LOCAL)
                        h.put(OBJECT + 0x21C, owner)
                        for house in (LOCAL, FOREIGN):
                            h.put(house + 0x1EC, flags & 1, 'B')
                            h.put(house + 0x1ED, (flags >> 1) & 1, 'B')
                        native_control = bool(flags) if variant == variants[1] and mode == 0 else owner == LOCAL
                        controlled = native_control if style == 'original' else True
                        assert h.run(0x50B6F0, owner) == int(controlled)
                        owner_index = int(owner == FOREIGN)
                        expected_index = owner_index if style in ('master_complete', 'master_unified_entry') or not controlled else 0
                        assert h.run(0x6339B0, OBJECT) == expected_index
                        assert h.run(0x637E37, owner, planning=True) == expected_index
                        count += 3
            rows.append(dict(variant=variant, style=style, function_results_checked=count))
            checks += count
    h = Harness(pe, variants[0], 'original')
    for current in (0, LOCAL, FOREIGN):
        h.put(0xA83D4C, current)
        for owner in (0, LOCAL, FOREIGN):
            assert h.run(0x50B716, owner) == int(owner == current)
            assert h.mu.reg_read(UC_X86_REG_ECX) == owner
            assert h.mu.reg_read(UC_X86_REG_EDX) == current
            checks += 1
    result = dict(executable_sha256=digest, checks=checks, status='PASS',
                  private_executable_modified=False, game_executed=False,
                  patch_applied_to_emulator_copy_only=True,
                  sites=sites,
                  predicate_variants=[dict(kind=variants[0],address='0x50b6f0',before='E9 FB 2A 67 00',after='E9 1B 00 00 00',length=5),
                                      dict(kind=variants[1],address='0x50b6f7',before='75 1D',after='EB 17',length=2)],
                  strict_target='0x50b716', true_target='0x50b710',
                  rows=rows, non_stack_data_writes=0,
                  boundary='Original supported PE and explicitly composed captured-native prefix; only predicates, owner-index function and planning owner-index slice execute. No complete command, Windows UI, game lifecycle or netplay claim.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(checks=checks,status='PASS',sha256=digest)))

if __name__ == '__main__':
    main()

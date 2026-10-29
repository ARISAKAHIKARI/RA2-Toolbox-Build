"""Read-only controlled audit of original Tactical render admission.

Requires pefile and Unicorn. Native sprite/surface drawing is not executed;
the audit stops at the world or feedback block, with one explicit query double.
No private executable or instruction bytes are emitted.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP

DATA, STACK, STOP = 0x2000000, 0x2100000, 0x2200000
RANGES = ((0x4F4480, 0x4F45A9), (0x6D3D10, 0x6D3E9B),
          (0x6D4582, 0x6D4764), (0x53BAE0, 0x53BAEE),
          (0x40D1B2, 0x40D1BC), (0x7E1964, 0x7E19AC))


def map_trace(image, base):
    # Do not restore a platform's detours from another executable. Its actual
    # original Map function is only run when the entire inspected body matches.
    fingerprint = hashlib.sha256(image[0x4F4480-base:0x4F45A9-base]).hexdigest()
    if fingerprint != '4a9c13e1beca48c5d0f1f0062a3c88a236362eb7d032cd33e8ddaafcf61481f3':
        return {'status': 'not_executed_platform_modified_body', 'cases': []}
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(base, (len(image)+4095)&~4095)
    u.mem_write(base, image)
    for page in (DATA, STACK, STOP):
        u.mem_map(page, 0x10000)

    def write(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def read(address):
        return struct.unpack('<I', u.mem_read(address, 4))[0]

    def ret(clean):
        sp = u.reg_read(UC_X86_REG_ESP)
        u.reg_write(UC_X86_REG_ESP, sp+4+clean)
        u.reg_write(UC_X86_REG_EAX, 0)
        u.reg_write(UC_X86_REG_EIP, read(sp))

    records, trace = [], []

    def hook(_, address, __, ___):
        if address == 0x6D3D10:
            sp = u.reg_read(UC_X86_REG_ESP)
            trace.append({'call': 'Tactical_Render', 'phase': read(sp+12),
                          'redraw': read(sp+8)&255})
            ret(12)
        elif address == STOP+0x200:
            trace.append({'call': 'Map_virtual_40', 'redraw': read(u.reg_read(UC_X86_REG_ESP)+4)&255})
            ret(4)
        elif address in (STOP+0x300, STOP+0x400):
            ret(8)
        elif address in (STOP+0x500, 0x5D49A0):
            ret(0)

    u.hook_add(UC_HOOK_CODE, hook)
    obj, vtable, mouse, mousevt, tactical = DATA, DATA+0x1000, DATA+0x2000, DATA+0x3000, DATA+0x4000
    write(obj, vtable)
    write(vtable+0x40, STOP+0x200)
    write(vtable+0x44, STOP+0x500)
    write(mouse, mousevt)
    write(mousevt+0x40, STOP+0x300)
    write(mousevt+0x3C, STOP+0x400)
    for dirty in (0, 1, 2):
        for gate in (0, 1, 2, 3):
            u.mem_write(STACK, bytes(0x10000))
            write(obj+0xC, dirty)
            for address, value in ((0x88731C, DATA+0x5000), (0x887314, 0x12345678),
                                   (0x887640, mouse), (0x887324, tactical),
                                   (0xA9FAB0, gate), (0xB0B519, 0), (0xA8EF54, 0),
                                   (0x887368, 0), (0xA8B8B4, 0)):
                write(address, value)
            sp = STACK+0x8000
            write(sp, STOP)
            u.reg_write(UC_X86_REG_ECX, obj)
            u.reg_write(UC_X86_REG_ESP, sp)
            trace.clear()
            u.emu_start(0x4F4480, STOP, count=5000)
            expected = ([0, 1, 2] if gate == 0 else [])
            assert [x['phase'] for x in trace if x['call']=='Tactical_Render'] == expected
            assert all(x['redraw'] == int(dirty != 0) for x in trace if x['call']=='Tactical_Render')
            assert all(x['redraw'] == int(dirty == 2) for x in trace if x['call']=='Map_virtual_40')
            assert sum(x['call']=='Map_virtual_40' for x in trace) == int(gate==0)
            assert read(obj+0xC)==0 and read(0x887314)==0x12345678
            assert u.reg_read(UC_X86_REG_ESP) == sp+4
            records.append({'redraw_level': dirty, 'coarse_gate_state': gate, 'calls': list(trace),
                            'redraw_consumed': True, 'surface_pointer_restored': True})
    return {'status': 'PASS_with_Tactical_surface_and_auxiliary_draw_doubles', 'cases': records}


def audit(path):
    raw = path.read_bytes()
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(base, (len(image) + 4095) & ~4095)
    machine.mem_write(base, image)
    for page in (DATA, STACK, STOP):
        machine.mem_map(page, 0x10000)

    def write(address, value):
        machine.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def read(address):
        return struct.unpack('<I', machine.mem_read(address, 4))[0]

    def return_from_call(value, clean):
        sp = machine.reg_read(UC_X86_REG_ESP)
        machine.reg_write(UC_X86_REG_EAX, value)
        machine.reg_write(UC_X86_REG_ESP, sp + 4 + clean)
        machine.reg_write(UC_X86_REG_EIP, read(sp))

    state = {}

    def trace(_, address, __, ___):
        if address == 0x6D9B50:
            # Actual query scans visible animated objects. Isolate its Boolean
            # result; this fixture does not emulate object virtual methods.
            return_from_call(state['animated'], 16)
        elif address == STOP + 0x100:
            state['surface_copies'] += 1
            return_from_call(1, 20)
        elif address in (0x6D3E9B, 0x6D45F2, 0x6D4B3E):
            state['outcome'] = {0x6D3E9B: 'world_block', 0x6D45F2: 'feedback_block',
                                0x6D4B3E: 'return_before_feedback'}[address]
            machine.emu_stop()

    machine.hook_add(UC_HOOK_CODE, trace)
    tactical, surface, vtable = DATA, DATA + 0x2000, DATA + 0x3000
    write(surface, vtable)
    write(vtable + 8, STOP + 0x100)
    records = []
    cases = (
        ('clean_static', 0, 0, 0, 0, 0),
        ('camera_changed', 0, 0, 0, 1, 0),
        ('dirty_regions', 0, 0, 1, 0, 0),
        ('redraw_requested', 0, 1, 0, 0, 0),
        ('tactical_redraw_flag', 1, 0, 0, 0, 0),
        ('animated_visible', 0, 0, 0, 0, 1),
    )
    for phase in (0, 1, 2):
        for name, dirty, redraw, objects, moved, animated in cases:
            machine.mem_write(tactical, bytes(0x1000))
            machine.mem_write(STACK, bytes(0x10000))
            for address, value in ((0x886FA0, 0), (0x886FA4, 0), (0x886FA8, 800),
                                   (0x886FAC, 600), (0xB0CE88, objects), (0xB0E63C, 0),
                                   (0x887314, surface), (0x8872FC, surface)):
                write(address, value)
            write(tactical + 0xD64, moved * 30)
            write(tactical + 0xD6C, 0)
            write(tactical + 0xB8, moved * 30)
            machine.mem_write(tactical + 0xD7D, bytes([dirty]))
            sp = STACK + 0x8000
            for offset, value in ((0, STOP), (4, surface), (8, redraw), (12, phase)):
                write(sp + offset, value)
            machine.reg_write(UC_X86_REG_ESP, sp)
            machine.reg_write(UC_X86_REG_ECX, tactical)
            state.clear()
            state.update(animated=animated, surface_copies=0, outcome=None)
            machine.emu_start(0x6D3D10, STOP, count=5000)
            expected = ('feedback_block' if phase == 2 else
                        'return_before_feedback' if name == 'clean_static' else 'world_block')
            assert state['outcome'] == expected, (path.name, phase, name, state, expected)
            records.append({'phase': phase, 'case': name, 'outcome': state['outcome'],
                            'surface_copies': state['surface_copies']})
    def word(address):
        return struct.unpack_from('<I', image, address-base)[0]
    mouse_vtable = 0x7E1964
    descriptor = word(word(mouse_vtable-4)+12)
    rtti_name = image[descriptor-base+8:descriptor-base+128].split(b'\0')[0].decode('ascii')
    assert rtti_name == '.?AVMouseClass@@'
    assert word(mouse_vtable+0x3C) == 0x4F4480 and word(mouse_vtable+0x40) == 0x6D0A20
    return {'sample': path.name, 'sha256': hashlib.sha256(raw).hexdigest(),
            'mouse_vtable': {'address': hex(mouse_vtable), 'rtti': rtti_name,
                             'render_slot_3c': '0x4F4480', 'draw_slot_40': '0x6D0A20',
                             'global_initializer_instruction': '0x40D1B2'},
            'map_trace': map_trace(image, base),
            'ranges': [{'start': hex(a), 'end_exclusive': hex(b),
                        'sha256': hashlib.sha256(image[a-base:b-base]).hexdigest()}
                       for a, b in RANGES], 'cases': records}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', action='append', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = {'scope': 'Original Tactical admission to world/feedback blocks; query/surface boundaries doubled, no full draw, Windows or game FPS proof',
              'product_changes': False, 'samples': [audit(p) for p in args.game]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS:', sum(len(x['cases']) for x in result['samples']),
          'original Tactical branch cases and',
          sum(len(x['map_trace']['cases']) for x in result['samples']),
          'Map call-order cases; no sprite rendering or native suppression implemented')


if __name__ == '__main__':
    main()

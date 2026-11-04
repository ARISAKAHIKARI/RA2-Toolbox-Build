"""Run the original DoList scheduler with two logical senders and synthetic Houses.

Usage: same_house_dolist_order_capture.py GAME_FIXTURE --output REPORT.json
Both commands address the same unit in the same frame, with different Move
destinations. Respond is a capture-only sink: no mission execution, world tick,
packet transport, production DLL or real multiplayer game is executed.
"""
from pathlib import Path

SCOPE = __doc__
fixture = Path(__file__).with_name('destruct_receiver_probe.py')
source = fixture.read_text()
marker = 'for i,h in enumerate(houses):'
assert marker in source, 'native receiver fixture boundary changed'
exec(compile(source.split(marker, 1)[0], str(fixture), 'exec'))

# The scheduler's timing/logging dependencies are deterministic fixture sinks.
clock = stop + 16
u.mem_write(clock, b'\xb8' + struct.pack('<I', 5678) + b'\xc3')
w(0x7E1530, clock)
seen = []


def capture_respond(uc, address, size, _):
    packet = uc.reg_read(UC_X86_REG_ECX)
    seen.append({
        'house': uc.mem_read(packet + 2, 1)[0],
        'kind': uc.mem_read(packet, 1)[0],
        'frame': r(packet + 3),
        'source': r(packet + 7),
        'mission': uc.mem_read(packet + 12, 1)[0],
        'destination': r(packet + 19),
    })
    sp = uc.reg_read(UC_X86_REG_ESP)
    uc.reg_write(UC_X86_REG_EIP, r(sp))
    uc.reg_write(UC_X86_REG_ESP, sp + 4)


u.hook_add(UC_HOOK_CODE, capture_respond, begin=0x4C6CB0, end=0x4C6CB0)
for index, house in enumerate(houses):
    u.mem_write(house, bytes(0x20000))
    w(house + 0x30, index)
    b8(house + 0x1ED, 1)
    w(items + index * 4, house)
w(0xA8022C, items)
w(0xA80238, 2)
w(0xA8ED84, 1234)
w(0xA8B238, 3)

rows = []
for preserve_owner in (False, True):
    outcomes = []
    for local_sender in (0, 1):
        w(0xA83D4C, houses[local_sender])
        for address, value in (
            (0xA8B1DC, 0), (0xA8B1F8, 0), (0xA8E2D8, 0),
            (0xA8E2DC, 0), (0xA8E2E0, 0xFFFFFFFF),
            (0x8B41F8, 2), (0x8B41FC, 0), (0x8B4200, 2),
            (0xA83ED0, 0), (0xA83ED4, 0), (0xA83ED8, 0),
        ):
            w(address, value)
        senders = (local_sender, 1 - local_sender)
        for slot, sender in enumerate(senders):
            packet = 0x8B4204 + slot * 111
            u.mem_write(packet, bytes(111))
            # Native constructor: fixed actor 101, Move=2, destination Cell.
            run(0x4C6860, packet, 1 if preserve_owner else sender,
                101, 0x34, 2, 0, 0, 100 + sender, 11, 0, 0)
            w(0xA70204 + slot * 4, 5678)
        seen.clear()
        run(0x64C380, 0, 0, 0, 0)
        assert len(seen) == 2
        assert all(row['kind'] == 4 and row['frame'] == 1234 and
                   row['source'] == 101 and row['mission'] == 2 for row in seen)
        assert all(u.mem_read(0x8B4204 + slot * 111 + 1, 1)[0] & 1 for slot in (0, 1))
        outcomes.append([row['destination'] for row in seen])
        rows.append({
            'preserve_owner': preserve_owner,
            'local_sender': local_sender,
            'dolist_sender_insertion_order': senders,
            'response_order': list(seen),
            'both_executed_flags_set': True,
            'status': 'PASS',
        })
    expected = [[100, 101], [101, 100]] if preserve_owner else [[100, 101], [100, 101]]
    assert outcomes == expected, outcomes

report = {
    'scope': SCOPE,
    'game_fixture_sha256': hashlib.sha256(b).hexdigest(),
    'supported_source_exe_sha256': '7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6',
    'game_tested': False,
    'multiplayer_tested': False,
    'conclusion': 'Two senders sharing one event House retain endpoint-specific '
                  'insertion order. Distinct sender House tags produce identical '
                  'Respond order despite reversed insertion order.',
    'results': rows,
}
Path(a.output).write_text(json.dumps(report, indent=2) + '\n')
print('PASS', len(rows), 'native scheduler cases; same-House order divergence reproduced')

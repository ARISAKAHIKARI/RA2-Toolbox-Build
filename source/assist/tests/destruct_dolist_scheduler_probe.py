"""Original DoList dispatch followed by original Destruct response on synthetic Houses.

No DLL, live game, network or full House AI/object destruction is executed.
"""
from pathlib import Path

fixture = Path(__file__).with_name('destruct_receiver_probe.py')
source = fixture.read_text()
exec(compile(source.split('for i,h in enumerate(houses):', 1)[0], str(fixture), 'exec'))
clock = stop + 16
u.mem_write(clock, b'\xb8' + struct.pack('<I', 5678) + b'\xc3')
w(0x7E1530, clock)
rows = []
for mode in (0, 5, 3, 4):
    for target in (0, 1):
        for human in (False, True):
            for index, house in enumerate(houses):
                u.mem_write(house, bytes(0x20000))
                w(house + 0x30, index)
                b8(house + 0x1EC, index == 0)
                b8(house + 0x1ED, index == 0 or human)
                w(items + index * 4, house)
            for address, value in (
                (0xA8022C, items), (0xA80238, 2), (0xA83D4C, houses[0]),
                (0xA8ED84, 1234), (0xA8B238, mode), (0xA8B1DC, 0),
                (0xA8B1F8, 0), (0xA8E2D8, 0), (0xA8E2DC, 0),
                (0xA8E2E0, 0xFFFFFFFF), (0x8B41F8, 1), (0x8B41FC, 0),
                (0x8B4200, 1), (0xA83ED0, 0), (0xA83ED4, 0), (0xA83ED8, 0),
            ):
                w(address, value)
            packet = 0x8B4204
            u.mem_write(packet, bytes(111))
            run(0x4C66C0, packet, target, 8)
            w(0xA70204, 5678)
            calls.clear()
            run(0x64C380, 0, 0, 0, 0)
            expected = target == 0 or human
            assert calls == ([houses[target]] if expected else []), (mode, target, human, calls)
            assert bool(u.mem_read(houses[target] + 0x1F6, 1)[0]) == expected
            assert bool(u.mem_read(packet + 1, 1)[0] & 1) == expected
            assert not u.mem_read(houses[1-target] + 0x1F6, 1)[0]
            rows.append(dict(session=mode, target=target, human=human,
                             responded=expected, status='PASS'))
Path(a.output).write_text(json.dumps(dict(scope=__doc__, game_sha256=hashlib.sha256(b).hexdigest(),
                                        results=rows), indent=2) + '\n')
print('PASS', len(rows), 'native scheduler/Destruct cases; AI House events are skipped')

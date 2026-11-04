"""Isolate native 64DAB0 inputs; no rendering, world tick or netplay is run.

Requires the owner's private capture archive and the sibling native CRC fixture.
The two draw-site mutations test whether the checksum reads those code bytes;
neither their jump targets nor the production rendering hooks are executed.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--capture', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()

# Reuse the original captured-code harness and its independent CRC oracle.
# Stop before its test runner; the fixture accepts the same CLI arguments.
fixture = Path(__file__).with_name('network_state_capture.py')
source = fixture.read_text()
marker = '\nrows=[crc_case(c)'
assert source.count(marker) == 1, 'native CRC fixture boundary changed'
namespace = {'__name__': 'native_crc_fixture', '__file__': str(fixture)}
exec(compile(source.split(marker)[0], str(fixture), 'exec'), namespace)

OriginalProbe = namespace['Probe']
S, STOP = namespace['S'], namespace['STOP']
current_case = 'baseline_alpha_0'


class IsolatedProbe(OriginalProbe):
    def __init__(self):
        super().__init__()
        # A complete synthetic mapped alpha buffer; no native renderer runs.
        alpha, begin, pixels = S + 0x50000, S + 0x60000, 1024
        self.w(0x87E8A4, alpha)
        self.w(alpha + 0x1C, begin + pixels * 2)
        self.w(alpha + 0x20, pixels * 2)
        self.w(alpha + 0x28, 32)
        value = {'half_alpha_64': 64, 'clear_alpha_127': 127}.get(current_case, 0)
        self.u.mem_write(begin, struct.pack('<H', value) * pixels)
        sites = {
            'shroud_site_patched': (0x47F038,),
            'both_draw_sites_patched': (0x47F038, 0x47F274),
        }.get(current_case, ())
        for address in sites:
            self.u.mem_write(address, b'\xE9' + struct.pack('<i', STOP - address - 5))


namespace['Probe'] = IsolatedProbe
rows = []
for current_case in (
    'baseline_alpha_0', 'half_alpha_64', 'clear_alpha_127',
    'shroud_site_patched', 'both_draw_sites_patched', 'house_MapIsClear_1',
):
    positive = current_case == 'house_MapIsClear_1'
    row = namespace['crc_case']('map_clear' if positive else 'baseline')
    row['case'] = current_case
    row['equals_baseline'] = row['native_crc'] == rows[0]['native_crc'] if rows else True
    assert row['equals_baseline'] == (not positive), row
    rows.append(row)

report = {
    'scope': __doc__,
    'supported_executable_sha256': '7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6',
    'capture_image_sha256': hashlib.sha256(namespace['image']).hexdigest(),
    'evidence': 'Complete captured native ComputeGameCRC and RNG with fresh identical '
                'world/RNG initialization per case; original logger only mocked.',
    'game_tested': False,
    'multiplayer_tested': False,
    'limitations': [
        'No native draw function, patched jump target or halfShroud implementation is executed.',
        'Code-site mutations isolate whether ComputeGameCRC scans these code bytes; '
        'they do not validate hooks or rendering side effects.',
        'No world tick, platform integrity check, packet send/receive or real multiplayer test.',
    ],
    'results': rows,
}
Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
print('PASS', len(rows), 'native shroud CRC isolation cases; no rendering or multiplayer validation')

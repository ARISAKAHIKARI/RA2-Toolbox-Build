"""Controlled original MapClass::SetVisibleRect inset replay; private capture input."""
from pathlib import Path
import hashlib
import json
import struct
import sys

from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP

MAP = 0x87F7E8
CASES = [(0,0,32,40), (1,1,32,40), (2,2,32,40), (3,4,32,40),
         (5,7,31,30), (2,2,26,32), (2,2,27,33), (2,2,28,34),
         (2,2,29,35), (5,4,23,30), (2,2,10,10), (18,17,10,12), (20,20,10,10)]

def main(path):
    count = 0
    summary = []
    for stage in ('01-off', '02-on', '03-off'):
        c = Capture(path, stage)
        u = c.emulator()
        u.mem_map(0x60000000, 0x10000)
        def put(address, value):
            u.mem_write(address, struct.pack('<i', value))
        def get(address):
            return struct.unpack('<i', u.mem_read(address, 4))[0]
        for left, top, columns, rows in CASES:
            map_width, map_height = 30, 40
            put(MAP + 0xF4, map_width)
            put(MAP + 0xF8, map_height)
            for offset, value in zip((0xFC,0x100,0x104,0x108),
                                     (left,top,columns,rows)):
                put(MAP + offset, value)
            u.reg_write(UC_X86_REG_ESI, MAP + 0xFC)
            u.reg_write(UC_X86_REG_EDI, MAP)
            u.reg_write(UC_X86_REG_ESP, 0x60008000)
            # Start after the native rectangle intersection and stop before
            # redraw call. These are unmodified original executable bytes.
            u.emu_start(0x56726E, 0x5672D3, count=100)
            actual = tuple(get(MAP + offset) for offset in (0xFC,0x100,0x104,0x108))
            adjusted_left, adjusted_top = max(left,2), max(top,2)
            expected = (adjusted_left,adjusted_top,
                        min(columns,map_width-adjusted_left-2),
                        min(rows,map_height-adjusted_top-6))
            assert actual == expected, (stage,(left,top,columns,rows),actual,expected)
            if stage == '01-off':
                summary.append({'requested':[left,top,columns,rows],'native':list(actual)})
            count += 1
    return {'supported_executable_sha256':SUPPORTED_SHA,
            'original_code':{'start':'0x56726E','end':'0x5672D3',
                'sha256':hashlib.sha256(c.read(0x56726E,0x65)).hexdigest()},
            'stages':3,'cases_per_stage':len(CASES),'cases':count,'mismatches':0,
            'rectangles':summary}

if __name__ == '__main__':
    print(json.dumps(main(Path(sys.argv[1])), ensure_ascii=False))

"""Compare original cached shroud/fog frames with their eight-neighbor selector.

The supported capture is an input only. Emit aggregate counts and instruction
hashes; never copy game memory, shape pixels, or cell locations to the report.
"""
import argparse
from bisect import bisect_right
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import zipfile

SUPPORTED_SHA = '7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6'
NEIGHBORS = ((-1,-1,64),(0,-1,128),(1,-1,1),(-1,0,32),(1,0,2),(-1,1,16),(0,1,8),(1,1,4))


class Capture:
    def __init__(self, path, stage):
        self.archive = zipfile.ZipFile(path) if path.is_file() else None
        self.root = next(name[:-len('session.json')] for name in self.archive.namelist()
                         if name.endswith('/session.json')) if self.archive else path
        session = json.loads(self.file('session.json'))
        assert session['file_sha256'].upper() == SUPPORTED_SHA
        records = [{'base':session['image_base'],'file':stage+'-game.mem','valid':stage+'-game.valid'}]
        records += json.loads(self.file(stage+'-private-regions.json'))
        self.regions = sorted(records,key=lambda item:int(item['base'],16))
        self.starts = [int(item['base'],16) for item in self.regions]
        self.cache = {}

    def file(self, name):
        return self.archive.read(self.root+name) if self.archive else (self.root/name).read_bytes()

    def read(self, address, size):
        result = bytearray()
        while size:
            index = bisect_right(self.starts,address)-1
            assert index>=0
            item = self.regions[index]
            if index not in self.cache:
                name = item['file']
                self.cache[index] = (self.file(name),self.file(item.get('valid',name+'.valid')))
            data,valid = self.cache[index]
            at = address-self.starts[index]
            count = min(size,len(data)-at)
            assert count>0 and 0 not in valid[at:at+count]
            result.extend(data[at:at+count]);address+=count;size-=count
        return bytes(result)

    def word(self, at):
        return struct.unpack('<I',self.read(at,4))[0]


def analyze(path, stage):
    capture = Capture(path,stage)
    table = struct.unpack('<256b',capture.read(0x7F4194,256))
    cells = capture.word(0x87F7E8+0x13C)
    count = struct.unpack('<i',capture.read(0x87F7E8+0x140,4))[0]
    assert 0<count<=512*512
    pointers = struct.unpack('<%dI'%count,capture.read(cells,count*4))
    state = {}
    for index,pointer in enumerate(pointers):
        if pointer<0x10000:continue
        x,y = index%512,index//512
        try:
            cell = capture.read(pointer,0x144)
            if struct.unpack_from('<hh',cell,0x24)!=(x,y):continue
            state[(x,y)] = (cell[0x12C],cell[0x140],*struct.unpack_from('<bb',cell,0x120))
        except (AssertionError,struct.error):
            continue
    modes = []
    for mode,all_bits,active,stored_at in ((0,0x18,0x08,2),(1,0x03,0x02,3)):
        expected = Counter();matches=mismatches=0
        for (x,y),cell in state.items():
            if any((x+dx,y+dy) not in state for dx,dy,_ in NEIGHBORS):continue
            flags = cell[mode]
            if not flags&all_bits:frame=-2
            elif not flags&active:frame=-1
            else:
                mask = sum(bit for dx,dy,bit in NEIGHBORS
                           if not state[(x+dx,y+dy)][mode]&active)
                frame = table[mask]
            expected['opaque' if frame==-2 else 'clear' if frame==-1 else 'edge']+=1
            matches+=frame==cell[stored_at]
            mismatches+=frame!=cell[stored_at]
        modes.append({'mode':'shroud' if mode==0 else 'fog','eligible_cells':matches+mismatches,
                      'expected_counts':dict(expected),'cached_matches':matches,'cached_mismatches':mismatches})
    return {'stage':stage,'valid_cells':len(state),'modes':modes,
            'selector_code_sha256':hashlib.sha256(capture.read(0x6D8700,0x6D8ADB-0x6D8700)).hexdigest(),
            'cell_draw_code_sha256':hashlib.sha256(capture.read(0x4801F0,0x48029A-0x4801F0)).hexdigest(),
            'lookup_table_sha256':hashlib.sha256(capture.read(0x7F4194,256)).hexdigest()}


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('capture',type=Path);parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    stages=[analyze(args.capture,stage) for stage in ('01-off','02-on','03-off')]
    assert all(item['cached_matches']>0 for stage in stages for item in stage['modes'])
    result={'supported_executable_sha256':SUPPORTED_SHA,'evidence':'original captured Cell flags and cached frames, selector reconstructed from original 0x6D8700 instructions; no original selector execution',
            'scoped_limit':'Capture is independent of the user screenshot. Cache agreement does not prove future/offscreen cache freshness. Renderer pixel composition requires separate validation.',
            'stages':stages}
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False))

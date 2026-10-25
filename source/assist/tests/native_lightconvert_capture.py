"""Read-only capture audit: native 16-bit LightConvert ramps and blitters.

Requires native_bib_capture.py on PYTHONPATH plus Unicorn. The input is private;
the JSON output contains aggregate numerical evidence, never LUT/asset bytes.
No game process, disk loader, window, or game update routine is run.
"""
import argparse
from collections import Counter
import hashlib
import json
import struct
from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def unpack565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def current_rgb(rgb, index, brightness, tint=(1000, 1000, 1000)):
    if 240 <= index <= 254:
        return tuple(rgb)
    shade = min(254, (261 * max(0, min(2000, brightness))) >> 11)
    level = min(62, 127 * shade * 62 // 32258)
    gain = [f32(f32(f32(level / 31) * t) / 1000) for t in tint]
    return tuple(int(min(255, max(0, f32(v * g)))) for v, g in zip(rgb, gain))


class Native:
    def __init__(self, capture):
        self.u = capture.emulator()
        self.u.mem_map(0x70000000, 0x10000)
        self.u.mem_map(0x71000000, 0x10000)
        self.u.mem_map(0x72000000, 0x1000)
        # Scalar drawing uses these only for pointer wrapping. Fixture buffers
        # deliberately hold one pixel and cannot read the captured framebuffer.
        self.write(0x87E8A4, 0x71001000)
        self.write(0x887644, 0x71002000)
        self.write(0x7100101C, 0x71003002)
        self.write(0x71001020, 2)
        self.write(0x7100201C, 0x71004002)
        self.write(0x71002020, 2)

    def read(self, p):
        return struct.unpack('<I', self.u.mem_read(p, 4))[0]

    def write(self, p, v):
        self.u.mem_write(p, struct.pack('<I', v & 0xFFFFFFFF))

    def call(self, fn, this, args):
        sp = 0x70008000
        self.u.mem_write(sp, struct.pack('<' + 'I' * (len(args) + 1), 0x72000000, *[x & 0xFFFFFFFF for x in args]))
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.reg_write(UC_X86_REG_ECX, this)
        self.u.reg_write(UC_X86_REG_FPCW, 0xE7F)
        self.u.emu_start(fn, 0x72000000, count=1000000)
        assert self.u.reg_read(UC_X86_REG_EIP) == 0x72000000
        return self.u.reg_read(UC_X86_REG_EAX)

    def pixel(self, blitter, brightness, alpha, index):
        self.u.mem_write(0x71003000, struct.pack('<H', alpha))
        self.u.mem_write(0x71004000, b'\xff\xff')
        self.u.mem_write(0x71005000, b'\xad\xde')
        self.u.mem_write(0x71006000, bytes([index]))
        self.u.mem_write(0x71007000, b'\0')
        fn = self.read(self.read(blitter) + 4)
        self.call(fn, blitter, [0x71005000, 0x71006000, 1, 0, 100,
                               0x71004000, 0x71003000, brightness, 0, 0x71007000])
        return struct.unpack('<H', self.u.mem_read(0x71005000, 2))[0]


def differences(palette, lut, table, brightnesses):
    result = {}
    for brightness in brightnesses:
        shade = min(254, (261 * max(0, min(2000, brightness))) >> 11)
        row = table[shade * 256 + 127] >> 8
        diff = []
        luma = []
        for index in range(1, 256):
            native = unpack565(lut[row * 256 + index])
            current = current_rgb(palette[index], index, brightness)
            d = [a-b for a, b in zip(current, native)]
            diff.extend(d)
            luma.append(sum(v*w for v, w in zip(d, (0.2126, 0.7152, 0.0722))))
        result[str(brightness)] = {
            'native_row_at_alpha_127': row,
            'current_level_63': min(62, 127*shade*62//32258),
            'mean_current_minus_native_channel': round(sum(diff)/len(diff), 6),
            'mean_current_minus_native_luma': round(sum(luma)/len(luma), 6),
            'min_channel_delta': min(diff), 'max_channel_delta': max(diff),
            'darker_channels': sum(v < 0 for v in diff),
            'equal_channels': sum(v == 0 for v in diff),
            'brighter_channels': sum(v > 0 for v in diff),
        }
    return result


def selections(c):
    """Run unchanged original source-selection branches with distinct drawers."""
    n=Native(c)
    n.u.mem_map(0x73000000,0x40000)
    b,t,h=0x71008000,0x7100A000,0x73000000
    a,at,cell=0x73022000,0x73023000,0x73024000
    owner_cv,custom_cv,cell_cv=0x73020000,0x73020400,0x73020800
    default_vector,custom_vector=0x73025000,0x73025100
    default_items,custom_items,default_scheme,custom_scheme=0x73025200,0x73025300,0x73026000,0x73026400
    native_b=next(x for x in c.vector(0xA8EB40) if c.word(x)==0x7E3EBC)
    native_a=c.vector(0xA8E9A8)[0]
    n.u.mem_write(b,c.read(native_b,0x700));n.u.mem_write(t,c.read(c.word(native_b+0x520),0x1800))
    n.u.mem_write(a,c.read(native_a,0x1C8));n.u.mem_write(at,c.read(c.word(native_a+0xC8),0x380))
    n.write(b+0x520,t);n.write(b+0x21C,h);n.write(h+0x16054,0)
    n.write(a+0xC8,at);n.write(a+0xCC,0);n.write(a+0xFC,1320)
    n.write(cell+0x34,cell_cv);n.u.mem_write(cell+0x10A,struct.pack('<hh',1150,850))
    n.write(0xB054D4,default_items);n.write(0xB054E0,1);n.write(default_items,default_scheme);n.write(default_scheme+0x30C,owner_cv)
    n.write(custom_vector+4,custom_items);n.write(custom_vector+16,1);n.write(custom_items,custom_scheme);n.write(custom_scheme+0x30C,custom_cv)
    n.write(0xA83D4C,h)
    plain=c.word(0x87F6C0)
    labels={owner_cv:'owner_scheme',custom_cv:'type_custom_scheme',cell_cv:'cell_terrain',plain:'anim_plain'}
    def map_boundary(uc,address,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP)
        uc.reg_write(UC_X86_REG_EAX,cell)
        uc.reg_write(UC_X86_REG_EIP,n.read(sp))
        uc.reg_write(UC_X86_REG_ESP,sp+8)
    hooks=[n.u.hook_add(UC_HOOK_CODE,map_boundary,begin=fn,end=fn) for fn in [0x5657A0,0x565730]]
    rows=[]
    sp=0x70008000
    for custom in [False,True]:
        n.write(t+0xDF0,custom_vector if custom else 0)
        for terrain in [False,True]:
            n.u.mem_write(t+0x1702,bytes([terrain]))
            n.write(sp+0x7C,1150);n.u.reg_write(UC_X86_REG_ESP,sp);n.u.reg_write(UC_X86_REG_ESI,b)
            n.u.emu_start(0x705EC1,0x705F89,count=1000)
            cv=n.u.reg_read(UC_X86_REG_EBX);value=n.read(sp+0x7C)
            assert cv==(cell_cv if terrain else custom_cv if custom else owner_cv)
            assert value==(850 if terrain else 1150)
            rows.append({'kind':'shp_building','custom':custom,'terrain_palette':terrain,'converter':labels[cv],'intensity':value})
            n.u.reg_write(UC_X86_REG_ESP,sp);n.u.reg_write(UC_X86_REG_EBP,b)
            n.u.emu_start(0x70718F,0x70719C,count=1000)
            cv=n.u.reg_read(UC_X86_REG_ESI)
            assert cv==(custom_cv if custom else owner_cv)
            rows.append({'kind':'voxel','custom':custom,'terrain_palette':terrain,'converter':labels[cv]})
    for branch in ['veins','cell','assigned','default','alternate']:
        n.u.mem_write(at+0x355,bytes([branch=='veins']))
        n.u.mem_write(a+0x196,bytes([branch=='cell']))
        n.u.mem_write(at+0x361,bytes([branch=='alternate']))
        n.write(a+0xD4,custom_cv if branch=='assigned' else 0)
        for normal in [False,True]:
            n.u.mem_write(at+0x35D,bytes([normal]));n.u.reg_write(UC_X86_REG_ESP,sp);n.u.reg_write(UC_X86_REG_ESI,a)
            n.u.emu_start(0x423114,0x423358,count=1000)
            cv=n.read(sp+0x30);value=n.read(sp+0x38)
            expected_cv={'veins':owner_cv,'cell':cell_cv,'assigned':custom_cv,'default':plain,'alternate':owner_cv}[branch]
            expected_value=1000 if normal else 850 if branch in ['veins','cell'] else 1320 if branch=='assigned' else 1150
            assert cv==expected_cv and value==expected_value
            rows.append({'kind':'animation','branch':branch,'normal_light':normal,'converter':labels[cv],'intensity':value})
    return rows


def voxel_indices(c):
    n=Native(c);count=c.word(0xB2FB58);assert count==32
    cases=0
    # Numerical lookup slice only: no voxel geometry, surfaces or window code.
    for page in range(count):
        n.u.mem_write(0xB45990+7,bytes([page]))
        for color in range(256):
            n.u.reg_write(UC_X86_REG_ESP,0x70008000)
            for reg,value in [(UC_X86_REG_EDX,7),(UC_X86_REG_EBX,color),(UC_X86_REG_EDI,0),(UC_X86_REG_EBP,0)]:
                n.u.reg_write(reg,value)
            n.u.emu_start(0x7576B9,0x7576F8,count=100)
            want=c.read(0xB41178+page*256+color,1)
            assert bytes(n.u.mem_read(0xB2FF78,2))==want*2
            cases+=1
    return cases


def inspect_stage(path, stage):
    c = Capture(path, stage)
    n = Native(c)
    schemes = [s for s in c.vector(0xB054D0) if c.word(s + 0x310) > 1]
    regen = 0
    for s in schemes:
        cv = c.word(s + 0x30C)
        count, ptr = c.word(cv + 0x16C), c.word(cv + 0x170)
        assert c.word(cv + 4) == 2 and c.word(cv + 0x188) == s + 4
        original = c.read(ptr, count * 512)
        tinted = c.read(cv + 0x1B0, 1)[0]
        tint = struct.unpack('<iii', c.read(cv + (0x1A4 if tinted else 0x198), 12))
        n.u.mem_write(ptr, b'\xA5' * len(original))
        n.call(0x556090, cv, [*tint, tinted])
        assert bytes(n.u.mem_read(ptr, len(original))) == original
        regen += 1
    s = schemes[0]
    cv = c.word(s+0x30C)
    count = c.word(cv+0x16C)
    blitter = n.call(0x490E50, cv, [0x6E00])
    assert n.read(n.read(blitter)+4) == 0x4990E0
    table = struct.unpack('<'+'H'*(255*256), c.read(n.read(blitter+8), 255*512))
    lut = struct.unpack('<'+'H'*(count*256), c.read(c.word(cv+0x170), count*512))
    palette = [tuple(c.read(s+4+index*3, 3)) for index in range(256)]
    # Confirm the selector against the stored table for every legal lookup.
    selector_mismatches = sum(row != (min(count-1, a*shade*(count-1)//32258) << 8)
                              for shade in range(255) for a in range(256)
                              for row in [table[shade*256+a]])
    assert selector_mismatches == 0
    cases = 0
    for brightness in [0, 128, 256, 500, 1000, 1056, 1084, 1112, 1140, 1168, 1500, 2000]:
        shade = min(254, (261*brightness)>>11)
        for alpha in [0, 64, 127, 128, 254]:
            row = table[shade*256+alpha] >> 8
            for index in [16, 32, 48, 64, 128, 160, 240, 254]:
                assert n.pixel(blitter, brightness, alpha, index) == lut[row*256+index]
                cases += 1
    # Gather only numerical per-type inventory, without addresses/coordinates.
    objects = []
    mapitems = c.word(0x87F7E8+0x13C)
    for b in c.vector(0xA8EC78):
        vtable = c.word(b)
        if (vtable not in [0x7E3EBC, 0x7F5C70] or c.signed(b+0x6C) <= 0
            or not c.read(b+0x74,1)[0] or c.read(b+0x81,1)[0] or not c.read(b+0x90,1)[0]):
            continue
        t = c.word(b+(0x520 if vtable == 0x7E3EBC else 0x6C4))
        x,y,z = struct.unpack('<iii', c.read(b+0x9C, 12))
        cell = c.word(mapitems+4*((y//256)*512+x//256))
        intensity = struct.unpack('<h', c.read(cell+0x10A, 2))[0]
        extra = struct.unpack('<h', c.read(t+0x1548, 2))[0] if vtable == 0x7E3EBC else 0
        native_converter = n.call(0x705D70, b, [])
        scheme_index = c.signed(c.word(b+0x21C)+0x16054)
        vector = c.word(t+0xDF0) or 0xB054D0
        expected_scheme = c.vector(vector)[scheme_index]
        assert c.word(expected_scheme+0x30C) == native_converter
        objects.append({'type':c.name(t+0x24,24), 'kind':'building' if vtable==0x7E3EBC else 'vehicle',
                        'cell_intensity':intensity, 'extra_light':extra,
                        'flashing':bool(c.word(b+0xF0)&2),
                        'converter_shades':c.word(native_converter+0x16C),
                        'converter_rgb':list(struct.unpack('<iii',c.read(native_converter+0x198,12)))})
    distinct_intensities = sorted(set(o['cell_intensity']+o['extra_light'] for o in objects))
    ab = c.word(0x87E8A4)
    acount = Counter(struct.unpack('<'+'H'*(c.word(ab+0x20)//2), c.read(c.word(ab+0x18),c.word(ab+0x20))))
    return {
        'stage':stage, 'native_lut_regenerations':regen, 'lut_words_per_regeneration':count*256,
        'native_pixel_cases':cases, 'selector_cases':255*256, 'selector_mismatches':0,
        'native_format':'RGB565; comparisons expand with bit replication',
        'shades':count, 'neutral_row':(c.word(cv+0x174)-c.word(cv+0x170))//512,
        'unlit_indices':[i for i,v in enumerate(c.read(c.word(cv+0x190),256)) if v==0],
        'alpha_buffer_range':[min(acount),max(acount)],
        'alpha_buffer_neutral_127_count':acount[127], 'alpha_buffer_above_127_count':sum(v for k,v in acount.items() if k>127),
        'live_objects':objects,
        'palette_unweighted_deltas':differences(palette,lut,table,sorted(set([0,500,1000,1500,2000]+distinct_intensities))),
        'neutral_gray_example':{'raw':[120,120,120], 'current':list(current_rgb([120]*3,48,1000)),
                                'native_565_word':lut[26*256+48], 'native_rgb8':list(unpack565(lut[26*256+48]))},
        'controlled_source_selections':selections(c),
        'native_vpl_index_cases':voxel_indices(c),
        'code_hashes':{f'{a:08X}..{a+size:08X}':hashlib.sha256(c.read(a,size)).hexdigest()
                       for a,size in [(0x556090,0x46C),(0x4990E0,0x19F),(0x490E50,0x200),(0x705D70,0x90),
                                      (0x705EC1,0xC8),(0x70718F,0xD),(0x423114,0x244),(0x7576B9,0x3F)]},
    }


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('capture')
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    report={'supported_executable_sha256':SUPPORTED_SHA,
            'purpose':'Numerical read-only lighting audit; no production changes or screenshot claims',
            'stages':[inspect_stage(args.capture,s) for s in ['01-off','02-on','03-off']]}
    with open(args.output,'w') as out:
        json.dump(report,out,indent=2);out.write('\n')
    print(json.dumps({'stages':len(report['stages']),
                      'lut_regenerations':sum(s['native_lut_regenerations'] for s in report['stages']),
                      'native_pixel_cases':sum(s['native_pixel_cases'] for s in report['stages']),
                      'selector_cases':sum(s['selector_cases'] for s in report['stages']),
                      'controlled_source_selections':sum(len(s['controlled_source_selections']) for s in report['stages']),
                      'native_vpl_index_cases':sum(s['native_vpl_index_cases'] for s in report['stages']),
                      'status':'passed'}))

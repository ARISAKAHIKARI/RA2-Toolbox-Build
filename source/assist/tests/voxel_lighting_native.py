"""Replay native voxel lighting on a private RA2Master capture.

Only numerical lighting/projection results and hashes are exported. No game
assets, code bytes, dumps, Win32 display, game update or multiplayer are run.
Requires unicorn; the capture directory is never distributed with the tests.
"""
import argparse, hashlib, json, math, struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('capture', type=Path)
p.add_argument('--portable-pages', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
session = json.loads((a.capture/'session.json').read_text())
assert session['file_sha256'].lower() == '7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6'
image_base = int(session['image_base'], 16)
assert image_base == 0x400000
stages = []
ranges = [(0x887470, 12), (0x887430, 48), (0xB44318, 48), (0x887420, 12), (0x88746C, 4)]
for stage in ['00-clean', '01-off', '02-on', '03-off']:
    data = (a.capture/f'{stage}-game.mem').read_bytes()
    valid = (a.capture/f'{stage}-game.valid').read_bytes()
    stable = (a.capture/f'{stage}-game.stable').read_bytes()
    values = {}
    for addr, length in ranges:
        off = addr-image_base
        assert 0 <= off and off+length <= len(data)
        assert all(valid[off:off+length]) and all(stable[off:off+length])
        values[hex(addr)] = list(struct.unpack_from('<'+'f'*(length//4), data, off))
    stages.append({'stage': stage, 'all_bytes_valid_and_stable': True, 'values': values})
    if stage == '00-clean':
        code, code_valid, code_stable = data, valid, stable
assert all(row['values'] == stages[0]['values'] for row in stages)

u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(image_base, 0x800000)
u.mem_write(image_base, code)
u.mem_map(0x20000000, 0x10000)
lib, header, tail, rotation, draw = [0x20000000+x for x in [0, 0x100, 0x200, 0x400, 0x500]]
stack, stop = 0x2000F000, 0x2000FFF0
put = lambda addr, fmt, *v: u.mem_write(addr, struct.pack('<'+fmt, *v))
floats = lambda addr, n: struct.unpack('<'+'f'*n, u.mem_read(addr, n*4))
put(lib+16, 'I', header)
put(lib+20, 'I', tail)
put(header, 'I', 0)
cw = struct.unpack_from('<I', code, 0x822D80-image_base)[0]

def call(addr, ecx=0, edx=0, args=()):
    put(stack, 'I'*(len(args)+1), stop, *args)
    u.reg_write(UC_X86_REG_ECX, ecx)
    u.reg_write(UC_X86_REG_EDX, edx)
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_FPCW, cw)
    u.emu_start(addr, stop, count=100000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    assert u.reg_read(UC_X86_REG_ESP) == stack+4+4*len(args)

def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]

def stored(v):
    n = f32(v)
    if abs(n) > abs(v):
        bits = struct.unpack('<I', struct.pack('<f', n))[0]-1
        n = struct.unpack('<f', struct.pack('<I', bits))[0]
    return n

def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b)*16777619)&0xffffffff
    return f'{h:08x}'

def dot(a, b):
    return sum(x*y for x, y in zip(a, b))

def old_page(n, direction):
    heading = direction*(math.tau/32)
    l = [-f32(math.cos(heading)), -f32(math.sin(heading)), 0.]
    xy = f32(f32(n[0]*l[0])+f32(n[1]*l[1]))
    h = f32(f32(xy+n[2])*f32(.70710678118))
    spec = f32(h/f32(3.-f32(2.*h)))
    return int(max(0., min(255., f32(f32(max(0., xy)+max(0., spec))*16.))))

vectors = {}
def read_vectors(uc, addr, size, _):
    vectors['source'] = floats(uc.reg_read(UC_X86_REG_ECX), 3)
    vectors['secondary'] = floats(uc.reg_read(UC_X86_REG_EDX), 3)
u.hook_add(UC_HOOK_CODE, read_vectors, begin=0x7586F0, end=0x7586F0)
portable = a.portable_pages.read_bytes()
assert len(portable) == 4*32*256
rows, all_native, comparisons = [], bytearray(), []
old_wrong = portable_wrong = compared = special = 0
projection_rows = []
projection_anchors = []
view = stages[0]['values']['0xb44318']
points = [(7,2,3), (-5,11,1), (13,-4,9), (-8,-3,-2), (2,7,15)]
hvas = {
    'body': (1,0,0,0,0,1,0,0,0,0,1,0),
    'turret_hva': (0,-1,0,3,1,0,0,5,0,0,1,2),
    'barrel_hva': (.8,0,.6,7,0,1,0,-3,-.6,0,.8,4)
}
temp_mat, temp_point, temp_out = 0x20000800, 0x20000900, 0x20000A00
def apply_native(matrix, point):
    put(temp_mat, '12f', *matrix)
    put(temp_point, '3f', *point)
    call(0x5AFB80, temp_out, temp_mat, (temp_point,))
    return floats(temp_out, 3)
for mode in range(1, 5):
    count = struct.unpack_from('<I', code, 0x8469F4+mode*4-image_base)[0]
    normal_addr = struct.unpack_from('<I', code, 0x8469E0+mode*4-image_base)[0]
    normals = [struct.unpack_from('<3f', code, normal_addr+12*i-image_base) for i in range(count)]
    hashes = []
    put(tail+0xA3, 'B', mode)
    for facing in range(32):
        angle = (facing-8)*(math.tau/32)
        put(rotation, '12f', 1,0,0,0,0,1,0,0,0,0,1,0)
        call(0x5AF1A0, rotation, args=(bits(stored(-angle)),))
        call(0x5AF980, draw, 0xB44318, (rotation,))
        u.mem_write(0xB45990, bytes(256))
        call(0x753D00, lib, 0, (0, draw, 0x887430, 0x887470, bits(3.)))
        native = bytes(u.mem_read(0xB45990, 256))
        all_native.extend(native)
        hashes.append(fnv(native))
        ours = portable[((mode-1)*32+facing)*256:((mode-1)*32+facing+1)*256]
        bad = []
        for i, normal in enumerate(normals):
            previous_normal = tuple(-x if mode == 4 and i == 241 and j == 0 else x for j,x in enumerate(normal))
            previous = old_page(previous_normal, facing)
            old_wrong += previous != native[i]
            portable_wrong += ours[i] != native[i]
            compared += 1
            if ours[i] != native[i]: bad.append({'normal': i, 'native': native[i], 'portable': ours[i]})
        assert native[253:256] == bytes([16,16,16]) == ours[253:256]
        special += 3
        if bad: comparisons.append({'mode': mode, 'facing': facing, 'mismatches': bad})
        if mode == 4:
            rows.append({'facing': facing, 'source_model': vectors['source'], 'secondary_model': vectors['secondary']})
            m = floats(draw, 12)
            body_facing = (facing+5)&31
            body_angle = (body_facing-8)*(math.tau/32)
            put(rotation, '12f', 1,0,0,0,0,1,0,0,0,0,1,0)
            call(0x5AF1A0, rotation, args=(bits(stored(-body_angle)),))
            call(0x5AF980, draw, 0xB44318, (rotation,))
            body_m = floats(draw,12)
            for label,hva in hvas.items():
                for original in points:
                    q=apply_native(hva,original)
                    for offset in ([0] if label=='body' else [0,7]):
                        native_p=list(apply_native(m,q));pivot=apply_native(body_m,(offset,0,0))
                        native_p=[v+d for v,d in zip(native_p,pivot)];native_p[1]*=-1
                        def project(mirror):
                            x,y,z=q
                            if mirror:y=-y
                            x,y=x*math.cos(angle)-y*math.sin(angle),x*math.sin(angle)+y*math.cos(angle)
                            x+=offset*math.cos(body_angle);y+=offset*math.sin(body_angle)
                            return [(x-y)/math.sqrt(2),(x+y)/math.sqrt(8)-z*math.sqrt(3)/2,(x+y)*math.sqrt(3/8)+z*.5]
                        old,new=project(False),project(True)
                        projection_rows.append({'facing':facing,'hva':label,'body_facing':body_facing,'offset':offset,'before_xy_error':math.hypot(old[0]-native_p[0],old[1]-native_p[1]),'after_xy_error':math.hypot(new[0]-native_p[0],new[1]-native_p[1]),'after_depth_error':abs(new[2]-native_p[2])})
                        if facing in [0,8,16,24] and original==points[0]:
                            projection_anchors.append({'facing':facing,'hva':label,'body_facing':body_facing,'offset':offset,'local':original,'transformed':q,'native_screen':native_p})
    rows.append({'mode': mode, 'normal_count': count, 'native_page_fnv1a32': hashes})

code_hashes = {}
for start,end in [(0x754C00,0x754CA7),(0x753D00,0x753DF4),(0x7586F0,0x758877),(0x5AF1A0,0x5AF23A),(0x5AF980,0x5AFB80),(0x4CAC40,0x4CAD49)]:
    lo,hi = start-image_base,end-image_base
    assert all(code_valid[lo:hi]) and all(code_stable[lo:hi])
    code_hashes[hex(start)] = {'length':hi-lo,'sha256':hashlib.sha256(code[lo:hi]).hexdigest()}
# Facing 8 corresponds to the identity model rotation.
world = next(row['source_model'] for row in rows if row.get('facing') == 8)
world_preview = [world[0],-world[1],world[2]]
screen = [(world_preview[0]-world_preview[1])/math.sqrt(2),(world_preview[0]+world_preview[1])/math.sqrt(8)-world_preview[2]*math.sqrt(3)/2]
summary = {
    'scope': __doc__.strip(), 'game_exe_sha256': session['file_sha256'].lower(),
    'code_hashes': code_hashes, 'capture_defaults': stages,
    'native_world_toward_light': world, 'preview_world_toward_light': world_preview,
    'screen_toward_light': screen, 'screen_incident_ray': [-x for x in screen],
    'signed_world_elevation_degrees': math.degrees(math.atan2(world[2], math.hypot(world[0],world[1]))),
    'normal_page_comparisons': compared, 'old_page_mismatches': old_wrong,
    'new_page_mismatches': portable_wrong, 'special_page_comparisons': special,
    'projection_point_comparisons':len(projection_rows),
    'projection_before_max_xy_error':max(r['before_xy_error'] for r in projection_rows),
    'projection_after_max_xy_error':max(r['after_xy_error'] for r in projection_rows),
    'projection_after_max_depth_error':max(r['after_depth_error'] for r in projection_rows),
    'native_projection_anchors': projection_anchors,
    'mode4_normal241': {'old':[.328188,.140251,.93414301],'native':list(struct.unpack_from('<3f',code,0x846F78+241*12-image_base))},
    'native_page_sha256':hashlib.sha256(all_native).hexdigest(),
    'native_vectors_and_hashes': rows, 'mismatches': comparisons,
    'limitations':['Controlled native numerical replay; no real-game screenshot comparison.', 'Original YR defaults only; custom or dynamically changed Phobos lighting is not sampled.', 'The camera projection remains unchanged; its ideal trigonometry differs slightly from the native lookup table.', 'SHP sprites have baked lighting; these directional calculations apply to voxel rendering only.']
}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k.endswith('comparisons') or k.endswith('mismatches') or 'max_' in k},indent=2))
assert portable_wrong == 0, f'{portable_wrong}/{compared} VPL pages differ from native'

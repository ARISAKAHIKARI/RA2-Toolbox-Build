"""Read-only PE replay of native SetVisibleRect; never attaches to the game.

Only redraw notification is mocked; rectangle intersection, Cell iteration and
usable predicates execute the supplied PE's original bytes. No PE/raw dump is
included in the report. Synthetic Cells and reported field values are public.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP


def main():
    p = argparse.ArgumentParser()
    p.add_argument('game', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cpp-exe', type=Path)
    args = p.parse_args()
    data = args.game.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    assert sha == '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
    pe = pefile.PE(data=data)
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    u.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, pe.get_memory_mapped_image())
    arena, cells, objects, rect = 0x10000000, 0x10100000, 0x10300000, 0x10001000
    u.mem_map(arena, 64 * 1024 * 1024)
    stop, stack, game_map = 0x20000000, 0x21000000, 0x87F7E8
    u.mem_map(stop, 4096); u.mem_write(stop, b'\x90')
    u.mem_map(stack, 1024 * 1024)
    def w(at, value): u.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def r(at): return struct.unpack('<i', u.mem_read(at, 4))[0]
    def intercepted(uc, at, size, context):
        if at == stop: uc.emu_stop()
        if at == 0x4F42F0:
            sp = uc.reg_read(UC_X86_REG_ESP)
            assert r(sp + 4) == 2
            target = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, sp + 8)
            uc.reg_write(UC_X86_REG_EIP, target)
    u.hook_add(UC_HOOK_CODE, intercepted)
    snapshots = []
    for width, height, raised in [(64, 64, False), (64, 72, True), (128, 96, True)]:
        u.mem_write(game_map, bytes(0x1600));u.mem_write(cells, bytes(512 * 512 * 4))
        w(game_map + 0xF4, width);w(game_map + 0xF8, height);w(game_map + 0x13C, cells);w(game_map + 0x140, 512*512);w(0xA8EC88, 0)
        physical = []
        for y in range(1, 512):
            for x in range(1, 512):
                if not (x+y>width and x-y<width and y-x<width and x+y<=width+2*height): continue
                at = objects + len(physical) * 0x150
                level = (x*13+y*7) % 7 if raised else 0
                slope = int(raised and (x+y) % 5 == 0)
                physical.append((x, y, level, slope))
                w(cells + (y*512+x)*4, at)
                u.mem_write(at+0x24, struct.pack('<hh', x, y));u.mem_write(at+0x11B, bytes([level, slope]))
        requests = [(0,0,width,height), (12,18,24,20), (12,24,24,14), (6,8,width-14,height-18), (0,0,width,height)]
        initial_backing = None
        for requested in requests:
            u.mem_write(rect, struct.pack('<4i', *requested))
            sp = stack + 1024*1024 - 16
            u.mem_write(sp, struct.pack('<II', stop, rect));u.reg_write(UC_X86_REG_ECX, game_map);u.reg_write(UC_X86_REG_ESP, sp)
            u.emu_start(0x654490, stop, count=10000000)
            assert u.reg_read(UC_X86_REG_EIP) == stop and u.reg_read(UC_X86_REG_ESP) == sp+8
            live = [r(game_map + at) for at in (0xFC,0x100,0x104,0x108)]
            native = [r(game_map + at) for at in (0x1490,0x1498,0x14A4,0x14A8)]
            left, top, columns, rows = live
            accepted = []
            for x, y, level, slope in physical:
                if slope and x+y<width+2*top+4+level: level += 1
                if x+y>width+2*top+level and x+y<=width+2*(top+rows)+2+level and x-y<2*(left+columns)-width and y-x<width-2*left: accepted.append((x,y))
            ox = width-2*left-1;oy = min(x+y for x,y in accepted)
            xs = [(max(0,x-y+ox),1 if x-y+ox in (-1,2*columns-1) else 2) for x,y in accepted]
            derived = [ox, oy, max(x+n for x,n in xs)-min(x for x,n in xs), max(x+y for x,y in accepted)-oy+1]
            assert native == derived, (live,native,derived)
            if initial_backing is None:
                initial_backing = native[2:]
                w(game_map+0x1240,initial_backing[0]);w(game_map+0x1244,initial_backing[1])
            backing = [r(game_map+0x1240),r(game_map+0x1244)]
            assert backing == initial_backing
            snapshots.append({'map':[width,height], 'raised':raised, 'request':requested, 'visible_rect':live,
                              'active_projection':native, 'allocated_backing':backing, 'cpp_formula_candidate':derived,
                              'same_backing_different_active_span':backing != native[2:]})
    cpp = None
    if args.cpp_exe:
        rows = [[*s['map'],int(s['raised']),*s['visible_rect'],*s['allocated_backing'],*s['active_projection']] for s in snapshots]
        run = subprocess.run([str(args.cpp_exe.resolve()), '--query'], input=''.join(' '.join(map(str,row))+'\n' for row in rows), capture_output=True, text=True, timeout=120)
        assert run.returncode == 0, run.stdout + run.stderr
        actual = [list(map(int,line.split())) for line in run.stdout.splitlines()]
        assert actual == [s['active_projection'] for s in snapshots], (actual,snapshots)
        cpp = {'cases':len(actual),'executable_sha256':hashlib.sha256(args.cpp_exe.read_bytes()).hexdigest(),'output':run.stdout}
    report = {'passed':True,'game_sha256':sha,'entry':'0x654490','native_dependencies':['0x567230','0x421B60','0x578350','0x578290','0x578460'],
              'mocked_boundaries':['0x4F42F0 redraw notification; empty object vector'], 'game_process_touched':False,
              'snapshots':snapshots,'cpp_comparison':cpp,'original_code_sha256':{hex(at):hashlib.sha256(pe.get_data(at-base,size)).hexdigest() for at,size in [(0x654490,0x1B7),(0x567230,0x139),(0x578460,0xD5)]}}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'passed':True,'cases':len(snapshots),'stale_backing_cases':sum(x['same_backing_different_active_span'] for x in snapshots),'output':str(args.output)}))


if __name__ == '__main__': main()

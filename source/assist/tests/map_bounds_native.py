"""Compare the portable boundary helper with captured original x86 functions.

Requires a private capture; publishes only counts, addresses and code hashes.
No game pixels, dumped bytes or executable are included in its JSON output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile
from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP


def main():
    p = argparse.ArgumentParser()
    p.add_argument("capture", type=Path)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    capture = Capture(args.capture, "02-on")
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    hashes = {}
    for address, size in ((0x568300, 0x45), (0x578540, 0xB0)):
        code = capture.read(address, size)
        u.mem_map(address & ~4095, 4096)
        u.mem_write(address, code)
        hashes[hex(address)] = hashlib.sha256(code).hexdigest()
    base, cell, stack, stop = 0x2000000, 0x2010000, 0x2020000, 0x2030000
    for address in (base, cell, stack, stop):
        u.mem_map(address, 4096)
    u.mem_write(stop, b"\x90")

    def native(address, bounds, x, y, level, slope):
        u.mem_write(base + 0xF4, struct.pack("<6i", *bounds))
        u.mem_write(cell + 0x24, struct.pack("<hh", x, y))
        u.mem_write(cell + 0x11B, struct.pack("<bB", level, slope))
        pointer = cell + 0x24 if address == 0x568300 else cell
        u.mem_write(stack + 2048, struct.pack("<III", stop, pointer, 1))
        u.reg_write(UC_X86_REG_ESP, stack + 2048)
        u.reg_write(UC_X86_REG_ECX, base)
        u.emu_start(address, stop, count=200)
        return bool(u.reg_read(UC_X86_REG_EAX) & 255)

    rng = random.Random(1385)
    cases = []
    for bounds in [(131,146,3,5,124,135), (64,64,2,2,60,56),
                   (64,64,12,15,24,20), (32,64,0,0,32,64)]:
        width, height, left, top, columns, rows = bounds
        points = {(rng.randrange(1, width+height+3), rng.randrange(1, width+height+3)) for _ in range(300)}
        # Include every inequality's exact edge and both adjacent cells.
        for y in range(1, width+height+2, 7):
            for level in (-1,0,1,4,14):
                for edge in (width+2*top+level-y, width+2*(top+rows)+2+level-y,
                             y+2*left-width, y+2*(left+columns)-width):
                    for dx in (-1,0,1):
                        if 0 <= edge+dx < 512:
                            points.add((edge+dx,y))
        for x,y in sorted(points):
            for level in (-1,0,1,4,14):
                for slope in (0,1):
                    legal = native(0x568300,bounds,x,y,level,slope)
                    usable = legal and native(0x578540,bounds,x,y,level,slope)
                    cases.append(((*bounds,x,y,level,slope), (int(legal),int(usable))))
    with tempfile.TemporaryDirectory(prefix="ra2-map-bounds-") as temp:
        exe = Path(temp)/"map_bounds"
        subprocess.run(["g++","-std=c++17","-O2",str(Path(__file__).with_name("map_bounds.cpp")),"-o",str(exe)], check=True)
        result = subprocess.run([str(exe),"--native-cases"], input="".join(" ".join(map(str,c))+"\n" for c,_ in cases), text=True, capture_output=True, check=True)
    actual = [tuple(map(int,row.split())) for row in result.stdout.splitlines()]
    assert len(actual) == len(cases)
    mismatches = [list(case) for (case,expected),got in zip(cases,actual) if expected != got]
    assert not mismatches, mismatches[:10]
    report = {"supported_exe_sha256":SUPPORTED_SHA.lower(),"native_code_sha256":hashes,
              "cases":len(cases),"mismatches":len(mismatches),
              "evidence":"original native CoordinatesLegal and IsWithinUsableArea machine code; controlled memory",
              "game_tested":False,"multiplayer_tested":False}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

"""Validate building component selection against original captured x86 code.

Accepts a private RA2Master capture ZIP or its extracted session directory. The
capture is never included in this repository. Only sanitized counters, type
names, frame decisions and code hashes are written. No game update, disk loader,
Win32 display or rasterizer is executed. Controlled cases run unmodified native
selection/frame instruction slices with map/height/draw boundaries intercepted.
"""
from pathlib import Path
from bisect import bisect_right
from collections import Counter
import argparse
import hashlib
import json
import struct
import zipfile

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESI,
    UC_X86_REG_ESP, UC_X86_REG_FPCW,
)

SUPPORTED_SHA = "7B8A068535D6AF06845EDF95AE829B113D00C02909330E16F197426CD7DB94B6"
PARTS = {
    "deploying": 0x14E4, "under_door": 0x14EC,
    "roof_deploying": 0x14FC, "under_roof_door": 0x1504,
    "door": 0x150C, "bib": 0x1518,
}
TARGET_TYPES = {"GAYARD", "NAYARD", "YAYARD", "GACNST", "NACNST", "YACNST"}
SLOT_NAMES = {3: "Active", 4: "ActiveTwo", 5: "ActiveThree", 8: "Production",
              10: "Special", 11: "SpecialTwo", 12: "SpecialThree", 18: "Idle"}


class Capture:
    """Validity-checked input shared by native building component probes."""
    def __init__(self, path, stage):
        self.archive = zipfile.ZipFile(path) if Path(path).is_file() else None
        if self.archive:
            name = next(n for n in self.archive.namelist() if n.endswith("/session.json"))
            self.base = name[:-len("session.json")]
        else:
            self.base = Path(path)
        self.session = json.loads(self.file("session.json"))
        assert self.session["file_sha256"].upper() == SUPPORTED_SHA, "unsupported executable"
        self.stage = stage
        records = [{"base": self.session["image_base"], "file": stage + "-game.mem",
                    "valid": stage + "-game.valid"}]
        records += json.loads(self.file(stage + "-private-regions.json"))
        self.regions = {}
        for r in records:
            data = self.file(r["file"])
            valid = self.file(r.get("valid", r["file"] + ".valid"))
            assert len(data) == len(valid)
            self.regions[int(r["base"], 16)] = (data, valid)
        self.starts = sorted(self.regions)

    def file(self, name):
        if self.archive:
            return self.archive.read(self.base + name)
        return (self.base / name).read_bytes()

    def read(self, address, size):
        result = bytearray()
        while size:
            i = bisect_right(self.starts, address) - 1
            assert i >= 0, "unmapped capture address"
            base = self.starts[i]
            data, valid = self.regions[base]
            offset = address - base
            count = min(size, len(data) - offset)
            assert count > 0 and 0 not in valid[offset:offset + count], "invalid capture bytes"
            result += data[offset:offset + count]
            address += count
            size -= count
        return bytes(result)

    def word(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]

    def signed(self, address):
        return struct.unpack("<i", self.read(address, 4))[0]

    def name(self, address, limit=32):
        return self.read(address, limit).split(b"\0", 1)[0].decode("ascii", "replace")

    def vector(self, address):
        count = self.signed(address + 16)
        assert 0 <= count <= 65536
        items = self.word(address + 4)
        return [self.word(items + 4 * i) for i in range(count)]

    def shape(self, pointer):
        if not pointer:
            return {"status": "absent"}
        try:
            tag, width, height, frames = struct.unpack("<HHHH", self.read(pointer, 8))
            name = ""
            if tag == 65535:
                filename = self.word(pointer + 8)
                if filename:
                    name = self.name(filename, 64)
                decoded = self.word(pointer + 12)
                if not decoded:
                    return {"status": "unloaded_reference", "filename": name}
                tag, width, height, frames = struct.unpack("<HHHH", self.read(decoded, 8))
            assert tag == 0 and 0 < frames <= 8192 and 0 < width <= 2048 and 0 < height <= 2048
            return {"status": "loaded", "filename": name, "frames": frames}
        except AssertionError:
            return {"status": "unavailable_capture_bytes"}

    def emulator(self):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        for base in self.starts:
            data, _ = self.regions[base]
            start = base & ~4095
            end = (base + len(data) + 4095) & ~4095
            u.mem_map(start, end - start)
            u.mem_write(base, data)
        return u


def inventory(capture):
    buildings = []
    types = {}
    for b in capture.vector(0xA8EB40):
        if capture.word(b) != 0x7E3EBC or capture.signed(b + 0x6C) <= 0:
            continue
        t = capture.word(b + 0x520)
        name = capture.name(t + 0x24, 24)
        buildings.append({"type": name, "mission": capture.signed(b + 0xAC),
                          "bstate": capture.signed(b + 0x534),
                          "timer_states": list(capture.read(b + 0x368, 2))})
        if name not in types:
            parts = {key: capture.shape(capture.word(t + offset)) for key, offset in PARTS.items()}
            types[name] = {"parts": parts, "door_stages": capture.signed(t + 0xF00)}
    counts = {key: dict(Counter(v["parts"][key]["status"] for v in types.values())) for key in PARTS}
    active = {name: value for name, value in sorted(types.items())
              if any(p["status"] != "absent" for p in value["parts"].values())}
    return {"live_buildings": len(buildings), "unique_types": len(types),
            "component_status_by_type": counts, "component_types": active,
            "unload_buildings": sum(b["mission"] == 16 for b in buildings)}


def target_inventory(capture):
    """Configuration is observable even when no live shipyard exists."""
    result = {}
    for t in capture.vector(0xA83C68):
        name = capture.name(t + 0x24, 24)
        if name not in TARGET_TYPES:
            continue
        slots = []
        for i in range(21):
            a = t + 0xF4C + i * 68
            names = [capture.name(a + offset, 16) for offset in [0, 16, 32]]
            if any(names):
                slots.append({"index": i, "role": SLOT_NAMES.get(i, str(i)), "names": names,
                              "position": list(struct.unpack("<ii", capture.read(a + 48, 8))),
                              "z_adjust": capture.signed(a + 56), "y_sort": capture.signed(a + 60)})
        result[name] = {"body": capture.shape(capture.word(t + 0xA4)),
                        "buildup": capture.shape(capture.word(t + 0xE00)),
                        "buildup_name": capture.name(t + 0xE5C, 16),
                        "buildup_frames": capture.signed(t + 0xF08),
                        "components": {key: capture.shape(capture.word(t + offset)) for key, offset in PARTS.items()},
                        "animation_slots": slots}
    return result


class Probe:
    STACK = 0x70000000
    FIXTURE = 0x71000000
    STOP = 0x72000000
    B = FIXTURE
    T = FIXTURE + 0x1000
    CELL = FIXTURE + 0x4000
    SHAPES = {name: 0x71005000 + i * 0x100 for i, name in enumerate(PARTS)}
    BODY = FIXTURE + 0x6000
    POINT = FIXTURE + 0x7000
    RECT = FIXTURE + 0x7100

    def __init__(self, capture):
        self.capture = capture
        self.u = capture.emulator()
        self.u.mem_map(self.STACK, 0x10000)
        self.u.mem_map(self.FIXTURE, 0x10000)
        self.u.mem_map(self.STOP, 0x1000)
        first = next(b for b in capture.vector(0xA8EB40)
                     if capture.word(b) == 0x7E3EBC and capture.signed(b + 0x6C) > 0)
        self.u.mem_write(self.B, capture.read(first, 0x700))
        self.u.mem_write(self.T, capture.read(capture.word(first + 0x520), 0x1800))
        self.write(self.B + 0x520, self.T)
        self.write(self.B + 0x534, 1)
        self.write(self.B + 0xF8, 0)
        self.write(self.B + 0x6C, 100)
        self.write(self.T + 0xA0, 100)
        self.write(self.B + 0xAC, 16)
        self.write(self.T + 0xF00, 8)
        for off in [0x157B, 0x16B7, 0x16BF, 0x16C0]:
            self.u.mem_write(self.T + off, b"\0")
        for name, address in self.SHAPES.items():
            self.write(self.T + PARTS[name], address)
            self.u.mem_write(address, struct.pack("<HHHH", 0, 100, 100, 32))
        self.u.mem_write(self.BODY, struct.pack("<HHHH", 0, 100, 100, 32))
        self.u.mem_write(self.CELL + 0x10A, struct.pack("<h", 1000))
        self.u.mem_write(self.POINT, struct.pack("<ii", 160, 160))
        self.u.mem_write(self.RECT, struct.pack("<iiii", 0, 0, 640, 480))
        self.height_entry = self.read(self.read(self.B) + 0x1D0)
        self.draws = []
        self.stubs = {0x5657A0: (self.CELL, 4), self.height_entry: (0, 0)}
        self.hook = self.u.hook_add(UC_HOOK_CODE, self.boundary)

    def read(self, address):
        return struct.unpack("<I", self.u.mem_read(address, 4))[0]

    def write(self, address, value):
        self.u.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def ret(self, value, size):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        dest = self.read(sp)
        self.u.reg_write(UC_X86_REG_EAX, value)
        self.u.reg_write(UC_X86_REG_ESP, sp + 4 + size)
        self.u.reg_write(UC_X86_REG_EIP, dest)

    def boundary(self, u, address, size, data):
        if address in self.stubs:
            self.ret(*self.stubs[address])
        elif address == 0x705E00:
            sp = u.reg_read(UC_X86_REG_ESP)
            args = [self.read(sp + 4 + 4 * i) for i in range(16)]
            self.draws.append(args)
            self.ret(0, 64)

    def reset(self):
        self.sp = self.STACK + 0x8000
        self.u.mem_write(self.sp - 0x100, bytes(0x1000))
        self.u.reg_write(UC_X86_REG_ESP, self.sp)
        self.u.reg_write(UC_X86_REG_ESI, self.B)
        self.u.reg_write(UC_X86_REG_EBP, self.RECT)
        self.u.reg_write(UC_X86_REG_EBX, self.POINT)
        self.u.reg_write(UC_X86_REG_EDI, 0)
        self.u.reg_write(UC_X86_REG_FPCW, self.read(0x822D80) & 65535)
        self.draws = []

    def execute(self, begin, end):
        self.u.emu_start(begin, end, count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP) == end, "native instruction budget exceeded"

    def execute_to_one(self, begin, ends):
        def stop(u, address, size, data):
            if address in ends:
                u.emu_stop()
        handle = self.u.hook_add(UC_HOOK_CODE, stop)
        try:
            self.u.emu_start(begin, self.STOP, count=100000)
            result = self.u.reg_read(UC_X86_REG_EIP)
            assert result in ends, "native branch did not reach an expected boundary"
            return result
        finally:
            self.u.hook_del(handle)

    def function(self, address, obj):
        self.reset()
        self.write(self.sp, self.STOP)
        self.u.reg_write(UC_X86_REG_ECX, obj)
        self.execute(address, self.STOP)
        assert self.u.reg_read(UC_X86_REG_ESP) == self.sp + 4
        return self.u.reg_read(UC_X86_REG_EAX)


def controlled(capture):
    p = Probe(capture)
    rules = p.read(0x8871E0)
    threshold = struct.unpack("<d", p.u.mem_read(rules + 0x1700, 8))[0]
    p.u.mem_write(rules + 0x1700, struct.pack("<d", 0.5))
    results = {"captured_condition_yellow": threshold}

    # Original bib branch, original GetCurrentFrame and native draw ABI. Only
    # cell lookup, height query and the renderer boundary are intercepted.
    cases = []
    for state in [0, 1, 2]:
        for present in [False, True]:
            for health in [100, 50]:
                p.write(p.B + 0x534, state)
                p.write(p.T + PARTS["bib"], p.SHAPES["bib"] if present else 0)
                p.write(p.B + 0x6C, health)
                expected = p.function(0x43EF90, p.B)
                p.reset()
                p.execute(0x43D864, 0x43D8EE)
                assert bool(p.draws) == (present and state != 0)
                if p.draws:
                    args = p.draws[0]
                    assert len(p.draws) == 1 and args[0] == p.SHAPES["bib"] and args[1] == expected
                    assert args[6] == 0xFFFFFFFF and args[7:9] == [0, 1] and args[11] == 0
                cases.append({"state": state, "present": present, "health": health,
                              "draw": bool(p.draws), "frame": expected if p.draws else None})
    results["bib_cases"] = cases
    p.write(p.T + PARTS["bib"], p.SHAPES["bib"])
    p.write(p.B + 0x534, 1)

    # Body replacement branch before projection/setup. Jumpjet/balloon selector
    # is separately established by native 0x43D33A-0x43D386; this slice consumes
    # the selected flag without inventing a production tick.
    cases = []
    for mission in [1, 16, 18, 19]:
        for roof in [0, 1]:
            for present in [False, True]:
                part = "roof_deploying" if roof else "deploying"
                p.write(p.B + 0xAC, mission)
                p.write(p.T + PARTS[part], p.SHAPES[part] if present else 0)
                p.reset()
                p.write(p.sp + 0x14, p.BODY)
                p.write(p.sp + 0x1C, -7)
                p.u.mem_write(p.sp + 0x13, bytes([roof]))
                p.execute(0x43D692, 0x43D6DB)
                selected = p.read(p.sp + 0x14)
                adjust = struct.unpack("<i", p.u.mem_read(p.sp + 0x1C, 4))[0]
                active = mission == 16 and present
                assert selected == (p.SHAPES[part] if active else p.BODY)
                assert adjust == ((-40 if roof else -20) if active else -7)
                cases.append({"mission": mission, "roof": bool(roof), "present": present,
                              "replaced": active, "z_adjust": adjust})
                p.write(p.T + PARTS[part], p.SHAPES[part])
    results["body_selection_cases"] = cases

    # Main draw's roof decision reads RadioLinks[0], then JumpJet/BalloonHover
    # from that linked object's GetTechnoType result. A controlled linked object
    # uses the same captured native type accessor; only these two flags differ.
    linked, linked_type, links = p.FIXTURE + 0xA000, p.FIXTURE + 0xB000, p.FIXTURE + 0x9000
    p.u.mem_write(linked, bytes(p.u.mem_read(p.B, 0x700)))
    p.write(linked + 0x520, linked_type)
    p.write(p.B + 0xE4, links)
    cases = []
    for mission in [1, 16]:
        for present in [False, True]:
            for jumpjet, balloon in [(0, 0), (1, 0), (0, 1), (1, 1)]:
                p.write(p.B + 0xAC, mission)
                p.write(links, linked if present else 0)
                p.u.mem_write(linked_type + 0xD94, bytes([jumpjet]))
                p.u.mem_write(linked_type + 0xD6A, bytes([balloon]))
                p.reset()
                p.u.reg_write(UC_X86_REG_EDX, p.read(p.B))
                p.u.reg_write(UC_X86_REG_ECX, p.B)
                p.execute(0x43D33A, 0x43D386)
                roof = p.u.mem_read(p.sp + 0x13, 1)[0]
                assert roof == int(mission == 16 and present and (jumpjet or balloon))
                cases.append({"mission": mission, "linked": present, "jumpjet": bool(jumpjet),
                              "balloon_hover": bool(balloon), "roof": bool(roof)})
    results["roof_selection_cases"] = cases

    # The native body's upper clamp is half, not half-1. Do not silently replace
    # it with the door frame clamp (which does use stages-1).
    cases = []
    p.write(p.B + 0x534, 1)
    p.write(p.B + 0x6C, 100)
    for frame_count in [31, 32]:
        for frame in [0, 1, 15, 16, 20]:
            p.u.mem_write(p.BODY + 6, struct.pack("<H", frame_count))
            p.write(p.B + 0xF8, frame)
            p.reset()
            p.write(p.sp + 0x14, p.BODY)
            p.execute(0x43D7A7, 0x43D7F0)
            selected = p.u.reg_read(UC_X86_REG_EAX)
            assert selected == min(frame, frame_count // 2)
            cases.append({"shape_frames": frame_count, "current_frame": frame, "draw_frame": selected})
    results["body_frame_cases"] = cases
    p.write(p.B + 0xF8, 0)

    # Both underlays select damage frame 0/1 and Ground/writeZ, no independent
    # animation timer. The renderer call itself is only recorded, not executed.
    cases = []
    p.write(p.B + 0xAC, 16)
    for roof in [0, 1]:
        for health in [100, 51, 50, 1]:
            p.write(p.B + 0x6C, health)
            p.reset()
            p.u.mem_write(p.sp + 0x13, bytes([roof]))
            begin, end = (0x43D9D8, 0x43DA6C) if roof else (0x43D93B, 0x43D9CE)
            p.execute(begin, end)
            part = "under_roof_door" if roof else "under_door"
            assert len(p.draws) == 1
            args = p.draws[0]
            assert args[0] == p.SHAPES[part] and args[1] == int(health <= 50)
            assert args[6:9] == [0, 0, 1] and args[11] == 0
            cases.append({"roof": bool(roof), "health": health, "frame": args[1]})
    results["underlay_cases"] = cases

    cases = []
    for mission in [1, 16, 18, 19]:
        for roof in [0, 1]:
            for present in [False, True]:
                part = "under_roof_door" if roof else "under_door"
                p.write(p.B + 0xAC, mission)
                p.write(p.T + PARTS[part], p.SHAPES[part] if present else 0)
                p.reset()
                p.u.mem_write(p.sp + 0x13, bytes([roof]))
                p.execute_to_one(0x43D8EE, {0x43D91D, 0x43D9CE, 0x43DA6C})
                assert bool(p.draws) == (mission == 16 and present)
                cases.append({"mission": mission, "roof": bool(roof), "present": present,
                              "draw": bool(p.draws)})
                p.write(p.T + PARTS[part], p.SHAPES[part])
    results["underlay_gate_cases"] = cases

    # Full native door frame logic including unmodified TransitionTimer helpers,
    # x87 PercentageDone/Ftol, health ratio and damaged-half selection.
    cases = []
    for states in [(0, 0), (0, 1), (1, 0), (1, 1)]:
        for elapsed in [0, 1, 7, 8, 15, 16, 24]:
            for damaged in [0, 1]:
                p.write(0xA8ED84, 100 + elapsed)
                p.write(p.B + 0x358, 100)
                p.write(p.B + 0x360, 16)
                p.write(p.B + 0x364, 16)
                p.u.mem_write(p.B + 0x368, bytes(states))
                p.u.mem_write(p.T + 0x1700, bytes([damaged]))
                p.write(p.B + 0x6C, 50)
                p.reset()
                p.u.reg_write(UC_X86_REG_EBP, p.B)
                p.execute(0x43DE19, 0x43DED9)
                frame = p.u.reg_read(UC_X86_REG_EDI)
                progress = min(elapsed, 16) / 16 if states[0] else 1
                expected = int(progress * 8)
                if states == (1, 0):
                    expected = 8 - expected
                if states == (0, 0):
                    expected = 0
                expected = max(0, min(expected, 7)) + damaged * 8
                assert frame == expected, (states, elapsed, damaged, frame, expected)
                cases.append({"states": list(states), "elapsed": elapsed,
                              "damaged_door": bool(damaged), "frame": frame})
    results["door_frame_cases"] = cases

    cases = []
    for states, start, left, total, current in [
            ((1, 1), -1, 12, 16, 200), ((1, 0), -1, 12, 16, 200),
            ((1, 1), 100, 0, 0, 100), ((1, 0), 100, 0, 0, 100),
            ((1, 1), 100, 10, 16, 103), ((1, 0), 100, 10, 16, 103),
            ((1, 1), 100, 16, 16, 99), ((1, 0), 100, 16, 16, 99)]:
        for health in [100, 51, 50, 1]:
            p.write(0xA8ED84, current)
            p.write(p.B + 0x358, start)
            p.write(p.B + 0x360, left)
            p.write(p.B + 0x364, total)
            p.u.mem_write(p.B + 0x368, bytes(states))
            p.u.mem_write(p.T + 0x1700, b"\1")
            p.write(p.B + 0x6C, health)
            p.reset()
            p.u.reg_write(UC_X86_REG_EBP, p.B)
            p.execute(0x43DE19, 0x43DED9)
            frame = p.u.reg_read(UC_X86_REG_EDI)
            left_now = left if start == -1 else max(0, left - (current - start))
            progress = (total - left_now) / total if total else 1
            expected = int(progress * 8)
            if states == (1, 0):
                expected = 8 - expected
            expected = max(0, min(expected, 7)) + (8 if health <= 50 else 0)
            assert frame == expected, (states, start, left, total, current, health, frame, expected)
            cases.append({"states": list(states), "start": start, "time_left": left,
                          "duration": total, "current": current, "health": health, "frame": frame})
    results["door_boundary_cases"] = cases

    cases = []
    for mission in [1, 16, 18, 19]:
        for present in [False, True]:
            p.write(p.B + 0xAC, mission)
            p.write(p.T + PARTS["door"], p.SHAPES["door"] if present else 0)
            p.reset()
            p.u.reg_write(UC_X86_REG_EBP, p.B)
            end = p.execute_to_one(0x43DDF1, {0x43DED9, 0x43DF5A})
            assert (end == 0x43DED9) == (mission == 16 and present)
            cases.append({"mission": mission, "present": present, "selected": end == 0x43DED9})
    results["door_gate_cases"] = cases

    # The shared renderer chooses shadow frame half + display frame. Execute
    # the original arithmetic up to, but not including, the graphics call.
    cases = []
    for frame_count in [31, 32]:
        for frame in [0, 1, 7]:
            p.u.mem_write(p.BODY + 6, struct.pack("<H", frame_count))
            p.reset()
            p.write(p.sp + 0x88, p.BODY)
            p.write(p.sp + 0x8C, frame)
            p.execute(0x7064B4, 0x7064CD)
            selected = p.u.reg_read(UC_X86_REG_EAX)
            assert selected == frame_count // 2 + frame
            cases.append({"shape_frames": frame_count, "display_frame": frame, "shadow_frame": selected})
    results["shadow_frame_cases"] = cases
    return results


def yard_and_construction(capture):
    """Original code, actual type configuration, controlled building/anim states.

    This does not assert shipyards are present or producing in the capture.
    PlayAnim is split around the allocation/constructor boundary; no allocation,
    simulation tick or new in-game object is performed.
    """
    p = Probe(capture)
    types = {capture.name(t + 0x24, 24): t for t in capture.vector(0xA83C68)}
    anim_types = {capture.name(t + 0x24, 24): t for t in capture.vector(0x8B4150)}
    get_image, buildup_frames, slot_bindings, animation_frames = [], [], [], []
    anim = p.FIXTURE + 0x8000
    for name in sorted(TARGET_TYPES):
        t = types[name]
        p.u.mem_write(p.T, capture.read(t, 0x1800))
        p.write(p.B + 0x520, p.T)
        body_entry = p.read(p.read(p.T) + 0x9C)
        buildup_entry = p.read(p.read(p.T) + 0xC0)
        for state in [-1, 0, 1]:
            for placed in [0, 1]:
                p.write(p.B + 0x534, state)
                p.u.mem_write(p.B + 0x6E4, bytes([placed]))
                p.reset()
                p.u.reg_write(UC_X86_REG_ECX, p.B)
                end = p.execute_to_one(0x4513D0, {body_entry, buildup_entry})
                selected = "body" if end == body_entry else "buildup"
                assert selected == ("body" if state != 0 and placed else "buildup")
                get_image.append({"type": name, "bstate": state, "placed": bool(placed), "selected": selected})
        p.write(p.B + 0x534, 0)
        count = capture.signed(t + 0xF08)
        start = capture.signed(t + 0xF04)
        for mission in [18, 19]:
            p.write(p.B + 0xAC, mission)
            for frame in [0, count // 2, count - 1]:
                p.write(p.B + 0xF8, frame)
                selected = p.function(0x43EF90, p.B)
                assert selected == (frame if mission == 18 else start + count - frame - 1)
                buildup_frames.append({"type": name, "mission": mission, "stage": frame, "draw_frame": selected})
        for slot in range(21):
            config = t + 0xF4C + 68 * slot
            for variant in [0, 1]:
                anim_name = capture.name(config + variant * 16, 16)
                if not anim_name:
                    continue
                assert anim_name in anim_types, (name, slot, anim_name)
                anim_type = anim_types[anim_name]
                p.u.mem_write(p.B + 0x55C, bytes(21 * 4))
                p.u.mem_write(p.B + 0x6E7, b"\0")
                p.u.mem_write(p.B + 0x6ED, b"\0")
                p.u.mem_write(p.B + 0x9C, struct.pack("<iii", 4096, 8192, 0))
                p.reset()
                p.write(p.sp + 0x3C, slot)
                p.execute(0x4518E3, 0x451943)
                xyz = struct.unpack("<iii", p.u.mem_read(p.u.reg_read(UC_X86_REG_ESP) + 0x14, 12))
                position = struct.unpack("<ii", capture.read(config + 48, 8))
                assert position == (0, 0), "this fixture only asserts zero-offset configured slots"
                assert xyz == (4096 - 128, 8192 - 128, 0)
                p.u.mem_write(anim, bytes(0x1C8))
                p.write(anim, 0x7E3354)
                p.write(anim + 0xC8, anim_type)
                p.u.mem_write(anim + 0x9C, struct.pack("<iii", *xyz))
                p.reset()
                p.u.reg_write(UC_X86_REG_EBP, anim)
                p.u.reg_write(UC_X86_REG_EDI, 68 * slot)
                p.write(p.sp + 0x3C, slot)
                p.execute(0x45197B, 0x451A36)
                z_adjust = struct.unpack("<i", p.u.mem_read(anim + 0x100, 4))[0]
                y_sort = struct.unpack("<i", p.u.mem_read(anim + 0x104, 4))[0]
                assert p.read(p.B + 0x55C + 4 * slot) == anim
                assert p.read(anim + 0xCC) == 0 and p.u.mem_read(anim + 0x118, 1)[0] == 1
                assert z_adjust == capture.signed(config + 56) and y_sort == capture.signed(config + 60)
                assert p.function(0x422BC0, anim) == sum(xyz[:2]) + y_sort
                assert p.function(0x424CB0, anim) == capture.word(anim_type + 0x364)
                slot_bindings.append({"type": name, "slot": slot, "role": SLOT_NAMES.get(slot, str(slot)),
                                      "anim": anim_name, "variant": "damaged" if variant else "normal",
                                      "relative_to_owner": False, "z_adjust": z_adjust, "y_sort": y_sort})
                start = capture.signed(anim_type + 0x2B4)
                for value in [0, 1]:
                    p.write(anim + 0xAC, value)
                    p.reset()
                    p.u.reg_write(UC_X86_REG_ESI, anim)
                    p.execute(0x42303F, 0x423061)
                    frame = p.read(p.sp + 0x2C)
                    assert frame == value + start
                    animation_frames.append({"anim": anim_name, "value": value, "start": start, "draw_frame": frame})
    return {"get_image_cases": get_image, "buildup_frame_cases": buildup_frames,
            "slot_binding_cases": slot_bindings, "animation_frame_cases": animation_frames}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stages", nargs="+", default=["01-off", "02-on", "03-off"])
    args = parser.parse_args()
    results = []
    hashes = {}
    for stage in args.stages:
        c = Capture(args.capture, stage)
        control = controlled(c)
        control.update(yard_and_construction(c))
        result = {"stage": stage, "inventory": inventory(c),
                  "target_configuration": target_inventory(c), "controlled": control}
        results.append(result)
        for begin, end in [(0x43D33A, 0x43D386), (0x43D692, 0x43D6DB),
                           (0x43D7A7, 0x43D7F0), (0x43D864, 0x43D8EE),
                           (0x43D8EE, 0x43DA75), (0x43DDF1, 0x43DF5A),
                           (0x43EF90, 0x43F174), (0x4A50F0, 0x4A5386),
                           (0x705E00, 0x70660C), (0x7C5F00, 0x7C5F3D),
                           (0x4513D0, 0x451400), (0x4518E3, 0x451943),
                           (0x45197B, 0x451A36), (0x42303F, 0x423061),
                           (0x422BC0, 0x422BD2), (0x424CB0, 0x424CD7)]:
            value = hashlib.sha256(c.read(begin, end - begin)).hexdigest()
            key = f"{begin:#010x}-{end:#010x}"
            assert key not in hashes or hashes[key] == value
            hashes[key] = value
        print(json.dumps({"stage": stage, "inventory": result["inventory"],
                          "native_cases": sum(len(v) for v in result["controlled"].values() if isinstance(v, list))}))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"scope": __doc__, "game_exe_sha256": SUPPORTED_SHA,
                                       "code_sha256": hashes, "results": results}, indent=2) + "\n")


if __name__ == "__main__":
    main()

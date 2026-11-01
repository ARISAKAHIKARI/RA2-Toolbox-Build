"""Private x86 reception investigation; no game process or executable writes."""
import sys, struct, json, hashlib, argparse
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_placement_no_code import *
from unicorn.x86_const import *

RULES, SCENARIO, OTHER_HOUSE = 0x200D0000, 0x200E0000, 0x200F0000
VT_STUB = 0x20110000

class Reception(Harness):
    def __init__(self, pe):
        super().__init__(pe)
        self.calls = {}
        self.mark_ok = True
        self.within_area = True
        self.house_result = None
        self.change_before_second_check = False
        self.mu.mem_map(0x20200000, 0x100000)
        self.put(0x87F924, 0x20200000)
        self.put(0xABDE88, 256)
        self.put(CELL + 0x12C, 8, 'B')
        for y in (52, 60): self.put(0x20200000 + (y * 512 + 50) * 4, CELL)
        self.trace = []
        self.put(0xA83D4C, OTHER_HOUSE) # remote observer: no local UI feedback
        self.put(OTHER_HOUSE + 0x30, 1)
        self.put(0x8871E0, RULES)
        self.put(RULES + 0x16BC, 256)
        self.put(0xA8B230, SCENARIO)
        self.put(0xA8E9A0, 1, 'B')
        self.put(0xA8E7AC, 0)
        self.put(0xA8B238, 0)
        self.put(OBJECT + 0x81, 1, 'B')
        self.put(OBJECT + 0x90, 1, 'B')
        self.put(TYPE_VT + 0x6C, VT_STUB + 0x6C)
        self.put(TYPE + 0xE74, 0xFFFFFFFF)
        self.put(CELL + 0x44, 0xFFFFFFFF)
        self.put(CELL + 0xEC, 0)
        self.put(0x89EA40, 1.0, 'f')
        self.put(0x89EA60, 1, 'B')
        self.put(OBJECT_VT + 0x2C, VT_STUB + 0x2C)
        for slot in (0x38, 0x78, 0x80, 0x84, 0x88, 0x124, 0x198, 0x1B4,
                     0x1BC, 0x200, 0x274, 0x278, 0x484, 0x488):
            self.put(OBJECT_VT + slot, VT_STUB + slot)
        self.put(BUILDER_VT + 0x274, VT_STUB + 0x274)
        self.put(BUILDER_VT + 0x2C, VT_STUB + 0x2C)
        self.put(BUILDER + 0x520, TYPE)

    def count(self, key):
        self.calls[key] = self.calls.get(key, 0) + 1

    def hook(self, mu, at, size, data):
        self.trace.append(at)
        self.trace = self.trace[-12:]
        sp = mu.reg_read(UC_X86_REG_ESP)
        if at in (0x6F6CA0, 0x5F4EC0, 0x47C620):
            self.count(hex(at))
        if at == 0x5F4EC0 and self.change_before_second_check:
            self.put(CELL + 0x124, 1)
        if at in (0x4C6AE0, 0x586360): self.count(hex(at))
        if at == 0x47C620:
            return # execute real cell legality instead of parent's substitute
        if at == 0x4FB512:
            self.house_result = int(mu.mem_read(sp + 0x13, 1)[0])
        if VT_STUB <= at < VT_STUB + 0x600:
            slot = at - VT_STUB
            self.count('virtual_substitute_' + hex(slot))
            if slot == 0x6C:
                dst, src = self.word(sp + 4), self.word(sp + 8)
                mu.mem_write(dst, bytes(mu.mem_read(src, 12)))
                self.ret(dst, 8)
            elif slot == 0x1B4:
                mu.mem_write(OBJECT + 0x9C, bytes(mu.mem_read(self.word(sp + 4), 12)))
                self.ret(0, 4)
            elif slot == 0x124:
                if self.mark_ok: self.put(OBJECT + 0x74, 1, 'B')
                self.ret(int(self.mark_ok), 4)
            else:
                result, args = {0x2C:(6,0), 0x38:(0,0), 0x78:(0xFFFFFFFF,0),
                    0x80:(0,0), 0x84:(TYPE,0), 0x88:(TYPE,0), 0x198:(1,4),
                    0x1BC:(0,0), 0x200:(0,0), 0x274:(0,4), 0x278:(0,8),
                    0x484:(0,8), 0x488:(0,20)}[slot]
                self.ret(result, args)
            return
        services = {0x47C3D0:(0,12), 0x47C4D0:(0,8), 0x578540:(int(self.within_area),8),
            0x578460:(1,8), 0x567DA0:(0,16), 0x502A80:(0,8),
            0x4C9300:(0,4), 0x4C9220:(0,4), 0x455F10:(0,4),
            0x4FD150:(0,0), 0x4FFA50:(0,4), 0x50B730:(1,0),
            0x4CA1A0:(0,0), 0x578080:(0,4), 0x4068E0:(0,0),
            0x750920:(0,8), 0x752700:(0,4), 0x4A8D50:(0,4), 0x4A8BF0:(0,4), 0x4FAA10:(0,16), 0x4FB6B0:(0,4)}
        if at == 0x6557F0:
            dst = self.word(sp + 4)
            self.put(dst,0);self.put(dst + 4,0)
            self.count('projection_substitute')
            self.ret(dst,12)
        elif at in services:
            self.count('service_substitute_' + hex(at))
            self.ret(*services[at])
        else:
            super().hook(mu,at,size,data)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == SUPPORTED, 'Unverified executable'
    pe = pefile.PE(data=raw)
    rows = []
    cases = [
        ('near_visible', {}, 1),
        ('far_visible', {'y':60}, 1),
        ('far_shrouded_local', {'y':60,'shrouded':True}, 1),
        ('far_shrouded_remote', {'y':60,'shrouded':True,'remote':True}, 1),
        ('occupied', {'occupied':True}, 0),
        ('impassable_land_speed', {'land_blocked':True}, 0),
        ('outside_usable_map', {'outside':True}, 0),
        ('mark_rejected_after_legality', {'mark':False}, 0),
        ('factory_incomplete', {'complete':False}, 0),
        ('occupied_between_first_and_second_check', {'change':True}, 0),
        ('PlaceAnywhere_changes_receiver_occupancy_result', {'occupied':True,'place_anywhere':True}, 1),
        ('already_on_map', {'already':True}, 0),
    ]
    for label, cfg, expected in cases:
        h = Reception(pe)
        h.put(0xA83D4C, HOUSE)
        h.put(POS, 50 | cfg.get('y',52) << 16)
        h.put(CELL + 0x12C, 0 if cfg.get('shrouded') else 8, 'B')
        if cfg.get('occupied'): h.put(CELL + 0x124,1)
        if cfg.get('land_blocked'): h.put(0x89EA40,0.0,'f')
        if cfg.get('outside'): h.within_area=False
        if cfg.get('place_anywhere'): h.put(TYPE + 0x1703,1,'B')
        if cfg.get('already'): h.put(OBJECT + 0x74,1,'B')
        h.mark_ok=cfg.get('mark',True)
        h.complete=cfg.get('complete',True)
        h.change_before_second_check=cfg.get('change',False)
        proximity = h.run(0x4A8EB0,0x87F7E8,[TYPE,3,FOUNDATION,POS])
        visible = h.run(0x4A9070,0x87F7E8,[TYPE,3,FOUNDATION,POS])
        assert proximity == int(cfg.get('y',52)==52)
        assert visible == int(not cfg.get('shrouded'))
        h.calls.clear()
        if cfg.get('remote'): h.put(0xA83D4C,OTHER_HOUSE)
        h.run(0x4C6AE0,EVENT,[3,0x0B,7,12,0,POS])
        assert h.mu.mem_read(EVENT,1)[0] == 0x0B and h.word(EVENT+7) == 7
        try:
            h.run(0x4C6CB0,EVENT,[])
        except Exception:
            print(label,'TRACE',[hex(x) for x in h.trace],h.calls)
            raise
        assert h.house_result == expected, (label,h.house_result,expected)
        assert h.calls.get('service_substitute_0x4ca1a0',0) == expected
        assert h.calls.get('service_substitute_0x4faa10',0) == expected
        assert not h.calls.get('0x4a8eb0') and not h.calls.get('0x4a9070')
        if cfg.get('change'): assert h.calls.get('0x449440')==2 and not h.calls.get('virtual_substitute_0x124')
        for section in pe.sections:
            if section.Characteristics & 0x20000000:
                at = pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
                original = section.get_data()
                assert bytes(h.mu.mem_read(at, len(original))) == original
        rows.append(dict(case=label,configuration=cfg,preview_proximity=proximity,preview_visibility=visible,
            house_result=h.house_result,in_limbo=h.mu.mem_read(OBJECT+0x81,1)[0],
            on_map=h.mu.mem_read(OBJECT+0x74,1)[0],calls=h.calls))
    result=dict(executable_sha256=SUPPORTED,checks=len(rows),status='PASS',cases=rows,
        executable_code_sections_unchanged=True,private_executable_modified=False,game_process_used=False,
        boundary='Original constructor, dispatch, House, Building/Techno/Object Unlimbo and Cell legality execute to normal return. Synthetic ordinary 1x1 building; factory, map lookup, occupant enumeration, map usable-area result, Mark registration, position adjustment, house bookkeeping, projection, radio, audio and presentation services are explicit substitutes. No real map registration, real factory mutation, queue insertion, game process or multiplayer test.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(checks=len(rows),status='PASS')))

if __name__ == '__main__': main()

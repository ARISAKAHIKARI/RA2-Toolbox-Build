"""Original Sell receiver, Foot::Sell, GetRefund and GiveMoney; not a full game.

The native refund arithmetic and cash credit run from the supplied EXE. Object
resolution, sound, country/plant multipliers and final deletion are modeled.
The separate SellCell test executes its actual wall-removal branch; redraw and
pathfinding updates are stubbed. No game code is patched or distributed.
"""
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

_base = Path(__file__).with_name('no_interception_runtime.py')
exec(compile(_base.read_text().split('for mode in (0,5,3,4):', 1)[0], str(_base), 'exec'))
_pe = pefile.PE(a.game)
_sell_guard = _pe.get_data(0x4d9f79 - _pe.OPTIONAL_HEADER.ImageBase, 8)
stock_sell = _sell_guard[:2] == b'\x74\x6e'
skipped = []

def prepare(mode, soylent, building, wall):
    m = fixture(mode)
    m.w(m.t, 0x7f6218)
    m.w(m.t + 0x610, 900)
    m.w(m.t + 0x614, soylent)
    rules = m.alloc(0x2000)
    m.w(0x8871e0, rules)
    m.u.mem_write(rules + 0x1738, struct.pack('<d', .5))
    m.b(m.house + 0x1ec, 1)
    m.w(m.unit + 0x9c, 20 * 256 + 128)
    m.w(m.unit + 0xa0, 30 * 256 + 128)
    m.w(m.crateCell + 0xe4, m.unit)
    m.w(m.unit + 0x30, m.building if building else 0)
    m.w(m.building + 0x30, 0)
    m.w(m.crateCell + 0x44, 5 if wall else 0xffffffff)
    m.w(m.crateCell + 0x50, 0 if wall else 0xffffffff)
    m.b(m.overlayType + 0x2a8, wall)
    # Preserve the native RTTI, coords, Sell and GetRefund virtual methods.
    vt = m.alloc(0x600)
    m.u.mem_write(vt, bytes(m.u.mem_read(0x7f5c70, 0x600)))
    m.w(m.unit, vt)
    m.saleCalls = 0
    m.deletionCalls = []
    def delete_stub(uc, at, size, _):
        m.deletionCalls.append(at)
        m.ret(0, 0)
    for i, offset in enumerate((0x3a0, 0xd4, 0xf8)):
        address = m.STUB + 0xe000 + i * 16
        m.w(vt + offset, address)
        m.u.hook_add(UC_HOOK_CODE, delete_stub, begin=address, end=address)
    def resolve(uc, at, size, _):
        target = m.u.reg_read(UC_X86_REG_ECX)
        assert m.r(target) == 10 and bytes(m.u.mem_read(target + 4, 1)) == b'\x34'
        m.ret(m.unit, 0)
    m.u.hook_add(UC_HOOK_CODE, resolve, begin=0x6e6f20, end=0x6e6f20)
    for address in (0x6386e0, 0x50b6f0):
        m.u.hook_add(UC_HOOK_CODE, lambda uc, at, size, _: m.ret(0, 0), begin=address, end=address)
    # Native x87 return of 1.0. Neither the GetRefund function nor the
    # GiveMoney function is stubbed; all credits below are original writes.
    fp = m.STUB + 0xe100
    m.u.mem_write(fp, b'\xd9\xe8\xc2\x04\x00')
    for address in (0x50bdf0, 0x50beb0):
        m.u.hook_add(UC_HOOK_CODE, lambda uc, at, size, _: uc.reg_write(UC_X86_REG_EIP, fp), begin=address, end=address)
    def sale_entry(uc, at, size, _):
        m.saleCalls += 1
    m.u.hook_add(UC_HOOK_CODE, sale_entry, begin=0x4d9f70, end=0x4d9f70)
    return m

for mode in (0, 5, 3, 4):
    for soylent, expected in ((0, 450), (900, 900), (300, 300)):
        def refund(mode=mode, soylent=soylent, expected=expected):
            m = prepare(mode, soylent, False, False)
            m.run(0x70ada0, (), m.unit)
            assert m.u.reg_read(UC_X86_REG_EAX) == expected
            assert not m.codeWrites
        check(f'native_getrefund_mode_{mode}_soylent_{soylent}', refund)

for mode in (0, 5, 3, 4):
    for soylent, expected in ((0, 450), (900, 900), (300, 300)):
        for building in (False, True):
            for wall in (False, True):
                for owner in (True, False):
                    if building and not stock_sell:
                        skipped.append(f'native_sell_mode_{mode}_soylent_{soylent}_building_{building}_wall_{wall}_owner_{owner}')
                        continue
                    def receiver(mode=mode, soylent=soylent, expected=expected, building=building, wall=wall, owner=owner):
                        m = prepare(mode, soylent, building, wall)
                        packet = m.alloc()
                        m.run(0x4c65e0, (0 if owner else 1, 0x16, 10, 0x34), packet)
                        before = m.r(m.house + 0x30c)
                        m.run(0x4c6cb0, (), packet)
                        sold = building and owner
                        assert m.r(m.house + 0x30c) - before == (expected if sold else 0)
                        assert m.saleCalls == int(sold)
                        assert len(m.deletionCalls) == 3 * int(sold)
                        assert m.r(m.crateCell + 0x44) == (5 if wall else 0xffffffff)
                        assert not m.codeWrites
                    check(f'native_sell_mode_{mode}_soylent_{soylent}_building_{building}_wall_{wall}_owner_{owner}', receiver)

for mode in (0, 5, 3, 4):
    def wall_event(mode=mode):
        m = prepare(mode, 900, False, True)
        m.w(m.buildT, 0x7e4570)
        m.w(m.buildT + 0xe54, m.overlayType)
        # Native wall GetActualCost result is discarded. Stub that calculation
        # and visual/path updates; retain native eligibility and wall writes.
        vt = m.alloc(0x100)
        m.u.mem_write(vt, bytes(m.u.mem_read(0x7e4570, 0x100)))
        cost = m.STUB + 0xe200
        m.w(vt + 0x84, cost)
        m.w(m.buildT, vt)
        m.u.hook_add(UC_HOOK_CODE, lambda uc, at, size, _: m.ret(50, 1), begin=cost, end=cost)
        for address, argc in ((0x750920, 2), (0x47d2b0, 1), (0x480630, 1), (0x6551c0, 1), (0x70d4a0, 0), (0x56d460, 1), (0x584550, 1)):
            m.u.hook_add(UC_HOOK_CODE, lambda uc, at, size, _, argc=argc: m.ret(0, argc), begin=address, end=address)
        cell = m.alloc()
        m.u.mem_write(cell, struct.pack('<hh', 20, 30))
        packet = m.alloc()
        m.run(0x4c6650, (0, 0x17, cell), packet)
        before = m.r(m.house + 0x30c)
        m.run(0x4c6cb0, (), packet)
        assert m.r(m.crateCell + 0x44) == 0xffffffff
        assert m.r(m.crateCell + 0x50) == 0xffffffff
        assert m.r(m.house + 0x30c) == before
        assert m.saleCalls == 0 and not m.deletionCalls and not m.codeWrites
    check(f'native_sellcell_removes_wall_without_selling_vehicle_mode_{mode}', wall_event)

Path(a.output).write_text(json.dumps({
    'inputs': {'game': hashlib.sha256(Path(a.game).read_bytes()).hexdigest(), 'GameRuntime.dll': hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},
    'scope': 'Original static EXE Sell receiver/refund/cash credit. World, price multipliers, sound, final deletion, wall redraw/pathfinding modeled. No loaded Ares/Phobos, full game or real netplay.',
    'stock_sell_entry': stock_sell,
    'sell_entry_bytes_at_4d9f79': _sell_guard.hex(),
    'skipped_full_sell_cases': skipped,
    'skip_reason': '' if stock_sell else 'The supplied EXE redirects Foot::Sell to code unavailable in this model; do not undo or bypass its patch. Direct native GetRefund and the no-building receiver rejection remain separately testable.',
    'results': results
}, indent=2) + '\n')
print('TOTAL', len(results))

"""Actual Runtime Z-planning input -> capture token -> original Move constructor.
Synthetic world, hit testing and Windows API; no aircraft physics or real netplay.
The baseline option proves the 1.0.22 planning-mode rejection on the same inputs.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP

p = argparse.ArgumentParser()
p.add_argument('game'); p.add_argument('dll'); p.add_argument('--output', required=True)
p.add_argument('--expect-rejected', action='store_true')
a = p.parse_args(); rows = []
for mode in (3, 4):
 for flag in (0x80000000, 0x20000000):
  for binding in (1024, 65536):
   spec = importlib.util.spec_from_file_location('air_input_fixture', Path(__file__).with_name('entry_runtime_regression.py'))
   m = importlib.util.module_from_spec(spec); m.fixture_paths = [a.game, a.dll]; spec.loader.exec_module(m)
   def state(): return m.Shared.from_buffer_copy(m.u.mem_read(m.SH, C.sizeof(m.Shared)))
   def save(s): m.u.mem_write(m.SH, bytes(s))
   m.w(0xa8b238, mode)
   airtype = m.alloc(0x2000); air = m.obj(0x7e22a4, airtype, m.house, 40, selected=True)
   m.vector(0xa8ec78, [m.unit, m.eng, m.building, m.enemyUnit, air])
   s = state(); s.flags = flag; s.externalDisplay = 1; save(s); s = m.hook()
   m.b(0x87f7e8 + 0x11b3, 1)  # DisplayClass::PlanningMode while Z is used.
   def click(u, at, size, _):
    sp = m.u.reg_read(UC_X86_REG_ESP); arg = lambda n: m.r(sp + 4 + n * 4)
    m.u.mem_write(arg(1), struct.pack('<hh', 20, 30))
    m.u.mem_write(arg(2), struct.pack('<iii', 5248, 7808, 0)); m.w(arg(3), 0); m.ret(0, 6)
   m.u.hook_add(UC_HOOK_CODE, click, begin=0x692300, end=0x692300)
   before = bytes(m.u.mem_read(air, 0x2000))
   s = state(); i = s.pointer.intent
   i.match = s.data.match; i.tick = m.clock; i.frame = m.r(0xa8ed84); i.house = m.house
   i.operation = 6; i.bindings = binding; i.width = 800; i.height = 600; i.x = 20; i.y = 30
   i.count = 1; i.sources[0].id = 40; i.sources[0].pointer = air
   s.pointer.request += 1; save(s); s = m.hook()
   if a.expect_rejected:
    assert s.pointer.ack == s.pointer.request and s.pointer.result == -4
   else:
    assert s.pointer.ack != s.pointer.request, 'physical click must wait one logic frame'
    m.w(0xa8ed84, m.r(0xa8ed84) + 1); s = m.hook()
    assert s.pointer.ack == s.pointer.request and s.pointer.result == 1
    assert s.pointer.accepted == 1 and s.pointer.resolvedCell == 30020 and s.pointer.sourceAccepted[0] == 1
   assert m.r(0xa802c8) == m.r(0x8b41f8) == 0, 'recording must not emit commands'
   assert bytes(m.u.mem_read(air, 0x2000)) == before
   assert m.u.mem_read(0x87f7e8 + 0x11b3, 1) == b'\x01', 'do not clear native planning mode'
   assert s.data.hookInstalled == 0
   if not a.expect_rejected:
    s.command = 21; s.arg = 40; s.arg2 = s.pointer.resolvedCell; s.requestToken = s.pointer.sourceTokens[0]
    s.request += 1; s.requestTick = m.clock; s.requestMatch = s.data.match; save(s); s = m.hook()
    assert s.result == 1 and s.count == 1 and m.r(0xa802c8) == 1
    event = bytes(m.u.mem_read(0xa802d4, 111)); assert event[0] == 4 and event[12] == 2
    assert bytes(m.u.mem_read(air, 0x2000)) == before
    # Other enhanced clicks still respect original planning mode.
    s.pointer.intent.operation = 1; s.pointer.intent.tick = m.clock; s.pointer.intent.frame = m.r(0xa8ed84)
    s.pointer.request += 1; save(s); s = m.hook()
    assert s.pointer.ack == s.pointer.request and s.pointer.result == -4
    assert m.r(0xa802c8) == 1
   label = f'mode_{mode}_flag_{flag:x}_button_{binding}'
   rows.append({'case': label, 'status': 'PASS', 'baseline_rejection': a.expect_rejected})
   print('PASS', label, flush=True)
Path(a.output).write_text(json.dumps({'dll_sha256': hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'game_sha256': hashlib.sha256(Path(a.game).read_bytes()).hexdigest(), 'scope': __doc__, 'results': rows}, indent=2) + '\n')

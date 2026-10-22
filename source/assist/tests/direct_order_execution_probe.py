"""Original event receiver and unit QueueMission in a synthetic world.

Runs original receiver/mission code; ID resolution, radio housekeeping and
target/destination/Sell endpoints are explicit test doubles. This proves
dispatch and queued unit state, not movement, completed sales or netplay.
"""
import argparse
import hashlib
import importlib.util
import json
import traceback
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP

p = argparse.ArgumentParser()
p.add_argument('game')
p.add_argument('dll')
p.add_argument('--output', required=True)
a = p.parse_args()
rows = []

def fixture():
    spec = importlib.util.spec_from_file_location('fixture', Path(__file__).with_name('entry_runtime_regression.py'))
    m = importlib.util.module_from_spec(spec)
    m.fixture_paths = [a.game, a.dll]
    spec.loader.exec_module(m)
    seen = []
    def hook(at, fn):
        m.u.hook_add(UC_HOOK_CODE, lambda uc, address, size, user: fn(), begin=at, end=at)
    def arg(i):
        return m.r(m.u.reg_read(UC_X86_REG_ESP) + 4 + 4*i)
    objects = {10:m.unit, 12:m.building, 13:m.enemyUnit}
    for at in (0x6e6f20, 0x6e6e20, 0x6e6ff0):
        hook(at, lambda: m.ret(objects.get(m.r(m.u.reg_read(UC_X86_REG_ECX)), 0), 0))
    for at, argc in ((0x50b6f0,0), (0x750920,2), (0x521b60,0),
                     (0x65acb0,1), (0x70c610,1), (0x4da1c0,0)):
        hook(at, lambda argc=argc: m.ret(0, argc))
    def sink(name, argc):
        seen.append([name, m.u.reg_read(UC_X86_REG_ECX), *[arg(i) for i in range(argc)]])
        m.ret(0, argc)
    hook(m.r(0x7f5c70+0x3c8), lambda: sink('SetTarget',1))
    hook(m.r(0x7f5c70+0x480), lambda: sink('SetDestination',2))
    hook(m.r(0x7e3ebc+0x1a0), lambda: sink('Sell',1))
    hook(0x5b35e0, lambda: seen.append(['QueueMission',m.u.reg_read(UC_X86_REG_ECX),arg(0),arg(1)]))
    m.b(m.enemyUnit+0x14, 4)  # Foot-class flag, so ordinary Foot cleanup executes.
    m.w(m.building+0x21c, m.enemy)
    m.w(0x8b41f8, 0)
    return m, seen

cases = ['direct_unit_queue', 'move_header_local', 'move_header_owner',
         'attack_header_local', 'attack_header_owner', 'dead_actor',
         'sell_header_local', 'sell_header_owner']
for case in cases:
    try:
        m, seen = fixture()
        if case == 'direct_unit_queue':
            m.run(0x5b35e0, (2,0), m.enemyUnit)
            assert m.r(m.enemyUnit+0xb4) == 2
            assert m.r(m.enemyUnit+0xac) == 5
            assert seen == [['QueueMission',m.enemyUnit,2,0]]
        elif case.startswith('sell'):
            header = int(case.endswith('owner'))
            packet = m.alloc()
            m.run(0x4c65e0, (header,0x16,12,0x34), packet)
            m.run(0x4c6cb0, (), packet)
            assert seen == ([['Sell',m.building,0xffffffff]] if header else [])
        else:
            header = int(case.endswith('owner'))
            mission = 1 if case.startswith('attack') else 2
            target = 10 if mission == 1 else 0
            destination = 12 if mission == 2 else 0
            if case == 'dead_actor':
                m.w(m.enemyUnit+0x6c, 0)
            packet = m.alloc()
            m.run(0x4c6860, (header,13,0x34,mission,target,0x34 if target else 0,
                            destination,0x34 if destination else 0,0,0), packet)
            m.run(0x4c6cb0, (), packet)
            if case == 'dead_actor':
                assert not seen and m.r(m.enemyUnit+0xb4) == 0xffffffff
            else:
                assert m.r(m.enemyUnit+0xb4) == mission
                assert seen == [['QueueMission',m.enemyUnit,mission,0],
                                ['SetTarget',m.enemyUnit,m.unit if target else 0],
                                ['SetDestination',m.enemyUnit,m.building if destination else 0,1]], seen
        assert m.r(m.enemyUnit+0x21c) == m.enemy
        assert m.r(m.building+0x21c) == m.enemy
        assert m.r(0xa83d4c) == m.house
        assert m.r(0xa802c8) == 0 and m.r(0x8b41f8) == 0
        rows.append({'case':case, 'status':'PASS', 'calls':seen,
                     'outlist_count':0, 'dolist_count':0})
    except Exception as exc:
        rows.append({'case':case,'status':'FAIL','error':repr(exc)})
        traceback.print_exc()
    print(rows[-1], flush=True)

out = Path(a.output)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'scope':__doc__,
    'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),
    'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
    'results':rows}, indent=2)+'\n')
raise SystemExit(any(row['status'] != 'PASS' for row in rows))

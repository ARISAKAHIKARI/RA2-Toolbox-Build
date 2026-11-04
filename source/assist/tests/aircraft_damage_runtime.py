"""Execute the final x86 Runtime and original attack/selection functions.

Host allocations use the production external reader and ranker against the same
synthetic memory image. Win32, world objects and read-only combat virtual queries
are controlled; native event constructors and Object::Deselect execute real code.
This does not certify projectile hits, a Windows input device, or multiplayer.
"""
from pathlib import Path
SCOPE = __doc__
import gc
import subprocess
import tempfile
import traceback

# Reuse the common final-DLL ABI, manager entry, original constructors and Win32
# fixture without executing that module's unrelated scenario suite.
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):', 1)[0], str(Path(__file__).with_name('no_interception_runtime.py')), 'exec'))

SOURCE = Path(__file__).resolve().parents[2]
FLAGS = 2097152 | 8192 | 1024  # aircraft, persistent advanced intents, native orders
rows, opened = [], []

HOST_DRIVER = r'''
#include "@SOURCE@/external_reader/source/cooperative_reader.h"
#include <cstdio>
#include <cstring>
#include <vector>
struct Span {uint32_t address;std::vector<uint8_t> bytes;};
struct Read {
 const std::vector<Span>* spans;
 bool operator()(uint32_t at,void*out,size_t n)const{
  for(const auto&s:*spans)if(at>=s.address&&uint64_t(at)+n<=uint64_t(s.address)+s.bytes.size()){memcpy(out,s.bytes.data()+at-s.address,n);return true;}
  return false;
 }
};
int main(int argc,char**){
 using namespace assist::cooperative;
 if(argc>1){uint32_t sizes[]={sizeof(Request),sizeof(Assignment)};fwrite(sizes,sizeof sizes,1,stdout);return 0;}
 Request original{},remaining{};uint32_t np=0,na=0,ns=0;
 if(fread(&original,sizeof original,1,stdin)!=1||fread(&np,4,1,stdin)!=1||np>MaxSources)return 2;
 std::vector<Assignment> processed(np);if(np&&fread(processed.data(),sizeof(Assignment),np,stdin)!=np)return 3;
 if(fread(&na,4,1,stdin)!=1||na>MaxSources)return 4;
 std::vector<Assignment> accepted(na);if(na&&fread(accepted.data(),sizeof(Assignment),na,stdin)!=na)return 5;
 if(fread(&ns,4,1,stdin)!=1||ns>16)return 6;std::vector<Span> spans(ns);
 for(auto&s:spans){uint32_t n=0;if(fread(&s.address,4,1,stdin)!=1||fread(&n,4,1,stdin)!=1||n>64*1024*1024)return 7;s.bytes.resize(n);if(n&&fread(s.bytes.data(),1,n,stdin)!=n)return 8;}
 std::vector<Assignment> orders;
 if(retrySourcePool(remaining,original,processed)){
  auto input=external::cooperativeInput(Read{&spans},remaining,[](uint32_t){return Quota{};});
  reserveAccepted(input,accepted);orders=allocate(input);
 }
 uint32_t n=orders.size();fwrite(&n,4,1,stdout);if(n)fwrite(orders.data(),sizeof(Assignment),n,stdout);
}
'''


def clear_queues(m):
    for at in (0x8B41F8, 0x8B41FC, 0x8B4200, 0xA802C8, 0xA802CC, 0xA802D0):
        m.w(at, 0)


def queue(m, room=0):
    single = m.r(0xA8B238) in (0, 5)
    limit, at = (16384, 0x8B41F8) if single else (112, 0xA802C8)
    m.w(at, limit - room)
    m.w(at + 4, 0)
    m.w(at + 8, 0)
    return limit


def objects(m):
    return {p: bytes(m.u.mem_read(p, 0x2000)) for p in m.planes}


def metadata(m):
    s = state(m)
    intents = {x.id: (x.cell, x.token, x.mode, getattr(x, 'global')) for x in s.world.intents[:s.world.intentCount]}
    tokens = {x.id: x.manualVersion for x in s.world.actors[:s.world.count]}
    return intents, tokens


def select_all(m, s, intent):
    selection = m.planes + getattr(m, 'extras', [])
    intent.count = len(selection)
    for i, p in enumerate(selection):
        intent.sources[i] = m.PointerSource(m.r(p + 0x10), p)


def setup(session, inside, powers=None):
    m = fixture(session, FLAGS)
    opened.append(m)
    m.vector(0xA8DA74, [])  # no captured source-process network node pointers
    m.w(m.enemy + 0x34, m.alloc())
    m.planes = [m.unit] + [m.obj(0x7F5C70, m.t, m.house, 20 + j, selected=True) for j in range(3)]
    m.target = m.enemyUnit
    m.vector(0xA8EC78, m.planes + [m.target])
    m.vector(0xA8ECB8, m.planes)
    m.w(0xA8ECB8, 0x7E4F64)  # original DynamicVectorClass::FindIndex virtual
    m.b(0x87F7E8 + 0x119C, 1)
    m.w(0x87F7E8 + 0x11A0, m.planes[-1])
    projectile, warhead, info = m.alloc(), m.alloc(), m.alloc()
    m.w(m.weapon + 0x9C, 1)
    m.w(m.weapon + 0xA4, 60)
    m.w(m.weapon + 0xB4, 4096)
    m.w(m.weapon + 0xA0, projectile)
    m.w(m.weapon + 0xAC, warhead)
    m.b(projectile + 0x2A5, 1)
    m.u.mem_write(warhead + 0xA0, struct.pack('<11d', *([1.] * 11)))
    m.w(info, m.weapon)
    for j, p in enumerate(m.planes):
        m.b(p + 0x134, 1)
        m.w(p + 0x18C, 0)
        m.w(p + 0x9C, 10000 - j * 128)
        m.w(p + 0xA0, 10000)
        m.u.mem_write(p + 0x160, struct.pack('<d', powers[j] if powers else 1.))
    m.w(m.target + 0x6C, 100)
    m.w(m.target + 0x9C, 10000)
    m.w(m.target + 0xA0, 10000)
    m.u.mem_write(m.target + 0x158, struct.pack('<d', 1.))
    # Model only read-only virtual queries. Do NOT hook Object::Deselect,
    # vector FindIndex or Display current-object accessors.
    hooks = set()
    def add(offset, fn):
        address = m.r(0x7F5C70 + offset)
        if address not in hooks:
            hooks.add(address)
            m.u.hook_add(UC_HOOK_CODE, fn, begin=address, end=address)
    add(0x2E4, lambda uc, at, size, extra: m.ret(0, 1))
    add(0x3F8, lambda uc, at, size, extra: m.ret(info, 1))
    add(0x3A8, lambda uc, at, size, extra: m.ret(1, 2))
    add(0x54, lambda uc, at, size, extra: m.ret(int(bool(m.r(uc.reg_read(UC_X86_REG_ECX) + 0x134) & 255)), 0))
    s = state(m)
    s.computeRequested = s.computeApplied = 32 if inside else 0
    s.aircraftAllocationPolicy = 2
    s.quotaConfigSeq = 2
    s.quotaConfig.defaults = m.Quota(1, 2, 0)
    save(m, s)
    # Populate actual persistent task metadata and nonzero manual tokens.
    # The external advanced planner is not run, so these are prior intents,
    # not an independent stream of automated gameplay events.
    m.queryObject = 0
    s = pointer(m, 1, 1, lambda s, i: select_all(m, s, i))
    assert s.pointer.accepted == 4 and count(m) == 0, ('seed metadata', s.pointer.accepted, count(m))
    tick(m)
    m.queryObject = m.target
    m.msg = m.alloc()
    m.before = objects(m)
    m.before_metadata = metadata(m)
    assert len(m.before_metadata[0]) == 4 and all(m.before_metadata[1][m.r(p + 0x10)] for p in m.planes)
    m.codeWrites.clear()
    return m


def host_orders(m, original=None, processed=(), accepted=()):
    request = original or state(m).cooperativeRequest
    segments = [(m.game.OPTIONAL_HEADER.ImageBase, m.game.OPTIONAL_HEADER.SizeOfImage), (m.HEAP, m.cursor - m.HEAP)]
    payload = bytes(request) + struct.pack('<I', len(processed)) + b''.join(bytes(x) for x in processed)
    payload += struct.pack('<I', len(accepted)) + b''.join(bytes(x) for x in accepted) + struct.pack('<I', len(segments))
    for at, length in segments:
        payload += struct.pack('<II', at, length) + bytes(m.u.mem_read(at, length))
    data = subprocess.run([str(host)], input=payload, stdout=subprocess.PIPE, check=True).stdout
    n = struct.unpack_from('<I', data)[0]
    size = C.sizeof(m.CooperativeAssignment)
    assert len(data) == 4 + n * size
    return [m.CooperativeAssignment.from_buffer_copy(data[4 + i * size:4 + (i + 1) * size]) for i in range(n)]


def submit_host(m, orders):
    s = state(m)
    s.cooperativePlan += 1
    s.cooperativeSerial = s.cooperativeRequest.serial
    s.cooperativeTick = m.clock
    s.cooperativeFrame = m.r(0xA8ED84)
    s.cooperativeMatch = s.data.match
    s.cooperativeMode = s.cooperativeRequest.mode
    s.cooperativeCount = len(orders)
    for i, order in enumerate(orders):
        s.cooperativeOrders[i] = order
    save(m, s)
    return m.hook()


def start(m, inside, bindings=16):
    s = pointer(m, 1, bindings, lambda s, i: select_all(m, s, i))
    if bindings & (1024 | 65536):
        s = tick(m)
    assert s.pointer.accepted == 4, ('capture selection', s.pointer.result, s.pointer.accepted)
    request = m.CooperativeRequest.from_buffer_copy(bytes(s.cooperativeRequest))
    assert request.mode == 5 and request.centerObject == m.target and request.count == 4
    assert (request.cameraX, request.cameraY) == (10000, 10000)
    if not inside:
        assert count(m) == 0
        orders = host_orders(m, request)
        assert orders and all(x.expectedDamage > 0 for x in orders)
        s = submit_host(m, orders)
        s = tick(m)  # acquire a new world snapshot after same-frame metadata cleanup
    return s, request


def event_ids(m):
    data = packets(m)
    result = []
    for i in range(count(m)):
        event = data[i * 111:(i + 1) * 111]
        assert event[0] == 4 and event[12] == 1 and event[18] == 0x34
        assert struct.unpack_from('<I', event, 14)[0] == m.r(m.target + 0x10)
        result.append(struct.unpack_from('<I', event, 7)[0])
    return result


def unchanged_unsent(m, sent):
    sent = set(sent)
    after_intents, after_tokens = metadata(m)
    for p, before in m.before.items():
        ident = m.r(p + 0x10)
        expected = before[:0x83] + b'\0' + before[0x84:] if ident in sent else before
        assert bytes(m.u.mem_read(p, 0x2000)) == expected, ('object mutation', ident)
        if ident not in sent:
            assert after_intents[ident] == m.before_metadata[0][ident], ('unsent task metadata', ident)
            assert after_tokens[ident] == m.before_metadata[1][ident], ('unsent manual token', ident)
    remaining_planes = [p for p in m.planes if m.r(p + 0x10) not in sent]
    expected_members = remaining_planes + getattr(m, 'extras', [])
    assert m.r(0xA8ECC8) == len(expected_members)
    assert [m.r(m.r(0xA8ECBC) + i * 4) for i in range(len(expected_members))] == expected_members
    assert set(after_intents) == {m.r(p + 0x10) for p in remaining_planes}
    s = state(m)
    for ident in sent:
        receipts = [r for r in s.data.recentReceipts[:s.data.receiptCount] if r.unit == ident and r.owner == 4]
        assert receipts and any(r.phase == 2 for r in receipts) and not any(r.phase in (5, 6) for r in receipts), ('new cooperative receipt was cancelled', ident)
    assert not m.codeWrites


def subset(session, inside, larger=False):
    m = setup(session, inside, [1 / 3, 13 / 6, 4 / 3, 1 / 6] if larger else None)
    _, request = start(m, inside)
    expected = [20] if larger else [10, 20]
    assert event_ids(m) == expected
    tick(m)  # settle the original unassigned source pool without replaying
    assert event_ids(m) == expected
    unchanged_unsent(m, expected)
    s = state(m)
    assert (s.runtimeCoopDone == request.serial) if inside else (s.cooperativeAck == s.cooperativePlan)


def full_queue(session, inside):
    m = setup(session, inside)
    maximum = queue(m)
    s = pointer(m, 1, 16, lambda s, i: select_all(m, s, i))
    assert count(m) == maximum and s.pointer.accepted == 0
    unchanged_unsent(m, [])
    clear_queues(m)
    s = tick(m)
    assert s.pointer.accepted == 4
    if not inside:
        s = submit_host(m, host_orders(m))
        s = tick(m)
    assert event_ids(m) == [10, 20]
    unchanged_unsent(m, [10, 20])


def replan_reserved(session, inside):
    m = setup(session, inside)
    # Capture externally before restricting the queue; Runtime capture itself
    # needs one queue slot but does not spend it until admission.
    if not inside:
        s = pointer(m, 1, 16, lambda s, i: select_all(m, s, i))
        original = m.CooperativeRequest.from_buffer_copy(bytes(s.cooperativeRequest))
        orders = host_orders(m, original)
        assert len(orders) == 2
        maximum = queue(m, 1)
        s = submit_host(m, orders)
    else:
        maximum = queue(m, 1)
        s = pointer(m, 1, 16, lambda s, i: select_all(m, s, i))
        original = m.CooperativeRequest.from_buffer_copy(bytes(s.cooperativeRequest))
    assert count(m) == maximum and m.u.mem_read(m.planes[0] + 0x83, 1) == b'\0'
    first_packet = bytes(m.u.mem_read(0x8B4204 if session == 0 else 0xA802D4, 111))
    assert first_packet[12] == 1 and struct.unpack_from('<I', first_packet, 7)[0] == 10
    m.w(m.target + 0x6C, 50)  # accepted damage 60 now exactly covers 120%
    clear_queues(m)
    if inside:
        s = tick(m)
        assert s.runtimeCoopDone == original.serial and s.runtimeCoopAccepted == 1 and s.runtimeCoopRejected == 0
    else:
        m.clock += 300  # expire unprocessed host plan; accepted reservation survives
        s = tick(m)
        assert C.c_int32(s.cooperativeResult).value == -7 and s.cooperativeProcessed == 1 and s.cooperativeAcceptedBits[0] == 1
        processed = orders[:s.cooperativeProcessed]
        accepted = [orders[i] for i in range(len(orders)) if s.cooperativeAcceptedBits[i // 32] & (1 << (i % 32))]
        assert not host_orders(m, original, processed, accepted)
    assert count(m) == 0
    unchanged_unsent(m, [10])


def hp_drop_during_submit(session, inside):
    m = setup(session, inside)
    calls = [0]
    def drop():
        calls[0] += 1
        if calls[0] == 1:
            m.w(m.target + 0x6C, 50)
    m.timer_side_effect = drop
    start(m, inside)
    assert event_ids(m) == [10]
    tick(m)
    assert event_ids(m) == [10]
    unchanged_unsent(m, [10])


def failed_source_replacement(session, inside):
    m = setup(session, inside)
    disabled = m.planes[1]
    def disable():
        m.b(disabled + 0x1C8, 1)
        # This is the controlled game-state change, not a Runtime write.
        m.before[disabled] = bytes(m.u.mem_read(disabled, 0x2000))
    if inside:
        once = [False]
        def after_first():
            if not once[0]:
                once[0] = True
                disable()
        m.timer_side_effect = after_first
        s, original = start(m, inside)
        assert event_ids(m) == [10]
        tick(m)
        s = tick(m)
        assert s.runtimeCoopDone == original.serial and s.runtimeCoopRejected == 1
    else:
        s = pointer(m, 1, 16, lambda s, i: select_all(m, s, i))
        original = m.CooperativeRequest.from_buffer_copy(bytes(s.cooperativeRequest))
        orders = host_orders(m, original)
        assert [o.id for o in orders] == [10, 20]
        disable()
        s = submit_host(m, orders)
        assert s.cooperativeAccepted == 1 and s.cooperativeRejected == 1 and s.cooperativeProcessed == 2
        accepted = [o for i, o in enumerate(orders) if s.cooperativeAcceptedBits[i // 32] & (1 << (i % 32))]
        replacement = host_orders(m, original, orders, accepted)
        assert [o.id for o in replacement] == [21], 'the original unassigned aircraft must survive replan'
        submit_host(m, replacement)
        tick(m)
        assert not host_orders(m, original, orders + replacement, accepted + replacement)
    assert event_ids(m) == [10, 21]
    unchanged_unsent(m, [10, 21])


def prior_higher_priority_lease(session, inside):
    m = setup(session, inside)
    # Submit real old Combat Attack events without cancelling persistent
    # metadata or changing the input tokens. A new explicit subset command
    # replaces only sent actors' old leases AFTER its own event is accepted.
    s = state(m)
    s.planRequest += 1
    s.planCount = len(m.planes)
    s.planTick = m.clock
    s.planFrame = m.r(0xA8ED84)
    s.planMatch = s.data.match
    for i, p in enumerate(m.planes):
        ident = m.r(p + 0x10)
        s.plan[i] = m.Order(ident, m.before_metadata[1][ident], m.r(m.target + 0x10), 0, 1, 0, 1, 0, 0)
    save(m, s)
    s = tick(m)
    assert s.planAccepted == len(m.planes) and count(m) == len(m.planes), ('seed Combat leases', s.planAccepted, s.planResult)
    old_receipts = {r.unit: r.serial for r in s.data.recentReceipts[:s.data.receiptCount] if r.owner == 5 and r.phase == 2}
    assert len(old_receipts) == len(m.planes)
    clear_queues(m)
    tick(m)
    start(m, inside)
    assert event_ids(m) == [10, 20]
    tick(m)
    unchanged_unsent(m, [10, 20])
    receipts = state(m).data.recentReceipts
    for ident in (21, 22):
        matching = [r for r in receipts if r.serial == old_receipts[ident]]
        assert matching and all(r.phase == 2 for r in matching), ('unsent old lease receipt changed', ident)


def five_cell_damage(session, inside, late=False):
    m = setup(session, inside)
    second = m.obj(0x7F5C70, m.t, m.enemy, 40, hp=100)
    m.w(second + 0x9C, 10000 + 1280)  # inclusive circular boundary
    m.w(second + 0xA0, 10000)
    m.u.mem_write(second + 0x158, struct.pack('<d', 1.))
    m.vector(0xA8EC78, m.planes + [m.target, second])
    s = state(m)
    s.aircraftAllocationPolicy = 1
    save(m, s)
    tick(m)
    calls = [0]
    if inside and late:
        def move():
            calls[0] += 1
            if calls[0] == 2:
                m.w(second + 0x9C, 10000 + 1281)
        m.timer_side_effect = move
    s = pointer(m, 1, 16, lambda s, i: select_all(m, s, i))
    assert s.pointer.accepted == 4 and s.cooperativeRequest.mode == 4
    if not inside:
        orders = host_orders(m)
        assert [o.target for o in orders] == [13, 13, 40, 40]
        if late:
            m.w(second + 0x9C, 10000 + 1281)
        s = submit_host(m, orders)
    assert count(m) == (2 if late else 4), ('five-cell admission', session, inside, late, count(m))
    data = packets(m)
    targets = [struct.unpack_from('<I', data, i * 111 + 14)[0] for i in range(count(m))]
    assert targets == ([13, 13] if late else [13, 13, 40, 40])
    assert all(data[i * 111] == 4 and data[i * 111 + 12] == 1 for i in range(count(m)))
    assert objects(m) == m.before and m.r(0xA8ECC8) == 4, 'five-cell mode must not deselect aircraft'
    assert not m.codeWrites


def native_policy(m, physical=4, override=True):
    s = state(m)
    values = [0] * 22
    values[0:3] = [s.data.match, 1, int(override)]
    values[11] = 16  # Bindings.enabled = AirFirst, mouse-only
    values[12 + 4] = physical  # MouseAirFirst
    s.pointerPolicy.values[:] = values
    s.pointerPolicySeq = 2
    save(m, s)


def message(m, kind):
    m.w(m.SH + m.Shared.heartbeat.offset, m.clock)
    m.u.mem_write(m.msg, struct.pack('<7I', 1, kind, 0, 100 | (100 << 16), m.clock, 0, 0))
    m.run(m.entry, (0, 1, m.msg))
    return m.r(m.msg + 4)


def input_guard(session, inside, case):
    m = setup(session, inside)
    extra_before = None
    if case == 'mixed':
        m.b(m.eng + 0x83, 1)
        m.extras = [m.eng]
        m.vector(0xA8EC78, m.planes + [m.target, m.eng])
        m.vector(0xA8ECB8, m.planes + m.extras)
        m.w(0xA8ECB8, 0x7E4F64)
        tick(m)
        extra_before = bytes(m.u.mem_read(m.eng, 0x2000))
    physical = 6 if case in ('left', 'left_double') else 4
    native_policy(m, physical, case != 'no_override')
    if case == 'full':
        queue(m)
    if case == 'busy':
        s = state(m)
        s.pointer.request += 1
        save(m, s)
    down = 0x201 if physical == 6 else 0x204
    if case == 'left_double':
        down = 0x203
    if case == 'right_double':
        down = 0x206
    assert message(m, down) == (down if case == 'no_override' else 0)
    up = 0x202 if physical == 6 else 0x205
    assert message(m, up) == (up if case == 'no_override' else 0)
    unchanged_unsent(m, [])
    if case not in ('full', 'busy', 'no_override'):
        start(m, inside, 16 | (65536 if physical == 6 else 1024))
        assert event_ids(m) == [10, 20]
        unchanged_unsent(m, [10, 20])
        if extra_before is not None:
            assert bytes(m.u.mem_read(m.eng, 0x2000)) == extra_before


def oversized_input(session, inside):
    m = setup(session, inside)
    native_policy(m)
    m.vector(0xA8ECB8, [m.planes[0]] * 513)
    m.w(0xA8ECB8, 0x7E4F64)
    assert message(m, 0x204) == 0 and message(m, 0x205) == 0
    assert count(m) == 0 and objects(m) == m.before and metadata(m) == m.before_metadata
    assert m.r(0xA8ECC8) == 513 and not state(m).cooperativeRequest.serial
    assert not m.codeWrites


def shift_input_guard(session, inside, physical, hold_shift=False, legacy=False):
    m = setup(session, inside)
    native_policy(m, physical or 4)
    s = state(m)
    s.pointerPolicy.values[7] = 16 if hold_shift else 17  # AirFirst hold
    if legacy:
        s.aircraftAllocationPolicy = 0
    save(m, s)
    # Modifier-only hold keys deliberately accept extra modifiers. This tests
    # a still-matching aircraft binding, not an unrelated native Shift click.
    m.held_keys.add(16)
    if not hold_shift:
        m.held_keys.add(17)
    if physical:
        down, up = (0x201, 0x202) if physical == 6 else (0x204, 0x205)
        assert message(m, down) == (down if legacy else 0)
        assert message(m, up) == (up if legacy else 0)
    if not legacy:
        physical_bit = 65536 if physical == 6 else 1024 if physical == 4 else 0
        s = pointer(m, 1, 16 | 128 | physical_bit, lambda s, i: select_all(m, s, i))
        assert s.pointer.ack == s.pointer.request and s.pointer.accepted == 0 and C.c_int32(s.pointer.result).value == -4
    assert count(m) == 0 and not state(m).cooperativeRequest.serial
    unchanged_unsent(m, [])


def unsuppressed(session, inside, physical):
    m = setup(session, inside)
    pointer(m, 1, 16 | (65536 if physical == 6 else 1024), lambda s, i: select_all(m, s, i))
    s = tick(m)
    assert s.pointer.ack == s.pointer.request and s.pointer.accepted == 0 and count(m) == 0
    unchanged_unsent(m, [])


def check_case(name, function):
    if a.filter and a.filter not in name:
        return
    try:
        function()
        rows.append({'case': name, 'status': 'PASS'})
        print('PASS', name, flush=True)
    except Exception as error:
        rows.append({'case': name, 'status': 'FAIL', 'error': repr(error)})
        traceback.print_exc()
    finally:
        while opened:
            opened.pop().u = None
        gc.collect()


with tempfile.TemporaryDirectory(prefix='aircraft-damage-runtime-') as temp:
    cpp, host = Path(temp) / 'host.cpp', Path(temp) / 'host'
    cpp.write_text(HOST_DRIVER.replace('@SOURCE@', str(SOURCE)))
    subprocess.run(['g++', '-std=c++17', '-O2', str(cpp), '-o', str(host)], check=True)
    for session in (0, 3):
        for inside in (False, True):
            prefix = f'{session}_{"runtime" if inside else "host"}'
            check_case(prefix + '_single_subset', lambda session=session, inside=inside: subset(session, inside))
            check_case(prefix + '_larger_damage_minimal_subset', lambda session=session, inside=inside: subset(session, inside, True))
            check_case(prefix + '_full_queue_no_cancel_or_deselect', lambda session=session, inside=inside: full_queue(session, inside))
            check_case(prefix + '_accepted_damage_replan_reservation', lambda session=session, inside=inside: replan_reserved(session, inside))
            check_case(prefix + '_HP_drop_late_guard', lambda session=session, inside=inside: hp_drop_during_submit(session, inside))
            check_case(prefix + '_failed_source_original_pool_replacement', lambda session=session, inside=inside: failed_source_replacement(session, inside))
            check_case(prefix + '_prior_higher_priority_lease', lambda session=session, inside=inside: prior_higher_priority_lease(session, inside))
            check_case(prefix + '_five_cell_two_targets', lambda session=session, inside=inside: five_cell_damage(session, inside))
            check_case(prefix + '_five_cell_late_radius_rejected', lambda session=session, inside=inside: five_cell_damage(session, inside, True))
            check_case(prefix + '_oversized_native_input_swallowed', lambda session=session, inside=inside: oversized_input(session, inside))
            for case in ('left', 'right', 'left_double', 'right_double', 'full', 'busy', 'no_override', 'mixed'):
                check_case(prefix + '_input_' + case, lambda session=session, inside=inside, case=case: input_guard(session, inside, case))
            for physical in (4, 6):
                check_case(prefix + f'_unsuppressed_{physical}_rejected', lambda session=session, inside=inside, physical=physical: unsuppressed(session, inside, physical))
                check_case(prefix + f'_extra_Shift_{physical}_consumed_rejected', lambda session=session, inside=inside, physical=physical: shift_input_guard(session, inside, physical))
                check_case(prefix + f'_Shift_hold_{physical}_consumed_rejected', lambda session=session, inside=inside, physical=physical: shift_input_guard(session, inside, physical, True))
                check_case(prefix + f'_legacy_Shift_{physical}_forwarded', lambda session=session, inside=inside, physical=physical: shift_input_guard(session, inside, physical, legacy=True))
            check_case(prefix + '_extra_Shift_exclusive_input_rejected', lambda session=session, inside=inside: shift_input_guard(session, inside, 0))
            check_case(prefix + '_Shift_hold_exclusive_input_rejected', lambda session=session, inside=inside: shift_input_guard(session, inside, 0, True))

output = Path(a.output)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps({'scope': SCOPE, 'dll_sha256': hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(), 'game_tested': False, 'multiplayer_tested': False, 'results': rows}, indent=2) + '\n')
raise SystemExit(any(row['status'] != 'PASS' for row in rows))

"""Execute private native hierarchical search on small synthetic subzone graphs.

Only Map::Get_Cell_Zone_Index is fixture IO; the original search, heap, score
arithmetic and route reconstruction execute in Unicorn. The optional stale
filter is an emulator experiment, NOT an installed or shipped game patch.
No executable bytes, raw dumps, real-game timing or multiplayer claims output.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP,
    UC_X86_REG_FPCW, UC_X86_REG_FPSW, UC_X86_REG_FPTAG,
)

GAME_SHA = '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
A = 0x20000000
QUEUE, HEAP, POOL = A + 0x1000, A + 0x2000, A + 0x3000
SUBZONES, FROM, TO = A + 0x4000, A + 0x4100, A + 0x4110
STACK, STOP = A + 0xf0000, A + 0xff000


def fixture(game, skip_stale=False, banned_count=0, cross_penalty=True):
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    base = game.OPTIONAL_HEADER.ImageBase
    u.mem_map(base, (game.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(base, game.get_memory_mapped_image())
    u.mem_map(A, 0x100000)
    def w(at, value):
        u.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def r(at):
        return struct.unpack('<I', u.mem_read(at, 4))[0]
    def f(at):
        return struct.unpack('<f', u.mem_read(at, 4))[0]

    # S--A--X--Y--G, S--B--X. S-A/S-B carry the same native 0.001
    # cross-block cost. A-X adds another cross-block penalty, whereas B-X
    # does not. A is inserted first at equal cost to B; X is later improved.
    # Synthetic graph data, not a claim that Map's builder emits this layout.
    edges = [(0, 1, True), (0, 2, True), (1, 3, cross_penalty),
             (2, 3, False), (3, 4, False), (4, 5, False)]
    graph = [[] for _ in range(6)]
    for left, right, cross in edges:
        graph[left].append((right, cross))
        graph[right].append((left, cross))
    w(A + 0x28, 17)
    w(A + 0x64, POOL)
    w(A + 0x68, QUEUE)
    w(QUEUE + 4, 256)
    w(QUEUE + 8, HEAP)
    w(QUEUE + 0x10, 0xffffffff)
    w(0x87f858, SUBZONES)
    u.mem_write(SUBZONES, struct.pack('<5h5h', 0, 0, 0, 0, 0, 5, 5, 5, 0, 0))
    w(FROM, 0)
    w(TO, 1)
    assert r(0x82a594) == 1, 'fixture requires native mzone 0 passability 0'
    for level in range(3):
        region = A + 0x10000 + level * 0x10000
        for off, delta in ((0x40, 0), (0x4c, 0x100), (0x58, 0x200)):
            w(A + off + level * 4, region + delta)
        tracking, links, banned = region + 0x1000, region + 0x2000, region + 0x3000
        w(0x87f878 + level * 24, tracking)
        w(A + 0x78 + level * 24, banned)
        w(A + 0x84 + level * 24, banned_count)
        for i in range(banned_count):
            w(banned + i * 4, ((128 + i) << 16) | (129 + i))
        for node, neighbors in enumerate(graph):
            ptr = tracking + node * 36
            w(ptr + 4, links + node * 64)
            w(ptr + 0x10, len(neighbors))
            u.mem_write(ptr + 0x18, struct.pack('<H', node))
            w(ptr + 0x1c, 0)  # native cost 1.0, passable for mzone 0
            for i, (target, cross) in enumerate(neighbors):
                u.mem_write(links + node * 64 + i * 8, struct.pack('<IB3x', target, cross))

    counts = {'pops': [0]*3, 'expansions': [0]*3, 'edge_checks': [0]*3,
              'banned_comparisons': [0]*3, 'stale_pops': [0]*3,
              'instructions': 0, 'zone_index_calls': 0}
    popped = []
    def hook(uc, at, size, _):
        counts['instructions'] += 1
        sp = u.reg_read(UC_X86_REG_ESP)
        if at == 0x56d3f0:
            counts['zone_index_calls'] += 1
            u.reg_write(UC_X86_REG_EAX, r(r(sp + 4)))
            u.reg_write(UC_X86_REG_EIP, r(sp))
            u.reg_write(UC_X86_REG_ESP, sp + 8)
        elif at in (0x42c4eb, 0x42c4fe, 0x42c52c, 0x42c656):
            level = r(sp + 0x38)
            if at == 0x42c4eb:
                node = u.reg_read(UC_X86_REG_EAX)
                subzone, score = r(node + 4), f(node + 8)
                best = f(r(A + 0x58 + level * 4) + subzone * 4)
                stale = score > best
                counts['pops'][level] += 1
                counts['stale_pops'][level] += int(stale)
                popped.append({'level': level, 'subzone': subzone,
                               'score': score, 'best': best, 'stale': stale})
                if skip_stale and stale:
                    u.reg_write(UC_X86_REG_EIP, 0x42c740)
            else:
                key = {0x42c4fe: 'expansions', 0x42c52c: 'edge_checks',
                       0x42c656: 'banned_comparisons'}[at]
                counts[key][level] += 1

    u.hook_add(UC_HOOK_CODE, hook)
    u.mem_write(STACK, struct.pack('<5I', STOP, FROM, TO, 0, 0))
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_ECX, A)
    u.reg_write(UC_X86_REG_FPCW, 0x37f)
    u.reg_write(UC_X86_REG_FPSW, 0)
    u.reg_write(UC_X86_REG_FPTAG, 0xffff)
    u.emu_start(0x42c290, STOP, count=1000000)
    assert u.reg_read(UC_X86_REG_EIP) == STOP, 'native function did not finish'
    assert u.reg_read(UC_X86_REG_EAX) & 255 == 1, 'native route failed'
    paths = []
    for level in range(3):
        n = r(A + 0xc74 + 4 * level)
        assert 0 < n <= 6, n
        paths.append(list(struct.unpack('<' + 'H' * n, u.mem_read(A + 0xbc + 1000 * level, 2 * n))))
    semantic_state = [bytes(u.mem_read(A + 0x10000 + i*0x10000, 0x300)).hex() for i in range(3)]
    return {'counts': counts, 'paths': paths, 'popped': popped, 'semantic_state': semantic_state}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    assert hashlib.sha256(args.game.read_bytes()).hexdigest() == GAME_SHA
    game = pefile.PE(str(args.game))
    results = []
    for banned in (0, 64, 256):
        native = fixture(game, banned_count=banned)
        filtered = fixture(game, skip_stale=True, banned_count=banned)
        assert native['paths'] == filtered['paths'] == [[0, 2, 3, 4, 5]] * 3
        assert native['semantic_state'] == filtered['semantic_state']
        assert native['counts']['stale_pops'] == [0, 0, 1]
        assert native['counts']['expansions'][2] == 6
        assert filtered['counts']['expansions'][2] == 5
        assert native['counts']['edge_checks'][2] - filtered['counts']['edge_checks'][2] == 3
        for result in (native, filtered):
            del result['semantic_state']
        results.append({'banned_entries': banned, 'native': native,
                        'emulated_stale_filter': filtered})
    no_duplicate = fixture(game, cross_penalty=False)
    assert no_duplicate['counts']['stale_pops'] == [0, 0, 0]
    report = {'scope': __doc__, 'game_sha256': GAME_SHA, 'results': results,
              'equal_cost_control': no_duplicate['counts'],
              'live_game_tested': False, 'shipped_patch': False}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': 'PASS', 'cases': len(results) + 1,
                      'coarse_native': results[0]['native']['counts'],
                      'coarse_filtered': results[0]['emulated_stale_filter']['counts'],
                      'banned_scaling': [{'entries': x['banned_entries'],
                          'comparisons': x['native']['counts']['banned_comparisons']} for x in results]}, indent=2))


if __name__ == '__main__':
    main()

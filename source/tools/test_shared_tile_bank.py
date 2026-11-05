#!/usr/bin/env python3
"""Compile real tile() bodies + actual immutable image bank, with controlled IO."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def tile_body(text):
    start = text.index('bool tile(uint32_t type,')
    opened = text.index('{', start)
    depth = 0
    for pos in range(opened, len(text)):
        depth += (text[pos] == '{') - (text[pos] == '}')
        if not depth:
            return text[start:pos + 1]
    raise AssertionError('unterminated production tile body')

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--cxx', default='g++')
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args()
    baseline = ROOT / 'assist/tests/fixtures/shared_tile_baseline/camera_tile.inc'
    old = baseline.read_text()
    # Baseline is source-bound to pre.11 e9e51d3; raw extracted function only.
    assert hashlib.sha256(old.encode()).hexdigest() == 'bd97ad6aa78102f475170b91fe4fa2798415534d5c9a3da9049325c61b6494a0'
    current = tile_body((ROOT / 'external_reader/source/camera_assets.h').read_text())
    with tempfile.TemporaryDirectory(prefix='shared-tile-bank-') as work:
        work = Path(work)
        (work / 'shared_tile_before.inc').write_text(old.replace('rendermemory::SharedTiles', 'TileBudget'))
        (work / 'shared_tile_after.inc').write_text(current)
        flags = ['-std=c++17', '-O1' if a.sanitize else '-O2', '-pthread', '-I', str(work)]
        if a.sanitize:
            flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        target = work / 'shared-tile-bank'
        subprocess.run([a.cxx, *flags, str(ROOT / 'assist/tests/shared_tile_bank.cpp'), '-o', str(target)], check=True)
        subprocess.run([str(target)], check=True)

if __name__ == '__main__':
    main()

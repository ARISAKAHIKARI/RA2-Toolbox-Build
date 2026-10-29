#!/usr/bin/env python3
"""Exercise mixed-architecture release gates on actual built PE files."""
from pathlib import Path
import argparse
import shutil
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from pe32_image import Image
from verify_windows_outputs import BROKER, NAMES, NEURAL, verify


def rejected(root, expected, neural_helper=False):
    _, errors = verify(root, 'x64', neural_helper)
    if not errors or not any(expected in error for error in errors):
        raise AssertionError(f'Expected rejection containing {expected!r}, got {errors!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--x86-root', type=Path, required=True)
    parser.add_argument('--x64-root', type=Path, required=True)
    parser.add_argument('--neural-helper', action='store_true')
    args = parser.parse_args()
    records, errors = verify(args.x86_root)
    assert not errors, errors
    assert set(records) == set(NAMES)
    records, errors = verify(args.x64_root, 'x64')
    assert not errors, errors
    assert records[NAMES[0]]['machine'] == 'PE32+ x64'
    assert records[NAMES[1]]['machine'] == records[BROKER]['machine'] == 'PE32 x86'
    assert verify(args.x64_root)[1], 'Default x86 gate accepted x64'
    if args.neural_helper:
        neural_records, errors = verify(args.x64_root, 'x64', True)
        assert not errors, errors
        assert neural_records[NEURAL]['machine'] == 'PE32+ x64'
    names = (*NAMES, BROKER, 'BUILD_SOURCE_MANIFEST.json') + ((NEURAL,) if args.neural_helper else ())
    originals = {name: (args.x64_root / name).read_bytes() for name in names}
    with tempfile.TemporaryDirectory(prefix='ra2-mixed-pe-negative-') as temp:
        root = Path(temp)

        def reset():
            for name, raw in originals.items():
                (root / name).write_bytes(raw)

        reset()
        shutil.copyfile(args.x86_root / NAMES[0], root / NAMES[0])
        rejected(root, 'expected x64 host')
        reset()
        (root / NAMES[1]).write_bytes(originals[NAMES[0]])
        rejected(root, 'expected x86 game component')
        reset()
        (root / BROKER).write_bytes(originals[NAMES[0]])
        rejected(root, 'expected x86 game component')
        reset()
        (root / BROKER).unlink()
        rejected(root, BROKER)
        reset()
        shutil.copyfile(args.x86_root / NAMES[0], root / BROKER)
        rejected(root, 'unexpected x86 broker export')
        reset()
        raw = bytearray(originals[NAMES[1]])
        pe = Image(raw)
        at = pe.offset(pe.exports()['AssistProtocolInfo'], 8)
        struct.pack_into('<I', raw, at, 81)
        (root / NAMES[1]).write_bytes(raw)
        rejected(root, 'protocol or Shared structure size mismatch')
        reset()
        raw = bytearray(originals[NAMES[0]])
        pe = Image(raw, allow_x64=True)
        symbol = pe.exports()['AssistHostMappingPrefix']
        raw[pe.offset(symbol, 2)] ^= 1
        (root / NAMES[0]).write_bytes(raw)
        rejected(root, 'host mapping namespace mismatch')
        reset()
        raw = originals[NAMES[0]]
        token = b'processorArchitecture="amd64"'
        first = raw.index(token)
        second = raw.index(token, first + len(token))
        changed = bytearray(raw)
        changed[second:second + len(token)] = token.replace(b'amd64', b'x86  ')
        (root / NAMES[0]).write_bytes(changed)
        rejected(root, 'Common Controls dependency identity/version/architecture mismatch')
        reset()
        raw = originals[BROKER]
        assert b'processorArchitecture="x86"' in raw
        (root / BROKER).write_bytes(raw.replace(b'processorArchitecture="x86"', b'processorArchitecture="arm"'))
        rejected(root, 'broker manifest assembly architecture mismatch')
        reset()
        raw = originals[BROKER]
        assert b'level="asInvoker"' in raw
        (root / BROKER).write_bytes(raw.replace(b'level="asInvoker"', b'level="highestAv"'))
        rejected(root, 'asInvoker/uiAccess=false')
        reset()
        raw = bytearray(originals[NAMES[0]])
        pe = Image(raw, allow_x64=True)
        rva, _ = pe.directory(1)
        for index in range(4096):
            row = pe.unpack('<5I', pe.offset(rva + index * 20, 20))
            if not any(row):
                raise AssertionError('Kernel32 import was not found')
            if pe.cstring(row[3]).lower() == 'kernel32.dll':
                at = pe.offset(row[3], 12)
                raw[at:at + 12] = b'missing1.dll'
                break
        (root / NAMES[0]).write_bytes(raw)
        rejected(root, 'unbundled non-system dependency')
        if args.neural_helper:
            reset()
            (root / NEURAL).unlink()
            rejected(root, NEURAL, True)
            reset()
            (root / NEURAL).write_bytes(originals[BROKER])
            rejected(root, 'expected x64 host', True)
            reset()
            (root / NEURAL).write_bytes(originals[NAMES[0]])
            rejected(root, 'unexpected neural helper export', True)
            reset()
            raw = originals[NEURAL]
            assert b'level="asInvoker"' in raw
            (root / NEURAL).write_bytes(raw.replace(b'level="asInvoker"', b'level="highestAv"'))
            rejected(root, 'asInvoker/uiAccess=false', True)
    print('PASS: x86 pair and x64 host/x86 runtime/broker; eleven real-PE negative architecture, ABI, namespace, manifest and dependency gates')
    print('NOTE: fixture mutates copies only; no Windows image is loaded or executed')
    if args.neural_helper:
        print('PASS: optional neural helper is separately gated; missing, x86, exported-entry and elevated-manifest substitutes rejected')


if __name__ == '__main__':
    main()

"""Run the production FPS callback using loader-owned data in a test-only PE."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--repeat', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.repeat <= 10:
        parser.error('--repeat must be between 1 and 10')
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    executable = out / 'single_player_fps_windows.exe'
    source_paths = [
        'assist/tests/single_player_fps_windows.cpp',
        'assist/source/single_player_fps_runtime.h',
        'assist/source/single_player_fps.h',
        'assist/source/tech_ui_breakpoint_core.h',
        'tools/test_single_player_fps_windows.py',
    ]
    digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    before = {name: digest(root / name) for name in source_paths}
    command = [
        str(args.zig.resolve()), 'c++', '-target', 'x86-windows-gnu',
        '-fms-extensions', '-fasm-blocks', '-std=c++17', '-O2', '-UNDEBUG',
        '-Wl,--image-base,0x400000',
        str(root / source_paths[0]), '-o', str(executable), '-lkernel32', '-luser32',
    ]
    result = dict(passed=False, scope=__doc__, assertions_enabled=True,
                  source_sha256=before, process_runs=[], game_tested=False,
                  multiplayer_tested=False, allocation_retry_used=False)
    try:
        build = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding='utf-8', errors='replace')
        (out / 'build.log').write_text(build.stdout, encoding='utf-8')
        print(build.stdout, end='', flush=True)
        result['build_exit_code'] = build.returncode
        build.check_returncode()

        # Existing sidebar SEH fixture policy: only this newly compiled test
        # executable loses DYNAMIC_BASE. Product EXE/DLL flags are untouched.
        raw = bytearray(executable.read_bytes())
        if len(raw) < 64 or raw[:2] != b'MZ':
            raise ValueError('Missing fixture DOS header')
        pe = struct.unpack_from('<I', raw, 60)[0]
        optional = pe + 24
        if optional + 72 > len(raw) or raw[pe:pe + 4] != b'PE\0\0':
            raise ValueError('Missing fixture PE header')
        machine = struct.unpack_from('<H', raw, pe + 4)[0]
        magic = struct.unpack_from('<H', raw, optional)[0]
        base = struct.unpack_from('<I', raw, optional + 28)[0]
        image_size = struct.unpack_from('<I', raw, optional + 56)[0]
        flags = struct.unpack_from('<H', raw, optional + 70)[0]
        if machine != 0x14c or magic != 0x10b or base != 0x400000 or base + image_size < 0xA8F000:
            raise ValueError('Fixture must be a covering x86 PE32 image at 0x400000')
        original_digest = hashlib.sha256(raw).hexdigest()
        struct.pack_into('<H', raw, optional + 70, flags & ~0x40)
        executable.write_bytes(raw)
        if struct.unpack_from('<H', executable.read_bytes(), optional + 70)[0] != flags & ~0x40:
            raise ValueError('Fixture image policy did not persist')
        result['fixture_image_policy'] = dict(
            base=base, size_of_image=image_size, arena_bytes=0x800000,
            dll_characteristics_before=flags, dll_characteristics_after=flags & ~0x40,
            original_sha256=original_digest, executed_sha256=digest(executable),
            ownership='Runtime verifies actual array bounds, MEM_IMAGE, committed writable pages and own module',
        )
        for index in range(args.repeat):
            run = subprocess.run([str(executable)], stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True,
                                 encoding='utf-8', errors='replace', timeout=60)
            (out / f'run-{index + 1}.log').write_text(run.stdout, encoding='utf-8')
            result['process_runs'].append(dict(run=index + 1, exit_code=run.returncode,
                                               stdout=run.stdout))
            print(f'Fresh process {index + 1}/{args.repeat}:\n{run.stdout}', end='', flush=True)
            run.check_returncode()
        result['source_hash_stable'] = before == {name: digest(root / name) for name in source_paths}
        if not result['source_hash_stable']:
            raise ValueError('Sources changed during the fixture checks')
        result['passed'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: all fresh processes executed the complete production FPS callback checks; no retries or skipped cases')


if __name__ == '__main__':
    main()

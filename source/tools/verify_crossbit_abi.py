#!/usr/bin/env python3
"""Compile the real Windows wire declarations for x86/x64 and compare their ABI.

The generated executables only expose constant layout data; they are never run.
Both compiles target Windows, so LLP64 long/wchar_t and target alignment are real,
not Linux typedef substitutes. No game installation, image or proprietary asset
is required.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import tempfile

from pe32_image import Image

ROOT = Path(__file__).resolve().parent.parent
DECLARATIONS = (
    ('assist/source/protocol.h', 'Shared', 'assist::Shared'),
    ('shared/source/debug_entry_types.h', 'Config', 'debugentries::Config'),
    ('shared/source/debug_entry_types.h', 'Observation', 'debugentries::Observation'),
    ('shared/source/debug_entry_types.h', 'Snapshot', 'debugentries::Snapshot'),
    ('assist/source/protocol.h', 'Snapshot', 'assist::Snapshot'),
    ('assist/source/native_placement_policy.h', 'Offer', 'assist::nativeplacement::Offer'),
    ('assist/source/native_placement_policy.h', 'Request', 'assist::nativeplacement::Request'),
    ('assist/source/native_placement_policy.h', 'Preview', 'assist::nativeplacement::Preview'),
    ('cursor/source/bridge.h', 'Shared', '::Shared'),
    ('unit_monitor/source/core.h', 'Shared', 'um::Shared'),
    ('unit_monitor/source/core.h', 'Snapshot', 'um::Snapshot'),
    ('shared/source/runtime_protocol.h', 'Control', 'runtime::Control'),
    ('shared/source/frame_signal_protocol.h', 'Channel', 'framesignal::Channel'),
    ('shared/source/world_frame_transport.h', 'DataRef', 'worldframe::DataRef'),
    ('shared/source/world_frame_transport.h', 'Slot', 'worldframe::Slot'),
    ('shared/source/world_frame_transport.h', 'Channel', 'worldframe::Channel'),
    ('shared/source/world_frame_transport.h', 'DataHeader', 'worldframe::DataHeader'),
    ('assist/source/cooperative_frame.h', 'FrameInput', 'assist::cooperative::FrameInput'),
    ('shared/source/runtime_broker_protocol.h', 'Packet', 'runtimebroker::Packet'),
    ('shared/source/game_entry_api.h', 'Request', 'gameentry::Request'),
    ('shared/source/neural_frame_protocol.h', 'Header', 'neuralframe::Header'),
)
PAYLOADS = (
    'assist::Label', 'assist::Super', 'assist::Type', 'assist::Threat',
    'assist::CrateMark', 'assist::CrateResult', 'assist::TaskReceiptSummary',
    'assist::PlayerTestInfo', 'assist::planner::World', 'assist::planner::Actor',
    'assist::planner::Weapon', 'assist::planner::Order',
    'assist::cooperative::Request', 'assist::cooperative::Assignment',
    'assist::pointerinput::Mailbox', 'assist::pointerinput::NativePolicy',
    'assist::pointerinput::SuppressedClick', 'assist::oreconfig::Config',
    'assist::featuretypes::Config', 'assist::construction::InputPolicy',
    'assist::construction::InputClick', 'assist::compute::LoadInput',
    'assist::compute::LoadResult', 'assist::compute::RepairInput',
    'assist::compute::RepairResult', 'assist::compute::QuotaConfig',
    'assist::unloadcompute::Input', 'assist::unloadcompute::Result',
    'assist::missilecompute::Input', 'assist::missilecompute::Result',
    'assist::burstcompute::Input', 'assist::burstcompute::Result',
    'assist::dodgecompute::Input', 'assist::dodgecompute::Result',
    'assist::formation::Policy', 'assist::formation::Preview',
    'assist::moorders::GesturePolicy', 'assist::moorders::Point',
    'assist::moorders::Line', 'assist::moorders::Preview',
    'assist::loading::Infantry', 'assist::loading::Vehicle',
    'assist::loading::Composition', 'um::Type', 'um::Player', 'um::Cell',
    'neuralframe::Layout', 'assist::cooperative::Unit', 'assist::cooperative::Target',
    'assist::hardwareperf::Counters', 'assist::hardwareperf::Snapshot',
    'worldframe::Key', 'worldframe::Page', 'worldframe::Counters',
)


def members(path, name):
    """Fail closed on unsupported syntax rather than silently omit new fields."""
    text = path.read_text(encoding='utf-8')
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    found = re.search(r'\bstruct\s+(?:alignas\([^)]*\)\s+)?' + re.escape(name) + r'\s*\{', text)
    if not found:
        raise ValueError(f'{path}: struct {name} missing')
    start, depth = found.end(), 1
    at = start
    while depth and at < len(text):
        if text[at] == '{':
            depth += 1
        elif text[at] == '}':
            depth -= 1
        at += 1
    if depth:
        raise ValueError(f'{path}: unterminated struct {name}')
    result = []
    for declaration in text[start:at - 1].split(';'):
        declaration = declaration.strip()
        if not declaration:
            continue
        match = re.fullmatch(r'(?:(?:volatile|const)\s+)*[\w:]+\s+(.+)', declaration, re.S)
        if not match:
            raise ValueError(f'Unsupported wire declaration: {name}: {declaration}')
        for item in match.group(1).split(','):
            field = re.fullmatch(r'\s*([A-Za-z_]\w*)(?:\s*\[[^\]]+\])*(?:\s*=[^,]+|\s*\{\})?\s*', item, re.S)
            if not field:
                raise ValueError(f'Unsupported wire field: {name}: {item}')
            result.append(field.group(1))
    if not result or len(set(result)) != len(result):
        raise ValueError(f'{path}: no fields or duplicate fields in {name}')
    return result


def fixture():
    entries = [(name, name) for name in ('assist::Version', 'runtime::Version',
                                       'gameentry::Version', 'runtimebroker::Version', 'neuralframe::Version', 'framesignal::Version')]
    for relative, name, cpp in DECLARATIONS:
        entries.extend(((f'sizeof({cpp})', f'sizeof({cpp})'),
                        (f'alignof({cpp})', f'alignof({cpp})')))
        entries.extend((cpp + '.' + field, f'offsetof({cpp},{field})')
                       for field in members(ROOT / relative, name))
    for cpp in PAYLOADS:
        entries.extend(((f'sizeof({cpp})', f'sizeof({cpp})'),
                        (f'alignof({cpp})', f'alignof({cpp})')))
    if any(len(name.encode()) >= 64 for name, _ in entries):
        raise ValueError('Layout label exceeds fixed record size')
    headers = '\n'.join('#include "' + str(ROOT / relative) + '"'
                        for relative in dict.fromkeys([row[0] for row in DECLARATIONS] + ['shared/source/world_frame.h']))
    types = set(PAYLOADS) | {row[2] for row in DECLARATIONS}
    assertions = '\n'.join(f'static_assert(std::is_standard_layout<{cpp}>::value && std::is_trivially_copyable<{cpp}>::value,"wire type must remain value-only: {cpp}");' for cpp in sorted(types))
    records = '\n'.join(' {' + json.dumps(name) + ',uint32_t(' + value + ')},'
                        for name, value in entries)
    return r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <type_traits>
''' + headers + '\n' + assertions + r'''
static_assert(sizeof(LONG)==4&&sizeof(DWORD)==4&&sizeof(int)==4&&sizeof(wchar_t)==2,"Windows LLP64 primitives");
static_assert(sizeof(((assist::Shared*)0)->hostWindow)==4,"Cross-process HWND stays 32 bits");
static_assert(sizeof(((assist::PlayerTestInfo*)0)->pointer)==4,"Game pointer stays 32 bits");
static_assert(sizeof(((::Shared*)0)->gameWindow)==4,"Cursor HWND stays 32 bits");
static_assert(sizeof(((um::Type*)0)->ptr)==4,"Monitor game pointer stays 32 bits");
static_assert(offsetof(assist::Shared,seq)%alignof(LONG)==0&&offsetof(assist::Shared,worldSeq)%alignof(LONG)==0&&offsetof(runtime::Control,dispatches)%alignof(LONG)==0,"Interlocked sequence fields remain naturally aligned");
struct LayoutEntry {char name[64];uint32_t value;};
static_assert(sizeof(LayoutEntry)==68,"Layout fixture record ABI");
extern "C" {
extern __declspec(dllexport) const uint32_t HostPointerBytes=sizeof(void*);
extern __declspec(dllexport) const LayoutEntry CrossBitLayout[]={
''' + records + r'''
};
extern __declspec(dllexport) const uint32_t CrossBitLayoutBytes=sizeof(CrossBitLayout);
}
int main(){return GetCurrentProcessId()==0;}
'''


def layout(image):
    exports = image.exports()
    count, = struct.unpack('<I', image.at_rva(exports['CrossBitLayoutBytes'], 4))
    if not count or count % 68:
        raise ValueError('Invalid layout fixture data size')
    raw = image.at_rva(exports['CrossBitLayout'], count)
    result = {}
    for at in range(0, count, 68):
        name, value = struct.unpack_from('<64sI', raw, at)
        name = name.split(b'\0', 1)[0].decode('ascii')
        if name in result:
            raise ValueError('Duplicate layout fixture label: ' + name)
        result[name] = value
    return result


def reject(callback, message):
    try:
        callback()
    except ValueError:
        return
    raise AssertionError(message)


def parser_checks(raw32, raw64):
    pe = Image(raw64, allow_x64=True)
    assert Image(raw32).pointer_bytes == 4 and pe.pointer_bytes == 8
    reject(lambda: Image(raw64), 'Default game-image parser accepted x64')
    assert any('GetCurrentProcessId' in functions for functions in pe.imports().values())
    changed = bytearray(raw64)
    struct.pack_into('<H', changed, pe.nt + 4, 0x14c)
    reject(lambda: Image(changed, allow_x64=True), 'Mixed machine/optional-header format accepted')
    changed = bytearray(raw64)
    struct.pack_into('<H', changed, pe.nt + 20, 96)
    reject(lambda: Image(changed, allow_x64=True), 'Truncated PE32+ optional header accepted')
    rva, _ = pe.directory(1)
    row = pe.unpack('<5I', pe.offset(rva, 20))
    thunk = pe.offset(row[0] or row[4], 8)
    changed = bytearray(raw64)
    struct.pack_into('<Q', changed, thunk, 0x800000000000007b)
    assert 'ordinal:123' in next(iter(Image(changed, allow_x64=True).imports().values()))
    changed = bytearray(raw64)
    struct.pack_into('<Q', changed, thunk, 0x100000001)
    reject(lambda: Image(changed, allow_x64=True).imports(), '64-bit non-ordinal import RVA silently truncated')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zig', default=os.environ.get('ZIG', 'zig'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    version = subprocess.check_output([args.zig, 'version'], text=True).strip()
    if version != '0.13.0':
        raise ValueError('Zig 0.13.0 is required')
    source = fixture()
    records, binaries = {}, {}
    with tempfile.TemporaryDirectory(prefix='ra2-crossbit-abi-') as temp:
        folder = Path(temp)
        cpp = folder / 'layout.cpp'
        cpp.write_text(source, encoding='utf-8')
        for arch, target in (('x86', 'x86-windows-gnu'), ('x64', 'x86_64-windows-gnu')):
            exe = folder / (arch + '.exe')
            subprocess.run([args.zig, 'c++', '-target', target, '-std=c++17', '-O2',
                            '-fno-exceptions', '-fno-rtti', str(cpp), '-o', str(exe),
                            '-lkernel32', '-luser32'], check=True)
            raw = exe.read_bytes()
            pe = Image(raw, allow_x64=arch == 'x64')
            expected = 8 if arch == 'x64' else 4
            assert struct.unpack('<I', pe.at_rva(pe.exports()['HostPointerBytes'], 4))[0] == expected
            records[arch] = layout(pe)
            binaries[arch] = raw
    parser_checks(binaries['x86'], binaries['x64'])
    differences = {name: {'x86': records['x86'].get(name), 'x64': records['x64'].get(name)}
                   for name in sorted(set(records['x86']) | set(records['x64']))
                   if records['x86'].get(name) != records['x64'].get(name)}
    baseline = {'assist::Version': 101, 'runtime::Version': 4, 'gameentry::Version': 3,
                'runtimebroker::Version': 1, 'sizeof(assist::Shared)': 23162528,
                'assist::Shared.aircraftAllocationPolicy': 23125936,
                'assist::Shared.nativeCombatQueries': 23125940,
                'assist::Shared.singlePlayerFPS': 23154136,
                'assist::Shared.singlePlayerFPSHits': 23154148,
                'assist::Shared.minerDefenseEnabled': 23154152,
                'assist::Shared.minerDefenseRangeCells': 23154156,
                'assist::Shared.minerDefenseActive': 23154160,
                'assist::Shared.minerDefenseAttacks': 23154164,
                'assist::Shared.minerDefenseResumes': 23154168,
                'assist::Shared.debugPolicySeq': 23154172,
                'assist::Shared.debugPolicy': 23154176,
                'assist::Shared.debugSnapshotSeq': 23155208,
                'assist::Shared.debugSnapshot': 23155212,
                'assist::Shared.debugVisible': 23162392,
                'assist::Shared.hardwarePerfSeq': 23162396,
                'assist::Shared.hardwarePerf': 23162400,
                'assist::Shared.worldSampleSeq': 23162496,
                'assist::Shared.worldSampleSerial': 23162500,
                'assist::Shared.worldSampleFrame': 23162524,
                'sizeof(assist::hardwareperf::Snapshot)': 96,
                'sizeof(worldframe::Key)': 64, 'sizeof(worldframe::Page)': 5128,
                'sizeof(worldframe::DataHeader)': 64, 'sizeof(worldframe::DataRef)': 8,
                'sizeof(debugentries::Config)': 1032, 'sizeof(debugentries::Observation)': 28,
                'sizeof(debugentries::Snapshot)': 7180,
                'sizeof(runtime::Control)': 176, 'sizeof(gameentry::Request)': 108,
                'sizeof(runtimebroker::Packet)': 148,
                'neuralframe::Version': 1, 'sizeof(neuralframe::Header)': 256}
    baseline_matches = all(records['x86'].get(name) == value for name, value in baseline.items())
    result = {'protocol': records['x86']['assist::Version'],
              'native_pointer_bytes': {'x86': 4, 'x64': 8},
              'layout_entries': len(records['x86']), 'differences': differences,
              'layouts': records, 'pe_parser_negative_checks': 'pass',
              'wire_baseline': baseline, 'wire_baseline_matches': baseline_matches,
              'passed': not differences and baseline_matches,
              'executed_windows_images': False,
              'fixture_sha256': hashlib.sha256(source.encode()).hexdigest()}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    if differences:
        print(json.dumps(differences, indent=2))
        raise SystemExit('FAIL: Windows x86/x64 shared-memory layouts differ')
    # Protocol94 enlarges fixed task, cooperation and reservation capacities.
    # Protocol95 appends the experimental permanent-grant switch and status.
    # Protocol97 adds single-player FPS; Protocol98 adds miner-defense control/status.
    # Protocol99 adds an independent SpiderDefense type filter.
    # Lock the measured x86/x64 layout instead of accepting arbitrary growth.
    assert baseline_matches, 'Protocol/version or deployed x86 wire baseline changed'
    print(f'PASS: {len(records["x86"])} Windows x86/x64 wire size/alignment/field offsets match; protocol101 Shared23162528, Runtime176, manager108')
    print('PASS: real PE32+/64-bit imports, ordinal bit, bounds and strict default x86 parser checks; no Windows image executed')


if __name__ == '__main__':
    main()

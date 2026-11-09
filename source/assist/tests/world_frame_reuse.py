"""Check publication identity, storage reuse and failures on real transport code."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--sanitize', default='undefined')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
windows = (repo / 'source/tests/winmock/windows.h').read_text() + r'''
#include <atomic>
#define PAGE_READWRITE 4
#define FILE_MAP_ALL_ACCESS 0xF001F
#define FILE_MAP_READ 4
#define ERROR_ALREADY_EXISTS 183
inline void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}
inline LONG InterlockedIncrement(volatile LONG*p){return __sync_add_and_fetch(p,1);}
inline LONG InterlockedExchange(volatile LONG*p,LONG n){return __sync_lock_test_and_set(p,n);}
extern "C" {
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,DWORD,const wchar_t*);
HANDLE OpenFileMappingW(DWORD,BOOL,const wchar_t*);
void*MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,SIZE_T);
BOOL UnmapViewOfFile(LPCVOID);
BOOL QueryPerformanceCounter(LARGE_INTEGER*);
}
'''
(out / 'windows.h').write_text(windows)
command = ['g++', '-std=c++17', '-O2', '-pthread', '-I' + str(out),
           str(Path(__file__).with_suffix('.cpp')), '-Wl,--wrap=malloc', '-o', str(out / 'world-frame-reuse')]
if args.sanitize:
    command += ['-fsanitize=' + args.sanitize, '-fno-sanitize-recover=all']
build = subprocess.run(command, text=True, capture_output=True)
(out / 'build.log').write_text(build.stdout + build.stderr)
build.check_returncode()
run = subprocess.run([str(out / 'world-frame-reuse')], text=True, capture_output=True)
(out / 'run.log').write_text(run.stdout + run.stderr)
paths = ['source/shared/source/world_frame.h', 'source/shared/source/world_frame_pool.h',
         'source/shared/source/world_frame_transport.h', 'source/assist/tests/world_frame_core.cpp',
         'source/assist/tests/world_frame_reuse.cpp', 'source/assist/tests/world_frame_reuse.py']
receipt = {'passed': run.returncode == 0, 'command': command, 'returncode': run.returncode,
           'asan_options': os.environ.get('ASAN_OPTIONS', ''),
           'rows': [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')],
           'source_sha256': {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in paths},
           'windows_executed': False, 'game_executed': False,
           'boundary': 'Real production pool, transport and snapshots; existing Win32 mapping model. Counts are not timings or game FPS.'}
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)

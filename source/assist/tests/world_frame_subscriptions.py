"""Run the actual Client/Server lifetime regression at controlled OS boundaries."""
from pathlib import Path
import argparse,hashlib,json,subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--expect-fixed',action='store_true')
args=parser.parse_args()
tests=Path(__file__).resolve().parent
source=tests.parent.parent
repo=source.parent
out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
windows=(source/'tests/winmock/windows.h').read_text()+r'''
#include <atomic>
#define PAGE_READWRITE 4
#define FILE_MAP_ALL_ACCESS 0xF001F
#define FILE_MAP_READ 4
#define ERROR_ALREADY_EXISTS 183
inline void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}
inline LONG InterlockedIncrement(volatile LONG*value){return __sync_add_and_fetch(value,1);}
inline LONG InterlockedExchange(volatile LONG*value,LONG next){return __sync_lock_test_and_set(value,next);}
extern "C" {
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,DWORD,const wchar_t*);
HANDLE OpenFileMappingW(DWORD,BOOL,const wchar_t*);
void*MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,SIZE_T);
BOOL UnmapViewOfFile(LPCVOID);
BOOL QueryPerformanceCounter(LARGE_INTEGER*);
}
'''
(out/'windows.h').write_text(windows)
names=['shared/source/world_frame.h','shared/source/world_frame_capture.h','shared/source/world_frame_transport.h',
       'assist/source/world_frame_runtime.h','assist/source/world_lifecycle.h',
       'external_reader/source/process_reader.h','external_reader/source/process_connection.h','external_reader/source/read_page_budget.h',
       'assist/tests/world_frame_core.cpp','assist/tests/world_frame_subscriptions.cpp','assist/tests/world_frame_subscriptions.py','tests/winmock/windows.h']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str(source/name):digest(source/name) for name in names}
binary=out/'world-frame-subscriptions'
command=['g++','-std=c++17','-O2','-UNDEBUG','-pthread','-I'+str(out),str(tests/'world_frame_subscriptions.cpp'),'-o',str(binary)]
compile_result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
(out/'compile.log').write_text(compile_result.stdout)
compile_result.check_returncode()
run_command=[str(binary)]+(['--expect-fixed'] if args.expect_fixed else [])
result=subprocess.run(run_command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
(out/'run.log').write_text(result.stdout)
try: metrics=json.loads(result.stdout.strip().splitlines()[-1])
except (json.JSONDecodeError,IndexError): metrics={'failure_output':result.stdout}
report=dict(pass_=result.returncode==0,required_contract=args.expect_fixed,command=command,run_command=run_command,exit_code=result.returncode,
            source_sha256=hashes,source_unchanged=all(digest(Path(p))==d for p,d in hashes.items()),
            result=metrics,windows_executed=False,game_executed=False)
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['result'],indent=2))
raise SystemExit(result.returncode)

"""Verify actual Runtime/transport retention, using existing Win32 memory seams."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',required=True,type=Path);p.add_argument('--expect-failure',action='store_true');a=p.parse_args()
out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);here=Path(__file__).resolve().parent;repo=here.parents[2]
windows=(repo/'source/tests/winmock/windows.h').read_text()+r'''
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
(out/'windows.h').write_text(windows);exe=out/'retention'
cmd=['g++','-std=c++17','-O1','-UNDEBUG','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(out),str(here/'world_frame_retention.cpp'),'-o',str(exe)]
r=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.stdout+r.stderr)
rows=[]
for case in ('demand','retry','busy','publication') + (() if a.expect_failure else ('leases','backoff','paused-demand','pending-demand')):
 r=subprocess.run([str(exe),case],capture_output=True,text=True);(out/(case+'.log')).write_text(r.stdout+r.stderr);print(case+': '+r.stdout+r.stderr,end='')
 assert (r.returncode!=0)==a.expect_failure,(case,r.returncode)
 rows.append({'case':case,'exit_code':r.returncode})
paths=['source/shared/source/world_frame.h','source/shared/source/world_frame_transport.h','source/assist/source/world_frame_runtime.h','source/assist/tests/world_frame_core.cpp','source/assist/tests/world_frame_retention.cpp']
(out/'result.json').write_text(json.dumps({'pass':True,'expected_failure':a.expect_failure,'results':rows,'source_sha256':{n:hashlib.sha256((repo/n).read_bytes()).hexdigest()for n in paths},'real_game_tested':False,'win32_io':'existing controlled fixture'},indent=2)+'\n')

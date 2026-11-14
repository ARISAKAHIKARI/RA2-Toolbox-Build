"""Verify actual Runtime/transport retention, using existing Win32 memory seams."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',required=True,type=Path);p.add_argument('--expect-failure',action='store_true');p.add_argument('--negative-priority',action='store_true');a=p.parse_args()
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
for case in ('demand','retry','busy','publication') + (() if a.expect_failure else ('leases','backoff','paused-demand','pending-demand') + (('priority',) if a.negative_priority else ())):
 r=subprocess.run([str(exe),case],capture_output=True,text=True);(out/(case+'.log')).write_text(r.stdout+r.stderr);print(case+': '+r.stdout+r.stderr,end='')
 assert (r.returncode!=0)==a.expect_failure,(case,r.returncode)
 rows.append({'case':case,'exit_code':r.returncode})
negative=None
if a.negative_priority:
 pre=out/'priority-current.ii';old=out/'priority-pre9.ii';badExe=out/'priority-pre9'
 subprocess.run(['g++','-std=c++17','-E','-I'+str(out),str(here/'world_frame_retention.cpp'),'-o',str(pre)],check=True)
 def function(text,marker):
  start=text.index(marker);end=text.index('{',start)+1;depth=1
  while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
  return text[start:end]
 baseline=subprocess.check_output(['git','show','b64b9007e2bc1cd4cab1c54d7870e62fa680b26d:source/external_reader/source/process_reader.h'],cwd=repo,text=True)
 text=pre.read_text();before=function(baseline,'void endWorldFrame(){');current=function(text,'void endWorldFrame(){');old.write_text(text.replace(current,before))
 subprocess.run(['g++','-std=c++17','-O1','-UNDEBUG','-pthread',str(old),'-o',str(badExe)],check=True,capture_output=True,text=True)
 r=subprocess.run([str(badExe),'priority'],capture_output=True,text=True);(out/'priority-pre9.log').write_text(r.stdout+r.stderr)
 negative={'baseline':'b64b9007e2bc1cd4cab1c54d7870e62fa680b26d','baseline_end_world_frame_sha256':hashlib.sha256(before.encode()).hexdigest(),'exit':r.returncode,'passed':r.returncode!=0 and 'demandRevision==first+1' in r.stderr}
 print('pre.9 lost immediate missing demand reproduced:',negative['passed']);assert negative['passed'],negative
paths=['source/shared/source/world_frame.h','source/shared/source/world_frame_transport.h','source/assist/source/world_frame_runtime.h','source/assist/tests/world_frame_core.cpp','source/assist/tests/world_frame_retention.cpp','source/assist/tests/world_frame_retention.py','source/external_reader/source/process_reader.h']
(out/'result.json').write_text(json.dumps({'pass':True,'priority_negative_control':negative,'expected_failure':a.expect_failure,'results':rows,'source_sha256':{n:hashlib.sha256((repo/n).read_bytes()).hexdigest()for n in paths},'real_game_tested':False,'win32_io':'existing controlled fixture'},indent=2)+'\n')

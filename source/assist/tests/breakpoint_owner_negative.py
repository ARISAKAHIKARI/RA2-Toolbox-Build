"""Reproduce pre-fix waiting counterexamples using its complete production Service.

Linux Win32 boundaries intentionally model an unscheduled worker, failed owner
resume, and failed wait with pending work. The parent kills ONLY the fixture
after a one-second bound. These expected hangs are negative controls, not
Windows execution or passing compatibility tests.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--legacy-core',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;core=here.parent/'source/tech_ui_breakpoint_core.h'
old=a.legacy_core.read_text();assert 'WaitForSingleObject(worker_,INFINITE)' in old and 'ResumeThread(s.thread_)' in old
old=old.replace('#include <windows.h>','')
old=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+str((core.parent/m[1]).resolve())+'"',old)
prefix=(here/'breakpoint_owner_rendezvous.cpp').read_text().split('struct BoundaryException')[0]
prefix=prefix.replace('#include <functional>','#include <functional>\n#include <chrono>\n#include <thread>')
prefix=prefix.replace('struct KernelThread {DWORD tid;bool dead=false;CONTEXT context{};};','struct KernelThread {DWORD tid;bool dead=false;CONTEXT context{};unsigned suspended=0;};')
start=prefix.index('DWORD WaitForSingleObject(');end=prefix.index('\nBOOL CloseHandle(',start)
prefix=prefix[:start]+r'''
static int failureMode=0;static HANDLE workerHandle=nullptr;static bool workerDone=false;
DWORD WaitForSingleObject(HANDLE h,DWORD timeout){
 if(h==workerHandle){
  if(timeout==INFINITE){std::fprintf(stderr,"reached production infinite wait; mode=%d suspended=%u done=%u\n",failureMode,current->suspended,unsigned(workerDone));std::fflush(stderr);if(failureMode==2)return WAIT_FAILED;while(!workerDone||current->suspended)std::this_thread::sleep_for(std::chrono::milliseconds(1));return WAIT_OBJECT_0;}
  return workerDone?WAIT_OBJECT_0:WAIT_TIMEOUT;
 }
 return h&&h->thread->dead?WAIT_OBJECT_0:WAIT_TIMEOUT;
}
''' +prefix[end:]
prefix+=r'''
constexpr DWORD THREAD_SUSPEND_RESUME=2,THREAD_GET_CONTEXT=8,THREAD_SET_CONTEXT=16;
void Sleep(DWORD ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
HANDLE OpenThread(DWORD,BOOL,DWORD){auto*h=new Handle{current};handles.push_back(h);return h;}
DWORD SuspendThread(HANDLE h){++productionSuspends;return h->thread->suspended++;}
DWORD ResumeThread(HANDLE){++productionResumes;return DWORD(-1);}
BOOL GetThreadContext(HANDLE h,CONTEXT*c){*c=h->thread->context;return 1;}
BOOL SetThreadContext(HANDLE h,const CONTEXT*c){h->thread->context=*c;return 1;}
HANDLE CreateThread(void*,unsigned,DWORD(*fn)(void*),void*raw,DWORD,void*){
 ++productionWorkers;workerHandle=new Handle{nullptr};
 if(failureMode==1){KernelThread helper{202};auto*owner=current;current=&helper;fn(raw);current=owner;workerDone=true;}
 return workerHandle;
}
'''
main=r'''
int main(int argc,char**argv){if(argc!=2)return 2;failureMode=std::atoi(argv[1]);assist::techbp::Service service;
 if(service.enable(0x401000,0x402000,nullptr)){std::fprintf(stderr,"unexpected safe return\n");return 3;}return 4;
}
'''
cpp=a.out/'legacy-production.cpp';cpp.write_text(prefix+'\n'+old+'\n'+main)
exe=a.out.resolve()/'legacy-production'
build=subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-pthread',str(cpp),'-o',str(exe)],capture_output=True,text=True)
(a.out/'build.log').write_text(build.stdout+build.stderr)
if build.returncode:raise RuntimeError(build.stdout+build.stderr)
cases=[]
for mode,name in enumerate(('worker_never_scheduled','ResumeThread_failed_owner_still_suspended','WAIT_FAILED_worker_still_pending')):
    try:
        run=subprocess.run([str(exe),str(mode)],capture_output=True,text=True,timeout=1)
    except subprocess.TimeoutExpired as error:
        stdout=(error.stdout or b'').decode() if isinstance(error.stdout,bytes) else error.stdout or ''
        stderr=(error.stderr or b'').decode() if isinstance(error.stderr,bytes) else error.stderr or ''
        assert 'reached production infinite wait' in stderr,(name,stderr)
        if mode==1:assert 'suspended=1 done=1' in stderr
        cases.append({'case':name,'outcome':'EXPECTED_COUNTEREXAMPLE_TIMEOUT','timeout_seconds':1,'stdout':stdout,'stderr':stderr,'killed_only_fixture_process':True})
    else:raise AssertionError((name,run.returncode,run.stdout,run.stderr))
result={'negative_controls_reproduced':True,'scope':__doc__,'legacy_source_sha256':hashlib.sha256(a.legacy_core.read_bytes()).hexdigest(),'fixture_source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'cases':cases,'windows_executed':False,'game_executed':False}
(a.out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'negative_controls_reproduced':True,'count':len(cases)}))

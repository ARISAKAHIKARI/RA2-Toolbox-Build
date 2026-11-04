"""Execute production start/poll/filter with delayed worker and modeled Win32 handles.
Checks transaction ordering and SEH filter decisions, not OS exception delivery or game behavior.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--core', type=Path, default=Path(__file__).parents[1] / 'source/tech_ui_breakpoint_core.h')
p.add_argument('--output', type=Path)
a = p.parse_args()
source = a.core.read_text(encoding='utf-8')
root = Path(__file__).parents[2]
guard_path = root / 'assist/source/guard.cpp'
fault_path = root / 'shared/source/runtime_fault.h'
guard = guard_path.read_text(encoding='utf-8')

def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

harness = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <cstdint>
using DWORD=uint32_t;using LONG=int32_t;
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,WAIT_FAILED=~0u,INFINITE=~0u;
thread_local DWORD tid=1;
struct Handle{bool target=false;std::atomic<bool> done{false};std::thread thread;};
using HANDLE=Handle*;
Handle target;
std::atomic<unsigned> launches{0},completed{0};
bool failCreate=false,failWait=false;
DWORD GetCurrentThreadId(){return tid;}
DWORD GetLastError(){return 5;}
LONG InterlockedExchange(volatile LONG*p,LONG v){return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST);}
LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){__atomic_compare_exchange_n(p,&expected,v,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return expected;}
void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}
void Sleep(DWORD ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
DWORD WaitForSingleObject(HANDLE h,DWORD timeout){
 if(!h)return WAIT_FAILED;
 if(h->target)return h->done?WAIT_OBJECT_0:WAIT_TIMEOUT;
 if(timeout==INFINITE){if(failWait){failWait=false;return WAIT_FAILED;}h->thread.join();return WAIT_OBJECT_0;}
 return h->done?WAIT_OBJECT_0:WAIT_TIMEOUT;
}
void CloseHandle(HANDLE h){if(h->thread.joinable())h->thread.join();delete h;}
HANDLE CreateThread(void*,unsigned,DWORD(*fn)(void*),void*raw,unsigned,void*){
 if(failCreate){failCreate=false;return nullptr;}
 ++launches;auto*h=new Handle;
 h->thread=std::thread([=]{tid=2;Sleep(20);fn(raw);h->done=true;});return h;
}
constexpr unsigned AllTags=31;constexpr LONG Off=0,Ready=1,System=-3;
struct Lease{unsigned uncertain=0,mask=0;unsigned tags()const{return mask;}};
struct Service{
 Lease lease;HANDLE thread_=&target,worker_=nullptr;DWORD owner_=1;
 volatile LONG job_=0,budget_=0,operation_=0,state=Off,lastError=0;
 DWORD budgetAt_=0;unsigned operationGroups_=0,wanted_=3;
 static DWORD worker(void*raw){auto&s=*static_cast<Service*>(raw);s.lease.mask=s.operation_==2?0:s.wanted_;++completed;InterlockedExchange(&s.state,s.lease.mask?Ready:Off);InterlockedExchange(&s.job_,2);return 0;}
'''
harness += function(source, ' void poll(){') + '\n'
harness += function(source, ' bool start(unsigned operation){') + '\n};\n'
harness += fault_path.read_text(encoding='utf-8').replace('#pragma once', '') + '\n'
harness += r'''
struct ExceptionRecord32 {unsigned code,flags;void*nested,*address;unsigned count,info[15];};
struct ExceptionPointers32 {ExceptionRecord32*record;void*context;};
unsigned reports=0,lastCode=0,lastBoundary=0,lastOp=0,lastAccess=0;
extern "C" void AssistReportException(unsigned boundary,unsigned code,unsigned,unsigned op,unsigned access){++reports;lastCode=code;lastBoundary=boundary;lastOp=op;lastAccess=access;}
'''
# The production x86 filter casts a pointer to a 32-bit diagnostic address.
# Widen that cast only for this portable harness; no Windows ABI is modeled.
harness += function(guard, 'extern "C" int AssistExceptionFilter(').replace('(unsigned)record->address', 'unsigned(uintptr_t(record->address))')
harness += r'''
int main(){
 target.target=true;unsigned checks=0;
 auto check=[&](bool b){++checks;if(!b){std::fprintf(stderr,"failed check %u\n",checks);std::exit(1);}};
 Service s;
 check(s.start(4));check(completed==1&&s.lease.mask==3&&s.job_==0&&s.worker_==nullptr);
 s.wanted_=1;check(s.start(4));check(completed==2&&s.lease.mask==1);
 check(s.start(3));check(completed==3&&s.job_==0);
 const unsigned before=launches;bool accepted=true;
 std::thread other([&]{tid=9;accepted=s.start(3);});other.join();
 check(!accepted&&launches==before&&s.state==Ready&&s.lease.mask==1);
 failWait=true;check(s.start(3));check(completed==4&&s.lease.mask==1);
 while(s.job_){s.poll();Sleep(1);}
 failCreate=true;check(!s.start(4));check(!s.job_&&!s.worker_&&s.lease.mask==1);
 check(s.start(2));check(completed==5&&!s.lease.mask&&s.state==Off);
 target.done=true;
 std::thread afterExit([&]{tid=9;accepted=s.start(2);});afterExit.join();
 check(accepted&&completed==6&&!s.job_);
 ExceptionRecord32 record{};ExceptionPointers32 pointers{&record,reinterpret_cast<void*>(0x1234)};
 const unsigned debugCodes[]={0x80000003u,0x80000004u};
 for(unsigned code:debugCodes){record.code=code;check(AssistExceptionFilter(&pointers,1)==0&&reports==0&&pointers.context==reinterpret_cast<void*>(0x1234));}
 check(AssistExceptionFilter(nullptr,1)==0&&!reports);
 record.code=0xc0000005u;record.count=2;record.info[0]=1;record.info[1]=0xdead;
 check(AssistExceptionFilter(&pointers,9)==1&&reports==1&&lastBoundary==9&&lastCode==record.code&&lastOp==1&&lastAccess==0xdead);
 record.code=0xc0000006u;check(AssistExceptionFilter(&pointers,9)==1&&reports==2);
 record.code=0xc000001du;check(AssistExceptionFilter(&pointers,1)==1&&reports==3&&lastOp==~0u&&!lastAccess);
 std::printf("PASS %u production transaction and debug-filter checks\n",checks);
}
'''

if os.environ.get('CXX'):
    compiler = shlex.split(os.environ['CXX'])
elif os.environ.get('ZIG_EXE') or os.environ.get('ZIG'):
    compiler = [os.environ.get('ZIG_EXE') or os.environ['ZIG'], 'c++']
else:
    compiler = [shutil.which('g++') or 'c++']
with tempfile.TemporaryDirectory(prefix='breakpoint-gate-') as directory:
    path = Path(directory)
    cpp, exe = path/'gate.cpp', path/('gate.exe' if os.name=='nt' else 'gate')
    cpp.write_text(harness, encoding='utf-8')
    subprocess.run([*compiler, '-std=c++17', '-O2', '-UNDEBUG', '-pthread', str(cpp), '-o', str(exe)], check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=20)
    if run.returncode:
        raise RuntimeError(run.stderr or run.stdout or f'exit {run.returncode}')
    print(run.stdout, end='')
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps({'scope':__doc__, 'pass':True, 'source_sha256':{
        str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (a.core,guard_path,fault_path)},
        'stdout':run.stdout, 'windows_exception_delivery_tested':False, 'game_tested':False}, indent=2)+'\n')

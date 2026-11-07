"""Run full production synchronous DR transactions and preserve SEH filter checks.
Win32 exception boundaries are controlled; no native Windows or game execution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--core',type=Path,default=Path(__file__).parents[1]/'source/tech_ui_breakpoint_core.h')
p.add_argument('--output',type=Path)
a=p.parse_args();here=Path(__file__).resolve().parent;root=here.parents[1]
guard_path=root/'assist/source/guard.cpp';fault_path=root/'shared/source/runtime_fault.h';policy_path=root/'assist/source/exception_policy.h'
guard=guard_path.read_text(encoding='utf-8')
def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


harness=r"""
#include <cstdio>
#include <cstdlib>
#include <cstdint>
"""
harness+=fault_path.read_text(encoding='utf-8').replace('#pragma once','')+'\n'
harness+=policy_path.read_text(encoding='utf-8').replace('#pragma once','')+'\n'
harness+=r"""
struct ExceptionRecord32 {unsigned code,flags;void*nested,*address;unsigned count,info[15];};
struct ExceptionPointers32 {ExceptionRecord32*record;void*context;};
unsigned reports=0,lastCode=0,lastBoundary=0,lastOp=0,lastAccess=0;
extern "C" void AssistReportException(unsigned boundary,unsigned code,unsigned,unsigned op,unsigned access){++reports;lastCode=code;lastBoundary=boundary;lastOp=op;lastAccess=access;}
"""
harness+=function(guard,'extern "C" int AssistExceptionFilter(').replace('(unsigned)record->address','unsigned(uintptr_t(record->address))')
harness+=r"""
int main(){
 unsigned checks=0;auto check=[&](bool b){++checks;if(!b){std::fprintf(stderr,"failed debug-filter check %u\n",checks);std::exit(1);}};
 ExceptionRecord32 record{};ExceptionPointers32 pointers{&record,reinterpret_cast<void*>(0x1234)};
 const unsigned debugCodes[]={0x80000003u,0x80000004u};
 for(unsigned code:debugCodes){record.code=code;check(AssistExceptionFilter(&pointers,1)==0&&reports==0&&pointers.context==reinterpret_cast<void*>(0x1234));}
 check(AssistExceptionFilter(nullptr,1)==0&&!reports);
 record.code=0xc0000005u;record.count=2;record.info[0]=1;record.info[1]=0xdead;
 check(AssistExceptionFilter(&pointers,9)==1&&reports==1&&lastBoundary==9&&lastCode==record.code&&lastOp==1&&lastAccess==0xdead);
 record.code=0xc0000006u;check(AssistExceptionFilter(&pointers,9)==1&&reports==2);
 record.code=0xc000001du;check(AssistExceptionFilter(&pointers,1)==0&&reports==3&&lastOp==~0u&&!lastAccess);
 std::printf("PASS %u retained production debug-filter checks\n",checks);
}
"""
compiler=shlex.split(os.environ['CXX']) if os.environ.get('CXX') else ([os.environ.get('ZIG_EXE') or os.environ['ZIG'],'c++'] if os.environ.get('ZIG_EXE') or os.environ.get('ZIG') else [shutil.which('g++') or 'c++'])
with tempfile.TemporaryDirectory(prefix='breakpoint-gate-') as directory:
 path=Path(directory);cpp=path/'filter.cpp';exe=path/('filter.exe' if os.name=='nt' else 'filter');cpp.write_text(harness)
 subprocess.run([*compiler,'-std=c++17','-O2','-UNDEBUG',str(cpp),'-o',str(exe)],check=True)
 run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
 if run.returncode:raise RuntimeError(run.stderr or run.stdout)
 print(run.stdout,end='')
 full=path/'owner';command=[sys.executable,str(here/'breakpoint_owner_rendezvous.py'),'--core',str(a.core.resolve()),'--out',str(full)]
 if len(compiler)==1:command+=['--cxx',compiler[0]]
 subprocess.run(command,check=True)
 report=json.loads((full/'result.json').read_text());report['scope']=__doc__;report['retained_debug_filter_checks']=6;report['stdout']=run.stdout
 for source in (guard_path,fault_path,policy_path,Path(__file__).resolve()):report['source_sha256'][str(source)]=hashlib.sha256(source.read_bytes()).hexdigest()
 if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')

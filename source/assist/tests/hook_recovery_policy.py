"""Execute the production cleanup predicate, disconnect and dispatcher with fake resources.
Entry/cleanup callback transport is substituted. No Windows exception delivery,
game execution or multiplayer behavior is tested.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).parents[1] / 'source/bridge.cpp')
p.add_argument('--output', type=Path)
a = p.parse_args()
source = a.source.read_text(encoding='utf-8')

def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

predicate = function('static bool featureRestorationComplete(){')
disconnect = function('static void disconnect(){')
dispatcher = function('static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message){')
fault_branch = function('if(fault){reset();snap.status=snap.bridgeStatus=-2;')
# Check placement in the actual entry: stop/dead-controller handling first,
# fault cleanup second, normal world inspection only after both return.
tick = function('static void tick(){')
assert tick.index('restoreFrameDriver();disconnect();return;') < tick.index(fault_branch) < tick.index('const auto worldSample=')
fault_handler = function('extern "C" void AssistFault(){')
assert fault_handler.index('fault=true;') < fault_handler.index('restoreMapVisibilityParameters();')

harness = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
using DWORD=uint32_t;using LONG=int32_t;using HANDLE=void*;
struct Registry{std::set<uint32_t> sites;bool active(uint32_t p)const{return sites.count(p);}
 unsigned installed()const{return unsigned(sites.size());}} codeHookRegistry;
bool buildTimeTooltipActive=false,crcMapInstalled=false,networkSenderInstalled=false;
bool enhancementRestoring=false,dataPending=false,mapRetired=true,techRetired=true;
bool enhanceRetired=true,controlRetired=true,fpsRetired=true,viewRetired=true,housePending=false;
struct Materials{bool active()const{return false;}}shroudMaterials;
bool techDataPending(){return dataPending;}bool mapVisibilityParametersRetired(){return mapRetired;}
bool techParametersRetired(){return techRetired;}bool techEnhanceParametersRetired(){return enhanceRetired;}
bool controlParametersRetired(){return controlRetired;}bool nativeViewRetired(){return viewRetired;}
bool singlePlayerFPSRetired(){return fpsRetired;}
bool controlHouseFlagsPending(){return housePending;}
constexpr unsigned Magic=123,Version=96;
struct Shared{unsigned magic=Magic,version=Version,pid=77,stop=0,owner=18;LONG gameWakePending=0;};
Shared shared;Shared*sh=&shared;
bool fault=true,commandDormant=true,worldCleanupPending=true,pendingAckReady=true;
unsigned controllerPID=9,controllerCheckAt=12,lastHouse=100,pendingAck=3;
HANDLE controllerProcess=(void*)1,mapping=(void*)2;
LONG hostWorkPending=1,messageGameThread=1,schedulerBusy=0;
unsigned resets=0,unmaps=0,closed=0,guards=0,maintained=0;
unsigned publications=0,normalWork=0;
struct Snapshot{int status=0,bridgeStatus=0,commandStatus=0,labels=0;}snap;
struct{int phase=0,source=0,target=0;}faultContext;
void publishSnapshot(DWORD){++publications;}
void restorePlayerContext(){}void reset(){++resets;}void restoreFrameDriver(){}
void MemoryBarrier(){}void CloseHandle(HANDLE){++closed;}void UnmapViewOfFile(void*){++unmaps;}
unsigned GetCurrentProcessId(){return 77;}unsigned GetCurrentThreadId(){return 88;}
LONG InterlockedExchange(LONG*p,LONG v){LONG old=*p;*p=v;return old;}
LONG InterlockedCompareExchange(LONG*p,LONG v,LONG old){LONG got=*p;if(got==old)*p=v;return got;}
struct Session{void update(bool,int,int,int){}}sessionTracker;
struct World{void reset(){}}hookWorld;
// These wrappers only deliver callbacks in this resource model; they do not
// model Windows SEH or the real entry manager's thread/token arbitration.
namespace runtimeunwind{template<class Body,class Cleanup>void run(Body&&body,Cleanup&&cleanup){
 try{body();}catch(...){cleanup();throw;}cleanup();
}}
namespace gameentry{enum class Client{Assist};template<class Fn>bool scoped(Client,bool,Fn&&fn){fn();return true;}}
namespace dispatchrouter{enum class Trigger{Message,Frame};}
struct MSG{unsigned message;uintptr_t wParam,lParam;};
constexpr unsigned WM_NULL=0,WakeTag=44;
void GuardedAssistTick(){++guards;}void maintainScheduler(){++maintained;}
'''
harness += '\n' + predicate + '\n' + disconnect + '\n' + dispatcher + '\n'
harness += 'void routeTick(DWORD now){' + fault_branch + ';++normalWork;}\n'
harness += r'''
int main(){
 unsigned checks=0;
 auto check=[&](bool b){++checks;if(!b){std::fprintf(stderr,"failed check %u\n",checks);std::abort();}};
 check(featureRestorationComplete());
 codeHookRegistry.sites={0x7F74D8,0x7F74C8};
 check(featureRestorationComplete()); // Cursor owns both tooltip slots while Assist restarts.
 codeHookRegistry.sites.insert(0x55DE81);check(featureRestorationComplete());
 buildTimeTooltipActive=true;check(!featureRestorationComplete());buildTimeTooltipActive=false;
 codeHookRegistry.sites.insert(0x4A8EB0);check(!featureRestorationComplete());
 codeHookRegistry.sites.erase(0x4A8EB0);
 for(bool*busy:{&crcMapInstalled,&networkSenderInstalled,&enhancementRestoring,&dataPending,&housePending}){
  *busy=true;check(!featureRestorationComplete());*busy=false;
 }
 for(bool*retired:{&mapRetired,&techRetired,&enhanceRetired,&controlRetired,&fpsRetired,&viewRetired}){
  *retired=false;check(!featureRestorationComplete());*retired=true;
 }
 shared.stop=1;techRetired=false;disconnect();
 check(sh==&shared&&fault&&shared.stop==1&&unmaps==0); // A live lease prevents detach.
 techRetired=true;disconnect();
 check(!sh&&!fault&&shared.stop==2&&unmaps==1);
 check(!mapping&&!controllerProcess&&!lastHouse&&!pendingAckReady&&!worldCleanupPending);
 sh=&shared;shared.stop=0;fault=true;hostWorkPending=1;
 dispatchFromTrigger(dispatchrouter::Trigger::Message,1,nullptr);
 check(guards==1&&schedulerBusy==0); // A fault must still receive a cleanup tick.
 dispatchFromTrigger(dispatchrouter::Trigger::Message,2,nullptr);check(guards==1);
 hostWorkPending=1;schedulerBusy=1;dispatchFromTrigger(dispatchrouter::Trigger::Message,3,nullptr);
 check(guards==1&&hostWorkPending==1); // Rejected reentry preserves the grant.
 schedulerBusy=0;dispatchFromTrigger(dispatchrouter::Trigger::Message,4,nullptr);check(guards==2);
 hostWorkPending=1;dispatchFromTrigger(dispatchrouter::Trigger::Frame,5,nullptr);check(guards==2&&hostWorkPending==1);
 fault=false;dispatchFromTrigger(dispatchrouter::Trigger::Message,6,nullptr);check(guards==3);
 fault=true;const unsigned before=resets;routeTick(7);
 check(resets==before+1&&publications==1&&normalWork==0);
 check(snap.status==-2&&snap.bridgeStatus==-2&&snap.commandStatus==0&&fault);
 fault=false;routeTick(8);check(normalWork==1&&publications==1);
 std::printf("PASS %u production cleanup and dispatcher checks\n",checks);
}
'''
if os.environ.get('CXX'):
    compiler = shlex.split(os.environ['CXX'])
elif os.environ.get('ZIG_EXE') or os.environ.get('ZIG'):
    compiler = [os.environ.get('ZIG_EXE') or os.environ['ZIG'], 'c++', '-target', 'x86_64-windows-gnu'] if os.name=='nt' else [os.environ.get('ZIG_EXE') or os.environ['ZIG'], 'c++']
else:
    compiler = [shutil.which('g++') or 'c++']
with tempfile.TemporaryDirectory(prefix='hook-recovery-') as directory:
    path = Path(directory)
    cpp=path/'test.cpp';exe=path/('test.exe' if os.name=='nt' else 'test')
    cpp.write_text(harness,encoding='utf-8')
    subprocess.run([*compiler,'-std=c++17','-O2','-UNDEBUG',str(cpp),'-o',str(exe)],check=True)
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    if run.returncode:
        raise RuntimeError(run.stderr or run.stdout or f'exit {run.returncode}')
    print(run.stdout,end='')
if a.output:
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'scope':__doc__,'production_source':str(a.source),'status':'PASS','output':run.stdout,'game_tested':False},indent=2)+'\n')

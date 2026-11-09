#!/usr/bin/env python3
"""Execute actual Host/Runtime read-only lifecycle functions with counted boundaries.

The old Host pause path is frozen byte-for-byte at final pre.5. Current and old
Host variants use identical inputs and the same controlled Runtime publication.
A second executable compiles actual Runtime reset, read-only transition, world
identity, snapshot publication and simulation admission functions. Win32,
physical memory/IPC and native feature retirement remain synthetic boundaries.
No Windows/game execution or display-quality claim is made.
"""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

BASELINE='source/assist/tests/fixtures/read_only_pre1475/main.cpp'
BASELINE_SHA='cedea56e1c259dca6c425b55f3e7e98dbe1ad3926f1fcb9b843c27d439c4fdc1'
BASELINE_COMMIT='18af5094f1cbd955ebaac9fbe3c85a0682ef7ce9'
BOUNDARY='source/research/evidence/pre147-hotkey-origin/bootstrap/cold_bootstrap.py'
SCOPE='actual Host pause/service/sample/merge/registration and Runtime read-only/reset/world-identity/admission functions with controlled Win32, IPC, memory and feature-retirement boundaries; no Windows/game execution'

RUNTIME_PREAMBLE=r'''
#include "protocol.h"
#include "world_lifecycle.h"
#include "session_epoch.h"
#include "activation_diagnostic.h"
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <climits>
using namespace assist;using DWORD=uint32_t;
static auto storage=std::make_unique<Shared>();static Shared*sh=storage.get();
static Snapshot snap{};static readonly::RuntimeState readOnlyState;
static sessionepoch::Tracker sessionTracker;static worldlife::Tracker hookWorld;
static bool commandDormant=false,worldCleanupPending=false,fault=false;
static uint32_t commandComputeEpoch=0,match=0,lastHouse=0,pendingAck=0;
static int lastFrame=-1,sceneFrame=-1;static DWORD now=10000,slow=0,sceneTick=0;
static unsigned snapshots=0,worldCaptures=0,worldRetires=0,offers=0,normalActions=0,holdReleases=0,indexCalls=0;
static bool restoreReady=true,holdReleaseReady=true,activeFeatures=false,wanted=true,worldReady=false,entryActive=true;
static bool pendingAckReady=false;static int repairAckPending=0;
static uint32_t capturedMatch=0,capturedHouse=0;static int capturedFrame=-1;
static std::map<uint32_t,uint8_t>memory;
template<class T>static void put(uint32_t at,T value){const auto*p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)memory[at+uint32_t(i)]=p[i];}
static bool readLiveWorld(uint32_t at,void*out,size_t size){auto*p=static_cast<uint8_t*>(out);for(size_t i=0;i<size;++i){auto it=memory.find(at+uint32_t(i));if(it==memory.end())return false;p[i]=it->second;}return true;}
template<class T>static T get(uint32_t at){T value{};readLiveWorld(at,&value,sizeof value);return value;}
static DWORD GetTickCount(){return now;}static DWORD sampleClock(const Shared*){return now;}
static void MemoryBarrier(){}static void InterlockedIncrement(volatile long*p){++*p;}
struct Clearable{void clear(){}};
static Clearable runtimeKeys,missionObserver,actorFrameArbiter,actorLeaseArbiter,loadLedger,pendingRepairs,missileFrames;
static int enhancementChangedCatalogue=1,stickyCount=3,catalogueReady=1,catalogueHash=9,catalogueAt=8,catalogueDirty=0,nativeMegaQueueCallDepth=0,nativeObjectQueueCallDepth=0,indexedFrame=3,indexedCount=4,outgoingFrame=5,outgoingBudget=64,repairCount=2,repairAt=1,repairDone=0,repairAckRequest=7,repairCalculationPending=1,productionBaseline=1,crateCount=2,cratesOn=1,autoWas=1,sceneResultSerial=1,lastIdle[3]{},techAllowed[8]{9};
static int missileHoldCount=0,missileFrame=-1,missileSourceCursor=0,missileTargetCursor=0,missileManagedCursor=0;
static uint32_t missileApplied=0;static bool missilePending=false,missileScanned=false,missileCleanup=false;
static void retireWorldFrame(){++worldRetires;worldReady=false;}
static void resetMissileControl(){missileHoldCount=0;missilePending=missileScanned=false;sh->missileHeld=0;}
static void stickyTick(bool){activeFeatures=false;}static void clearNativeOrders(bool){activeFeatures=false;}
static void maintainTechParameters(){}static void restorePlayerContext(){}static bool featureRestorationComplete(){return restoreReady;}
static void indexWorld(int frame){++indexCalls;indexedFrame=frame;}
static bool simulationReady(bool allowNewerFrame);
static bool releaseMissileHolds(uint32_t house,int frame,DWORD){
 ++holdReleases;
 if(activeFeatures||!missileCleanup||!simulationReady(false)||house!=lastHouse||frame!=snap.frame||indexedFrame!=frame)return false;
 if(!holdReleaseReady)return false;missileHoldCount=0;return true;
}
namespace gameentry{enum class Client{Assist};static bool active(Client){return entryActive;}
 struct State{unsigned conflicts=0,installed=0,gameThread=1,entries=1,reentrant=0,contended=0,wrongThread=0;};static State inspect(){return {};}}
namespace external{namespace display{struct State{bool visible()const{return true;}};template<class F>static State inspect(F){return {};}}}
namespace eventdelivery{static bool mayQueue(int mode){return mode==0||mode==3||mode==4||mode==5;}}
static struct{bool wanted(uint32_t,DWORD){return ::wanted;}uint32_t demandMask(){return 1;}}worldFrameServer;
template<class Read>static bool captureWorldFrame(Read,const worldlife::Sample&world,uint32_t house,DWORD,uint32_t){++worldCaptures;capturedMatch=match;capturedFrame=world.frame;capturedHouse=house;worldReady=true;return true;}
static bool missileManualOverflowed(){return false;}static void publishDebugEntries(DWORD){}static void nativePlacementOffers(DWORD){++offers;}
static void publishPlayers(){}static void notifyHost(){++snapshots;}
'''

RUNTIME_TESTS=r'''
static unsigned checks=0,failures=0,caseFailures=0,scenarios=0;
static void check(bool okay,const char*name){++checks;if(!okay){++failures;++caseFailures;std::cout<<"CHECK_FAILED "<<name<<"\n";}}
static void world(int frame=100,uint32_t scenario=0x100000,bool running=true){
 put<uint32_t>(0xA8B230,scenario);put<int>(0xA8B238,3);put<uint32_t>(0x87F924,0x200000);put<int>(0x87F928,4096);
 put<int>(0xA8ED84,frame);put<uint8_t>(0xA8E9A0,running);put<uint32_t>(0xA83D4C,0x300000);put<uint32_t>(0xAC1198,0);
 put<uint32_t>(0x300000+0x1F5,0);put<uint32_t>(0xA8022C,0x400000);put<int>(0xA80230,1);put<int>(0xA80238,1);put<uint32_t>(0x400000,0x300000);put<uint8_t>(0xA8BC79,0);
}
static void fresh(){
 storage=std::make_unique<Shared>();sh=storage.get();snap={};readOnlyState={};sessionTracker={};hookWorld={};
 commandComputeEpoch=match=lastHouse=0;lastFrame=-1;sceneFrame=-1;commandDormant=worldCleanupPending=fault=false;
 snapshots=worldCaptures=worldRetires=offers=normalActions=holdReleases=indexCalls=0;
 restoreReady=holdReleaseReady=entryActive=wanted=true;activeFeatures=worldReady=false;
 pendingAckReady=false;repairAckPending=0;pendingAck=0;missileHoldCount=0;missilePending=missileScanned=false;
 now+=1000;sh->owner=77;sh->heartbeat=now;sh->computeRequested=sh->computeApplied=0;
 memory.clear();world();snap.status=1;snap.commandStatus=1;snap.session=3;snap.frame=100;
}
static void finish(const char*name){++scenarios;std::cout<<"{\"case\":\""<<name<<"\",\"passed\":"<<(caseFailures?"false":"true")<<",\"failures\":"<<caseFailures<<",\"world_captures\":"<<worldCaptures<<",\"snapshots\":"<<snapshots<<",\"read_only_request\":"<<sh->readOnlyRequest<<",\"read_only_ack\":"<<sh->readOnlyAck<<",\"match\":"<<match<<",\"compute_applied\":"<<sh->computeApplied<<"}\n";caseFailures=0;}
static void pause(){readonly::request(*sh,[]{MemoryBarrier();});}
static void pending(){sh->request=10;sh->ack=0;sh->selectRequest=11;sh->planRequest=12;sh->cooperativePlan=13;sh->cooperativeCount=3;sh->pointer.request=14;sh->pointer.intent.count=2;sh->productionRequest=15;sh->uiSoundRequest=16;sh->formationKey=17;sh->reserveInput=18;sh->cameraSeq=20;pendingAck=9;pendingAckReady=true;}
static void drained(){
 check(sh->ack==10&&sh->result==-7&&!pendingAckReady,"pause cancels command and supersedes delayed ACK");
 check(sh->selectAck==11&&sh->planAck==12&&sh->cooperativeAck==13&&sh->pointer.ack==14,"all action mailboxes are terminal");
 check(sh->productionAck==15&&sh->uiSoundAck==16&&sh->formationKeyAck==17&&sh->reserveConsumed==18&&sh->cameraAck==20,"all auxiliary mailboxes drained");
}
int main(){
 fresh();check(!serviceReadOnlyRuntime(now),"ordinary mode retains normal tick entry");check(worldCaptures==0&&snapshots==0,"ordinary mode has no readonly side effects");finish("normal_control_entry");

 fresh();pause();check(!simulationReady(false),"readonly blocks simulation before callback");
 check(serviceReadOnlyRuntime(now),"cold readonly owns tick");
 check(sh->readOnlyAck==1&&snap.commandStatus==activation::HostReadOnly,"cold readonly ACKs retirement and advertises no commands");
 check(worldCaptures==1&&worldReady&&capturedMatch==match&&match!=0,"cold readonly publishes a real world identity");
 check(offers==0,"readonly snapshot skips native placement offers");
 now+=16;world(101);serviceReadOnlyRuntime(now);check(capturedFrame==101&&worldCaptures==2,"readonly samples continue with frame growth");finish("cold_readonly_publication");

 fresh();pause();pending();activeFeatures=true;serviceReadOnlyRuntime(now);drained();
 check(!activeFeatures&&!simulationReady(false)&&offers==0,"warm pause disarms features and command admission");
 sh->readOnly=0;check(!serviceReadOnlyRuntime(now),"acknowledged pause resumes normal entry");drained();
 check(sh->ack==sh->request,"resume does not recreate cancelled command");finish("pending_ack_no_replay");

 fresh();pause();pending();restoreReady=false;activeFeatures=true;
 check(serviceReadOnlyRuntime(now)&&readOnlyState.pending,"delayed retirement retains gate");
 check(sh->readOnlyAck==0&&worldCaptures==0&&snap.commandStatus==activation::CleanupPending,"pending retirement neither ACKs nor publishes a world");
 check(!activeFeatures&&!simulationReady(false)&&offers==0,"no feature executes during pending retirement");drained();
 sh->readOnly=0;check(!readonly::commands(sh),"quick enable cannot erase retirement sequence");
 check(serviceReadOnlyRuntime(now)&&sh->readOnlyAck==0,"quick enable still waits while cleanup is pending");
 restoreReady=true;check(!serviceReadOnlyRuntime(now)&&sh->readOnlyAck==1,"completed cleanup permits queued resume");drained();finish("delayed_retirement_fast_resume");

 fresh();lastHouse=0x300000;match=sessionTracker.epoch=1;sessionTracker.active=true;sessionTracker.house=lastHouse;sessionTracker.mode=3;sessionTracker.frame=100;
 missileHoldCount=1;missilePending=missileScanned=true;holdReleaseReady=false;activeFeatures=true;pause();
 check(serviceReadOnlyRuntime(now)&&readOnlyState.pending,"held order retirement can defer");
 check(!activeFeatures&&!missilePending&&!missileScanned&&missileHoldCount==1,"only previously owned hold identity survives");
 check(indexCalls==1&&holdReleases==1&&sh->readOnlyAck==0,"release reindexes current world without acknowledging too soon");
 now+=16;world(101);sh->heartbeat=now;holdReleaseReady=true;
 check(serviceReadOnlyRuntime(now)&&sh->readOnlyAck==1&&!readOnlyState.pending,"hold release can finish after pending gate was published");
 check(missileHoldCount==0&&!missilePending&&!missileScanned&&worldCaptures==1,"retired holds cannot restart automation");finish("held_order_retirement_recovery");

 fresh();pause();serviceReadOnlyRuntime(now);const auto first=match;
 now+=16;world(1,0x110000);serviceReadOnlyRuntime(now);
 check(match!=first&&capturedMatch==match&&capturedFrame==1,"readonly new match gets a fresh epoch");
 check(snap.commandStatus==activation::HostReadOnly&&!simulationReady(false),"world replacement never enables commands");finish("readonly_world_replacement");

 fresh();pause();serviceReadOnlyRuntime(now);const auto epoch=match;
 now+=16;world(100,0x100000,false);serviceReadOnlyRuntime(now);
 check(match==epoch&&worldCaptures==2&&snap.commandStatus==activation::HostReadOnly,"paused simulation preserves readonly publication");
 sh->foreground=0;now+=16;world(101);serviceReadOnlyRuntime(now);
 check(worldCaptures==3&&!simulationReady(false),"focus loss retains producer without commands");finish("game_pause_and_focus_loss");

 fresh();pause();serviceReadOnlyRuntime(now);const auto epochBefore=match;const auto countBefore=worldCaptures;
 memory.erase(0xA8B230);serviceReadOnlyRuntime(now);check(!worldReady&&worldCaptures==countBefore,"unreadable world retires publication");
 world(102);serviceReadOnlyRuntime(now);check(worldReady&&match==epochBefore,"readable world recovers without false new epoch");
 put<uint32_t>(0xA8B230,0);put<uint32_t>(0x87F924,0);put<int>(0x87F928,0);put<uint8_t>(0xA8E9A0,0);serviceReadOnlyRuntime(now);
 check(!worldReady&&snap.status==0&&snap.match==0,"world exit clears visible identity");
 world(1,0x120000);serviceReadOnlyRuntime(now);check(worldReady&&match!=epochBefore,"new world after exit recovers");finish("world_unreadable_exit_recovery");

 fresh();pause();serviceReadOnlyRuntime(now);sh->computeRequested=compute::All;sh->readOnly=0;
 check(!serviceReadOnlyRuntime(now),"cold readonly resume opens normal path after ACK");applyCommandComputeEpoch();
 check(sh->computeApplied==compute::All&&commandComputeEpoch==match,"cold readonly first command applies newly published configuration");finish("cold_readonly_compute_initialization");

 fresh();pause();serviceReadOnlyRuntime(now);sh->readOnly=0;serviceReadOnlyRuntime(now);sh->computeRequested=1;applyCommandComputeEpoch();
 pause();serviceReadOnlyRuntime(now);sh->computeRequested=2;sh->readOnly=0;serviceReadOnlyRuntime(now);applyCommandComputeEpoch();
 check(sh->computeApplied==1,"same-match pause resume keeps next-match-only calculation placement");finish("same_match_compute_preserved");

 fresh();pause();serviceReadOnlyRuntime(now);sh->readOnly=0;serviceReadOnlyRuntime(now);sh->computeRequested=1;applyCommandComputeEpoch();
 pause();serviceReadOnlyRuntime(now);world(1,0x130000);serviceReadOnlyRuntime(now);sh->computeRequested=2;sh->readOnly=0;serviceReadOnlyRuntime(now);applyCommandComputeEpoch();
 check(sh->computeApplied==2&&commandComputeEpoch==match,"new match observed while paused initializes on command resume");finish("paused_match_compute_initialization");

 fresh();pause();serviceReadOnlyRuntime(now);wanted=false;serviceReadOnlyRuntime(now);
 check(!worldReady&&snap.commandStatus==activation::HostReadOnly,"no world demand retires publication but keeps explicit mode");
 wanted=true;now+=16;world(101);serviceReadOnlyRuntime(now);check(worldReady&&capturedFrame==101,"world demand can recover");finish("demand_disable_reenable");
 std::cout<<"{\"suite\":\"read_only_runtime_integration\",\"passed\":"<<(failures?"false":"true")<<",\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}
'''

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_run(root, directory, code):
    directory.mkdir(parents=True,exist_ok=True)
    cpp=directory/'production.cpp';cpp.write_text(code)
    command=['g++','-std=c++17','-O2','-UNDEBUG','-DRA2_UNIFIED=1','-MMD','-MF',str(directory/'production.d'),
             '-I',str(root/'source/assist/source'),str(cpp),'-o',str(directory/'production')]
    built=subprocess.run(command,capture_output=True,text=True)
    (directory/'compile.log').write_text(built.stdout+built.stderr)
    if built.returncode:raise RuntimeError(str(directory)+' failed compilation:\n'+built.stderr)
    run_command=[str(directory/'production')]
    executed=subprocess.run(run_command,capture_output=True,text=True)
    (directory/'run.log').write_text(executed.stdout+executed.stderr)
    objects=[json.loads(line) for line in executed.stdout.splitlines() if line.startswith('{')]
    if not objects:raise RuntimeError(str(directory)+' produced no actual summary: '+executed.stderr)
    dependencies={}
    # GCC MMD lists the real local headers compiled by this executable.
    dep=(directory/'production.d').read_text().replace('\\\n',' ')
    for item in dep.split(':',1)[1].split():
        p=Path(item).resolve()
        try:relative=p.relative_to(root).as_posix()
        except ValueError:continue
        dependencies[relative]=digest(p)
    return {'compile_command':command,'compile_exit_code':built.returncode,'run_command':run_command,
            'run_exit_code':executed.returncode,'summary':objects[-1],'cases':objects[:-1],
            'stdout':executed.stdout,'stderr':executed.stderr,'run_log_sha256':digest(directory/'run.log'),
            'generated_cpp_sha256':digest(cpp),'header_dependencies':dependencies}


def host_code(boundary, current, selected):
    preamble=boundary.PREAMBLE
    preamble=preamble.replace('static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;',
        'static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;\nstatic bool readerValid=true,readerReadable=true,producerReady=false;static readonly::RuntimeState runtimeBoundary;')
    preamble=preamble.replace('static bool secondaryDisplayAvailable(){return false;}', 'static bool secondaryDisplayAvailable();')
    preamble=preamble.replace('static bool ensureBridge(){', 'static bool installProducerBoundary(){')
    preamble=preamble.replace('static struct{unsigned size=0;}commandQueue;', 'static struct{unsigned size=0;void clear(){size=0;}}commandQueue;')
    preamble=preamble.replace('bool valid()const{return true;}', 'bool valid()const{return readerValid;}bool canRead()const{return readerValid&&readerReadable;}int connectionIdentity()const{return 1;}')
    preamble=preamble.replace('bound(shared!=nullptr)', 'bound(producerReady&&readerReadable)')
    mocks=HOST_MOCKS+'''
static void MemoryBarrier(){}
namespace nativeviewhost{static uint32_t capabilityRequest(Shared*){return 0;}}
namespace gameinput{static void setPointerContext(uintptr_t,uint32_t,bool,uint32_t=0){}static void setFastTactical(uintptr_t,bool){}static void setFrameSampling(uintptr_t,bool){}}
static void readSnapshot(){snap=incoming;mergeExternal();}static void result(){}
'''
    ensure=boundary.function(selected,'ensureBridge')
    guard=re.search(r'if\(!enabled[^\n]*?return false;',ensure)
    assert guard,'missing actual disabled-session admission'
    initialization=''
    for field in ('readOnly','readOnlyRequest'):
        value=re.search(r'shared->'+field+r'=[^;]+;',ensure)
        if value:initialization+=value.group()
    mocks+='static bool secondaryDisplayRequested();\nstatic bool ensureBridge(){'+guard.group()+'if(shared)return true;bool ok=installProducerBoundary();if(ok){hook=1;mapping=module=reinterpret_cast<void*>(1);'+initialization+'}return ok;}\n'
    names=['secondaryDisplayRequested','displayFrameRequested','commandLink','commandMatch','displayMatch','commandChannelReady',
           'commandHotkeyContext','desiredHotkeyMode','modifiers','synchronizeHotkeyKeys','registerKey','registerKeys',
           'releaseBridge','discardExternalWorld','mergeExternal','sampleExternal','uninitializedBridgeTimedOut']
    functions='\n'.join(boundary.function(current,name) for name in names)
    # The negative control swaps only the complete old pause/service functions
    # and their old installation guard. All other current readers are shared.
    optional=boundary.function(selected,'maintainReadOnlyBridge',optional=True)
    functions+='\n'+optional+'\n'+boundary.function(selected,'pauseAssistControls')+'\n'+boundary.function(selected,'serviceReadOnlyDisplays')
    callsite=re.search(r'sampleExternal\(now\);uint32_t configured=flags\(\);\n bool needsBridge=[^\n]+\n if\(needsBridge&&!shared\)ensureBridge\(\);',current)
    early=re.search(r'if\(serviceReadOnlyDisplays\(now\)\)return;',current)
    assert callsite and early
    resume='if(shared){MemoryBarrier();shared->readOnly=0;}\n' if 'MemoryBarrier();shared->readOnly=0;' in selected else ''
    wrapper='static bool secondaryDisplayAvailable(){return secondaryDisplayRequested();}\nstatic void oneTick(){if(registeredHotkeyMode!=desiredHotkeyMode())registerKeys();\n'+early.group()+'\n'+callsite.group()+'\n'+resume+'}\n'
    return preamble+mocks+functions+wrapper+HOST_TESTS


def runtime_code(boundary, bridge):
    reset=boundary.function(bridge,'reset')
    restore_names=set(re.findall(r'\b(restore\w+|resetHookReadiness)\(',reset))
    reset_names=set(re.findall(r'\b(reset\w+|clearFlashes|clearOreOrders)\(',reset))
    names=(restore_names|reset_names|{'minerDefenseReset','superweaponGrantReset','nativePlacementReset','nativePlacementCompletionReset'})-{'reset','resetMissileControl','restorePlayerContext'}
    stubs='\n'.join('static void '+name+'(){activeFeatures=false;}' for name in sorted(names))
    stubs+='\nstatic bool techDataPending(){return false;}\n'
    functions='\n'.join(boundary.function(bridge,name) for name in
        ('reset','publishSnapshot','simulationReady','cancelReadOnlyMailboxes','retireForReadOnly','applyCommandComputeEpoch','serviceReadOnlyRuntime'))
    return RUNTIME_PREAMBLE+stubs+functions+RUNTIME_TESTS


def additional_stage(root,out,name,script,arguments):
    directory=out/name;directory.mkdir(parents=True,exist_ok=True)
    command=['python3','-B',str(root/script),*arguments,'--out',str(directory)]
    executed=subprocess.run(command,capture_output=True,text=True)
    (directory/'driver.log').write_text(executed.stdout+executed.stderr)
    path=directory/'result.json'
    if not path.is_file():raise RuntimeError(name+' produced no structured result: '+executed.stdout+executed.stderr)
    report=json.loads(path.read_text())
    if executed.returncode or report.get('passed') is not True:
        raise RuntimeError(name+' failed actual execution: '+executed.stdout+executed.stderr)
    logs={}
    for log in sorted(directory.rglob('run.log')):
        logs[log.relative_to(directory).as_posix()]=digest(log)
    if not logs:raise RuntimeError(name+' produced no raw binary log')
    return {'command':command,'exit_code':executed.returncode,
            'driver_stdout':executed.stdout,'driver_stderr':executed.stderr,
            'driver_log_sha256':digest(directory/'driver.log'),
            'report':report,'report_sha256':digest(path),'raw_logs':logs}


def run(root,out):
    root=root.resolve();out=out.resolve();out.mkdir(parents=True,exist_ok=True)
    if digest(root/BASELINE)!=BASELINE_SHA:raise RuntimeError('frozen pre.5 source identity changed')
    spec=importlib.util.spec_from_file_location('bootstrap_boundary',root/BOUNDARY)
    boundary=importlib.util.module_from_spec(spec);spec.loader.exec_module(boundary)
    paths=['source/assist/source/'+name for name in ('main.cpp','bridge.cpp','protocol.h','read_only_lifecycle.h','activation_diagnostic.h',
        'native_message_policy.h','tech_ui_breakpoint_bridge.h','pointer_host.h','world_frame_runtime.h','world_lifecycle.h','session_epoch.h')]
    paths += [BASELINE,BOUNDARY,'source/assist/tests/read_only_lifecycle.cpp','source/assist/tests/read_only_lifecycle_integration.py',
              'source/assist/tests/cooperative_read_only_recovery.py','source/assist/tests/fps_read_only_recovery.py',
              'source/assist/tests/native_readonly_pairing.py',
              'source/assist/source/single_player_fps_runtime.h',
              'source/assist/tests/fixtures/read_only_pre_cooperative/read_only_lifecycle.h',
              'source/assist/tests/fixtures/entry_pre1475/single_player_fps_runtime.h',
              'source/assist/tests/fixtures/read_only_pre_pairing/native_message_policy.h',
              'source/assist/tests/fixtures/read_only_pre_pairing/README.md']
    sources={path:digest(root/path) for path in paths}
    main=(root/'source/assist/source/main.cpp').read_text();bridge=(root/'source/assist/source/bridge.cpp').read_text()
    baseline=(root/BASELINE).read_text()
    current=compile_run(root,out/'host-current',host_code(boundary,main,main))
    old=compile_run(root,out/'host-pre5-negative',host_code(boundary,main,baseline))
    runtime=compile_run(root,out/'runtime-current',runtime_code(boundary,bridge))
    expected={'paused_secondary_cold','paused_secondary_cursor_only','paused_secondary_after_warm'}
    failed={case['case'] for case in old['cases'] if not case['passed']}
    negative=old['run_exit_code']==1 and failed==expected
    passed=current['run_exit_code']==0 and current['summary']['passed'] is True and runtime['run_exit_code']==0 and runtime['summary']['passed'] is True and negative
    stages={}
    for name,script,arguments in [
        ('cooperative_recovery','source/assist/tests/cooperative_read_only_recovery.py',['--root',str(root),'--negative-control']),
        ('fps_recovery','source/assist/tests/fps_read_only_recovery.py',['--root',str(root),'--negative-control']),
        ('input_pairs','source/assist/tests/native_readonly_pairing.py',['--source-root',str(root),'--negative-control'])]:
        stages[name]=additional_stage(root,out,name,script,arguments)
        child=stages[name]['report']
        if child['current']['compile_exit']!=0 or child['current']['run_exit']!=0 or child['negative_control']['passed'] is not True:
            raise RuntimeError(name+' did not execute current success and compiled negative control')
        for path,value in child['sources'].items():
            if path in sources and sources[path]!=value:raise RuntimeError(name+' source identity differs: '+path)
            sources[path]=value
    for part in (current,runtime):
        for path,value in part['header_dependencies'].items():
            if path in sources and sources[path]!=value:raise RuntimeError('compiled dependency differs: '+path)
            sources[path]=value
    for path,value in sources.items():
        if digest(root/path)!=value:raise RuntimeError('source changed during test: '+path)
    # Keep a combined raw stdout log while also retaining each actual binary log.
    raw=current['stdout']+current['stderr']+old['stdout']+old['stderr']+runtime['stdout']+runtime['stderr']
    for name,stage in stages.items():
        for path in stage['raw_logs']:raw+=(out/name/path).read_text()
    (out/'run.log').write_text(raw)
    stage_checks=sum(stage['report']['current']['execution']['checks'] for stage in stages.values())
    stage_scenarios=sum(stage['report']['current']['execution'].get('scenarios',0) for stage in stages.values())
    report={'suite':'read_only_lifecycle_integration','passed':passed,'scope':SCOPE,
            'scenarios':current['summary']['scenarios']+runtime['summary']['scenarios']+stage_scenarios,
            'checks':current['summary']['checks']+runtime['summary']['checks']+stage_checks,
            'scenario_count_scope':'Sum of actual reported scenarios only; cooperative recovery reports 11 checks and no scenario count.',
            'sources':sources,'host_current':current,'runtime_current':runtime,
            'additional_stages':stages,
            'negative_control':{'reproduced':negative,'source_commit':BASELINE_COMMIT,'fixture_sha256':BASELINE_SHA,
                'expected_failure_cases':sorted(expected),**old},
            'windows_executed':False,'game_tested':False,'run_log_sha256':digest(out/'run.log')}
    (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return report

HOST_MOCKS='\nstatic bool minimapSecondary=false,clockRunning=false,cursorActive=false;\nstatic unsigned retiredCount=0,readOnlyServices=0;\nstatic uintptr_t hook=0,game=1;static void*mapping=nullptr,*module=nullptr;static DWORD gameThread=2;\nstatic DWORD attachAt=0,lastPulse=0,snapshotSeq=0,lastReceiptWrite=0,lastReceiptMatch=0;\nstatic int worldRevision=-1;static unsigned plannerAck=0,selectionAck=0;\nstatic unsigned cooperativeDelivered=0,cooperativeReceipt=0;static bool retryCooperative=false;\nstatic std::vector<int>submittedPlan,submittedCooperative,acceptedCooperative,processedCooperative;\nstatic int cooperativeSourcePool=0;\nstatic DWORD sentAt=0,inFlightMatch=0,inFlightRetries=0;static std::vector<int>pointerMatches;\nstruct RetiredBoundary {Shared*shared;void*mapping,*module;uintptr_t hook,game;DWORD at,thread;int connection;};\nstatic std::vector<RetiredBoundary>retiredBridges;\nnamespace nativeviewhost{static void mappingLost(Shared*){}}\nnamespace commandcursor{static void clear(){}}\nnamespace gameentry{enum class Client{Assist};}\nnamespace gameinput{static void release(gameentry::Client,uintptr_t){}}\nstatic void UnmapViewOfFile(Shared*){}static void CloseHandle(void*){}\nstatic void logBridgeHealth(bool){}static void resetPlanning(){}\nstatic void reapBridges(){\n for(auto&r:retiredBridges){if(r.shared&&r.shared->stop==1){r.shared->stop=2;producerReady=false;++retiredCount;}}\n retiredBridges.clear();\n}\nstatic void resetConstruction(){}static void resetAirPath(){}static void resetPlacementEvent(){}\nnamespace worldrender{static void invalidate(){}}\nstatic void hideSidebarOverlay(){}static void hideWorldOverlay(){}static void hideAircraftButton(){}static void hideConstruction(){}\nstatic struct{void suspend(){}}overlayRecovery;\nnamespace minimap{static void hidePrimary(){}static void hide(){}}\nnamespace watchwindows{static void hidePrimary(){}static void hide(){}}\nnamespace overlayvisibility{static bool visible(){return true;}}\nstatic void clockState(bool value,int){clockRunning=value;}\nstatic void setStatus(const std::wstring&){}\nstatic void serviceReadOnlyWatchKeys(){++readOnlyServices;}\nstatic void logHotkeyBindings(){}\nstatic void detach(const wchar_t*);\n'
HOST_TESTS='\nstatic unsigned checks=0,failures=0,caseFailures=0,scenarios=0;\nstatic void check(bool value,const char*name){++checks;if(!value){++failures;++caseFailures;std::cout<<"CHECK_FAILED "<<name<<"\\n";}}\nstatic void reset(){\n now+=20000;lastData=0;shared=nullptr;std::memset(storage.get(),0,sizeof(Shared));hook=0;mapping=module=nullptr;producerReady=false;runtimeBoundary={};\n snap={};incoming={};externalPublished={};externalData=nullptr;externalAt=0;externalSourceConfirmed=false;\n externalWorldSource.reset();externalSampler.reset();retiredBridges.clear();\n bridgeAttempts=samplerCalls=scopeAttempts=scopeFailures=wakes=retiredCount=readOnlyServices=0;\n enabled=focused=readerValid=readerReadable=true;debugPageActive=techMode=buildTimeTooltipEnabled=hasSelection=false;\n configuredFlags=0;minimapSecondary=cursorActive=clockRunning=false;extra={};extra.missionModeKeys[selectedmode::OwnGround]=0;\n extra.watchSecondary=true;commandQueue.clear();\n for(auto&key:hotkeys)key=0;hotkeys[11]=\'L\'|0x600;\n keysRegistered=false;registeredHotkeyMode=0;registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();\n}\nstatic void detach(const wchar_t*){releaseBridge();discardExternalWorld();readerValid=readerReadable=false;}\nstatic void publishCommandBoundary(){\n if(!shared)return;\n const auto admission=runtimeBoundary.service(*shared,[]{readonly::cancelMailboxes(*shared,[]{MemoryBarrier();});},[]{return true;},[]{MemoryBarrier();});\n producerReady=admission!=readonly::Admission::Retiring;\n incoming.status=incoming.bridgeStatus=1;incoming.commandStatus=admission==readonly::Admission::Commands?1:12;\n incoming.match=1;incoming.chat=0;lastData=now;snap=incoming;mergeExternal();\n}\nstatic void ticks(unsigned count){for(unsigned i=0;i<count;++i){now+=100;oneTick();publishCommandBoundary();}}\nstatic void result(const char*name,unsigned startSamples,unsigned startAttempts){\n ++scenarios;\n std::cout<<"{\\"case\\":\\""<<name<<"\\",\\"passed\\":"<<(caseFailures?"false":"true")<<",\\"failures\\":"<<caseFailures\n <<",\\"fresh_samples\\":"<<(samplerCalls-startSamples)<<",\\"new_bridge_attempts\\":"<<(bridgeAttempts-startAttempts)\n <<",\\"scope_failures\\":"<<scopeFailures<<",\\"retired_bridges\\":"<<retiredCount<<",\\"producer_active\\":"<<(producerReady?"true":"false")\n <<",\\"cursor_active\\":"<<(cursorActive?"true":"false")<<",\\"command_mode\\":"<<desiredHotkeyMode()<<"}\\n";\n caseFailures=0;\n}\nint main(){\n reset();ticks(12);\n check(samplerCalls>0,"enabled Host produces fresh secondary samples");\n check(bridgeAttempts==1,"enabled Host establishes only one producer");\n check(desiredHotkeyMode()==2,"enabled healthy command mode remains enabled");\n result("enabled_control",0,0);\n\n for(bool cursor:{false,true}){\n  reset();enabled=false;cursorActive=cursor;unsigned beforeSamples=samplerCalls,beforeAttempts=bridgeAttempts;ticks(12);\n  check(!commandHotkeyContext(),"paused Host never admits commands");\n  check(readOnlyServices==12,"read-only branch remains live");\n  check(samplerCalls>beforeSamples,"paused secondary cold start receives new world samples");\n  check(bridgeAttempts>beforeAttempts,"paused secondary requests its producer");\n  result(cursor?"paused_secondary_cursor_only":"paused_secondary_cold",beforeSamples,beforeAttempts);\n }\n\n reset();ticks(5);unsigned beforeSamples=samplerCalls,beforeAttempts=bridgeAttempts;\n check(beforeSamples>0,"warm precondition actually sampled a valid world");\n enabled=false;pauseAssistControls();ticks(12);\n check(!commandHotkeyContext(),"paused warm Host never admits commands");\n check(readOnlyServices==12,"warm read-only branch keeps servicing");\n check(samplerCalls>beforeSamples,"pausing preserves continued fresh secondary publication");\n result("paused_secondary_after_warm",beforeSamples,beforeAttempts);\n\n reset();ticks(5);enabled=false;pauseAssistControls();ticks(5);\n beforeSamples=samplerCalls;beforeAttempts=bridgeAttempts;enabled=true;ticks(12);\n check(samplerCalls>beforeSamples,"reenabling recovers fresh publication");\n check(bridgeAttempts>=beforeAttempts,"reenabling retains or reinstalls a producer");\n check(desiredHotkeyMode()==2,"reenabling restores command mode");\n result("reenable_control",beforeSamples,beforeAttempts);\n\n reset();ticks(5);beforeSamples=samplerCalls;beforeAttempts=bridgeAttempts;\n enabled=false;extra.watchSecondary=false;pauseAssistControls();\n check(!producerReady&&!shared,"no secondary demand retires Runtime");\n check(!commandHotkeyContext(),"no secondary demand does not admit commands");\n check(!externalData&&!clockRunning,"no secondary demand intentionally clears display");\n result("paused_without_secondary_control",beforeSamples,beforeAttempts);\n\n std::cout<<"{\\"suite\\":\\"paused_secondary_lifecycle\\",\\"passed\\":"<<(failures?"false":"true")\n <<",\\"scenarios\\":"<<scenarios<<",\\"checks\\":"<<checks<<",\\"failures\\":"<<failures<<"}\\n";\n return failures?1:0;\n}\n'

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[3])
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    report=run(args.source_root,args.out)
    print(json.dumps({key:report[key] for key in ('suite','passed','scenarios','checks','scope')},ensure_ascii=False))
    raise SystemExit(0 if report['passed'] else 1)

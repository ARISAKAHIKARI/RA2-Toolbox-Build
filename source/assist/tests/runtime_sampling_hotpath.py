"""Production dispatcher/dodge admission with controlled boundaries.

Counts whole-world threat collections, not live game time or FPS. The actual
production functions are extracted below; Win32 transport and live observation
are mocked. --baseline supplies the unchanged dodge header as a negative control.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile


def function(text, name):
    start = text.index("static bool " + name + "(")
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--baseline", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / "source"
bridge = (root / "bridge.cpp").read_text()
dodge = (args.baseline or root / "dodge_bridge.h").read_text()
frame = bridge.split("bool frameDue=", 1)[1].split(";", 1)[0]
code = r'''
#include "protocol.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace assist;
using DWORD=uint32_t;
static auto state=std::make_unique<Shared>();
static Shared*sh=state.get();
static uint32_t match=17,dodgeApplied,dodgeTask;
static int lastFrame=20;
static bool externalInputFrame,dodgePending,reverseWasEnabled,ready=true;
static int dodgeCount,dodgeCursor,dodgeJobCursor;
struct Job{uint32_t pointer;bool active;};static Job dodgeJobs[512];
static dodgecompute::Input dodgeInput;
static dodgecompute::Result dodgeResult;
static compute::FrameHistory<dodgecompute::Input> dodgeFrames;
static struct{bool chat;int techOpen;}snap;
static struct{int frame=21;}worldSample;
static int collections,maintenance,applications,reversals,notifications;
static int worldCount;static bool removeJob,publishResultDuringMaintenance,enableReverseDuringMaintenance;
static int formationJobCount,stickyCount,burstCount,missileHoldCount,nativeCount,unloadCount;
static bool techPending,runtimeCoopPending,loadCalculationPending,repairCalculationPending,repairAckPending;
static int repairAt,repairCount,entryReservationCount,cooperativeEffectCount;
static struct{int count;}loadLedger,pendingRepairs,actorLeaseArbiter;
static struct{int count;bool pending()const{return count!=0;}}missionObserver;
namespace ownground {static struct{bool pending;}job;}
static bool defensePending,placementPending,dataPending,moPending,orePending;
static bool minerDefensePending(){return defensePending;}
static bool nativePlacementCompletionPending(){return placementPending;}
static bool techDataPending(){return dataPending;}
static bool moOrdersPending(){return moPending;}
static bool oreHarvestNeedsWork(){return orePending;}
static bool simulationReady(){return ready;}
static void MemoryBarrier(){}
static long InterlockedIncrement(volatile long*p){return ++*p;}
static void collectDodgeThreats(){++collections;}
static void updateDodgeJobs(uint32_t,int,DWORD){++maintenance;if(removeJob)dodgeCount=0;if(publishResultDuringMaintenance)sh->dodgeComputed=9;if(enableReverseDuringMaintenance){sh->dodgeEnabled=1;sh->dodgeMode=0;}}
static bool reverseDodgeTick(uint32_t,int,DWORD){++reversals;return true;}
static void applyDodgeResult(const dodgecompute::Result&,uint32_t,int,DWORD,const dodgecompute::Input* =nullptr){++applications;}
static void notifyHost(){++notifications;}
struct Vec{int count;};
static Vec vec(uint32_t){return {worldCount};}
static uint32_t item(Vec,int){return 0x20000;}
template<class T>static T get(uint32_t){return T(123);}
static bool dodgeSource(uint32_t,uint32_t){return true;}
static bool dodgeOwns(uint32_t){return false;}
static void dodgeObserve(uint32_t p,uint32_t,dodgecompute::Source&s){s.pointer=p;s.id=123;s.count=1;s.freshThreat=1;s.frame=21;}
@HOST@
static bool frameDue(DWORD now){return @FRAME@;}
@DODGE@
static void reset(){
 std::memset(sh,0,sizeof(*sh));sh->dodgeMode=1;sh->dodgeEnabled=1;
 dodgeCount=dodgeCursor=dodgeJobCursor=0;dodgeApplied=dodgeTask=0;
 collections=maintenance=applications=reversals=notifications=0;
 worldCount=0;removeJob=publishResultDuringMaintenance=enableReverseDuringMaintenance=false;ready=true;snap={};dodgeFrames.clear();
 externalInputFrame=false;dodgePending=false;dodgeInput={};dodgeResult={};
 formationJobCount=stickyCount=burstCount=missileHoldCount=nativeCount=unloadCount=0;
 techPending=runtimeCoopPending=loadCalculationPending=repairCalculationPending=repairAckPending=false;
 repairAt=repairCount=entryReservationCount=cooperativeEffectCount=0;
 loadLedger.count=pendingRepairs.count=actorLeaseArbiter.count=missionObserver.count=0;
 ownground::job.pending=defensePending=placementPending=dataPending=moPending=orePending=false;
 worldSample.frame=21;lastFrame=20;
}
static void check(bool v,const char*message){if(!v){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(){
 reset();dodgeControlTick(1,21,1000);
 check(collections==0,"Host same-frame wake repeated whole-world threat collection");
 check(maintenance==1,"identity/lease maintenance must still run");
 reset();externalInputFrame=true;worldCount=1;dodgeControlTick(1,21,1000);
 check(collections==1&&notifications==1&&sh->dodgeTask!=0,"fresh unified frame must collect/publish");
 externalInputFrame=false;for(int n=0;n<99;++n)dodgeControlTick(1,21,1001+n);
 check(collections==1&&maintenance==100&&notifications==1,"100 wakes in one frame use one input collection");
 std::printf("PASS 100 Host wakes: threat collections=1, maintenance=100, input publications=1\n");
 reset();dodgecompute::Input source;source.match=match;source.frame=21;source.tick=1000;
 dodgeFrames.retain(9,source,{7,8,0});sh->dodgeComputed=9;dodgeControlTick(1,21,1000);
 check(collections==1&&applications==1&&dodgeApplied==9&&dodgeCursor==7&&dodgeJobCursor==8,"new result requires fresh live threat verification and source provenance");
 reset();dodgeFrames.retain(9,source,{7,8,0});publishResultDuringMaintenance=true;dodgeControlTick(1,21,1000);
 check(collections==1&&applications==1&&dodgeApplied==9,"result arriving during maintenance must collect before applying");
 reset();externalInputFrame=true;dodgeFrames.retain(9,source,{7,8,0});publishResultDuringMaintenance=true;dodgeControlTick(1,21,1000);
 check(collections==1&&applications==1,"input and arriving result share one fresh observation");
 reset();sh->dodgeComputed=9;sh->dodgeResultSeq=1;dodgeControlTick(1,21,1000);
 check(collections==0&&applications==0&&dodgeApplied==0,"incomplete result cannot execute or cause a world scan");
 reset();sh->dodgeComputed=9;dodgeControlTick(1,21,1000);
 check(collections==0&&applications==0,"result without retained input cannot cause a world scan");
 reset();externalInputFrame=true;worldCount=1;sh->computeApplied=1u<<compute::Dodge;dodgeControlTick(1,21,1000);
 check(collections==1&&applications==1&&notifications==0,"Runtime calculation path remains immediate");
 reset();sh->dodgeMode=0;dodgeControlTick(1,21,1000);
 check(collections==1&&maintenance==1&&reversals==1,"reverse mode preserves existing synchronous observation");
 reset();enableReverseDuringMaintenance=true;dodgeControlTick(1,21,1000);
 check(collections==1&&reversals==1,"switch to reverse during maintenance collects at consumption");
 reset();sh->dodgeEnabled=0;dodgeCount=1;enableReverseDuringMaintenance=true;dodgeControlTick(1,21,1000);
 check(collections==1&&reversals==1,"reenabling reverse with a previous job collects at consumption");
 reset();sh->dodgeEnabled=0;dodgeControlTick(1,21,1000);
 check(collections==0&&maintenance==0,"disabled/no-job path stays idle");
 reset();sh->dodgeEnabled=0;dodgeCount=1;removeJob=true;dodgeControlTick(1,21,1000);
 check(collections==0&&maintenance==1&&dodgeCount==0,"disabled live jobs retain identity cleanup");
 reset();snap.chat=true;dodgeControlTick(1,21,1000);check(collections==0&&maintenance==0,"chat gate");
 reset();ready=false;dodgeControlTick(1,21,1000);check(collections==0&&maintenance==0,"simulation gate");
 std::puts("PASS dodge result, Runtime, reverse, cleanup and suspension boundaries");
 reset();sh->dodgeEnabled=0;
 check(!hostFrameCaptureWanted(1000)&&!frameDue(1000),"all Host placement without a consumer is idle");
 sh->flags=AutoRepair;check(hostFrameCaptureWanted(1000)&&frameDue(1000),"Host automatic repair subscribes");
 sh->computeApplied=1u<<compute::Repair;check(!hostFrameCaptureWanted(1000),"Runtime repair is not a Host subscription");
 sh->flags=AutoOreReturn;sh->computeApplied=0;check(hostFrameCaptureWanted(1000),"Host ore subscribes");
 sh->computeApplied=1u<<compute::Ore;check(!hostFrameCaptureWanted(1000),"Runtime ore is not a Host subscription");
 sh->flags=0;sh->computeApplied=0;auto&r=sh->cooperativeRequest;
 r.serial=1;r.match=match;r.tick=1000;r.mode=cooperative::Enter;r.viewWidth=2560;r.viewHeight=1440;
 check(hostFrameCaptureWanted(1000),"initial Host cooperation requires a sample");
 sh->cooperativeInputFrame.request.serial=1;check(!hostFrameCaptureWanted(1000),"satisfied cooperation stops sampling");
 sh->cooperativeFrameWanted=1;check(hostFrameCaptureWanted(1000),"requested cooperation refresh is admitted");
 sh->computeApplied=1u<<compute::Enter;check(!hostFrameCaptureWanted(1000),"Runtime cooperation does not subscribe");
 sh->computeApplied=0;r.mode=cooperative::AirFirst;check(hostFrameCaptureWanted(1000),"Host aircraft request");
 sh->computeApplied=1u<<compute::Aircraft;check(!hostFrameCaptureWanted(1000),"Runtime aircraft request");
 sh->computeApplied=0;r.match=match+1;check(!hostFrameCaptureWanted(1000),"foreign world request rejected");
 r.match=match;check(!hostFrameCaptureWanted(6001),"expired intent rejected");
 r.count=cooperative::MaxSources+1;check(!hostFrameCaptureWanted(1000),"invalid request rejected");
 std::puts("PASS Host subscription mode, demand, provenance and freshness");
 reset();sh->dodgeEnabled=0;
 int*pending[]={&formationJobCount,&stickyCount,&burstCount,&missileHoldCount,&nativeCount,&unloadCount,
 &loadLedger.count,&repairCount,&pendingRepairs.count,&entryReservationCount,&cooperativeEffectCount,&missionObserver.count,&actorLeaseArbiter.count};
 for(auto*p:pending){*p=1;check(frameDue(1000),"live job must retain frame cadence");worldSample.frame=lastFrame;check(!frameDue(1000),"same logic frame is not new input");worldSample.frame=21;*p=0;}
 repairAckPending=true;check(frameDue(1000),"repair ACK continuation");repairAckPending=false;
 ownground::job.pending=true;check(frameDue(1000),"owned ground order continuation");
 std::puts("PASS outstanding job, receipt, lease and cooperative maintenance cadence");
}
'''.replace("@HOST@", function(bridge, "hostFrameCaptureWanted")).replace("@FRAME@", frame).replace("@DODGE@", function(dodge, "dodgeControlTick"))
with tempfile.TemporaryDirectory(prefix="runtime-sampling-") as tmp:
    cpp = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["g++", "-std=c++17", "-O2", "-I", str(root), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

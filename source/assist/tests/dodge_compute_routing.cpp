#include "protocol.h"
#include "compute_frame.h"
#include <cassert>
#include <cstdio>
using namespace assist;using DWORD=uint32_t;
struct FakeShared {bool dodgeEnabled=true;int dodgeMode=1;uint32_t computeApplied=0,dodgeTask=0,dodgeComputed=0,dodgeActive=0;long dodgeInputSeq=0,dodgeResultSeq=0;dodgecompute::Input dodgeInput;dodgecompute::Result dodgeResult;};
static FakeShared data,*sh=&data;static struct {bool chat=false;}snap;
static bool externalInputFrame=true,reverseWasEnabled=false,dodgePending=false,readable=true,ready=true;
static int dodgeCount=0,dodgeCursor=0,dodgeJobCursor=0;static struct {bool active=false;}dodgeJobs[2];
static uint32_t dodgeTask=0,dodgeApplied=0,notifies=0,applications=0,maintenance=0,reverseCalls=0,collections=0;
static int sourceFrame=-1;static dodgecompute::Input dodgeInput;static dodgecompute::Result dodgeResult;
static compute::FrameHistory<dodgecompute::Input>dodgeFrames;
static bool simulationReady(){return ready;}static bool collectDodgeThreats(){++collections;return readable;}
static void updateDodgeJobs(uint32_t,int,DWORD){++maintenance;}
static void MemoryBarrier(){}static void InterlockedIncrement(long*p){++*p;}static void notifyHost(){++notifies;}
static void fill(int frame,DWORD tick){dodgeInput={};dodgeInput.match=1;dodgeInput.frame=frame;dodgeInput.tick=tick;dodgeInput.count=1;dodgeInput.sources[0].id=123;dodgeInput.sources[0].freshThreat=1;dodgeInput.sources[0].count=1;}
static bool collectDodgeInput(uint32_t,int f,DWORD n,int&cursor,int&job){if(!readable)return false;fill(f,n);cursor=17;job=19;return true;}
static void applyDodgeResult(const dodgecompute::Result&r,uint32_t,int,DWORD,const dodgecompute::Input*source=nullptr){++applications;const auto&in=source?*source:dodgeInput;assert(r.match==in.match&&r.tick==in.tick&&r.frame==in.frame);sourceFrame=in.frame;}
static bool reverseDodgeTick(uint32_t,int,DWORD);
#include "routing.h"
static bool reverseDodgeTick(uint32_t h,int f,DWORD n){++reverseCalls;reverseWasEnabled=true;fill(f,n);dispatchDodgeCalculation(h,f,n,-1,-1,0);return true;}
static void reset(int mode,bool internal){data={};sh=&data;data.dodgeMode=mode;data.computeApplied=internal?(1u<<compute::Dodge):0;externalInputFrame=readable=ready=true;reverseWasEnabled=dodgePending=false;dodgeCount=dodgeCursor=dodgeJobCursor=0;dodgeTask=dodgeApplied=notifies=applications=maintenance=reverseCalls=collections=0;dodgeFrames.clear();sourceFrame=-1;snap.chat=false;}
static void complete(){data.dodgeResult=dodgecompute::calculate(data.dodgeInput);data.dodgeComputed=data.dodgeTask;}
int main(){unsigned cases=0;
 for(int mode:{0,1})for(bool internal:{false,true}){
  reset(mode,internal);assert(dodgeControlTick(1,50,1000));assert(maintenance==1&&reverseCalls==unsigned(mode==0));
  if(internal){assert(applications==1&&!notifies&&!data.dodgeTask&&sourceFrame==50);}
  else {assert(!applications&&notifies==1&&data.dodgeTask==1&&!(data.dodgeInputSeq&1));const auto*in=dodgeFrames.find(1);const auto*c=dodgeFrames.cursors(1);assert(in&&in->frame==50&&c&&(*c)[2]==mode);complete();externalInputFrame=false;assert(dodgeControlTick(1,51,1016)==bool(mode==0));assert(applications==1&&sourceFrame==50&&data.dodgeTask==1&&dodgeApplied==1);if(mode)assert(dodgeCursor==17&&dodgeJobCursor==19);}
  ++cases;
 }
 for(int mode:{0,1}){
  reset(mode,false);assert(dodgeControlTick(1,50,1000));complete();externalInputFrame=false;data.dodgeMode=1-mode;
  dodgeControlTick(1,51,1016);assert(!applications&&dodgeApplied==1);++cases;
  reset(mode,false);assert(dodgeControlTick(1,50,1000));complete();externalInputFrame=false;readable=false;
  assert(!dodgeControlTick(1,51,1016)&&!applications&&!dodgeApplied);readable=true;dodgeControlTick(1,52,1032);assert(applications==1&&sourceFrame==50);++cases;
  reset(mode,false);assert(dodgeControlTick(1,50,1000));complete();externalInputFrame=false;data.dodgeResultSeq=1;
  dodgeControlTick(1,51,1016);assert(!applications&&!dodgeApplied);data.dodgeResultSeq=2;dodgeControlTick(1,52,1032);assert(applications==1);++cases;
 }
 // Disabling reverse mode keeps ordinary active-job maintenance and external
 // result processing alive, without accepting an old reverse-mode answer.
 reset(0,false);assert(dodgeControlTick(1,50,1000));complete();data.dodgeEnabled=false;dodgeCount=1;dodgeJobs[0].active=true;
 assert(dodgeControlTick(1,51,1016));assert(!applications&&data.dodgeTask==2&&dodgeFrames.cursors(2)->at(2)==1);complete();externalInputFrame=false;dodgeControlTick(1,52,1032);assert(applications==1);++cases;
 printf("PASS dodge routing: %u cases; both modes honor Host/Runtime selection, retain exact inputs, reject mode changes, retry incomplete/racing results and maintain disabled jobs\n",cases);
}

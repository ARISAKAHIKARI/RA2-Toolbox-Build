// Extracted production repair collection, completion and dispatch. Native queue
// and sampled game objects are explicit boundaries; no game process executes.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>
#include <array>
#include <algorithm>
#include "../../shared/source/world_frame.h"
#include "../source/compute_tasks.h"
#include "../source/compute_frame.h"
#include "../source/compute_policy.h"
#include "../source/plan_freshness.h"
#include "../source/repair_delivery.h"
using namespace assist;
using DWORD=uint32_t;
constexpr uint32_t AutoRepair=64,RepairUnrepairable=134217728;
namespace featuretypes {constexpr int RepairBuilding=1;}
struct Shared {
 uint32_t flags=0,repairMs=2000,computeApplied=compute::Default,repairTask=0,repairComputed=0;
 int32_t repairResultSeq=0;compute::RepairInput repairInput;compute::RepairResult repairResult;int result=0,count=0;
};
static Shared shared,*sh=&shared;
struct Snapshot{uint32_t autoRepair=0,repairTotal=0;int frame=0;};static Snapshot snap;
struct Building{uint32_t pointer,type,id,house;int hp=50,maximum=100;bool repairing=false,repairable=true,allowed=true,alive=true;};
static std::vector<Building> buildings;
static uint32_t match=1,lastHouse=0x200000;
static int currentFrame=0;static bool chat=false,externalInputFrame=false,queueAvailable=true;
static unsigned buildingScans=0,notifications=0,queueAttempts=0,indexCalls=0;static int perFrameBudget=64;
struct Queued{uint32_t id;int frame;};static std::vector<Queued> sent;
static uint32_t repairIDs[8192];static int repairCount,repairAt,repairDone;
static bool pendingAckReady,repairAckPending;static uint32_t pendingAck,repairAckRequest;static DWORD repairAckStarted;
static bool repairCalculationPending;static DWORD repairCalculationStarted;static uint32_t repairTaskSeen;
static bool autoWas;static DWORD lastAuto;
static repairdelivery::Pending<> pendingRepairs;
static worldframe::Snapshot worldFrame;
static bool worldFrameReady=false,sampleReadComplete=true,sampleAvailable=true;
static unsigned sampleReadDepth=0;static uint32_t missingSamplePage=0;
static std::array<uint64_t,8192> runtimeDemandBits{};static std::vector<uint32_t> runtimeWorldDemands;
#include SAMPLE_SCOPE
struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static bool read(uint32_t at,void*out,size_t n){if(at==0xA8EB40)++buildingScans;if(!sampleReadDepth)throw std::runtime_error("collector read escaped scope");return readWorldSample(at,out,n);}
#include "../source/tactical_frame_read.h"
template<class T>static T get(uint32_t at);
static Vec vec(uint32_t at){Vec out{};read(at,&out,sizeof out);return out;}
static uint32_t item(Vec v,int i){return get<uint32_t>(v.items+uint32_t(i)*4);}
static Building* byPointer(uint32_t p){for(auto&b:buildings)if(b.pointer==p)return &b;return nullptr;}
static uint32_t worldByID(uint32_t id){for(const auto&b:buildings)if(b.alive&&b.id==id)return b.pointer;return 0;}
static bool own(uint32_t p,uint32_t house){if(sampleReadDepth)return get<bool>(p+0x74)&&get<uint32_t>(p+0x21C)==house;auto*b=byPointer(p);return b&&b->alive&&b->house==house;}
static int kind(uint32_t p){if(sampleReadDepth)return get<bool>(p+0x74)?3:-1;auto*b=byPointer(p);return b&&b->alive?3:-1;}
static uint32_t typeOf(uint32_t p,int){if(sampleReadDepth)return get<uint32_t>(p+0x520);auto*b=byPointer(p);return b?b->type:0;}
static bool featureTypeAllowed(int,uint32_t t){for(const auto&b:buildings)if(b.type==t)return b.allowed;return false;}
template<class T>static T get(uint32_t at){
 if(sampleReadDepth){T out{};readWorldSample(at,&out,sizeof out);return out;}
 if(at==0xA8ED84)return T(currentFrame);if(at==0xA8BC60+25)return T(chat);if(at==lastHouse+0x30)return T(0);
 for(const auto&b:buildings){if(at==b.pointer+0x10)return T(b.id);if(at==b.pointer+0x6C)return T(b.hp);if(at==b.pointer+0x6E8)return T(b.repairing);if(at==b.type+0xA0)return T(b.maximum);if(at==b.type+0x157A)return T(b.repairable);}
 throw std::runtime_error("unexpected read");
}
static void prepareSample(){
 snap.frame=currentFrame;if(worldFrameReady&&worldFrame.key.match==match&&worldFrame.key.frame==currentFrame)return;
 worldFrameReady=false;if(!sampleAvailable)return;
 static std::vector<uint8_t> bytes(0xC00000);std::fill(bytes.begin(),bytes.end(),0);
 auto set=[&](uint32_t at,const auto&value){std::memcpy(bytes.data()+at,&value,sizeof value);};
 Vec list{0,0x100000,int(buildings.size()),0,int(buildings.size()),0};set(0xA8EB40,list);set(0xA8ED84,currentFrame);
 for(size_t i=0;i<buildings.size();++i){const auto&b=buildings[i];set(list.items+uint32_t(i)*4,b.pointer);set(b.pointer+0x10,b.id);set(b.pointer+0x6C,b.hp);set(b.pointer+0x74,b.alive);set(b.pointer+0x21C,b.house);set(b.pointer+0x520,b.type);set(b.pointer+0x6E8,b.repairing);set(b.type+0xA0,b.maximum);set(b.type+0x157A,b.repairable);}
 worldframe::Key key{};key.match=match;key.frame=currentFrame;key.house=lastHouse;worldFrame.reset(key);
 auto acquire=[&](uint32_t at,void*out,size_t n){if(missingSamplePage&&at<missingSamplePage+4096&&uint64_t(at)+n>missingSamplePage)return false;std::memcpy(out,bytes.data()+at,n);return true;};
 worldFrame.prefetch(acquire,0xA8EB40,sizeof list);worldFrame.prefetch(acquire,list.items,buildings.size()*4);
 for(const auto&b:buildings){worldFrame.prefetch(acquire,b.pointer,0x800);worldFrame.prefetch(acquire,b.type,0x1600);}
 worldFrameReady=worldFrame.seal();
}
static void MemoryBarrier(){}
static void notifyHost(){if(sampleReadDepth)throw std::runtime_error("publication inside sample scope");++notifications;}
static void indexWorld(int){++indexCalls;}
enum class EventLane{Repair};static int recurringBudget(EventLane){return perFrameBudget;}
static bool nativeObjectEvent(int player,int event,uint32_t id){if(sampleReadDepth)throw std::runtime_error("native event inside sample scope");if(player!=0||event!=0x15)throw std::runtime_error("wrong event");++queueAttempts;if(!queueAvailable)return false;sent.push_back({id,currentFrame});return true;}
#include REPAIR_EVENT
#include REPAIR_CALCULATION
#include REPAIR_TICK
// Exactly the manual Repair branch and terminal publication from command().
static void manualRepair(uint32_t req,DWORD now){prepareSample();uint32_t house=lastHouse;int result=1,n=0;
#include REPAIR_MANUAL
 sh->result=result;sh->count=n;pendingAck=req;pendingAckReady=true;
}
static void productionWorldReset(){
#include REPAIR_RESET
}
static unsigned checks=0,failed=0,cases=0;static bool regressionOnly=false;
static void expect(bool condition,const char*name){++checks;if(!condition)throw std::runtime_error(name);}
static void setup(bool automatic=true,bool internal=true,unsigned count=1){
 shared={};snap={};sh=&shared;sh->flags=automatic?AutoRepair:0;sh->computeApplied=internal?compute::Default:0;
 ++match;lastHouse=0x200000;currentFrame=0;chat=externalInputFrame=false;queueAvailable=true;perFrameBudget=64;buildingScans=notifications=queueAttempts=indexCalls=0;sent.clear();buildings.clear();
 for(unsigned i=0;i<count;++i)buildings.push_back({0x300000+i*0x10000,0x800000+i*0x10000,100+i,lastHouse});
 repairCount=repairAt=repairDone=0;repairAckRequest=repairAckStarted=pendingAck=0;repairAckPending=pendingAckReady=false;repairCalculationPending=false;repairCalculationStarted=repairTaskSeen=0;autoWas=false;lastAuto=0;pendingRepairs.clear();repairFrames.clear();
 worldFrameReady=false;sampleAvailable=true;sampleReadDepth=0;sampleReadComplete=true;missingSamplePage=0;runtimeDemandBits.fill(0);runtimeWorldDemands.clear();
}
static void tick(DWORD now,int frame,bool input=false){currentFrame=frame;externalInputFrame=input;prepareSample();repairTick(lastHouse,now);}
struct WorkerResult{uint32_t task;compute::RepairResult value;};
static WorkerResult calculate(){return {sh->repairTask,compute::repair(sh->repairInput)};}
static void deliver(const WorkerResult&r){++sh->repairResultSeq;sh->repairResult=r.value;sh->repairComputed=r.task;++sh->repairResultSeq;}
template<class F>static void run(const char*name,bool regression,F fn){if(regressionOnly&&!regression)return;const unsigned before=checks;++cases;try{fn();std::printf("CASE\t%s\tPASS\t%u\n",name,checks-before);}catch(const std::exception&e){++failed;std::printf("CASE\t%s\tFAIL\t%u\t%s\n",name,checks-before,e.what());}}
int main(int argc,char**argv){regressionOnly=argc>1&&std::string(argv[1])=="--regression-only";
 run("missing_sample_preserves_immediate_retry",false,[]{
  setup();sampleAvailable=false;tick(1000,1);expect(!autoWas&&lastAuto==0&&sent.empty(),"missing frame must not consume automatic interval");sampleAvailable=true;tick(1016,2);expect(sent.size()==1&&lastAuto==1016&&autoWas,"next complete frame retries without two-second delay");
 });
 run("missing_page_preserves_due_retry",false,[]{
  setup(true,true,0);tick(1000,1);buildings.push_back({0x300000,0x800000,100,lastHouse});missingSamplePage=0x300000;tick(3000,2);expect(!autoWas&&lastAuto==1000&&sent.empty(),"incomplete collector must not advance timer or publish empty candidates");missingSamplePage=0;tick(3016,3);expect(sent.size()==1&&lastAuto==3016,"page recovery retries on next admitted frame");
 });
 run("late_automatic_result_after_off",true,[]{
  setup(true,false);tick(1000,1);auto old=calculate();expect(repairCalculationPending,"automatic calculation started");
  sh->flags=0;tick(1008,2);deliver(old);tick(1016,3);
  expect(sent.empty(),"disabled automatic calculation must not enqueue a late Repair toggle");expect(!repairCalculationPending,"disabled calculation retired");expect(!repairFrames.find(old.task),"cancelled provenance retired");expect(!snap.autoRepair,"off published");
 });
 run("off_then_manual_in_same_dispatch",true,[]{
  setup(true,false);tick(1000,1);auto old=calculate();sh->flags=0;currentFrame=2;manualRepair(17,1008);
  expect(repairAckPending&&repairAckRequest==17,"manual request must not be rejected by cancelled automatic work");auto manual=calculate();expect(manual.task!=old.task,"manual has new task serial");
  deliver(old);tick(1016,3);expect(sent.empty()&&repairAckPending,"old automatic result does not satisfy manual request");
  deliver(manual);tick(1024,4);expect(sent.size()==1&&sh->count==1&&sh->result==1,"manual repair completes with automation off");expect(pendingAckReady&&pendingAck==17&&!repairAckPending,"manual terminal ACK retained");
 });
 run("off_on_rejects_old_generation",true,[]{
  setup(true,false);tick(1000,1);auto old=calculate();sh->flags=0;tick(1008,2);sh->flags=AutoRepair;tick(1016,3);auto fresh=calculate();
  expect(fresh.task!=old.task,"re-enable must capture a new cohort");expect(!repairFrames.find(old.task),"old task cannot be reused");
  deliver(old);tick(1024,4);expect(sent.empty()&&repairCalculationPending,"late cancelled task rejected after re-enable");deliver(fresh);tick(1032,5);expect(sent.size()==1&&!repairCalculationPending,"fresh task works");
 });
 run("off_before_result_poll_same_tick",false,[]{
  setup(true,false);tick(1000,1);auto old=calculate();deliver(old);sh->flags=0;tick(1008,2,true);
  expect(sent.empty()&&!repairCalculationPending,"ready old result cancelled before polling");expect(notifications==1,"off does not publish replacement automatic jobs");
 });
 run("off_keeps_manual_async_work",false,[]{
  setup(false,false);currentFrame=1;manualRepair(25,1000);auto manual=calculate();expect(repairAckPending,"manual pending");
  sh->flags=AutoRepair;tick(1008,2);sh->flags=0;tick(1016,3);expect(repairAckPending&&repairCalculationPending,"automatic toggles do not cancel manual computation");
  deliver(manual);tick(1024,4);expect(sent.size()==1&&pendingAck==25&&sh->count==1,"manual delivered once and acknowledged");
 });
 run("manual_internal_supersedes_unsent_auto_batch",false,[]{
  setup(true,true,3);perFrameBudget=1;tick(1000,1);expect(sent.size()==1&&repairAt<repairCount,"automatic partial batch");
  sh->flags=0;currentFrame=2;manualRepair(31,1008);expect(repairAckPending&&repairCount==2,"manual retains only unsubmitted buildings");tick(1008,2);tick(1016,3);
  expect(sent.size()==3&&sh->count==2&&pendingAck==31,"manual completion counts its own submissions");expect(pendingRepairs.count==3,"already queued identities retained");
 });
 run("off_discards_unsubmitted_batch",false,[]{
  setup(true,true,3);perFrameBudget=1;tick(1000,1);sh->flags=0;tick(1008,2);tick(1016,3);
  expect(sent.size()==1&&repairAt>=repairCount,"disabled batch stops before remaining targets");expect(pendingRepairs.count==1,"submitted toggle receipt survives off");expect(!buildings[0].repairing,"tool does not mutate native repairing flag");
 });
 run("queue_full_off_stops_retry",false,[]{
  setup();queueAvailable=false;tick(1000,1);expect(queueAttempts==1&&sent.empty(),"queue refusal captured");sh->flags=0;queueAvailable=true;tick(1008,2);tick(1016,3);
  expect(queueAttempts==1&&sent.empty(),"closing prevents unsent queue retry");
 });
 run("default_interval_and_no_frame_scan",false,[]{
  setup(true,true,0);tick(1000,1);expect(buildingScans==1,"enable scans immediately");for(int i=1;i<20;++i)tick(1000+DWORD(i*100),i+1);
  expect(buildingScans==1,"no recurring scan before two seconds");tick(3000,21);expect(buildingScans==2,"two-second deadline");tick(3000,21);expect(buildingScans==2,"duplicate wake cannot rescan");
 });
 run("live_interval_shortening_and_lengthening",false,[]{
  setup(true,true,0);tick(1000,1);sh->repairMs=500;tick(1499,2);expect(buildingScans==1,"new shorter interval not early");tick(1500,3);expect(buildingScans==2,"shorter interval applies now");
  sh->repairMs=4000;tick(2000,4);expect(buildingScans==2,"longer interval postpones next sweep");tick(5499,5);expect(buildingScans==2,"long interval just before deadline");tick(5500,6);expect(buildingScans==3,"long interval exact deadline");
  sh->repairMs=500;tick(6300,7);expect(buildingScans==4,"shortening after elapsed interval triggers one sweep");
 });
 run("interval_clamp_extremes",false,[]{
  for(uint32_t value:{0u,1u,499u,500u,2000u,30000u,30001u,UINT32_MAX}){const DWORD expected=value<500?500:value>30000?30000:value;expect(clampedRepairInterval(value)==expected,"runtime interval clamp");
   setup(true,true,0);sh->repairMs=value;tick(0,1);tick(expected-1,2);expect(buildingScans==1,"clamped interval not early");tick(expected,3);expect(buildingScans==2,"clamped interval deadline");}
 });
 run("clock_wrap_interval",false,[]{
  setup(true,true,0);sh->repairMs=500;const DWORD start=UINT32_MAX-249;tick(start,1);tick(DWORD(start+499),2);expect(buildingScans==1,"wrap does not shorten interval");tick(DWORD(start+500),3);expect(buildingScans==2,"wrap deadline works");
 });
 run("enable_after_off_scans_immediately",false,[]{
  setup(true,true,0);sh->repairMs=30000;tick(1000,1);sh->flags=0;tick(1008,2);sh->flags=AutoRepair;tick(1016,3);expect(buildingScans==2,"re-enable does not inherit old long delay");
 });
 run("queued_toggle_dedup_survives_off_on",false,[]{
  setup();sh->repairMs=500;tick(1000,1);expect(sent.size()==1,"first toggle queued");sh->flags=0;tick(1008,2);sh->flags=AutoRepair;tick(1016,3);
  for(int i=1;i<=5;++i)tick(1016+DWORD(500*i),3+i);expect(sent.size()==1&&pendingRepairs.count==1,"unobserved toggle is never retried by timers");
  buildings[0].repairing=true;tick(3600,10);expect(!pendingRepairs.count&&sent.size()==1,"native repairing evidence retires receipt");
  buildings[0].repairing=false;buildings[0].hp=100;tick(4100,11);expect(sent.size()==1,"healthy building not toggled");buildings[0].hp=50;tick(4600,12);expect(sent.size()==2,"new repair need can be submitted later");
 });
 run("manual_native_repair_untouched",false,[]{
  setup(false,true,2);buildings[0].repairing=true;currentFrame=1;manualRepair(40,1000);tick(1000,1);expect(sent.size()==1&&sent[0].id==buildings[1].id,"already repairing building excluded");
  expect(buildings[0].repairing&&sh->count==1&&pendingAck==40,"manual existing repair preserved");
 });
 run("same_frame_submission_budget",false,[]{
  setup(true,true,3);perFrameBudget=1;tick(1000,1);tick(1001,1);expect(sent.size()==1,"duplicate callback shares frame budget");tick(1008,2);expect(sent.size()==2,"next frame resumes batch");tick(1016,3);expect(sent.size()==3,"batch completes");
 });
 run("duplicate_host_ids_submit_once",false,[]{
  setup(true,false);tick(1000,1);auto result=calculate();result.value.count=2;result.value.ids[1]=result.value.ids[0];deliver(result);tick(1008,2);
  expect(sent.size()==1&&queueAttempts==1,"duplicate candidates never duplicate Repair event");
 });
 run("manual_timeout_retained_while_off",false,[]{
  setup(false,false);currentFrame=1;manualRepair(50,1000);tick(2000,2);expect(repairAckPending&&repairCalculationPending,"manual stays pending through deadline");tick(2001,3);
  expect(!repairAckPending&&!repairCalculationPending&&pendingAck==50&&sh->result==-7&&sent.empty(),"manual timeout follows existing terminal ACK");
 });
 run("automatic_timeout_respects_scan_interval",false,[]{
  setup(true,false);tick(1000,1);tick(2001,2);expect(!repairCalculationPending&&notifications==1,"timeout does not scan each frame");tick(2999,3);expect(notifications==1,"retry waits interval");tick(3000,4);expect(repairCalculationPending&&notifications==2,"next scheduled attempt");
 });
 run("manual_timeout_clock_wrap",false,[]{
  setup(false,false);const DWORD start=UINT32_MAX-499;currentFrame=1;manualRepair(60,start);tick(DWORD(start+1000),2);expect(repairAckPending,"timeout wrap not early");tick(DWORD(start+1001),3);expect(pendingAck==60&&sh->result==-7&&!repairCalculationPending,"timeout wrap terminal");
 });
 run("new_world_resets_timer_and_receipts",false,[]{
  setup();sh->repairMs=30000;tick(1000,1);expect(sent.size()==1&&pendingRepairs.count==1,"old world receipt");productionWorldReset();++match;tick(1008,1);
  expect(sent.size()==2&&snap.repairTotal==1&&buildingScans==2,"new world scans immediately with fresh accounting");
 });
 run("old_world_result_rejected",false,[]{
  setup(true,false);tick(1000,1);auto old=calculate();productionWorldReset();++match;tick(1008,1);auto fresh=calculate();deliver(old);tick(1016,2);expect(sent.empty(),"old world result cannot publish candidates");deliver(fresh);tick(1024,3);expect(sent.size()==1,"fresh world result accepted");
 });
 run("task_serial_wrap_rejects_cancelled_work",false,[]{
  setup(true,false);sh->repairTask=sh->repairComputed=UINT32_MAX-1;tick(1000,1);auto old=calculate();expect(old.task==UINT32_MAX,"last nonzero task");sh->flags=0;tick(1008,2);sh->flags=AutoRepair;tick(1016,3);auto fresh=calculate();expect(fresh.task==1,"task wrap skips zero");
  deliver(old);tick(1024,4);expect(sent.empty(),"cancelled task before wrap rejected");deliver(fresh);tick(1032,5);expect(sent.size()==1,"new wrapped task accepted");
 });
 run("manual_refresh_survives_auto_off",false,[]{
  setup(false,false);currentFrame=1;manualRepair(70,1000);tick(1008,2,true);expect(notifications==2&&repairAckPending,"manual can refresh source even with automation off");auto fresh=calculate();deliver(fresh);tick(1016,3);expect(sent.size()==1&&pendingAck==70,"refreshed manual work completed");
 });
 std::printf("SUMMARY\t%u\t%u\t%u\n",cases,checks,failed);return failed?1:0;
}

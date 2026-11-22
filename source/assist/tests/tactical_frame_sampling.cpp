// Actual tactical collector functions are extracted by tactical_frame_sampling.py.
// Memory/native seams are synthetic; no original game or Windows is executed.
#include "protocol.h"
#include "object_index.h"
#include "ore_queue.h"
#include "native_orders.h"
#include "unload_followup_state.h"
#include "repair_delivery.h"
#include "unit_task_arbiter.h"
#include "execution_proof.h"
#include "../../shared/source/world_frame.h"
#include "../../shared/source/world_frame_capture.h"
#include "../../shared/source/world_frame_command_index.h"
#include "../../shared/source/runtime_fault.h"
#include "../../external_reader/source/cooperative_reader.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <unordered_map>
#include <set>
#include <memory>
#include <string>
#include <vector>
using namespace assist;
using DWORD=uint32_t;
static Shared*sh;static Snapshot snap;static uint32_t match=1,lastHouse=0x20000;
static bool fault=false,externalInputFrame=true;static DWORD clockNow=1000;
static uint64_t readRequests=0;
static unsigned leaseChecks=0;static bool leaseAllowed=true;
static unsigned nativeCalls=0,hostNotifies=0,submissions=0,liveReads=0,liveRosterReads=0,missingLiveReads=0;
static bool denyLivePage=false;static uint32_t missingPage=0,nextAddress=0x100000;static int enterResult=3;
static std::vector<uint8_t> memory(0xC00000);
static std::function<void()> nativeMutation;
static std::map<uint32_t,int> airOverride;
static std::set<uint64_t> forbiddenEnter;
static std::vector<std::pair<uint32_t,uint32_t>> assignments;
static worldframe::Snapshot worldFrame;
static worldframe::Snapshot worldFrameCandidate;
static bool worldFrameCandidateAvailable=false;
static const worldframe::Snapshot*sampleWorldSource=nullptr;
static uint32_t sampleScopeTick=0;
static bool observationalFrameReady(){
 return worldFrameCandidateAvailable&&worldFrameCandidate.sealed()&&
        worldFrameCandidate.key.match==match&&worldFrameCandidate.key.frame<=snap.frame;
}
static uint32_t GetTickCount(){return clockNow;}
// This fixture seals raw synthetic bytes, then builds the same worker-owned
// CommandRoster used by production before exposing the immutable index.
static worldframe::CaptureScratch worldFrameCaptureScratch;
static bool worldFrameReady=false,sampleReadComplete=true;
static unsigned sampleReadDepth=0;
static std::array<uint64_t,8192> runtimeDemandBits{};
static std::vector<uint32_t> runtimeWorldDemands;
static bool runtimeDemandOverflow=false,runtimeDemandBufferPrepared=false;
static uint32_t runtimeDemandRevision=1;
struct CommandWorkerFixture {static bool enabled(){return true;}};static CommandWorkerFixture worldFrameCommandWorker;
static worldframe::CommandRoster worldFrameCommandRoster;
// The fixture models the canonical worker as already completed for each
// sealed synthetic frame. Observation-only candidates remain a separate type
// and are never treated as current command authority.
static worldframe::CommandRoster worldFrameCandidateRoster;
static uint32_t worldFrameCandidateVersion=0;
static bool candidateReadyForCooperative=false;
static bool candidateRosterReady(){return candidateReadyForCooperative;}
static bool candidateEntryCohortReady(){return candidateRosterReady()&&worldFrameCandidateRoster.count>0&&worldFrameCandidateRoster.count<=64;}
static bool verifyCandidateID(uint32_t id,uint32_t pointer){
 if(sampleReadDepth)return worldFrameCandidateRoster.objects->byId(id)==pointer;
 uint32_t liveID=0;std::memcpy(&liveID,memory.data()+pointer+0x10,4);return liveID==id;
}
static bool verifyCandidatePointer(uint32_t pointer){
 uint32_t id=0;
 if(sampleReadDepth)return worldFrameCandidateRoster.objects->contains(pointer);
 std::memcpy(&id,memory.data()+pointer+0x10,4);return worldFrameCandidateRoster.objects->byId(id)==pointer;
}
static bool commandRosterReady(){
 return worldFrameReady&&worldFrameCommandWorker.enabled()&&
        worldFrameCommandRoster.current(worldFrame.key)&&
        worldFrame.key.match==match&&worldFrame.key.frame==snap.frame&&
        worldFrame.key.house==lastHouse&&
        worldFrameCommandRoster.global==bool(sh&&sh->magnetronGlobal);
}
#include "sample_scope_production.h"
static bool liveRead(uint32_t at,void*out,size_t n){
 ++liveReads;if(at==0xA8EC78||(at>=0x800000&&at<0x810000))++liveRosterReads;
 if(missingPage&&at<missingPage+4096&&uint64_t(at)+n>missingPage){++missingLiveReads;if(denyLivePage)return false;}
 if(at<0x10000||uint64_t(at)+n>memory.size())return false;
 std::memcpy(out,memory.data()+at,n);return true;
}
static bool read(uint32_t at,void*out,size_t n){++readRequests;return sampleReadDepth?readWorldSample(at,out,n):liveRead(at,out,n);}
template<class T>static T get(uint32_t at){T value{};read(at,&value,sizeof value);return value;}
template<class T>static void set(uint32_t at,T value){assert(uint64_t(at)+sizeof value<=memory.size());std::memcpy(memory.data()+at,&value,sizeof value);}
static uint32_t alloc(){const uint32_t out=nextAddress;nextAddress+=0x2000;assert(nextAddress<0x400000);return out;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t at,int limit=16384){Vec v{};if(!read(at,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||v.items<0x10000)return {};return v;}
static uint32_t item(Vec v,int n){return n>=0&&n<v.count?get<uint32_t>(v.items+uint32_t(n)*4):0;}
#include "tactical_frame_read.h"
static ObjectIndexView worldObjects;static ManualVersions manualVersions;
static int kind(uint32_t p){const uint32_t vt=get<uint32_t>(p);return vt==0x7F5C70?2:vt==0x7EB058?1:vt==0x7E3EBC?3:vt==0x7E22A4?0:-1;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
static bool live(uint32_t p){return p>=0x10000&&get<int>(p+0x6C)>0&&get<bool>(p+0x74)&&!get<bool>(p+0x81)&&get<bool>(p+0x90);}
static bool own(uint32_t p,uint32_t h){return live(p)&&get<uint32_t>(p+0x21C)==h;}
static uint32_t worldByID(uint32_t id){uint32_t p=worldObjects.byId(id),actual=0;return p&&read(p+0x10,&actual,4)&&actual==id?p:0;}
static uint32_t rawObjectID(uint32_t p){uint32_t id=0;return worldObjects.contains(p)&&read(p+0x10,&id,4)?id:0;}
static bool nativeEnemy(uint32_t p,uint32_t h){
 if(!live(p)||kind(p)<0)return false;uint32_t owner=get<uint32_t>(p+0x21C);if(owner<0x10000||owner==h)return false;
 uint32_t country=get<uint32_t>(owner+0x34);if(country<0x10000||get<bool>(country+0x1A6))return false;
 int idx=get<int>(owner+0x30);return idx>=0&&idx<32&&!(get<uint32_t>(h+0x5788)&(1u<<idx));
}
static bool unloadBuildingCandidate(uint32_t p,uint32_t h,bool){return nativeEnemy(p,h);}
static uint32_t readMapCell(int x,int y){if(x<=0||y<=0||x>=512||y>=512)return 0;const int index=y*512+x,count=get<int>(0x87F7E8+320);uint32_t cells=get<uint32_t>(0x87F7E8+316);return cells>=0x10000&&count>index&&count<=512*512?get<uint32_t>(cells+uint32_t(index)*4):0;}
static bool sameid(const char*a,const char*b){for(int i=0;i<24;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return true;}
static featuretypes::Config featureConfig{};
static bool refreshFeatureTypes(){return true;}static long featureConfigRevision=0;
static bool featureTypeAllowed(featuretypes::Feature,uint32_t type){return type>=0x10000;}
static bool featureObjectAllowed(featuretypes::Feature f,uint32_t p){return p&&featureTypeAllowed(f,typeOf(p,kind(p)));}
static bool stickyOwns(uint32_t){return false;}static bool formationOwns(uint32_t){return false;}static bool burstOwns(uint32_t){return false;}
static bool minerDefensePending(){return false;}static bool minerDefenseMiner(uint32_t);static bool minerDefenseType(uint32_t,int){return false;}static bool minerDefenseOwns(uint32_t){return false;}static bool minerDefenseThreat(uint32_t,uint32_t,bool){return false;}
static bool antiTheftBuilding(uint32_t){return false;}static bool antiTheftThreat(uint32_t,uint32_t){return false;}
struct NativeOrder {uint32_t id=0,cell=0,token=0,mode=0;bool global=false;uint32_t pointer=0;};
static NativeOrder nativeOrders[128];static int nativeCount=0;
static int orderIndex(uint32_t p){for(int n=0;n<nativeCount;++n)if(nativeOrders[n].pointer==p)return n;return -1;}
static int dodgeCount=0,missileHoldCount=0,dodgeCancelled=0,dodgeObserved=0;
static taskarbiter::LeaseArbiter<64> actorLeaseArbiter;
static oreconfig::Config oreConfig{};static long oreConfigRevision=0;
static bool refreshOreConfig(){return true;}
static DWORD sampleClock(Shared*){return clockNow;}
static void MemoryBarrier(){}
template<class T>static void InterlockedIncrement(volatile T*p){assert(!sampleReadDepth);++*p;}
static void notifyHost(){assert(!sampleReadDepth);++hostNotifies;}
static void nativeBoundary(){assert(!sampleReadDepth);++nativeCalls;if(nativeMutation){auto mutation=std::move(nativeMutation);nativeMutation={};mutation();}}
template<class R,class...Args>static R call(uint32_t,uint32_t,Args...){nativeBoundary();return R(1);}
static bool rawMagnetronDrop(uint32_t p,uint32_t q,uint32_t){nativeBoundary();return p&&q;}
namespace gameentry {enum class Client{Assist};static bool active(Client){++leaseChecks;return leaseAllowed;}}
static runtimefault::Context faultContext;
struct FaultPhase {runtimefault::Context previous=faultContext;FaultPhase(unsigned phase,unsigned source=0,unsigned target=0){faultContext={phase,source,target};}~FaultPhase(){faultContext=previous;}};
extern "C" int GuardedEnterAction(unsigned,unsigned p,unsigned target,unsigned*out){
 if(!leaseAllowed)std::abort();nativeBoundary();const uint64_t key=(uint64_t(get<uint32_t>(p+0x10))<<32)|get<uint32_t>(target+0x10);
 *out=live(p)&&live(target)&&!forbiddenEnter.count(key)?unsigned(enterResult):0;return 1;
}
extern "C" int GuardedObjectAir(unsigned p,int*out){nativeBoundary();auto found=airOverride.find(p);*out=found==airOverride.end()?(kind(p)==0||get<bool>(p+0x134)):found->second;return 1;}
extern "C" int GuardedCombatWeapon(unsigned p,unsigned q,unsigned*weapon,int*slot,int*close,int*air){nativeBoundary();*weapon=get<uint32_t>(typeOf(p,kind(p))+0x898);*slot=0;*close=1;auto found=airOverride.find(q);*air=found==airOverride.end()?(kind(q)==0||get<bool>(q+0x134)):found->second;return *weapon?1:0;}
static bool interfacesReady=true;
static unsigned loadingFinished=0,loadingBoarded=0,loadingResends=0;
static bool loadInterfaces(){assert(!sampleReadDepth);return interfacesReady;}
enum class EventLane {Boarding};
static int recurringBudget(EventLane){return 4;}
static bool outgoingRoom(){return true;}
static int planningRoom(int){return 64;}
static bool nativeReady(){return true;}
static int cooperativeSubmitted=0;
static uint32_t cooperativeUnreadable=0;
static int admitCooperative(const cooperative::Assignment&o,int,uint32_t house,int,DWORD now,const executionproof::Ticket*proof,bool=false){
 assert(!sampleReadDepth&&proof&&proof->actor.id==o.id&&proof->target.id==o.target);
 if(o.id==cooperativeUnreadable)return -2;
 const auto result=executionproof::verify(*proof,{match,house,now},
   [](uint32_t id){return worldByID(id);},
   [](uint32_t id){return manualVersions.get(id);},
   [](uint32_t p,uint32_t h){return get<uint32_t>(p+0x21C)==h;});
 if(result!=executionproof::Failure::None)return -1;
 ++cooperativeSubmitted;return 1;
}
static int loadingSubmitResult=1;
static bool loadingCandidatePath=false,candidateCapacityReady=false;
static unsigned candidateCapacityChecks=0;
static bool candidateLoadAdmission(uint32_t,uint32_t,uint32_t,uint32_t,int){
 assert(!sampleReadDepth);++candidateCapacityChecks;return candidateCapacityReady;
}
static bool enterEvent(uint32_t,uint32_t,uint32_t,const executionproof::Ticket&,uint32_t*ticket){++loadingResends;if(ticket)*ticket=1234;return true;}
static loading::Ledger<> loadLedger;
// This fixture intentionally isolates AutoLoad sampling; cooperative pending
// capacity is exercised separately against source-extracted production code.
static bool cooperativePendingActor(uint32_t){return false;}
static int cooperativePendingSlots(uint32_t,uint32_t){return 0;}
static repairdelivery::Pending<> pendingRepairs;
static compute::FrameHistory<compute::RepairInput> repairFrames;
static uint32_t repairIDs[8192],repairTaskSeen;static int repairCount,repairAt,repairDone;
static bool repairCalculationPending;static DWORD repairCalculationStarted;
static void finishLoadingReservation(const loading::Reservation&,uint32_t,int,bool boarded){assert(!sampleReadDepth);++loadingFinished;if(boarded)++loadingBoarded;}
static int submitLoading(uint32_t,const loading::Infantry*soldiers,int ns,const loading::Vehicle*vehicles,int nv,const int*assignment,const uint32_t*tokens,bool candidateOnly=false){
 loadingCandidatePath=candidateOnly;
 if(loadingSubmitResult==-30)return -30;
 assert(!sampleReadDepth);++submissions;assignments.clear();
 for(int n=0;n<ns;++n){const int target=assignment[n];if(target>=0&&target<nv&&worldByID(soldiers[n].id)==soldiers[n].object&&manualVersions.get(soldiers[n].id)==tokens[n])assignments.emplace_back(soldiers[n].id,vehicles[target].id);}
 return 1;
}
static int indexedFrame=-1,indexedCount=-1,globalVehicleCandidateCount=0;
static uint32_t indexedItems=0,indexedVehicleHouse=0,globalVehicleCandidates[16384]{};static bool indexedGlobalVehicles=false;
static void minerDefenseReindexManual(ManualVersions&){}
#include "tactical_frame_production.h"
static void cancelDodge(uint32_t id){for(int i=0;i<dodgeCount;++i)if(dodgeJobs[i].id==id){dodgeJobs[i]=dodgeJobs[--dodgeCount];++dodgeCancelled;return;}}
static void recordReceipt(const gateway::Receipt&){++dodgeObserved;}
#include "fixtures/tactical_frame/missile_edge_pre1513.h"
static compute::FrameHistory<compute::LoadInput>legacyLoadFrames;
static DWORD loadCalculationStarted; // Used only by the frozen pre-migration controls.
#define loadFrames legacyLoadFrames
#include "fixtures/tactical_frame/autoload_before.h"
#include "fixtures/tactical_frame/autoload_pre1513.h"
#undef loadFrames
static int harvestEvents=0;static planner::Order harvestLast{};
static int admitPlannerOrder(const planner::Order&o,uint32_t house){assert(!sampleReadDepth);if(!oreHarvestOrderValid(o,house))return -1;++harvestEvents;harvestLast=o;return 1;}
static void check(bool value,const char*why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static void vectorAt(uint32_t at,uint32_t items,const std::vector<uint32_t>&list){Vec value{0,items,int(list.size()),0,int(list.size()),0};std::memcpy(memory.data()+at,&value,sizeof value);if(!list.empty())std::memcpy(memory.data()+items,list.data(),list.size()*4);}
static uint32_t makeType(){uint32_t type=alloc();set<bool>(type+0x231,true);set<bool>(type+0xD3A,true);set<bool>(type+0xD38,true);set<int>(type+0x684,-1);set<uint32_t>(type+0x10,type/0x2000);return type;}
static uint32_t object(uint32_t type,int k,uint32_t id,uint32_t owner=0x20000,int x=1000,int y=1000){
 const uint32_t tables[]={0x7E22A4,0x7EB058,0x7F5C70,0x7E3EBC};uint32_t p=alloc();set<uint32_t>(p,tables[k]);set<uint32_t>(p+0x10,id);set<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4),type);
 set<uint32_t>(p+0x21C,owner);set<int>(p+0x6C,100);set<bool>(p+0x74,true);set<bool>(p+0x90,true);set<int>(p+0x9C,x);set<int>(p+0xA0,y);set<int>(p+0xAC,5);set<int>(p+0xB4,-1);set<int>(p+0x2FC,20);set<uint32_t>(tables[k]+0x74,0x600000);worldObjects.add(p,id);return p;
}
static uint32_t makeWeapon(uint32_t type,bool air=true,bool ground=true){uint32_t w=alloc(),projectile=alloc(),warhead=alloc();set<uint32_t>(type+0x898,w);set<int>(w+0xB4,4096);set<int>(w+0xA4,40);set<uint32_t>(w+0xA0,projectile);set<uint32_t>(w+0xAC,warhead);set<bool>(projectile+0x2A4,air);set<bool>(projectile+0x2A5,ground);for(int n=0;n<11;++n)set<double>(warhead+0xA0+n*8,1.);return w;}
static void seal(uint32_t omit=0,bool knownFailure=false){
 check(!sampleReadDepth,"scope leaked into next capture");worldFrameReady=false;
 worldObjects.unbind();worldObjects.clear();worldFrameCommandRoster.invalidate();
 indexedFrame=indexedCount=-1;indexedItems=0;globalVehicleCandidateCount=0;
 indexedGlobalVehicles=false;indexedVehicleHouse=0;
 worldframe::Key key{};key.match=match;key.frame=snap.frame;key.house=lastHouse;key.tick=clockNow;worldFrame.reset(key);
 runtimeWorldDemands.clear();runtimeDemandBits.fill(0);missingPage=omit&~4095u;
 for(uint32_t page=0x10000;page<memory.size();page+=4096){if(page==missingPage&&!knownFailure)continue;
  auto acquire=[&](uint32_t at,void*out,size_t n){if(missingPage&&at<missingPage+4096&&uint64_t(at)+n>missingPage)return false;return liveRead(at,out,n);};
  worldFrame.prefetch(acquire,page,4096);
 }
 check(worldFrame.seal(),"seal failed");
 worldFrameReady=worldFrameCommandRoster.build(worldFrame,bool(sh&&sh->magnetronGlobal),worldFrameCaptureScratch.list);
 if(worldFrameReady)check(refreshWorldIndex(snap.frame),"worker roster failed to bind");
 liveReads=liveRosterReads=missingLiveReads=0;
}
static void advance(uint32_t omit=0,bool failure=false){++snap.frame;clockNow+=16;set<int>(0xA8ED84,snap.frame);seal(omit,failure);}
static void reset(){
 check(!sampleReadDepth,"scope leaked between tests");memory.assign(memory.size(),0);::new(static_cast<void*>(sh)) Shared{};std::memset(&snap,0,sizeof snap);
 leaseChecks=0;leaseAllowed=true;worldObjects.unbind();worldObjects.clear();worldFrameCommandRoster.invalidate();worldFrameCandidateRoster.invalidate();worldFrameCandidateAvailable=false;indexedFrame=indexedCount=-1;indexedItems=0;globalVehicleCandidateCount=0;indexedGlobalVehicles=false;indexedVehicleHouse=0;manualVersions.clear();actorLeaseArbiter.clear();loadLedger.clear();nextAddress=0x100000;lastHouse=0x20000;++match;clockNow=1000;snap.status=snap.commandStatus=1;snap.frame=100;snap.session=3;snap.match=match;
 worldFrameReady=false;worldFrameCaptureScratch.reset();nativeCalls=hostNotifies=submissions=liveReads=liveRosterReads=0;missingPage=missingLiveReads=0;denyLivePage=false;nativeMutation={};airOverride.clear();forbiddenEnter.clear();assignments.clear();enterResult=3;nativeCount=dodgeCount=missileHoldCount=0;oreConfig={};oreConfigRevision=0;fault=false;
 loadCalculationPending=loadCaptureWaiting=false;candidateReadyForCooperative=false;loadSelectionCount=0;loadCarrierSelectionCount=-1;loadFrames.clear();loadTaskSeen=0;
 missileSendCount=missileManualCount=missileMotionCursor=missileSourceCursor=missileTargetCursor=missileManagedCursor=0;for(auto&m:missileMotion)m={};
 dodgeThreatCount=dodgeCursor=dodgeJobCursor=0;unloadSampleTargetFrame=-1;unloadSampleTargetCount=0;planningFrame=-1;planningWorld.count=planningWorld.intentCount=0;planningSerial=inputSerial=0;std::memset(weaponCache,0,sizeof weaponCache);oreHarvestReset();harvestEvents=0;
 repairFrames.clear();pendingRepairs.clear();repairCount=repairAt=repairDone=0;repairTaskSeen=repairCalculationStarted=0;repairCalculationPending=false;
 minerDefenseLinkedRefinery.clear();minerDefenseRefineryCount=0;minerDefenseRefineryIncomplete=false;
 set<uint32_t>(0xA83D4C,lastHouse);set<uint32_t>(lastHouse+0x10,101);set<uint32_t>(lastHouse+0x34,0x40000);set<int>(lastHouse+0x30,0);set<int>(0x30000+0x30,1);set<uint32_t>(0x30000+0x34,0x50000);
 set<uint32_t>(0xA8B230,0x60000);set<int>(0xA8B238,3);set<int>(0xA8ED84,100);set<uint8_t>(0xA8E9A0,1);set<double>(0xB0CD48,0.0866025403784);
 set<uint32_t>(0x87F924,0x400000);set<int>(0x87F928,512*512);const int bounds[]={100,100,0,0,100,100};std::memcpy(memory.data()+0x87F7E8+0xF4,bounds,sizeof bounds);
 vectorAt(0xA8EC78,0x800000,{});vectorAt(0xA8ED40,0x810000,{});vectorAt(0xA8ECB8,0x820000,{});vectorAt(0x8B4108,0x830000,{});vectorAt(0xA8EB40,0x840000,{});vectorAt(0xA83CE0,0x850000,{});
}
static void testCooperative(){
 reset();const uint32_t tt=makeType(),bt=makeType();set<double>(tt+0x380,3.);set<bool>(tt+0xD2E,true);set<bool>(bt+0x16AB,true);std::memcpy(memory.data()+bt+0x24,"TEST",5);
 uint32_t p=object(tt,2,7,lastHouse,25600,25600),target=object(bt,3,8,lastHouse,25600,25600);vectorAt(0xA8EC78,0x800000,{p,target});seal();
 cooperative::Request request{};request.serial=1;request.match=match;request.target=8;request.type=bt;request.mode=cooperative::Enter;request.count=1;request.sources[0]={7,0};request.cameraX=-320;request.cameraY=2700;request.viewWidth=640;request.viewHeight=480;
 compute::QuotaConfig quotas{};quotas.count=1;std::memcpy(quotas.entries[0].id,"TEST",5);quotas.entries[0].quota.first=7;
 cooperative::Input input;check(collectCooperativeInput(request,quotas,input)&&input.units.size()==1&&input.targets.size()==1,"cooperative complete capture");
 set<int>(p+0x6C,0);std::memcpy(memory.data()+bt+0x24,"LIVE",5);check(collectCooperativeInput(request,quotas,input)&&input.units.size()==1&&input.targets[0].quota.first==7,"cooperative shares frozen objects and quota type bytes");check(liveReads==0,"cooperative pure converter made a live read");
 set<int>(p+0x6C,100);advance(bt);check(!collectCooperativeInput(request,quotas,input)&&input.units.size()==1&&input.targets[0].quota.first==7,"cooperative missing page must preserve input");check(!missingLiveReads,"cooperative missing page fell back live");
 std::memcpy(memory.data()+bt+0x24,"TEST",5);seal();
 request.tick=clockNow;sh->cooperativeRequest=request;sh->quotaConfig=quotas;
 worldFrameCandidate.swap(worldFrame);worldFrameCandidateAvailable=true;worldFrameReady=false;
 ++snap.frame;clockNow+=16;set<int>(0xA8ED84,snap.frame);
 captureCooperativeFrame(clockNow,false);
 auto&frame=sh->cooperativeInputFrame;
 check(frame.request.serial==request.serial&&frame.frame==snap.frame-1&&frame.tick==request.tick,
       "cooperative candidate lost its source age during publication");
 sh->cooperativeFrameWanted=request.serial;
 const auto readsAfterPublication=readRequests;
 for(int i=0;i<12;++i){
  ++snap.frame;++clockNow;set<int>(0xA8ED84,snap.frame);
  captureCooperativeFrame(clockNow,false);
 }
 check(readRequests==readsAfterPublication&&frame.frame==snap.frame-13&&frame.tick==request.tick,
       "unsampled game frames rebuilt or retimestamped the cooperative graph");
 sh->quotaConfig.entries[0].quota.first=9;sh->quotaConfigSeq+=2;
 captureCooperativeFrame(clockNow,false);
 check(frame.targets[0].quota.first==9&&frame.tick==request.tick,
       "revised quota did not update the graph from the same sealed observation");
 ++snap.frame;++clockNow;set<int>(0xA8ED84,snap.frame);seal();
 const int secondSample=snap.frame;const DWORD secondTick=clockNow;
 worldFrameCandidate.swap(worldFrame);worldFrameReady=false;
 snap.frame+=9;clockNow+=10;set<int>(0xA8ED84,snap.frame);
 captureCooperativeFrame(clockNow,false);
 check(frame.frame==secondSample&&frame.tick==secondTick,
       "a newer delayed observation could not advance cooperative input");
 seal();
 captureCooperativeFrame(clockNow,true);
 check(frame.frame==snap.frame&&frame.tick==clockNow,
       "same-frame strict capture could not replace the pending cooperative observation");
 cooperative::CapturedIdentities identities;
 check(collectCooperativeInput(request,quotas,input,nullptr,&identities)&&
       identities.house==lastHouse&&identities.actors.front().pointer==p&&identities.targets.front().pointer==target,
       "capture identities did not come from the same sealed graph as the assignments");
 std::puts("PASS cooperative sealed fields, quota bytes and missing-frame retention");
}
static void testDelayedAircraftPlan(){
 reset();cooperativeCaptures={};cooperativePlanSeen=cooperativeAt=0;cooperativeCandidateAt=0;cooperativeSubmitted=0;
 uint32_t type=makeType(),target=object(type,2,30,0x30000);
 cooperative::CapturedIdentities identities;
 identities.stamp={1,match,clockNow,snap.frame};identities.house=lastHouse;
 identities.targets.push_back({30,target});
 for(uint32_t id=16;id>=10;--id){uint32_t p=object(type,0,id);identities.actors.push_back({id,p,0});}
 cooperativeCaptures.keep(std::move(identities));
 sh->flags=AircraftSpread;sh->cooperativeRequest.serial=1;sh->cooperativeRequest.match=match;sh->cooperativeRequest.tick=clockNow;
 sh->cooperativeRequest.mode=sh->cooperativeMode=cooperative::AirDamage;
 sh->cooperativeSerial=1;sh->cooperativeMatch=match;sh->cooperativeFrame=snap.frame;sh->cooperativeTick=clockNow;
 sh->cooperativeCount=7;for(int i=0;i<7;++i)sh->cooperativeOrders[i]={uint32_t(10+i),0,30,type,20};
 sh->cooperativePlan=1;candidateReadyForCooperative=true;
 snap.frame+=24;clockNow+=30;set<int>(0xA8ED84,snap.frame);
 const uint64_t start=uint64_t(clockNow)*1000;
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start);
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start);
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start+3999);
 check(cooperativeSubmitted==4&&sh->cooperativeProcessed==4&&!sh->cooperativeAck,
       "cross-frame plan exceeded its budget or acknowledged before completion");
 manualVersions.change(14);
 cooperativeUnreadable=15;worldFrameCandidate.key.frame=snap.frame;
 clockNow+=4;
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start+4000);
 check(cooperativeSubmitted==4&&sh->cooperativeAcceptedBits[0]==15&&sh->cooperativeProcessed==5&&
       sh->cooperativeResult==1&&!sh->cooperativeAck,
       "UNKNOWN aircraft identity must preserve accepted prefix and unprocessed suffix");
 cooperativeUnreadable=0;
 clockNow+=4;
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start+8000);
 check(cooperativeSubmitted==6&&sh->cooperativeAcceptedBits[0]==111&&sh->cooperativeProcessed==7&&
       sh->cooperativeAccepted==6&&sh->cooperativeRejected==1&&sh->cooperativeAck==1,
       "next complete game observation must drain remaining actors once without replaying prefix");
 sh->cooperativePlan=2;sh->cooperativeMode=cooperative::Enter;
 ++snap.frame;set<int>(0xA8ED84,snap.frame);
 serviceCooperativePlan(lastHouse,snap.frame,clockNow,true,start+12000);
 check(sh->cooperativeAck==1,"capacity-dependent boarding used an aircraft-only candidate");
 candidateReadyForCooperative=false;
 std::puts("PASS delayed aircraft admission uses frozen identity; UNKNOWN retains suffix, later 4ms pass accepts once");
}
static void setupLoad(int variant){
 reset();sh->computeApplied=compute::All;sh->loadMode=variant%2;sh->loadRadius=30;
 uint32_t tt=makeType(),it=makeType();set<int>(tt+0x5E0,4);set<double>(tt+0x388,4.);set<double>(tt+0x380,2.);set<double>(it+0x380,1.);
 uint32_t a=object(tt,2,10,lastHouse,1000,1000),b=object(tt,2,11,lastHouse,1500,1000);std::vector<uint32_t> all{a,b},selected;
 for(int n=0;n<6;++n){uint32_t p=object(it,n%2?1:2,20+n,lastHouse,900+n*180,1000);all.push_back(p);selected.push_back(p);}
 if(variant&2){selected.push_back(a);selected.push_back(b);}if(variant&4){set<int>(all[2]+0xAC,7);set<uint32_t>(all[2]+0x500,a);}
 if(variant&8){set<int>(b+0x114,1);set<uint32_t>(b+0x118,all[3]);set<uint32_t>(all[3]+0x11C,b);}
 if(variant&16){forbiddenEnter.insert((uint64_t(20)<<32)|10);forbiddenEnter.insert((uint64_t(21)<<32)|11);}
 vectorAt(0xA8EC78,0x800000,all);vectorAt(0xA8ECB8,0x820000,selected);seal();
}
static void testLoad(){
 for(int variant=0;variant<32;++variant){setupLoad(variant);int old=legacyAutoLoad(lastHouse,nullptr);auto expected=assignments;int oldUnits=snap.loadUnits,oldWaiting=snap.loadWaiting,oldUnassigned=snap.loadUnassigned;check(old==1,"legacy load fixture rejected");
  setupLoad(variant);check(pre1513AutoLoad(lastHouse,nullptr)==old&&assignments==expected,"pre.3 assignment reference changed");
  const auto oldLeases=leaseChecks,oldQueries=nativeCalls;assignments.clear();submissions=0;liveRosterReads=0;loadCalculationPending=false;leaseChecks=nativeCalls=0;int next=autoLoad(lastHouse);check(nativeCalls==oldQueries,"load changed native legality queries");check(leaseChecks==1&&oldLeases>1,"load batch repeated manager validation");check(next==old&&assignments==expected,"load assignment changed");check(snap.loadUnits==oldUnits&&snap.loadWaiting==oldWaiting&&snap.loadUnassigned==oldUnassigned,"load cohort counters changed");check(liveRosterReads==0,"load collector reacquired Techno roster");}
 setupLoad(0);uint32_t passenger=get<uint32_t>(0x820000),carrier=get<uint32_t>(0x800000);seal(carrier);const uint32_t task=sh->loadTask;
 check(autoLoad(lastHouse)==-30&&loadCaptureWaiting&&sh->loadTask==task&&!submissions,"load missing page published or executed");check(!missingLiveReads,"load missing raw page fell back live");
 vectorAt(0xA8ECB8,0x820000,{});advance();compute::LoadInput cohort{};check(autoLoad(lastHouse,&cohort)==1&&submissions==1&&!assignments.empty()&&assignments[0].first==get<uint32_t>(passenger+0x10),"load retry lost original click cohort");
 setupLoad(0);passenger=get<uint32_t>(0x820000);const uint32_t replacement=get<uint32_t>(passenger+0x10)+1000;set<uint32_t>(passenger+0x10,replacement);
 check(autoLoad(lastHouse)==-30&&loadCaptureWaiting&&!submissions,"load combined a new click identity with old sealed facts");
 advance();check(autoLoad(lastHouse,&cohort)==1&&submissions==1&&!assignments.empty()&&assignments[0].first==replacement,"load replacement identity did not retry on its own frame");
 setupLoad(0);leaseAllowed=false;const auto beforeQueries=nativeCalls;check(autoLoad(lastHouse)==-12&&nativeCalls==beforeQueries&&!submissions,"load queried native engine without a lease");leaseAllowed=true;
 setupLoad(0);sh->computeApplied=0;
 check(autoLoad(lastHouse)==-30&&loadCalculationPending,"load did not stage the external calculation");
 static loading::Workspace workspace;
 sh->loadResult=compute::load(sh->loadInput,workspace);sh->loadComputed=sh->loadTask;
 loadingSubmitResult=-30;
 loadingCalculationTick(lastHouse,clockNow);
 check(loadCalculationPending&&loadTaskSeen!=sh->loadComputed,
       "load consumed a result before capacity pages were complete");
 loadingSubmitResult=1;advance();loadingCalculationTick(lastHouse,clockNow);
 check(!loadCalculationPending&&submissions==1,
       "load did not resume the same result after capacity pages became available");

 setupLoad(0);passenger=get<uint32_t>(0x820000);carrier=get<uint32_t>(0x800000);
 const uint32_t uid=get<uint32_t>(passenger+0x10),carrierID=get<uint32_t>(carrier+0x10);
 check(autoLoad(lastHouse,nullptr,true)==-30&&loadCalculationPending&&loadSelectionCount>0&&
       !submissions,"candidate input did not retain the saved click");
 const uint32_t other=get<uint32_t>(0x820004);
 vectorAt(0xA8ECB8,0x820000,{other});
 worldFrameCandidate.swap(worldFrame);worldFrameCandidateAvailable=candidateReadyForCooperative=true;
 check(worldFrameCandidateRoster.build(worldFrameCandidate,false,worldFrameCaptureScratch.list),
       "candidate load roster could not be assembled");
 worldFrameReady=false;++snap.frame;clockNow+=16;set<int>(0xA8ED84,snap.frame);
 check(refreshWorldIndex(snap.frame)&&!commandRosterReady(),"candidate load did not bind");
 liveRosterReads=0;
 loadingCalculationTick(lastHouse,clockNow,true);
 check(!loadCalculationPending&&loadingCandidatePath&&submissions==1&&!assignments.empty()&&
       assignments[0].first==uid&&!liveRosterReads,"delayed load lost the original selection or scanned the live roster");
 loading::Reservation waiting{passenger,uid,carrier,carrierID,1,snap.frame};
 waiting.kind=kind(passenger);waiting.manualToken=manualVersions.get(uid);waiting.initial=loadingObservation(passenger);
 check(loadLedger.add(waiting),"fixture could not reserve candidate passenger");
 candidateCapacityReady=false;candidateCapacityChecks=loadingResends=0;
 autoLoadTick(lastHouse,snap.frame,true);
 check(candidateCapacityChecks==1&&!loadingResends&&loadLedger.count==1,"unknown seats issued or retired a candidate load");
 candidateCapacityReady=true;++snap.frame;set<int>(0xA8ED84,snap.frame);refreshWorldIndex(snap.frame);
 autoLoadTick(lastHouse,snap.frame,true);autoLoadTick(lastHouse,snap.frame,true);
 check(candidateCapacityChecks==2&&loadingResends==1&&snap.loadIssued==1&&loadLedger.count==1,
       "proved candidate seats did not release the staged load exactly once");
 std::puts("PASS 32 load assignment variants, frozen candidate click, bounded seat proof and single retry");
}
static void testBoardingLedgerUnknown(){
 setupLoad(0);const uint32_t passenger=get<uint32_t>(0x820000),carrier=get<uint32_t>(0x800000),passengerID=get<uint32_t>(passenger+0x10),carrierID=get<uint32_t>(carrier+0x10);
 const int sourceKind=kind(passenger);
 loading::Reservation r{passenger,passengerID,carrier,carrierID,1,snap.frame};
 r.kind=sourceKind;r.initial=loadingObservation(passenger);r.lastSent=snap.frame;r.progress=snap.frame;r.retryAfter=snap.frame+30;
 check(loadLedger.add(r),"boarding ledger fixture did not reserve");
 interfacesReady=true;loadingFinished=loadingBoarded=loadingResends=0;
 // Unknown live UID/owner/motion must not cancel, send, or change the ledger.
 for(uint32_t offset:{0x10u,0x21Cu,0x9Cu,0xACu}){
  advance();missingPage=(passenger+offset)&~4095u;denyLivePage=true;
  const int previousProgress=loadLedger.records[0].progress;
  autoLoadTick(lastHouse,snap.frame);
  check(loadLedger.count==1&&!loadingFinished&&!loadingResends&&loadLedger.records[0].progress==previousProgress,"missing live boarding field changed ledger");
  denyLivePage=false;missingPage=0;
 }
 // A temporary query-interface outage does not discard accepted native tickets.
 advance();interfacesReady=false;autoLoadTick(lastHouse,snap.frame);
 check(loadLedger.count==1&&!loadingFinished&&!loadingResends,"interface outage erased pending boarding ticket");
 interfacesReady=true;
 // Confirm the transport pointer even when the passenger has left the roster.
 set<uint32_t>(passenger+0x11C,carrier);vectorAt(0xA8EC78,0x800000,{carrier});
 advance();autoLoadTick(lastHouse,snap.frame);
 check(!loadLedger.count&&loadingFinished==1&&loadingBoarded==1&&!loadingResends,"confirmed boarding did not complete native ticket");
 // A UID replacement cannot retain or complete the predecessor's reservation.
 setupLoad(0);const uint32_t p2=get<uint32_t>(0x820000),v2=get<uint32_t>(0x800000);
 loading::Reservation old{p2,get<uint32_t>(p2+0x10),v2,get<uint32_t>(v2+0x10),1,snap.frame};
 old.kind=kind(p2);old.initial=loadingObservation(p2);old.lastSent=snap.frame;old.retryAfter=snap.frame+30;
 check(loadLedger.add(old),"replacement fixture ledger missing");
 loadingFinished=loadingBoarded=loadingResends=0;
 set<uint32_t>(p2+0x10,old.id+1000);advance();autoLoadTick(lastHouse,snap.frame);
 check(!loadLedger.count&&loadingFinished==1&&!loadingBoarded&&!loadingResends,"recycled source UID was not retired");
 // Native gateway unavailable is UNKNOWN, not a reason to expire a still-valid
 // reservation. The original UID and manual-version proof remains required.
 setupLoad(0);const uint32_t p3=get<uint32_t>(0x820000),v3=get<uint32_t>(0x800000);
 loading::Reservation expires{p3,get<uint32_t>(p3+0x10),v3,get<uint32_t>(v3+0x10),1,snap.frame};
 expires.kind=kind(p3);expires.initial=loadingObservation(p3);check(loadLedger.add(expires),"expiry fixture ledger missing");
 loadingFinished=loadingBoarded=loadingResends=0;interfacesReady=false;
 snap.frame+=901;clockNow+=901*16;set<int>(0xA8ED84,snap.frame);
 autoLoadTick(lastHouse,snap.frame);
 check(loadLedger.count==1&&!loadingFinished&&!loadingBoarded&&!loadingResends,"unavailable gateway must retain valid booking despite 900 game frames");
 interfacesReady=true;
 std::puts("PASS automatic boarding ledger: missing live reads retain, boarding confirms, identity replacement retires, age alone cannot expire");
}
static void testDodge(){
 reset();sh->dodgeEnabled=1;uint32_t type=makeType(),airType=makeType(),p=object(type,2,20),owner=object(airType,0,21,0x30000),bullet=alloc();
 set<uint32_t>(bullet,0x7E46E4);set<uint32_t>(bullet+0x10,30);set<int>(bullet+0x6C,25);set<bool>(bullet+0x90,true);set<uint32_t>(bullet+0xB0,owner);set<uint32_t>(bullet+0x10C,p);set<int>(bullet+0x140,1000);set<int>(bullet+0x144,1000);set<double>(bullet+0xE8,5.);
 vectorAt(0xA8EC78,0x800000,{p,owner});vectorAt(0xA8ED40,0x810000,{bullet});seal();int a=0,b=0;
 check(collectDodgeThreats()&&collectDodgeInput(lastHouse,snap.frame,clockNow,a,b)&&dodgeInput.count==1&&dodgeInput.sources[0].shots[0]==30,"dodge captured shot");check(liveReads==0,"dodge pure collector read live");
 reversedodge::Point delta{};set<double>(bullet+0xE8,-5.);check(collectReverseTargets(dodgeThreatRefs[0],lastHouse,delta)&&reverseTargetCount==1&&reverseTargets[0].id==20&&delta.x>0&&liveReads==0,"reverse targets/vector escaped sealed projectile");
 set<uint32_t>(bullet+0x10,31);check(collectDodgeInput(lastHouse,snap.frame,clockNow,a,b)&&dodgeInput.sources[0].shots[0]==30,"dodge sample changed after seal");check(!dodgeThreatCurrent(dodgeThreatRefs[0]),"dodge execute guard accepted reused bullet");
 advance(bullet,true);const int count=dodgeThreatCount;check(!collectDodgeThreats()&&dodgeThreatCount==count,"dodge failed roster became empty");check(!missingLiveReads,"dodge known failed page fell back live");
 // A genuinely empty bullet array is complete, including a null item buffer.
 set<uint32_t>(bullet+0x10,30);vectorAt(0xA8ED40,0,{});advance();check(collectDodgeThreats()&&dodgeThreatCount==0,"empty bullet roster rejected");
 std::puts("PASS dodge shared Bullet/Techno input, live reuse guard and failed/empty roster distinction");
}
static void testDodgeLeaseMissing(){
 reset();sh->dodgeEnabled=1;uint32_t type=makeType(),p=object(type,2,20);
 vectorAt(0xA8EC78,0x800000,{p});seal();
 dodgeJobs[0]={};dodgeCount=1;dodgeCancelled=dodgeObserved=0;
 auto&job=dodgeJobs[0];job.id=20;job.pointer=p;job.token=manualVersions.get(20);job.active=true;
 job.x=1000;job.y=1000;job.quietFrame=snap.frame;
 check(dodgeSourceKnowledge(p,lastHouse)==1,"complete same-frame dodge source should be eligible");
 // A live read can fail AFTER the sealed source was captured. That must not
 // synthesize movement, observe Scatter, or mutate the lease checkpoint.
 set<int>(p+0x9C,0);missingPage=p&~4095u;denyLivePage=true;
 updateDodgeJobs(lastHouse,snap.frame,clockNow);
 check(dodgeCount==1&&dodgeCancelled==0&&!dodgeObserved&&job.x==1000&&job.quietFrame==snap.frame,"missing motion page changed active dodge state");
 denyLivePage=false;missingPage=0;
 updateDodgeJobs(lastHouse,snap.frame,clockNow);
 check(dodgeCount==1&&dodgeCancelled==0&&job.x==0&&job.observed&&dodgeObserved==1,"recovered movement not observed");
 // Incomplete sealed source is unknown, not verified ineligible.
 advance(p);check(dodgeSourceKnowledge(p,lastHouse)==0,"missing dodge source did not report unknown");
 updateDodgeJobs(lastHouse,snap.frame,clockNow);
 check(dodgeCount==1&&!dodgeCancelled,"missing source retired active dodge owner");
 // Proven native mission loss still retires the old dodge task.
 set<int>(p+0xAC,0);advance();check(dodgeSourceKnowledge(p,lastHouse)==-1,"complete invalid native mission did not prove retirement");
 updateDodgeJobs(lastHouse,snap.frame,clockNow);
 check(dodgeCount==0&&dodgeCancelled==1,"confirmed ineligible dodge owner not retired");
 std::puts("PASS dodge lease state-machine: sealed unknown and live motion failures cannot cancel or advance job");
}
static void testReverse(){
 reset();sh->dodgeEnabled=1;uint32_t type=makeType(),airType=makeType(),p=object(type,2,20),owner=object(airType,0,21,0x30000),bullet=alloc(),cell=alloc();
 set<uint32_t>(bullet,0x7E46E4);set<uint32_t>(bullet+0x10,30);set<int>(bullet+0x6C,25);set<bool>(bullet+0x90,true);set<uint32_t>(bullet+0xB0,owner);set<uint32_t>(bullet+0x10C,cell);set<double>(bullet+0xE8,5.);
 set<uint32_t>(0x400000+uint32_t(3*512+3)*4,cell);vectorAt(0xA8EC78,0x800000,{p,owner});vectorAt(0xA8ED40,0x810000,{bullet});seal();reversedodge::Point delta{};
 check(collectDodgeThreats()&&collectReverseTargets(dodgeThreatRefs[0],lastHouse,delta)&&reverseTargetCount==1&&reverseTargets[0].id==20,"reverse Cell target lookup");check(!liveReads,"reverse Cell collector reacquired roster live");
 dodgeInput.count=1;dodgeInput.sources[0].id=77;dodgeJobs[0]={};dodgeJobs[0].active=true;dodgeJobs[0].pointer=p;dodgeJobs[0].id=20;dodgeCount=1;
 advance(p);check(!collectReverseTargets(dodgeThreatRefs[0],lastHouse,delta)&&!collectReverseDodgeInput(lastHouse,snap.frame,clockNow)&&dodgeInput.count==1&&dodgeInput.sources[0].id==77&&!missingLiveReads,"reverse missing input advanced old facts");
 reset();sh->dodgeEnabled=1;type=makeType();uint32_t crusherType=makeType();p=object(type,2,20);uint32_t crusher=object(crusherType,2,31,0x30000,1100,1000);set<bool>(type+0x22D,true);set<bool>(crusherType+0xD28,true);set<uint32_t>(crusher+0x2B4,p);set<int>(crusher+0xAC,2);vectorAt(0xA8EC78,0x800000,{p,crusher});seal();
 check(collectDodgeThreats()&&collectReverseDodgeInput(lastHouse,snap.frame,clockNow)&&dodgeInput.count==1&&dodgeInput.sources[0].id==20&&dodgeInput.sources[0].freshThreat&&dodgeInput.sources[0].count==1,"reverse crusher raw input");check(!liveReads&&!nativeCalls,"reverse pure crusher collector called live/native");
 std::puts("PASS reverse-dodge object/Cell candidates, pure crusher facts and failed-input retention");
}
static void testMissile(){
 reset();uint32_t type=makeType(),airType=makeType(),p=object(type,2,20),q=object(airType,0,40,0x30000,1300,1000);makeWeapon(type);set<bool>(airType+0xD68,true);set<int>(q+0xA4,200);sh->missileScope=0;vectorAt(0xA8EC78,0x800000,{p,q});seal();int sc=0,tc=0,mc=0;
 check(collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileInput.count==1&&missileInput.targetCount==1&&missileInput.edges[0][0].damage==40,"missile raw capture");check(!nativeCalls&&liveReads==0,"raw missile collector escaped sample");const auto expected=missileInput.edges[0][0];
 missileManual[0]={20,p,snap.frame-20};missileManualCount=1;
 check(collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileManualCount==1,"pure missile capture consumed a manual record");
 check(!missileManuallyOwned(p)&&missileManualCount==0,"live missile observation did not retire completed manual record");
 set<int>(q+0x9C,9000);check(collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileInput.edges[0][0].distance==expected.distance,"missile coordinates were recaptured live");
 advance(q);check(!collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileInput.targetCount==1&&missileInput.edges[0][0].distance==expected.distance,"missile incomplete target replaced input");check(!missingLiveReads,"missile missing target fell back live");
 reset();type=makeType();airType=makeType();p=object(type,2,20);makeWeapon(type);std::vector<uint32_t> objects{p};
 for(int n=0;n<257;++n){uint32_t candidate=object(airType,2,100+n,0x30000,1100,1000);objects.push_back(candidate);airOverride[candidate]=n==256;}
 sh->nativeCombatQueries=1;sh->missileScope=1;vectorAt(0xA8EC78,0x800000,objects);seal();
 check(collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileInput.targetCount==1&&missileInput.targets[0].id==356,"native airborne phase starved a late target");check(nativeCalls>=258&&missileInput.edges[0][0].damage==40,"native missile queries or raw edge facts missing");
 reset();type=makeType();airType=makeType();p=object(type,2,20);q=object(airType,0,40,0x30000,1300,1000);makeWeapon(type);set<bool>(airType+0xD68,true);set<int>(q+0xA4,200);
 const uint32_t alternateType=makeType(),weapon=makeWeapon(alternateType);vectorAt(0xA8EC78,0x800000,{p,q});sh->nativeCombatQueries=sh->missileEnabled=1;seal(weapon,true);
 nativeMutation=[&]{set<uint32_t>(type+0x898,weapon);pauseMissileForManual(20,p);manualVersions.change(20,p,false);};
 check(!collectMissileInput(lastHouse,snap.frame,clockNow,sc,tc,mc)&&missileManualCount==1&&manualVersions.get(20)!=0,"failed missile capture erased a later native-boundary manual pause");

 {
 // Dense, non-close cohort: exact pre.3 edge parity while sharing only
 // Runtime-owned record/motion lookups inside this sealed observation.
 reset();type=makeType();airType=makeType();const uint32_t weapon=makeWeapon(type);
 std::vector<uint32_t>sources,targets,all;for(int n=0;n<32;++n){sources.push_back(object(type,2,500+n,lastHouse,1000+n,1000));targets.push_back(object(airType,0,1000+n,0x30000,2200+n,1000));all.push_back(sources.back());all.push_back(targets.back());}
 vectorAt(0xA8EC78,0x800000,all);seal();missileSendCount=1024;
 for(int n=0;n<1024;++n){missileSends[n]={};missileSends[n].id=10000+n;}
 for(int n=0;n<32;++n){auto&r=missileSends[992+n];r.id=500+n;r.pointer=sources[n];auto&m=missileMotion[224+n];m.id=1000+n;m.pointer=targets[n];m.known=true;m.vx=-20;}
 static missilecompute::Edge beforeEdges[32][32];readRequests=0;
 {SampleReadScope scope;for(int n=0;n<32;++n)for(int t=0;t<32;++t)beforeEdges[n][t]=legacyMissileEdgeFacts(sources[n],targets[t],weapon,0,1);check(scope.complete(),"reference matrix incomplete");}
 const auto oldReads=readRequests;readRequests=0;int motions[32];std::fill_n(motions,32,-2);
 {SampleReadScope scope;for(int n=0;n<32;++n){const int record=missileRecord(500+n);for(int t=0;t<32;++t){const auto edge=missileEdgeFacts(sources[n],targets[t],weapon,0,1,record,&motions[t]);check(!std::memcmp(&edge,&beforeEdges[n][t],sizeof edge),"cached missile edge changed");}}check(scope.complete(),"indexed matrix incomplete");}
 const auto newReads=readRequests;check(newReads*4<oldReads,"missile pair loop repeated per-target lookup");
 std::printf("PASS missile dense edge parity: 1024 pairs, logical field reads %llu -> %llu; source history lookup once per row and motion identity once per column\n",(unsigned long long)oldReads,(unsigned long long)newReads);
 }
 std::puts("PASS missile raw/fresh selector facts, missing retention, manual generations and native query boundaries");
}
static void testCapturedRosterIndex(){
 reset();const auto type=makeType(),p=object(type,2,20),q=object(type,2,21);
 vectorAt(0xA8EC78,0x800000,{p,q});seal();indexedFrame=-1;
 // seal() has already built and bound the exact worker-owned UID roster.
 readRequests=0;const auto beforeLive=liveReads;
 check(refreshWorldIndex(snap.frame)&&indexedFrame==snap.frame,"worker roster index marked incomplete");
 check(worldObjects.byId(20)==p&&worldObjects.byId(21)==q,"sealed worker roster lost native UIDs");
 check(liveReads==beforeLive&&readRequests==0,"worker index binding reacquired the live Techno roster");
 ++worldFrame.key.frame;indexedFrame=-1;readRequests=0;
 check(!refreshWorldIndex(snap.frame)&&indexedFrame==-1&&!worldObjects.byId(20),
       "stale worker frame remained visible");
 check(liveReads==beforeLive&&readRequests==0,"stale worker index fell back to a live roster scan");
 advance();
 check(refreshWorldIndex(snap.frame)&&worldObjects.byId(20)==p&&worldObjects.byId(21)==q,
       "next complete worker sample did not restore UID index");
 std::puts("PASS worker-owned native index: immutable UID handoff, stale serial fail-close and no live roster reads");
}
static void testIdentitySampleFailure(){
 reset();const auto t=makeType(),carrier=object(t,2,20),passenger=object(t,1,21);
 vectorAt(0xA8EC78,0x800000,{carrier,passenger});
 sh->magnetronGlobal=1;manualVersions.change(21,passenger,false);manualVersions.change(21,passenger,false);
 for(unsigned id=100;id<100+ManualVersions::Size/2;++id)manualVersions.change(id,0,false);
 seal(passenger);
 check(!worldFrameReady&&!refreshWorldIndex(snap.frame)&&!worldObjects.contains(carrier)&&
       !worldObjects.contains(passenger)&&!missingLiveReads&&manualVersions.get(21)==2,
       "incomplete worker UID graph was published or triggered a live roster fallback");
 advance(passenger);
 check(!worldFrameReady&&!refreshWorldIndex(snap.frame)&&!worldObjects.contains(passenger)&&
       !missingLiveReads&&manualVersions.get(21)==2,
       "repeated incomplete graph changed identity/manual state");
 advance();
 check(worldFrameReady&&refreshWorldIndex(snap.frame)&&worldObjects.contains(carrier)&&
       worldObjects.contains(passenger)&&manualVersions.get(21)==2&&!missingLiveReads,
       "complete next worker graph failed to restore index/manual state");
 std::puts("PASS identity sample fail-closed: incomplete graph stays UNKNOWN, no live roster fallback, next complete worker graph recovers");
}

static void testMissionReceiptIncomplete(){
 reset();uint32_t t=makeType(),p=object(t,2,20);set<int>(p+0xAC,0);set<int>(p+0xB4,-1);
 vectorAt(0xA8EC78,0x800000,{p});seal();
 auto observed=observeMissionReceipt(20);
 check(observed.valid&&observed.mission==0&&observed.queued==-1,"complete mission 0 observation rejected");
 // Native receipt must not treat an unreadable mission as a zero-value mission.
 for(uint32_t off:{0x10u,0xACu,0xB4u,0x11Cu}){
  missingPage=(p+off)&~4095u;denyLivePage=true;
  check(!observeMissionReceipt(20).valid,"missing UID/mission/queue/carrier yielded a valid native receipt");
  denyLivePage=false;missingPage=0;
 }
 gateway::MissionObserver<4> observer;
 gateway::MissionState before{true,2,-1,0};
 const uint32_t ticket=observer.record(match,20,0,0,snap.frame,clockNow,before,taskarbiter::Owner::Dodge);
 check(ticket&&observer.pending(),"cannot start receipt observer");
 missingPage=p&~4095u;denyLivePage=true;
 unsigned received=0;
 check(observer.pollWithReceipts(match,snap.frame+1,clockNow+16,[](uint32_t id){return observeMissionReceipt(id);},
    [&](const gateway::Receipt&){++received;})==0&&observer.pending()&&!received,"missing page acknowledged native order");
 denyLivePage=false;missingPage=0;
 check(observer.pollWithReceipts(match,snap.frame+2,clockNow+32,[](uint32_t id){return observeMissionReceipt(id);},
    [&](const gateway::Receipt&){++received;})==1&&!observer.pending()&&received==1,"complete native mission did not resolve receipt");
 std::puts("PASS native mission receipts: missing UID/mission/queue/carrier never acknowledges default-zero mission");
}
static void testLeasesNeedCompleteIdentityNotSecondLiveRead(){
 reset();const uint32_t type=makeType(),p=object(type,2,20);vectorAt(0xA8EC78,0x800000,{p});seal();
 check(actorLeaseArbiter.record(match,snap.frame,20,p,manualVersions.get(20),taskarbiter::Owner::Dodge,77),"cannot seed original automatic lease");
 // The live roster has already produced a complete indexed UID. A second
 // readWorldByID may nevertheless fail between the producer and sweep.
 missingPage=p&~4095u;denyLivePage=true;
 check(!worldByID(20),"fixture did not reproduce double live UID failure");
 const unsigned attempted=liveReads;
 sweepActiveLeases(snap.frame);
 check(actorLeaseArbiter.find(20)>=0&&liveReads==attempted,"secondary UID failure released valid automatic lease or read live again");
 denyLivePage=false;missingPage=0;
 manualVersions.change(20,p,false);sweepActiveLeases(snap.frame);
 check(actorLeaseArbiter.find(20)<0,"explicit manual revision must still release lease");
 manualVersions.clear();check(actorLeaseArbiter.record(match,snap.frame,20,p,0,taskarbiter::Owner::Dodge,79),"cannot seed replacement lease");
 vectorAt(0xA8EC78,0x800000,{});advance();sweepActiveLeases(snap.frame);
 check(actorLeaseArbiter.find(20)<0,"complete index absence must retire old lease");
 std::puts("PASS shared automatic lease sweep: unknown second UID does not retire ownership; manual and complete absence do");
}
static void testUnload(){
 reset();uint32_t type=makeType(),bt=makeType(),p=object(type,1,20),target=object(bt,3,30,0x30000);set<bool>(type+0xEC3,true);sh->flags=SmartUnloadOrders;enterResult=9;vectorAt(0xA8EC78,0x800000,{p,target});seal();
 UnloadPassenger r{};r.p=p;r.id=20;r.job=1;r.mode=0;r.carrierType=bt;r.frame=r.landedFrame=snap.frame;r.x=r.y=1000;static unloadcompute::Input input;input.count=input.targetCount=0;
 check(collectUnloadSource(input,r,lastHouse,snap.frame)&&input.count==1&&input.targetCount==1&&input.actions[0][0]==uint8_t(9|64),"unload specialist query capture");check(nativeCalls==1,"unload native query lost");check(unloadSourceCurrent(r,input.sources[0],lastHouse,snap.frame),"unload valid live source rejected");
 // A completed sealed sample does not imply later native fields remain
 // readable. Failed live source or specialist type reads cannot admit orders.
 missingPage=p&~4095u;denyLivePage=true;
 check(!unloadSourceCurrent(r,input.sources[0],lastHouse,snap.frame),
       "unload accepted a missing live mission/owner/UID page at final admission");
 denyLivePage=false;missingPage=type&~4095u;denyLivePage=true;
 check(!unloadSourceCurrent(r,input.sources[0],lastHouse,snap.frame),
       "unload accepted unreadable specialist flags at final admission");
 denyLivePage=false;missingPage=missingLiveReads=0;
 check(unloadSourceCurrent(r,input.sources[0],lastHouse,snap.frame),
       "unload final admission did not recover after missing live pages");
 set<int>(p+0xAC,2);check(!unloadSourceCurrent(r,input.sources[0],lastHouse,snap.frame),"unload local apply would accept later mission");
 const auto queries=nativeCalls;set<uint32_t>(p+0x10,44);r.id=44;check(!collectUnloadSource(input,r,lastHouse,snap.frame)&&input.count==1&&nativeCalls==queries,"unload combined a new job identity with old sampled fields");r.id=20;set<uint32_t>(p+0x10,20);
 advance(target);const int before=input.count;check(!collectUnloadSource(input,r,lastHouse,snap.frame)&&input.count==before,"unload missing target published partial source");check(!missingLiveReads,"unload missing target fell back live");
 std::puts("PASS unload raw facts/native legality split, source-current guard and failed-source retention");
}
static void testPlanning(){
 reset();uint32_t type=makeType(),targetType=makeType(),p=object(type,2,20),q=object(targetType,2,30,0x30000,1200,1000),weapon=makeWeapon(type);nativeOrders[0]={20,0,0,planner::OrdinaryMove,false,p};nativeCount=1;sh->flags=AdvancedInput;vectorAt(0xA8EC78,0x800000,{p,q});seal();
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&planningWorld.count==2&&planningWorld.actors[0].x==1000,"planning raw capture");check(liveReads==0,"planning pure projection read live");const auto seq=sh->worldSeq;
 advance(p);check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sh->worldSeq==seq&&planningWorld.actors[0].x==1000,"incomplete planning input replaced world");check(!missingLiveReads,"planning missing source fell back live");
 set<bool>(weapon+0x15C,true);set<bool>(get<uint32_t>(weapon+0xAC)+0x15B,true);set<uint32_t>(p+0x2AC,q);sh->magnetronDrop=1;sh->nativeCombatQueries=1;
 sh->flags|=AutoOreReturn;set<bool>(type+0xE0E,true);set<int>(type+0x800,20);set<int>(p+0xAC,2);advance();
 nativeMutation=[&]{++inputSerial;manualVersions.change(20,p,true);};
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&nativeCalls>=2,"planning native enrichments missing");
 check(planningSerial==planningWorld.inputSerial&&planningSerial!=inputSerial,"planning publication swallowed a newer input generation");
 check(manualVersions.oreState[manualVersions.slot(20)]==1,"old sealed mission confirmed a newer manual pause");
 check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&manualVersions.oreState[manualVersions.slot(20)]==1,"same-frame reprojection confirmed a newer manual pause");
 std::puts("PASS planning staging publication, missing-frame retention and native enrichment boundaries");
}
static void testRepair(){
 reset();uint32_t type=makeType(),other=makeType(),a=object(type,3,20),b=object(type,3,21),c=object(type,3,22),d=object(other,3,23);
 set<int>(type+0xA0,200);set<uint8_t>(type+0x157A,1);set<int>(other+0xA0,200);set<bool>(b+0x6E8,true);pendingRepairs.queued(22,c);
 vectorAt(0xA8EB40,0x840000,{a,b,c,d});sh->computeApplied=1u<<compute::Repair;seal();
 set<int>(a+0x6C,200);set<uint8_t>(type+0x157A,0);
 check(collectRepairs(lastHouse,clockNow)==1&&repairCount==1&&repairIDs[0]==20,"repair internal sealed candidates");check(!liveReads&&!hostNotifies,"repair internal collector escaped sample");
 repairAt=1;repairDone=3;advance(a);check(collectRepairs(lastHouse,clockNow)==-6&&repairCount==1&&repairAt==1&&repairDone==3&&repairIDs[0]==20,"repair missing sample cleared internal candidates");check(!missingLiveReads,"repair missing page fell back live");
 set<int>(a+0x6C,100);set<uint8_t>(type+0x157A,1);advance();sh->computeApplied=0;
 check(collectRepairs(lastHouse,clockNow)==-30&&repairCalculationPending&&sh->repairInput.count==3&&sh->repairInput.items[0].id==20&&sh->repairInput.items[1].view.alreadyRepairing&&sh->repairInput.items[2].view.issuedRecently,"repair Host publication facts");
 const auto task=sh->repairTask;const auto notices=hostNotifies;const auto started=repairCalculationStarted;const auto published=sh->repairInput;
 repairCount=2;repairAt=1;repairDone=5;repairIDs[0]=70;repairIDs[1]=71;advance(type);
 check(collectRepairs(lastHouse,clockNow,true)==-6&&repairCount==2&&repairAt==1&&repairDone==5&&repairIDs[0]==70&&repairIDs[1]==71,"repair missing refresh cleared candidate IDs");
 check(sh->repairTask==task&&hostNotifies==notices&&repairCalculationStarted==started&&repairCalculationPending&&!std::memcmp(&published,&sh->repairInput,sizeof published)&&repairFrames.find(task),"repair missing refresh replaced Host input or history");
 worldFrameReady=false;check(collectRepairs(lastHouse,clockNow,true)==-6&&sh->repairTask==task&&repairCount==2,"repair absent sample published empty work");
 std::puts("PASS repair internal/Host outputs, pending-toggle facts and failed refresh retention");
}
struct OreFixture {uint32_t miner,type,refinery,refineryType,links,resource,distant;};
static uint32_t resource(int x,int y){uint32_t cell=alloc();set<int16_t>(cell+0x24,int16_t(x));set<int16_t>(cell+0x26,int16_t(y));set<int>(cell+0x44,0);set<int>(cell+0xEC,0);set<uint32_t>(0x400000+uint32_t(y*512+x)*4,cell);return cell;}
static OreFixture setupOre(){
 reset();sh->flags=AutoOreReturn;OreFixture f{};f.type=makeType();f.refineryType=makeType();f.miner=object(f.type,2,20,lastHouse,51*256,62*256);f.refinery=object(f.refineryType,3,30,lastHouse,50*256,60*256);f.links=alloc();
 set<bool>(f.type+0xE0E,true);set<int>(f.type+0x800,20);set<int>(f.type+0x67C,1);uint32_t docks=alloc();vectorAt(f.type+0x3E8,docks,{f.refineryType});
 set<float>(f.miner+0x33C,20);set<bool>(f.miner+0x6D1,true);set<int>(f.miner+0xAC,16);set<uint32_t>(f.miner+0x5A4,f.refinery);set<uint32_t>(f.refinery+0xE4,f.links);set<int>(f.refinery+0xE8,1);set<uint32_t>(f.links,f.miner);
 uint32_t overlay=alloc(),tiberium=alloc(),overlayItems=alloc(),tibItems=alloc();vectorAt(0xA83D80,overlayItems,{overlay});vectorAt(0xB0F4E8,tibItems,{tiberium});set<bool>(overlay+0x2A9,true);set<int>(overlay+0x294,0);set<uint32_t>(tiberium+0xE0,overlay);set<int>(tiberium+0xE8,1);set<int>(tiberium+0xB8,25);
 for(int land=0;land<12;++land)for(int speed=0;speed<8;++speed)set<float>(0x89EA40+uint32_t(land*9+speed)*4,1.);
 vectorAt(0xA8EC78,0x800000,{f.miner,f.refinery});vectorAt(0x8B4108,0x830000,{f.miner});vectorAt(0xA8EB40,0x840000,{f.refinery});vectorAt(0xA83CE0,0x850000,{f.type});f.resource=resource(56,62);f.distant=resource(80,62);seal();return f;
}
static void unloaded(const OreFixture&f){set<float>(f.miner+0x33C,0);set<bool>(f.miner+0x6D1,false);set<int>(f.miner+0xAC,10);set<uint32_t>(f.miner+0x5A4,f.distant);set<int>(f.refinery+0xE8,0);}
static void testOre(){
 auto f=setupOre();oreHarvestObserve(lastHouse,snap.frame,clockNow);check(oreHarvestPending()&&!oreHarvestNeedsWork(),"ore unload episode not captured");
 unloaded(f);advance(f.refinery);oreHarvestObserve(lastHouse,snap.frame,clockNow);check(oreHarvestPending()&&!oreHarvestNeedsWork()&&oreHarvestObservedFrame==100,"ore failed observer committed partial state");check(!missingLiveReads,"ore observer read missing refinery live");
 advance(f.resource);oreHarvestTick(lastHouse,snap.frame,clockNow);check(oreHarvestNeedsWork()&&!harvestEvents,"ore missing search page submitted");
 const auto before=oreHarvestJobs[0].search;check(before.radius==0&&before.at==0&&!before.done,"ore incomplete search advanced checkpoint");
 advance();oreHarvestTick(lastHouse,snap.frame,clockNow,false);
 check(oreHarvestJobs[0].search.done&&oreHarvestJobs[0].attemptedFrame!=snap.frame&&!harvestEvents,
       "deferred ore search failed to prepare the destination without consuming its submit turn");
 oreHarvestTick(lastHouse,snap.frame,clockNow);check(harvestEvents==1&&harvestLast.destination==62056&&!oreHarvestPending(),"ore retry failed to find/send exact nearest cell");
 f=setupOre();oreHarvestObserve(lastHouse,snap.frame,clockNow);unloaded(f);advance();set<int>(f.resource+0x44,-1);oreHarvestTick(lastHouse,snap.frame,clockNow);check(!harvestEvents,"ore execution guard used stale sampled resource");
 std::puts("PASS ore Unit/Building observation, failed transaction/search retry and live admission guard");
}
static void testDelayedPlanning(){
 auto f=setupOre();const auto sourceFrame=snap.frame;const auto sourceTick=clockNow;
 worldFrameCandidate.swap(worldFrame);worldFrameCandidateAvailable=true;worldFrameReady=false;
 ++snap.frame;clockNow+=16;set<int>(0xA8ED84,snap.frame);
 set<int>(f.miner+0x9C,52*256);
 check(!commandRosterReady()&&publishPlanningWorld(lastHouse,snap.frame,clockNow,false),
       "a late complete observation should publish without strict index authority");
 check(planningWorld.frame==sourceFrame&&planningWorld.tick==sourceTick&&
       planningWorld.actors[0].x==51*256&&!liveReads&&!nativeCalls&&!submissions,
       "delayed planning substituted current live data or advanced its source age");
 const auto seq=sh->worldSeq;
 ++snap.frame;clockNow+=16;set<int>(0xA8ED84,snap.frame);
 check(!publishPlanningWorld(lastHouse,snap.frame,clockNow,false)&&sh->worldSeq==seq,
       "the same candidate was republished as a new world frame");
 seal();
 check(commandRosterReady()&&publishPlanningWorld(lastHouse,snap.frame,clockNow,true)&&
       planningWorld.frame==snap.frame&&planningWorld.actors[0].x==52*256,
       "strict data arriving in the same dispatcher frame did not replace the candidate");
 std::puts("PASS delayed planner observations keep source age and never use native enrichment before strict admission");
}
static void testDefenseIndex(){
 auto f=setupOre();check(publishPlanningWorld(lastHouse,snap.frame,clockNow)&&minerDefenseIndexRefineries()&&minerDefenseRefineryCount==1&&minerDefenseLinkedRefinery.at(20)==30&&!minerDefenseRefineryIncomplete,"defense shared radio index");
 set<uint32_t>(f.links,0);check(minerDefenseIndexRefineries()&&minerDefenseLinkedRefinery.at(20)==30&&!liveReads,"defense recaptured radio links live");
 advance(f.links);planningWorld.frame=snap.frame;check(!minerDefenseIndexRefineries()&&minerDefenseRefineryIncomplete&&minerDefenseRefineryCount==1&&minerDefenseLinkedRefinery.at(20)==30&&!missingLiveReads,"defense missing radio index replaced prior facts");
 --planningWorld.frame;check(!minerDefenseIndexRefineries()&&minerDefenseLinkedRefinery.at(20)==30,"defense mixed prior planning world with a new frame");
 std::puts("PASS harvester-defense shared refinery/radio index, missing retention and frame identity");
}
#ifndef TACTICAL_FRAME_NO_MAIN
int main(int argc,char**){sh=new Shared{};if(argc>1){testIdentitySampleFailure();return 0;}testCooperative();testDelayedAircraftPlan();testLoad();testBoardingLedgerUnknown();testDodge();testDodgeLeaseMissing();testReverse();testMissile();testCapturedRosterIndex();testIdentitySampleFailure();testMissionReceiptIncomplete();testLeasesNeedCompleteIdentityNotSecondLiveRead();testUnload();testPlanning();testRepair();testOre();testDelayedPlanning();testDefenseIndex();check(!sampleReadDepth,"final scope leaked");delete sh;std::puts("PASS all tactical sampling production-path checks (synthetic CPU fixture)");}
#endif

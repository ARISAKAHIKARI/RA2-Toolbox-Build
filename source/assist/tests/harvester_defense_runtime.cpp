// Real production runtime/ore/raw-combat headers. Linux-only DWORD declaration;
// no replacement defense model and no calls into a game executable.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "../source/protocol.h"
#include "../source/ore_queue.h"
#include "../source/object_index.h"
#include "../source/planner_index.h"
#include "../source/unit_task_arbiter.h"
#include "../source/command_intent.h"
#ifdef NDEBUG
#error This fixture requires active assertions.
#endif
using DWORD = uint32_t;
using namespace assist;
#define MemoryBarrier() std::atomic_thread_fence(std::memory_order_seq_cst)
constexpr uint32_t House=0x20010000,Miner=0x20020000,MinerType=0x20030000,Refinery=0x20040000,RefineryType=0x20050000;
constexpr uint32_t Overlay=0x20060000,Tiberium=0x20070000,OverlayItems=0x20080000,TiberiumItems=0x20081000,Links=0x20082000,Docks=0x20083000;
constexpr uint32_t Scenario=0x20090000,EnemyHouse=0x20091000,EnemyCountry=0x20092000,Drone=0x200A0000,Drone2=0x200A1000,DroneType=0x200B0000;
constexpr uint32_t Gun=0x200C0000,ParasiteGun=0x200C1000,Projectile=0x200C2000,Warhead=0x200C3000,ParasiteWarhead=0x200C4000,EliteGun=0x200C5000;
constexpr uint32_t Cells=0x21000000,CellBase=0x30000000,Map=0x87F7E8,NativeUnitItems=0x203F0000,NativeTechnoItems=0x203E0000;
constexpr uint32_t MinerID=11,RefineryID=21,DroneID=31,Drone2ID=32,ResourceCell=62056,WaitCell=62053;
static Shared*sh;
static Snapshot snap;
static planner::World planningWorld;
static uint32_t match=7,lastHouse=House,inputSerial=0;
static DWORD clockNow=1000;
static bool fault=false,planningOkay=true,outgoingOkay=true,nativeOkay=true;
static bool held=false,formed=false,bursted=false,dodging=false,ordered=false;
static uint32_t failRead=0;
static unsigned reads=0,cellReads=0,consumed=0,releases=0,nativeCalls=0;
static uint8_t globals[0x500000]{},objects[0x400000]{};
static ManualVersions manualVersions;
static ObjectIndex worldObjects;
static taskarbiter::FrameArbiter<> actorFrameArbiter;
static taskarbiter::LeaseArbiter<> actorLeaseArbiter;
static oreconfig::Config oreConfig;
static long oreConfigRevision=-1;
static std::vector<intent::EventOrder> events;
static std::vector<uint32_t> actors;
struct Cell {int overlay=-1,land=0;uint32_t occupation=0;uint8_t amount=0;int8_t level=0;uint8_t slope=0;};
static std::map<uint32_t,Cell> cells;
static std::set<uint32_t> nullCells;
static void*memory(uint32_t at,size_t n){
 if(at>=0x700000&&uint64_t(at)+n<=0xC00000)return globals+at-0x700000;
 if(at>=0x20000000&&uint64_t(at)+n<=0x20400000)return objects+at-0x20000000;
 return nullptr;
}
template<class T>static void set(uint32_t at,T value){void*p=memory(at,sizeof value);assert(p);memcpy(p,&value,sizeof value);}
static uint32_t cellPointer(int x,int y){return CellBase+uint32_t(y*512+x)*0x200;}
#include "fixtures/tactical_frame/runtime_sample_fixture.h"
static bool readFixtureLive(uint32_t at,void*out,size_t n){
 assert(!sampleReadDepth);
 ++reads;
 if(failRead&&at<=failRead&&uint64_t(at)+n>failRead){if(n)memset(out,0xCC,n-1);return false;}
 if(fixtureReadMap(at,out,n)>0)return true;
 void*p=memory(at,n);if(!p)return false;memcpy(out,p,n);return true;
}
template<class T>static T get(uint32_t at){T value{};if(!read(at,&value,sizeof value))return {};return value;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t at,int limit=16384){Vec v{};if(!read(at,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||(v.count&&v.items<0x10000))return {};return v;}
static uint32_t item(Vec v,int i){return i>=0&&i<v.count?get<uint32_t>(v.items+uint32_t(i)*4):0;}
static int kind(uint32_t p){uint32_t vt=get<uint32_t>(p);return vt==0x7F5C70?2:vt==0x7EB058?1:vt==0x7E3EBC?3:vt==0x7E22A4?0:-1;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
static bool live(uint32_t p){return p&&get<int>(p+0x6C)>0&&get<bool>(p+0x90);}
static bool own(uint32_t p,uint32_t house){return live(p)&&get<uint32_t>(p+0x21C)==house&&get<bool>(p+0x74)&&!get<bool>(p+0x81);}
#include "../source/feature_type_bridge.h"
static bool stickyOwns(uint32_t){return held;}
static bool formationOwns(uint32_t){return formed;}
static bool burstOwns(uint32_t){return bursted;}
static bool dodgeOwns(uint32_t){return dodging;}
static int orderIndex(uint32_t){return ordered?0:-1;}
static bool planningRoom(int){return planningOkay;}
static bool outgoingRoom(){return outgoingOkay;}
static void consumePlanning(int){++consumed;}
static void releaseAutoOwner(uint32_t id){++releases;actorLeaseArbiter.manual(id);}
// Complete helper functions extracted verbatim by the runner, not recoded.
#include "miner_runtime_extract.h"
#include "../source/raw_combat.h"
#include "../source/harvester_defense_raw.h"
// Native EventClass delivery is a boundary, not simulated game execution. A
// separate observation helper must explicitly make a queued order visible.
static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,uint32_t destination,int tag,
 uint32_t nearest,int nearestTag,taskarbiter::Owner owner,uint32_t*receipt){
 ++nativeCalls;assert(!sampleReadDepth&&owner==taskarbiter::Owner::HarvesterDefense&&house==House);
 const uint32_t p=worldByID(id);const int frame=get<int>(0xA8ED84);const uint32_t revision=manualVersions.get(id);
 actorFrameArbiter.begin(match,frame);
 if(!nativeOkay||!outgoingOkay||!own(p,house)||!actorFrameArbiter.canIssue(id,revision)||
    !actorLeaseArbiter.canIssue(match,frame,id,p,revision,owner))return false;
 if(target&&!worldByID(target))return false;
 if(tag==0x34&&destination&&!worldByID(destination))return false;
 events.push_back(intent::mega(get<int>(House+0x30),id,mission,target,target?0x34:0,destination,uint8_t(tag),nearest,uint8_t(nearestTag)));
 const uint32_t serial=uint32_t(events.size());actorFrameArbiter.record(id,revision,owner);actorLeaseArbiter.record(match,frame,id,p,revision,owner,serial);if(receipt)*receipt=serial;
 return true;
}
#include "../source/ore_harvest_bridge.h"
static int admitPlannerOrder(const planner::Order&,uint32_t){assert(false&&"ore observer does not dispatch in this fixture");return -1;}
#include "../source/harvester_defense_bridge.h"
// The unrelated missile pause is a boundary; manual revisions, ore pausing,
// defense cancellation and serial changes use the real host-input handler.
static void pauseMissileForManual(uint32_t,uint32_t){}
#include "miner_manual_extract.h"

static void vectorAt(uint32_t at,uint32_t list,int count){Vec v{0,list,count,0,count,0};memcpy(memory(at,sizeof v),&v,sizeof v);}
static void object(uint32_t p,uint32_t vt,uint32_t id,uint32_t type,uint32_t owner){
 set<uint32_t>(p,vt);set<uint32_t>(p+0x10,id);set<int>(p+0x6C,800);set<uint32_t>(p+0x21C,owner);
 set<uint8_t>(p+0x74,1);set<uint8_t>(p+0x90,1);set<uint32_t>(p+(vt==0x7E3EBC?0x520:vt==0x7EB058?0x6C0:0x6C4),type);set<int>(p+0xB4,-1);worldObjects.add(p,id);
}
static void position(uint32_t p,int x,int y,int z=0){set<int>(p+0x9C,x);set<int>(p+0xA0,y);set<int>(p+0xA4,z);}
static void resource(int x,int y,int land=0,uint32_t occupation=0){cells[uint32_t(y*1000+x)]={0,land,occupation,0,0,0};}
// Snapshot publication is a boundary. All combat/parasite properties below are
// obtained from the production raw helpers; tests can keep this snapshot stale.
static void sampleWorld(){
 // Native roster membership is independent of liveness and read failures.
 // A dead Unit stays listed until the fixture explicitly removes it; inspect
 // backing bytes here so fault injection cannot manufacture native absence.
 int unitCount=0;for(uint32_t p:actors){uint32_t vt=0;memcpy(&vt,memory(p,4),4);
  if(vt==0x7F5C70u){assert(unitCount<16384);set<uint32_t>(NativeUnitItems+uint32_t(unitCount++)*4,p);}
 }vectorAt(0x8B4108,NativeUnitItems,unitCount);
 for(size_t n=0;n<actors.size();++n)set<uint32_t>(NativeTechnoItems+uint32_t(n)*4,actors[n]);vectorAt(0xA8EC78,NativeTechnoItems,int(actors.size()));
 fixtureCaptureWorld(clockNow);SampleReadScope frameRead;if(!frameRead)return;
 static planner::World candidate;candidate.match=match;candidate.frame=snap.frame;candidate.tick=clockNow;candidate.inputSerial=inputSerial;candidate.count=0;
 for(uint32_t p:actors){
  if(!live(p))continue;
  auto&a=candidate.actors[candidate.count++];a={};a.id=get<uint32_t>(p+0x10);a.kind=kind(p);a.type=typeOf(p,a.kind);
  a.flags=get<uint32_t>(p+0x21C)==House?planner::Own:planner::Enemy;
  if(a.kind==2&&oreMiner(a.type))a.flags|=planner::Harvester;
  if(get<bool>(p+0x1C8))a.flags|=planner::Disabled;
  if(get<uint32_t>(p+0x11C))a.flags|=planner::Transported;
  if(get<int>(p+0x220))a.flags|=planner::Cloaked;
  if(!get<bool>(p+0x74))a.flags|=planner::OffMap;
  if(get<bool>(p+0x81))a.flags|=planner::Limbo;
  if(get<bool>(p+0x6D1))a.flags|=planner::Unloading;
  if(manualVersions.orePaused(a.id))a.flags|=planner::OrePaused;
  if(minerDefenseThreat(p,House,false))a.flags|=planner::ParasiteThreat;
  a.manualVersion=manualVersions.get(a.id);a.x=get<int>(p+0x9C);a.y=get<int>(p+0xA0);a.mission=get<int>(p+0xAC);a.queued=get<int>(p+0xB4);
  a.destination=rawObjectID(get<uint32_t>(p+0x5A4));rawCombatInfo(p,a.type,a.kind,a,(a.flags&planner::Own)!=0,false);
 }
 if(frameRead.complete())planningWorld=candidate;
}
static void reset(){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);sh->~Shared();new(sh)Shared{};snap={};planningWorld.~World();new(&planningWorld)planner::World{};
 manualVersions.clear();worldObjects.clear();actorFrameArbiter.clear();actorLeaseArbiter.clear();oreHarvestReset();minerDefenseReset();oreConfig={};oreConfigRevision=-1;resetFeatureTypes();
 events.clear();actors={Miner,Refinery,Drone};cells.clear();nullCells.clear();failRead=0;fixtureResetWorld();
 fault=held=formed=bursted=dodging=ordered=false;planningOkay=outgoingOkay=nativeOkay=true;reads=cellReads=consumed=releases=nativeCalls=0;
 match=7;lastHouse=House;inputSerial=0;clockNow=1000;sh->minerDefenseEnabled=1;sh->minerDefenseRangeCells=1;sh->flags=0;
 snap.status=snap.commandStatus=1;snap.session=3;snap.match=7;snap.frame=300;
 set<uint32_t>(0xA83D4C,House);set<uint32_t>(House+0x10,101);set<int>(House+0x30,3);set<uint32_t>(0xA8B230,Scenario);set<int>(0xA8B238,3);set<int>(0xA8ED84,300);
 set<int>(EnemyHouse+0x30,4);set<uint32_t>(EnemyHouse+0x34,EnemyCountry);
 set<uint32_t>(MinerType+0x10,301);set<uint8_t>(MinerType+0xE0E,1);set<int>(MinerType+0x800,20);set<int>(MinerType+0x67C,1);set<int>(MinerType+0x684,-1);
 memcpy(memory(MinerType+0x24,5),"HARV",5);set<uint32_t>(RefineryType+0x10,501);set<uint32_t>(DroneType+0x10,601);
 for(uint32_t type:{MinerType,DroneType,RefineryType})set<uint8_t>(type+0x231,1);
 vectorAt(MinerType+0x3E8,Docks,1);set<uint32_t>(Docks,RefineryType);
 set<uint32_t>(MinerType+0x898,Gun);set<uint32_t>(DroneType+0x898,ParasiteGun);
 for(uint32_t weapon:{Gun,ParasiteGun,EliteGun}){set<int>(weapon+0xB4,weapon==ParasiteGun?256:4*256);set<uint32_t>(weapon+0xA0,Projectile);set<uint32_t>(weapon+0xAC,weapon==ParasiteGun?ParasiteWarhead:Warhead);}
 set<uint8_t>(Projectile+0x2A5,1);set<uint8_t>(ParasiteWarhead+0x159,1);
 for(int armor=0;armor<11;++armor){set<double>(Warhead+0xA0+armor*8,1.0);set<double>(ParasiteWarhead+0xA0+armor*8,1.0);}
 object(Miner,0x7F5C70,MinerID,MinerType,House);object(Refinery,0x7E3EBC,RefineryID,RefineryType,House);object(Drone,0x7F5C70,DroneID,DroneType,EnemyHouse);
 set<int>(Miner+0xAC,10);set<uint32_t>(Miner+0x5A4,cellPointer(56,62));set<float>(Miner+0x33C,3);
 position(Miner,51*256,62*256);position(Refinery,50*256,60*256);position(Drone,55*256,62*256);
 set<int>(Refinery+0xE8,0);set<uint32_t>(Refinery+0xE4,Links);set<uint32_t>(Links,Miner);
 int bounds[]={100,100,0,0,100,100};memcpy(memory(Map+0xF4,sizeof bounds),bounds,sizeof bounds);set<uint32_t>(0x87F924,Cells);set<int>(0x87F928,512*512);
 vectorAt(0xA83D80,OverlayItems,1);set<uint32_t>(OverlayItems,Overlay);set<uint8_t>(Overlay+0x2A9,1);set<int>(Overlay+0x294,0);
 vectorAt(0xB0F4E8,TiberiumItems,1);set<uint32_t>(TiberiumItems,Tiberium);set<uint32_t>(Tiberium+0xE0,Overlay);set<int>(Tiberium+0xE8,1);set<int>(Tiberium+0xB8,25);
 vectorAt(0xA8EB40,0x20085000,1);set<uint32_t>(0x20085000,Refinery);
 for(int land=0;land<12;++land)for(int speed=0;speed<8;++speed)set<float>(0x89EA40+uint32_t(land*9+speed)*4,land==2?0.f:1.f);
 resource(56,62);resource(80,62);refreshOreConfig();sampleWorld();
}
static bool step(int frames=1,DWORD elapsed=16,bool refresh=true){
 snap.frame+=frames;clockNow+=elapsed;set<int>(0xA8ED84,snap.frame);if(refresh)sampleWorld();else fixtureCaptureWorld(clockNow);
 return minerDefenseTick(House,snap.frame,clockNow);
}
static void observeAttack(uint32_t target=Drone){set<int>(Miner+0xAC,1);set<int>(Miner+0xB4,-1);set<uint32_t>(Miner+0x2B4,target);set<uint32_t>(Miner+0x5A4,0);}
static void observeGuard(uint32_t target=0,uint32_t destination=0,int queued=-1){set<int>(Miner+0xAC,5);set<int>(Miner+0xB4,queued);set<uint32_t>(Miner+0x2B4,target);set<uint32_t>(Miner+0x5A4,destination);}
static void kill(uint32_t p=Drone){set<int>(p+0x6C,0);}
static void observe(const intent::EventOrder&e){
 set<int>(Miner+0xAC,e.mission);set<int>(Miner+0xB4,-1);set<uint32_t>(Miner+0x2B4,e.b?worldByID(uint32_t(e.b)):0);
 set<uint32_t>(Miner+0x5A4,e.tagC==11?cellPointer(e.c%1000,e.c/1000):e.tagC==0x34?worldByID(uint32_t(e.c)):0);
}
static void addDrone2(){object(Drone2,0x7F5C70,Drone2ID,DroneType,EnemyHouse);position(Drone2,54*256,62*256);actors.push_back(Drone2);}
static void refineryWork(int mission=12){set<int>(Miner+0xAC,mission);set<float>(Miner+0x33C,20);set<uint32_t>(Miner+0x5A4,Refinery);set<int>(Refinery+0xE8,1);if(mission==16)set<uint8_t>(Miner+0x6D1,1);}
static void rememberWait(){
 sh->flags|=AutoOreReturn;set<float>(Miner+0x33C,20);set<int>(Miner+0xAC,2);set<uint32_t>(Miner+0x5A4,cellPointer(53,62));
 const planner::Order order{MinerID,manualVersions.get(MinerID),0,WaitCell,2,11,planner::OreOrder,RefineryID};minerDefenseRememberOre(order,House);
}
static void defenderTypes(const wchar_t* ids,bool custom=true){
 auto&filter=sh->featureConfig.filters[featuretypes::SpiderDefense];
 assert(oreconfig::parse(std::wstring(ids),filter.ids,filter.count));filter.custom=custom;sh->featureConfigSeq+=2;
}
// Reuse the controlled actor address for an ordinary vehicle or infantry. Its
// type has no miner flags/storage; Infantry's Unit-only fields are poisoned so
// a misplaced miner read cannot accidentally pass on zero-initialized memory.
static void passiveDefender(int k=2,int mission=5){
 set<uint8_t>(MinerType+0xE0E,0);set<uint8_t>(MinerType+0xE0F,0);set<int>(MinerType+0x800,0);
 const char*id=k==1?"E1":"MTNK";memset(memory(MinerType+0x24,24),0,24);memcpy(memory(MinerType+0x24,strlen(id)+1),id,strlen(id)+1);
 object(Miner,k==1?0x7EB058:k==3?0x7E3EBC:k==0?0x7E22A4:0x7F5C70,MinerID,MinerType,House);
 set<int>(Miner+0xAC,mission);set<int>(Miner+0xB4,-1);set<uint32_t>(Miner+0x5A4,0);set<uint32_t>(Miner+0x2B4,0);
 set<float>(Miner+0x33C,k==1?std::numeric_limits<float>::quiet_NaN():0.f);
 if(k==1){set<uint32_t>(Miner+0x6C4,0);set<uint8_t>(Miner+0x6D1,1);}
 defenderTypes(k==1?L"e1":L"mTnk");
}

static unsigned checks=0,failed=0,passed=0;
static const char*caseName="";
static void check(bool value,const char*expression,int line){++checks;if(!value){std::fprintf(stderr,"FAIL %s line %d: %s (events=%zu active=%u frame=%d)\n",caseName,line,expression,events.size(),sh->minerDefenseActive,snap.frame);throw std::runtime_error("check");}}
#define CHECK(...) check((__VA_ARGS__),#__VA_ARGS__,__LINE__)
static void attackStarted(){step();CHECK(events.size()==1);CHECK(events.back().mission==1&&events.back().b==DroneID&&events.back().tagB==0x34);CHECK(sh->minerDefenseActive==1&&sh->minerDefenseAttacks==1&&sh->minerDefenseResumes==0);}
static void harvestTuple(const intent::EventOrder&e,uint32_t cell=ResourceCell,uint32_t actor=MinerID){CHECK(e.mission==10&&e.a==int(actor)&&e.b==0&&e.tagB==0&&e.c==int(cell)&&e.tagC==11&&e.d==0&&e.nearestTag==0);}
static void passiveMoveTuple(const intent::EventOrder&e){CHECK(e.mission==2&&e.a==MinerID&&e.b==0&&e.tagB==0&&e.c==62051&&e.tagC==11&&e.d==62051&&e.nearestTag==11);}
static void passiveGuardTuple(const intent::EventOrder&e,int mission){CHECK(e.mission==mission&&e.a==MinerID&&e.b==0&&e.tagB==0&&e.c==0&&e.tagC==0&&e.d==0&&e.nearestTag==0);}
int main(int argc,char**argv){
 sh=new Shared{};std::vector<std::string> failures;
 auto run=[&](const char*name,const std::function<void()>&body){if(argc>1&&std::string(name).find(argv[1])==std::string::npos)return;caseName=name;reset();try{body();++passed;}catch(const std::exception&){++failed;failures.emplace_back(name);}};
 run("mining_entry_exit_and_harvest_tuple",[]{
  CHECK(!(sh->flags&AutoOreReturn));attackStarted();CHECK(minerDefenseJobs.at(MinerID).mining&&minerDefenseJobs.at(MinerID).resourceCell==ResourceCell);
  observeAttack();for(int n=0;n<80;++n)step();CHECK(events.size()==1&&minerDefenseOwns(MinerID));
  kill();step();CHECK(events.size()==2&&sh->minerDefenseActive==1&&sh->minerDefenseResumes==1);harvestTuple(events.back());
  step();CHECK(events.size()==2&&sh->minerDefenseActive==1);observe(events.back());step();CHECK(!minerDefensePending()&&sh->minerDefenseActive==0&&actorLeaseArbiter.find(MinerID)<0);
 });
 run("range_entry_and_exit_hysteresis",[]{
  position(Drone,51*256+5*256+1,62*256);step();CHECK(events.empty());position(Drone,51*256+5*256,62*256);attackStarted();observeAttack();
  position(Drone,51*256+6*256,62*256);step();CHECK(events.size()==1);position(Drone,51*256+6*256+1,62*256);step();CHECK(events.size()==2);harvestTuple(events.back());
 });
 run("refinery_return_enter_recovery",[]{
  refineryWork();attackStarted();CHECK(!minerDefenseJobs.at(MinerID).mining&&minerDefenseJobs.at(MinerID).identity.refinery==RefineryID);
  observeAttack();kill();step();CHECK(events.size()==2);auto&e=events.back();CHECK(e.mission==7&&e.b==0&&e.c==RefineryID&&e.tagC==0x34&&e.d==0&&e.nearestTag==0);
  observe(e);step();CHECK(!minerDefensePending());
 });
 run("native_unload_is_legitimate_work",[]{refineryWork(16);attackStarted();CHECK(minerDefenseJobs.at(MinerID).originalMission==16);observeAttack();set<uint8_t>(Miner+0x6D1,0);kill();step();CHECK(events.size()==2&&events.back().mission==7);});
 run("native_linked_unload_with_cleared_destination_recovers",[]{
  refineryWork(16);set<uint32_t>(Miner+0x5A4,0);attackStarted();CHECK(minerDefenseJobs.at(MinerID).identity.refinery==RefineryID);
  observeAttack();set<uint8_t>(Miner+0x6D1,0);step();kill();step();CHECK(events.size()==2&&events.back().mission==7);
  set<int>(Miner+0xAC,16);set<uint8_t>(Miner+0x6D1,1);set<uint32_t>(Miner+0x5A4,0);set<uint32_t>(Miner+0x2B4,0);step();CHECK(!minerDefensePending()&&events.size()==2);
 });
 run("managed_wait_move_preserves_queue_metadata",[]{
  rememberWait();CHECK(minerDefenseWork.count(MinerID)==1);attackStarted();CHECK(minerDefenseJobs.at(MinerID).managedQueue);
  observeAttack();kill();step();CHECK(events.size()==2);const auto&e=events.back();CHECK(e.mission==2&&e.b==0&&e.c==WaitCell&&e.tagC==11&&e.d==WaitCell&&e.nearestTag==11);
  CHECK(minerDefenseWork.at(MinerID).waitCell==WaitCell&&sh->minerDefenseActive==1);observe(e);position(Miner,53*256+128,62*256+128);step();
  CHECK(!minerDefensePending()&&minerDefenseWork.count(MinerID)==1);for(const auto&event:events)CHECK(event.mission!=7);
 });
 run("managed_blocked_wait_never_bypasses_with_enter",[]{
  rememberWait();attackStarted();observeAttack();kill();cells[WaitCell].occupation=0x80;step();CHECK(events.size()==2&&events.back().mission==5);
  observe(events.back());step();CHECK(!minerDefensePending()&&minerDefenseWork.count(MinerID)==1);for(const auto&e:events)CHECK(e.mission!=7);
 });
 run("blocked_wait_timeout_guard_delivery_lag_preserves_ledger",[]{
  rememberWait();attackStarted();observeAttack();kill();step();CHECK(events.size()==2&&events.back().mission==2);observe(events.back());
  // The old waiting Move remains blocked far from the destination. Its
  // interruptible Guard fallback is queued at the documented timeout.
  step(90,1500);CHECK(events.size()==3&&events.back().mission==5&&minerDefensePending());
  step();CHECK(events.size()==3&&minerDefensePending()&&minerDefenseWork.count(MinerID)==1);
  observe(events.back());step();CHECK(!minerDefensePending()&&minerDefenseWork.count(MinerID)==1);
  for(const auto&e:events)CHECK(e.mission!=7);
 });
 run("disabled_feature_drains_active_work",[]{
  attackStarted();observeAttack();step();sh->minerDefenseEnabled=0;step();CHECK(events.size()==2&&sh->minerDefenseActive==1);harvestTuple(events.back());
  for(int n=0;n<5;++n)step();CHECK(sh->minerDefenseActive==1&&events.size()==2);observe(events.back());step();CHECK(sh->minerDefenseActive==0&&!minerDefensePending());
  for(int n=0;n<10;++n)step();CHECK(events.size()==2);
 });
 run("default_miners_ignore_independent_ore_type_selection",[]{
  for(bool empty:{false,true}){reset();sh->oreConfig.customMiners=1;sh->oreConfig.minerCount=empty?0:1;
   memcpy(sh->oreConfig.miners[0],"OTHER",6);sh->oreConfigSeq+=2;CHECK(refreshOreConfig()&&!oreMiner(MinerType));
   attackStarted();CHECK(!(planningWorld.actors[0].flags&planner::Harvester));
   observeAttack();kill();step();CHECK(events.size()==2);harvestTuple(events.back());observe(events.back());step();CHECK(!minerDefensePending());
  }
 });
 run("native_miner_manual_cancel_ignores_ore_type_exclusion",[]{
  for(bool observed:{false,true}){reset();sh->oreConfig.customMiners=1;sh->oreConfig.minerCount=1;memcpy(sh->oreConfig.miners[0],"OTHER",6);sh->oreConfigSeq+=2;
   CHECK(refreshOreConfig()&&!oreMiner(MinerType));attackStarted();if(observed){observeAttack();step();}
   cancelOreJob(Miner);CHECK(manualVersions.orePaused(MinerID)&&!minerDefensePending());
   for(int n=0;n<5;++n)step(30,500);CHECK(events.size()==1);
  }
 });
 run("custom_attacker_list_filters_source_not_parasite",[]{
  for(const wchar_t*id:{L"hArV",L"OTHER",L"DRON",L""}){reset();defenderTypes(id);
   if(std::wstring(id)==L"hArV"){attackStarted();CHECK(minerDefenseJobs.at(MinerID).mining);}
   else{for(int n=0;n<3;++n)step();CHECK(events.empty()&&!minerDefensePending());}
  }
  reset();defenderTypes(L"OTHER",false);attackStarted();
  reset();sh->featureConfig.filters[featuretypes::Dodge].custom=1;sh->featureConfigSeq+=2;attackStarted();
 });
 run("custom_ground_types_only_enroll_idle_guard_work",[]{
  for(int k:{1,2})for(int mission:{0,1,2,7,10,12,16}){reset();passiveDefender(k,mission);step();CHECK(events.empty()&&!minerDefensePending());}
  for(int k:{0,3}){reset();passiveDefender(k);step();CHECK(events.empty()&&!minerDefensePending());}
  for(int k:{1,2}){reset();passiveDefender(k);set<int>(Miner+0xB4,2);step();CHECK(events.empty()&&!minerDefensePending());
   reset();passiveDefender(k);set<uint32_t>(Miner+0x2B4,Drone);step();CHECK(events.empty()&&!minerDefensePending());
   reset();passiveDefender(k);set<uint32_t>(Miner+0x5A4,cellPointer(80,62));step();CHECK(events.empty()&&!minerDefensePending());
   reset();passiveDefender(k);set<uint32_t>(MinerType+0x898,0);step();CHECK(events.empty()&&!minerDefensePending());
  }
 });
 run("custom_vehicle_and_infantry_return_to_original_guard",[]{
  for(int k:{1,2})for(int mission:{5,11}){reset();passiveDefender(k,mission);attackStarted();
   CHECK(!(planningWorld.actors[0].flags&planner::Harvester));CHECK(!minerDefenseJobs.at(MinerID).mining&&!minerDefenseJobs.at(MinerID).managedQueue);
   observeAttack();step();position(Miner,57*256,62*256);kill();step();CHECK(events.size()==2&&minerDefensePending());passiveMoveTuple(events.back());
   step();CHECK(events.size()==2&&minerDefensePending());observe(events.back());step();CHECK(events.size()==2&&minerDefensePending());
   position(Miner,51*256+128,62*256+128);step(12,350);CHECK(events.size()==3&&minerDefensePending());passiveGuardTuple(events.back(),mission);
   step();CHECK(events.size()==3&&minerDefensePending());observe(events.back());step();CHECK(!minerDefensePending()&&actorLeaseArbiter.find(MinerID)<0);
   for(const auto&e:events)CHECK(e.mission!=7&&e.mission!=10);
  }
 });
 run("custom_defender_at_origin_restores_passive_without_travel",[]{
  for(int mission:{5,11}){reset();passiveDefender(2,mission);attackStarted();observeAttack();step();kill();step();
   CHECK(events.size()==2&&minerDefensePending());
   if(events.back().mission==2){passiveMoveTuple(events.back());observe(events.back());step(12,350);CHECK(events.size()==3&&minerDefensePending());}
   passiveGuardTuple(events.back(),mission);
   observe(events.back());step();CHECK(!minerDefensePending());
  }
 });
 run("live_filter_removal_restores_mining_without_readmission",[]{
  for(const wchar_t*id:{L"OTHER",L""}){reset();attackStarted();observeAttack();step();defenderTypes(id);step();
   CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());observe(events.back());step();CHECK(!minerDefensePending());
   for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==2);
  }
 });
 run("live_filter_removal_restores_passive_vehicle_and_infantry",[]{
  for(int k:{1,2})for(bool toDefault:{false,true}){reset();passiveDefender(k,11);attackStarted();observeAttack();step();
   position(Miner,57*256,62*256);defenderTypes(L"",!toDefault);step();CHECK(events.size()==2&&minerDefensePending());passiveMoveTuple(events.back());
   observe(events.back());position(Miner,51*256+128,62*256+128);step(12,350);CHECK(events.size()==3);passiveGuardTuple(events.back(),11);
   observe(events.back());step();CHECK(!minerDefensePending());for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==3);
  }
 });
 run("coherent_filter_reload_keeps_active_allowed_defender",[]{
  passiveDefender();attackStarted();observeAttack();step();defenderTypes(L"OTHER,MTNK");
  for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==1&&minerDefensePending());
 });
 run("invalid_or_incomplete_filter_never_uses_stale_admission",[]{
  for(int mode:{0,1,2}){reset();passiveDefender();CHECK(refreshFeatureTypes());
   if(mode==0)++sh->featureConfigSeq;
   if(mode==1){sh->featureConfig.filters[featuretypes::SpiderDefense].count=513;sh->featureConfigSeq+=2;}
   if(mode==2){sh->featureConfig.filters[featuretypes::SpiderDefense].custom=2;sh->featureConfigSeq+=2;}
   for(int n=0;n<4;++n)step();CHECK(events.empty()&&!minerDefensePending());
  }
 });
 run("custom_passive_manual_token_blocks_old_episode_and_same_pass_readmission",[]{
  for(bool attackObserved:{false,true}){reset();passiveDefender();attackStarted();
   if(attackObserved){observeAttack();step();set<int>(Miner+0xAC,5);set<uint32_t>(Miner+0x2B4,0);set<uint32_t>(Miner+0x5A4,0);}
   manualVersions.change(MinerID,0,false);step();CHECK(events.size()==1&&!minerDefensePending());
   for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==1);
  }
 });
 run("manual_cancel_read_failure_keeps_captured_passive_hold",[]{
  passiveDefender();attackStarted();
  // The player's cancel arrives while the old Guard is still visible and a
  // fresh type-ID read fails. Dropping the episode must retain its captured
  // cancellation identity, so a recovered read cannot revive that stale work.
  failRead=MinerType+0x10;cancelOreJob(Miner);failRead=0;
  CHECK(manualVersions.get(MinerID)==1&&!minerDefensePending());
  for(int n=0;n<5;++n)step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("manual_guard_hold_survives_live_id_slot_compaction",[]{
  constexpr uint32_t deadID=MinerID+ManualVersions::Size;
  CHECK(ObjectIndex::hash(deadID)==ObjectIndex::hash(MinerID));manualVersions.change(deadID,0,false);
  passiveDefender();attackStarted();cancelOreJob(Miner);CHECK(!minerDefensePending());
  const unsigned oldSlot=manualVersions.slot(MinerID);CHECK(oldSlot!=ObjectIndex::hash(MinerID));
  CHECK(minerDefenseManual[oldSlot].id==MinerID);const uint32_t token=manualVersions.get(MinerID);
  // The real indexWorld compaction boundary, with a dead colliding identity
  // removed. The still-live actor moves to another slot with the same token.
  static ManualVersions next;next.copyLive(manualVersions,[](uint32_t id){return worldByID(id)!=0;});
  CHECK(next.slot(MinerID)!=oldSlot&&next.get(MinerID)==token);
  minerDefenseReindexManual(next);manualVersions=next;
  CHECK(minerDefenseManual[manualVersions.slot(MinerID)].id==MinerID);
  for(int n=0;n<5;++n)step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("custom_passive_manual_native_transition_always_wins",[]{
  for(int stage:{0,1,2})for(int mission:{0,1,2}){reset();passiveDefender(1,11);attackStarted();
   if(stage){observeAttack();step();position(Miner,57*256,62*256);kill();step();CHECK(events.size()==2);}
   if(stage==2){observe(events.back());position(Miner,51*256+128,62*256+128);step(12,350);CHECK(events.size()==3);}
   const size_t before=events.size();set<int>(Miner+0xAC,mission);set<int>(Miner+0xB4,-1);
   if(mission==1){addDrone2();position(Drone2,90*256,90*256);set<uint32_t>(Miner+0x2B4,Drone2);set<uint32_t>(Miner+0x5A4,0);}
   else{set<uint32_t>(Miner+0x2B4,0);set<uint32_t>(Miner+0x5A4,mission==2?cellPointer(90,90):0);}
   step(30,500);CHECK(events.size()==before&&!minerDefensePending());
  }
 });
 run("custom_passive_identity_owner_and_health_invalidations",[]{
  for(int k:{1,2})for(int mode:{0,1,2,3}){reset();passiveDefender(k);attackStarted();observeAttack();step();
   if(mode==0)set<uint32_t>(Miner+0x10,99);if(mode==1)set<uint32_t>(MinerType+0x10,999);
   if(mode==2)set<uint32_t>(Miner+0x21C,EnemyHouse);if(mode==3)set<int>(Miner+0x6C,0);
   kill();step();CHECK(events.size()==1&&!minerDefensePending());
  }
 });
 run("manual_token_precedes_synchronized_mission",[]{
  attackStarted();observeAttack();step();const auto serial=inputSerial;manualVersions.change(MinerID,Miner,true);CHECK(!minerDefenseOwns(MinerID));
  step();CHECK(inputSerial==serial&&events.size()==1&&!minerDefensePending());for(int n=0;n<30;++n)step();CHECK(events.size()==1);
 });
 run("explicit_harvest_token_cancels_old_episode",[]{
  attackStarted();observeAttack();step();manualVersions.change(MinerID,Miner,true);manualVersions.resumeOre(MinerID);kill();step();CHECK(events.size()==1&&!minerDefensePending());
 });
 run("dead_target_reacquire_then_resume",[]{
  attackStarted();observeAttack();step();addDrone2();kill();step();CHECK(events.size()==2&&events.back().mission==1&&events.back().b==Drone2ID);
  observeAttack(Drone2);step();kill(Drone2);step();CHECK(events.size()==3&&sh->minerDefenseAttacks==2);harvestTuple(events.back());
 });
 run("retarget_delivery_lag_preserves_previous_owned_attack",[]{
  attackStarted();observeAttack();step();position(Drone,70*256,62*256);addDrone2();step();CHECK(events.size()==2&&events.back().b==Drone2ID);
  // The first enemy is still alive. The just-queued replacement Attack has
  // not executed yet, so the next snapshot correctly still targets Drone.
  step();CHECK(events.size()==2&&minerDefensePending());observeAttack(Drone2);step();CHECK(events.size()==2&&minerDefensePending());
  kill(Drone);kill(Drone2);step();CHECK(events.size()==3);harvestTuple(events.back());
 });
 run("retarget_grace_does_not_accept_unrelated_manual_target",[]{
  attackStarted();observeAttack();step();position(Drone,70*256,62*256);addDrone2();step();CHECK(events.size()==2&&events.back().b==Drone2ID);
  constexpr uint32_t third=0x200A2000;object(third,0x7F5C70,33,DroneType,EnemyHouse);position(third,90*256,90*256);actors.push_back(third);
  observeAttack(third);step();CHECK(events.size()==2&&!minerDefensePending());
 });
 run("new_entry_budgets_retry_without_ownership_leak",[]{
  for(int gate=0;gate<3;++gate){reset();planningOkay=gate!=0;outgoingOkay=gate!=1;nativeOkay=gate!=2;
   for(int n=0;n<5;++n)step();CHECK(events.empty()&&!minerDefensePending()&&actorLeaseArbiter.count==0&&consumed==0);
   planningOkay=outgoingOkay=nativeOkay=true;attackStarted();CHECK(consumed==1);
  }
 });
 run("restore_budget_failure_does_not_burn_retries",[]{
  attackStarted();observeAttack();step();kill();planningOkay=false;step();CHECK(events.size()==1&&minerDefenseJobs.at(MinerID).restoring);
  for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==1&&sh->minerDefenseActive==1&&minerDefenseJobs.at(MinerID).retries==0);
  planningOkay=true;step();CHECK(events.size()==2);harvestTuple(events.back());
  outgoingOkay=false;for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==2&&sh->minerDefenseActive==1&&minerDefenseJobs.at(MinerID).retries==0);
  outgoingOkay=true;step();CHECK(events.size()==3&&minerDefenseJobs.at(MinerID).retries==1);
 });
 run("attack_not_repeated_every_tick_and_retry_is_bounded",[]{
  attackStarted();for(int n=0;n<10;++n)step();CHECK(events.size()==1);
  step(20,400);CHECK(events.size()==2);step(20,400);CHECK(events.size()==3);step(20,400);CHECK(events.size()==4);
  step(20,400);CHECK(events.size()==5&&events.back().mission==10&&sh->minerDefenseAttacks==4);
 });
 run("state_read_uncertainty_retains_without_order",[]{
  attackStarted();observeAttack();step();failRead=Miner+0x74;step(1,16,false);CHECK(events.size()==1&&sh->minerDefenseActive==1);
  failRead=0;kill();step();CHECK(events.size()==2);harvestTuple(events.back());
 });
 run("exhausted_original_resource_resumes_bounded_live_search",[]{
  attackStarted();observeAttack();step();kill();cells.clear();resource(80,62);
  for(int n=0;n<40&&events.size()==1;++n){const unsigned before=cellReads;step();CHECK(cellReads-before<2000);CHECK(sh->minerDefenseActive==1);}
  CHECK(events.size()==2);harvestTuple(events.back(),62080);observe(events.back());step();CHECK(!minerDefensePending());
 });
 run("position_read_uncertainty_retains_work_for_recovery",[]{
  attackStarted();observeAttack();step();failRead=Miner+0x9C;step(1,16,false);CHECK(events.size()==1&&sh->minerDefenseActive==1);
  failRead=0;kill();step();CHECK(events.size()==2);harvestTuple(events.back());
 });
 run("native_target_read_uncertainty_retains_work_for_recovery",[]{
  attackStarted();observeAttack();step();failRead=Miner+0x2B4;step(1,16,false);CHECK(events.size()==1&&sh->minerDefenseActive==1);
  failRead=0;kill();step();CHECK(events.size()==2);harvestTuple(events.back());
 });
 run("target_uid_and_read_uncertainty_do_not_emit_stale_attack",[]{
  for(int mode=0;mode<2;++mode){reset();attackStarted();observeAttack();step();
   if(mode==0)set<uint32_t>(Drone+0x10,1001);else failRead=Drone+0x6C;
   step(1,16,false);CHECK(events.size()==1);failRead=0;set<uint32_t>(Drone+0x10,DroneID);kill();step();CHECK(events.size()==2);harvestTuple(events.back());
  }
 });
 run("identity_reuse_owner_and_manual_invalidations",[]{
  for(int mode=0;mode<5;++mode){reset();attackStarted();observeAttack();step();kill();
   if(mode==0)set<uint32_t>(Miner+0x10,12);if(mode==1)set<uint32_t>(MinerType+0x10,302);if(mode==2)set<uint32_t>(Miner+0x21C,EnemyHouse);
   if(mode==3)set<int>(Miner+0x6C,0);if(mode==4)set<uint32_t>(Miner+0x6C4,DroneType);step();CHECK(events.size()==1&&!minerDefensePending());
  }
 });
 run("scenario_house_session_and_frame_invalidate_restore",[]{
  for(int mode=0;mode<5;++mode){reset();attackStarted();observeAttack();step();sh->minerDefenseEnabled=0;
   if(mode==0)set<uint32_t>(0xA8B230,Scenario+0x100);if(mode==1)set<uint32_t>(House+0x10,102);
   if(mode==2){++match;snap.match=match;}if(mode==3){snap.session=0;set<int>(0xA8B238,0);}if(mode==4)snap.frame=100;
   step();CHECK(events.size()==1&&!minerDefensePending()&&sh->minerDefenseActive==0);
  }
 });
 run("disabled_and_ineligible_sources_never_enroll",[]{
  for(int mode=0;mode<14;++mode){reset();
   if(mode==0)set<uint8_t>(Miner+0x1C8,1);if(mode==1)set<uint32_t>(Miner+0x11C,Refinery);if(mode==2)set<uint8_t>(Miner+0x81,1);if(mode==3)set<uint8_t>(Miner+0x74,0);
   if(mode==4)set<uint8_t>(Miner+0x134,1);if(mode==5)set<uint32_t>(Miner+0x694,Drone);if(mode==6)set<uint32_t>(Miner+0x2B0,Drone);
   if(mode==7)set<int>(Miner+0xAC,2);if(mode==8)set<int>(Miner+0xB4,2);if(mode==9)held=true;if(mode==10)formed=true;if(mode==11)bursted=true;if(mode==12)dodging=true;if(mode==13)ordered=true;
   step();CHECK(events.empty()&&!minerDefensePending());
  }
 });
 run("nonthreat_ground_air_neutral_allied_and_protected",[]{
  for(int mode=0;mode<11;++mode){reset();
   if(mode==0)set<uint8_t>(ParasiteWarhead+0x159,0);if(mode==1)set<uint8_t>(DroneType+0xCCE,1);if(mode==2)set<uint8_t>(Drone+0x134,1);
   if(mode==3)set<int>(Drone+0x220,1);if(mode==4)set<uint32_t>(Drone+0x11C,Miner);if(mode==5)set<uint8_t>(Drone+0x81,1);
   if(mode==6)set<uint32_t>(Drone+0x21C,House);if(mode==7)set<uint8_t>(EnemyCountry+0x1A6,1);if(mode==8)set<uint32_t>(House+0x5788,1u<<4);
   if(mode==9)set<uint8_t>(DroneType+0x233,1);if(mode==10)set<uint32_t>(Drone+0x2B0,Miner);
   step();CHECK(events.empty()&&!minerDefensePending());
  }
 });
 run("elite_weapon_and_armor_selection_are_real_raw_reads",[]{
  position(Drone,57*256,62*256);step();CHECK(events.empty());set<uint32_t>(MinerType+0xA94,EliteGun);set<int>(EliteGun+0xB4,6*256);set<float>(Miner+0x150,2);attackStarted();
  reset();set<double>(Warhead+0xA0,0.0);step();CHECK(events.empty());
 });
 run("freshness_chat_and_duplicate_frame_guards",[]{
  snap.chat=1;step();CHECK(events.empty());snap.chat=0;step(10,200,false);CHECK(events.empty());step();CHECK(events.size()==1);
  CHECK(!minerDefenseTick(House,snap.frame,clockNow));CHECK(events.size()==1);
 });
 run("higher_priority_owner_wins",[]{
  attackStarted();observeAttack();step();actorLeaseArbiter.manual(MinerID);CHECK(actorLeaseArbiter.record(match,snap.frame,MinerID,Miner,manualVersions.get(MinerID),taskarbiter::Owner::Dodge,99));
  kill();step();CHECK(events.size()==1&&!minerDefensePending());CHECK(actorLeaseArbiter.find(MinerID)>=0&&actorLeaseArbiter.leases[actorLeaseArbiter.find(MinerID)].owner==taskarbiter::Owner::Dodge);
 });
 run("pure_range_boundaries_clamping_z_and_eligibility",[]{
  auto source=planningWorld.actors[0],target=planningWorld.actors[2];target.x=source.x+4*256;target.y=source.y;target.z=source.z;
  CHECK(minerdefense::inRange(source,target,0));++target.x;CHECK(!minerdefense::inRange(source,target,0));CHECK(minerdefense::inRange(source,target,1));
  target.x=source.x+5*256;CHECK(minerdefense::inRange(source,target,1));++target.x;CHECK(!minerdefense::inRange(source,target,1));CHECK(minerdefense::inRange(source,target,1,true));
  target.x=source.x+6*256;CHECK(minerdefense::inRange(source,target,1,true));++target.x;CHECK(!minerdefense::inRange(source,target,1,true));
  target.x=source.x+4*256;CHECK(minerdefense::inRange(source,target,-10));target.x=source.x+34*256;CHECK(minerdefense::inRange(source,target,999));++target.x;CHECK(!minerdefense::inRange(source,target,999));
  target.x=source.x+4*256;target.z=source.z+1;CHECK(!minerdefense::inRange(source,target,0));target.z=source.z;
  source.weapons[0].minimum=100;target.x=source.x;CHECK(minerdefense::inRange(source,target,0)); // Native Attack owns minimum-range reposition.
  source.flags|=planner::Disabled;CHECK(!minerdefense::inRange(source,target,30));source.flags&=~planner::Disabled;target.flags|=planner::Protected;CHECK(!minerdefense::inRange(source,target,30));
  target.flags&=~planner::Protected;target.armor=1;source.weapons[0].armorMask=source.weapons[0].selectMask=1;source.weapons[1]=source.weapons[0];source.weapons[1].range=8*256;source.weapons[1].armorMask=source.weapons[1].selectMask=2;
  target.x=source.x+8*256;CHECK(planner::weaponIndex(source,target)==1&&minerdefense::inRange(source,target,0));source.flags|=planner::FixedWeapon;CHECK(!minerdefense::inRange(source,target,0));
 });
 run("disabled_restore_permanent_read_or_send_failure_is_bounded",[]{
  for(int mode=0;mode<3;++mode){reset();attackStarted();observeAttack();step();sh->minerDefenseEnabled=0;
   if(mode==0)failRead=0x87F924;
   if(mode==1){cells.erase(ResourceCell);failRead=cellPointer(51,62)+0x44;}
   if(mode==2)nativeOkay=false;
   step();CHECK(sh->minerDefenseActive==1&&events.size()==1);
   for(int n=0;n<20&&minerDefensePending();++n)step(40,1000);
   CHECK(!minerDefensePending()&&sh->minerDefenseActive==0&&events.size()==1);
  }
 });
 run("restore_expiry_does_not_advance_while_simulation_is_paused",[]{
  attackStarted();observeAttack();step();sh->minerDefenseEnabled=0;nativeOkay=false;step();CHECK(sh->minerDefenseActive==1);
  step(0,20000);CHECK(sh->minerDefenseActive==1&&events.size()==1);
  step(601,16);CHECK(!minerDefensePending()&&events.size()==1);
 });
 run("more_than_six_active_jobs_share_attack_and_restore_budget",[]{
  actors={Refinery,Drone};std::map<uint32_t,uint32_t> minerPointers;
  for(unsigned n=0;n<8;++n){const uint32_t p=0x200D0000+n*0x1000,id=110+n;memcpy(memory(p,0x800),memory(Miner,0x800),0x800);set<uint32_t>(p+0x10,id);worldObjects.add(p,id);actors.push_back(p);minerPointers[id]=p;}
  step();CHECK(events.size()==6&&sh->minerDefenseActive==6);for(const auto&e:events){uint32_t p=minerPointers.at(uint32_t(e.a));set<int>(p+0xAC,1);set<uint32_t>(p+0x2B4,Drone);}
  step();CHECK(events.size()==8&&sh->minerDefenseActive==8);for(const auto&e:events){uint32_t p=minerPointers.at(uint32_t(e.a));set<int>(p+0xAC,1);set<uint32_t>(p+0x2B4,Drone);}
  step();CHECK(events.size()==8);
  // Choose the last current active-map entry, then let it lose all threats.
  // The other seven continuously reacquire alternating live drones, consuming
  // six available sends each tick unless active work receives a fair turn.
  uint32_t restoreID=0;for(const auto&entry:minerDefenseJobs)restoreID=entry.first;position(minerPointers.at(restoreID),90*256,90*256);addDrone2();kill(Drone2);
  bool restored=false;unsigned previous=unsigned(events.size());
  for(int n=0;n<12&&!restored;++n){uint32_t alive=n%2?Drone:Drone2,dead=n%2?Drone2:Drone;set<int>(alive+0x6C,800);kill(dead);step();
   CHECK(events.size()-previous<=6);
   for(unsigned at=previous;at<events.size();++at){const auto&e=events[at];uint32_t p=minerPointers.at(uint32_t(e.a));
    if(e.a==int(restoreID)&&e.mission==10){harvestTuple(e,ResourceCell,restoreID);restored=true;}
    if(e.mission==1){set<int>(p+0xAC,1);set<uint32_t>(p+0x2B4,worldByID(uint32_t(e.b)));}
   }previous=unsigned(events.size());
  }
  CHECK(restored);
 });
 run("capacity_1023_plus_six_candidates_stays_at_1024",[]{
  actors={Refinery,Drone};
  for(unsigned n=0;n<MinerDefenseCapacity+5;++n){const uint32_t p=0x20100000+n*0x800,id=1000+n;
   memcpy(memory(p,0x800),memory(Miner,0x800),0x800);set<uint32_t>(p+0x10,id);worldObjects.add(p,id);actors.push_back(p);
  }
  ++snap.frame;set<int>(0xA8ED84,snap.frame);sampleWorld();minerDefenseIndexRefineries();CHECK(oreHarvestContext(House,snap.frame,minerDefenseWorld));
  // Seed valid in-flight work by the production capture/attack functions.
  // Only this setup bypasses the worker's six-send budget; the actual boundary
  // check below enters minerDefenseTick with 1023 jobs and six new candidates.
  for(unsigned n=0;n<MinerDefenseCapacity-1;++n){const uint32_t id=1000+n,p=0x20100000+n*0x800;MinerDefenseJob job{};
   CHECK(minerDefenseCapture(planningWorld.actors[n+2],House,job));auto inserted=minerDefenseJobs.emplace(id,job);
   CHECK(minerDefenseAttack(inserted.first->second,DroneID,House,snap.frame,clockNow,false));set<int>(p+0xAC,1);set<uint32_t>(p+0x2B4,Drone);set<uint32_t>(p+0x5A4,0);
  }
  minerDefensePublish();CHECK(minerDefenseJobs.size()==1023);const size_t before=events.size();step();
  CHECK(minerDefenseJobs.size()==MinerDefenseCapacity&&sh->minerDefenseActive==MinerDefenseCapacity&&events.size()==before+1);
  step();CHECK(minerDefenseJobs.size()==MinerDefenseCapacity&&events.size()==before+1);
 });
 run("post_kill_queued_enter_waits_for_actual_restore_observation",[]{
  for(bool staleUnloading:{false,true}){reset();refineryWork(staleUnloading?16:12);const planner::Order order{MinerID,0,0,RefineryID,7,0x34,planner::OreOrder,RefineryID};minerDefenseRememberOre(order,House);CHECK(minerDefenseWork.count(MinerID)==1);
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2&&events.back().mission==7);
  set<int>(Miner+0xB4,7);set<uint32_t>(Miner+0x5A4,Refinery);step();CHECK(events.size()==2&&minerDefensePending()&&minerDefenseWork.count(MinerID)==1);
  // Queued Enter has not become the current mission. If it disappears into
  // Guard, keep the saved work and retry instead of declaring it restored.
  observeGuard();step(12,350);CHECK(events.size()==3&&events.back().mission==7&&minerDefensePending());
  observe(events.back());step();CHECK(!minerDefensePending()&&minerDefenseWork.count(MinerID)==1);
  }
 });
 run("different_harvest_destination_during_restore_is_manual",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);set<int>(Miner+0xAC,10);set<uint32_t>(Miner+0x5A4,cellPointer(80,62));step(30,500);CHECK(events.size()==2&&!minerDefensePending());
 });
 run("uncaptured_manual_guard_before_attack_observation",[]{
  attackStarted();set<int>(Miner+0xAC,5);set<uint32_t>(Miner+0x5A4,0);step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("uncaptured_stop_mission_before_attack_observation",[]{
  attackStarted();set<int>(Miner+0xAC,0);set<uint32_t>(Miner+0x5A4,0);step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("uncaptured_manual_move_before_attack_observation",[]{
  attackStarted();set<int>(Miner+0xAC,2);set<uint32_t>(Miner+0x5A4,cellPointer(90,90));step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("uncaptured_manual_attack_before_attack_observation",[]{
  attackStarted();addDrone2();observeAttack(Drone2);step(30,500);CHECK(events.size()==1&&!minerDefensePending());
 });
 run("uncaptured_manual_move_during_restoration",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);set<int>(Miner+0xAC,2);set<uint32_t>(Miner+0x5A4,cellPointer(90,90));step(30,500);CHECK(events.size()==2&&!minerDefensePending());
 });
 run("uncaptured_manual_attack_during_restoration",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);addDrone2();position(Drone2,90*256,90*256);observeAttack(Drone2);step(30,500);CHECK(events.size()==2&&!minerDefensePending());
 });
 run("uncaptured_stop_mission_during_restoration",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);set<int>(Miner+0xAC,0);set<uint32_t>(Miner+0x5A4,0);step(30,500);CHECK(events.size()==2&&!minerDefensePending());
 });
 run("native_idle_guard_after_observed_attack_can_resume",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);set<int>(Miner+0xAC,5);set<uint32_t>(Miner+0x2B4,0);set<uint32_t>(Miner+0x5A4,0);
  // Guard5 with empty native target/destination is also the natural Attack
  // completion shape. Without a changed input token it cannot identify Stop.
  step(30,500);CHECK(events.size()==3&&minerDefensePending());harvestTuple(events.back());
 });
 run("post_kill_guard_before_any_attack_snapshot_resumes_mining",[]{
  for(bool clearAlive:{false,true}){reset();attackStarted();CHECK(!minerDefenseJobs.at(MinerID).observedAttack);
  // Attack and the kill complete between two sampler observations. The next
  // visible source state is native idle Guard, not the fleeting Attack.
  if(clearAlive)set<uint8_t>(Drone+0x90,0);else kill();observeGuard();
  step(12,350);CHECK(events.size()==2&&sh->minerDefenseResumes==1&&minerDefensePending());harvestTuple(events.back());
  CHECK(get<int>(Miner+0xAC)==5);step();CHECK(events.size()==2&&minerDefensePending());
  observe(events.back());step();CHECK(get<int>(Miner+0xAC)==10&&!minerDefensePending());
  }
 });
 run("post_kill_guard_with_dead_target_and_original_destination_resumes",[]{
  for(int residue:{0,1,2}){reset();attackStarted();observeAttack();step();kill();
   observeGuard(residue!=1?Drone:0,residue!=0?cellPointer(56,62):0);step(12,350);
   CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());
   step();CHECK(events.size()==2&&minerDefensePending());observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
  }
 });
 run("post_kill_attack_target_clears_before_guard_still_recovers",[]{
  attackStarted();kill();observeAttack(0);step();CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());
  observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
 });
 run("post_kill_attack_with_queued_guard_recovers",[]{
  for(bool targetCleared:{false,true}){reset();attackStarted();kill();observeAttack(targetCleared?0:Drone);set<int>(Miner+0xB4,5);
   step(12,350);CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());
   step();CHECK(events.size()==2&&minerDefensePending());observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
  }
 });
 run("post_kill_retired_victim_without_readable_corpse_restores",[]{
  for(int mode:{0,1,2,3}){reset();attackStarted();CHECK(!minerDefenseJobs.at(MinerID).observedAttack);
   if(mode==2){ // Reused memory contains a new non-parasite Unit, never our old target.
    object(Drone,0x7F5C70,999,MinerType,EnemyHouse);position(Drone,55*256,62*256);
   }else{
    actors.erase(std::remove(actors.begin(),actors.end(),Drone),actors.end());
    if(mode==0)memset(memory(Drone,0x800),0,0x800);
    if(mode==1)failRead=Drone;
    // mode 3 leaves stale live-looking bytes and an old worldObjects entry.
    // Only the complete native roster establishes that the target is gone.
   }
   observeGuard();step(12,350);CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());
   CHECK(get<int>(Miner+0xAC)==5);step();CHECK(events.size()==2&&minerDefensePending());
   failRead=0;observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
   for(const auto&e:events)CHECK(e.mission!=1||e.b==DroneID);
  }
 });
 run("post_kill_partial_roster_does_not_invent_retirement",[]{
  for(int mode:{0,1}){reset();attackStarted();actors.erase(std::remove(actors.begin(),actors.end(),Drone),actors.end());memset(memory(Drone,0x800),0,0x800);
   if(mode==0)failRead=NativeUnitItems;
   else{object(Drone2,0x7F5C70,999,MinerType,EnemyHouse);position(Drone2,90*256,90*256);actors.push_back(Drone2);failRead=Drone2+0x10;}
   observeGuard();step(12,350);CHECK(events.size()==1&&minerDefensePending());
   step();CHECK(events.size()==1&&minerDefensePending());
   // A later complete read can establish retirement and recover the work.
   failRead=0;step();CHECK(events.size()==2&&minerDefensePending());harvestTuple(events.back());
   observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
  }
 });
 run("post_kill_skipped_attack_returns_native_return_and_unload_work",[]{
  for(int mission:{12,16}){reset();refineryWork(mission);attackStarted();CHECK(!minerDefenseJobs.at(MinerID).observedAttack);
   kill();set<uint8_t>(Miner+0x6D1,0);observeGuard(0,Refinery);step(12,350);
   CHECK(events.size()==2&&events.back().mission==7&&events.back().c==RefineryID&&minerDefensePending());
   step();CHECK(events.size()==2&&minerDefensePending());observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==7);
  }
 });
 run("post_kill_skipped_attack_preserves_managed_wait_recovery",[]{
  rememberWait();attackStarted();CHECK(minerDefenseWork.count(MinerID)==1);kill();observeGuard(0,cellPointer(53,62));step(12,350);
  CHECK(events.size()==2&&events.back().mission==2&&events.back().c==WaitCell&&minerDefensePending()&&minerDefenseWork.count(MinerID)==1);
  step();CHECK(events.size()==2&&minerDefensePending());observe(events.back());position(Miner,53*256+128,62*256+128);step();
  CHECK(!minerDefensePending()&&minerDefenseWork.count(MinerID)==1);for(const auto&e:events)CHECK(e.mission!=7);
 });
 run("post_kill_queued_harvest_waits_for_actual_restore_observation",[]{
  attackStarted();observeAttack();step();kill();step();CHECK(events.size()==2);harvestTuple(events.back());
  set<int>(Miner+0xB4,10);set<uint32_t>(Miner+0x5A4,cellPointer(56,62));step();CHECK(events.size()==2&&minerDefensePending());
  observeGuard();step(12,350);CHECK(events.size()==3&&minerDefensePending());harvestTuple(events.back());
  step();CHECK(events.size()==3&&minerDefensePending());observe(events.back());step();CHECK(!minerDefensePending()&&get<int>(Miner+0xAC)==10);
 });
 run("post_kill_explicit_manual_stop_move_attack_always_win",[]{
  for(int mission:{0,1,2}){reset();attackStarted();kill();cancelOreJob(Miner);
   if(mission==1){addDrone2();position(Drone2,90*256,90*256);observeAttack(Drone2);}
   else{set<int>(Miner+0xAC,mission);set<int>(Miner+0xB4,-1);set<uint32_t>(Miner+0x2B4,0);set<uint32_t>(Miner+0x5A4,mission==2?cellPointer(90,90):0);}
   step(12,350);CHECK(events.size()==1&&!minerDefensePending());
   observeGuard();for(int n=0;n<4;++n)step(30,500);CHECK(events.size()==1&&!minerDefensePending());
  }
 });
 run("post_kill_inference_requires_captured_target_and_known_destination",[]{
  for(int mode:{0,1,2,3,4,5,6,7}){reset();attackStarted();observeGuard();
   if(mode==0)failRead=Drone+0x6C; // A failed read is not a zero-HP observation.
   if(mode==1){ // Pointer mismatch does not prove retirement while the old UID remains listed.
    set<uint32_t>(Drone+0x10,999);object(Drone2,0x7F5C70,DroneID,DroneType,EnemyHouse);position(Drone2,90*256,90*256);actors.push_back(Drone2);
   }
   if(mode==2)set<int>(Drone+0x220,1); // Cloak invalidates targeting, not life.
   if(mode==3)set<uint32_t>(Drone+0x21C,House); // Ownership change is not death.
   if(mode==4){kill();set<uint32_t>(Miner+0x5A4,cellPointer(90,90));} // Unrelated native order residue.
   if(mode==5)set<uint8_t>(Drone+0x81,1); // Limbo alone is not observed death.
   if(mode==6)set<uint8_t>(Drone+0x74,0); // Neither is an off-map transition.
   if(mode==7)set<uint32_t>(DroneType+0x10,999); // Replaced type identity.
   step(12,350);CHECK(events.size()==1);failRead=0;for(int n=0;n<3;++n)step();CHECK(events.size()==1);
  }
 });
 std::printf("{\"checks\":%u,\"passed_cases\":%u,\"failed_cases\":%u,\"assertions_active\":true,\"production_headers_executed\":true,\"failures\":[",checks,passed,failed);
 for(size_t n=0;n<failures.size();++n)std::printf("%s\"%s\"",n?",":"",failures[n].c_str());std::printf("],\"pass\":%s}\n",failed?"false":"true");delete sh;return failed?1:0;
}

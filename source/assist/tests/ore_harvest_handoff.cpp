// Production observer/search and extracted production order admission.
// Read-only synthetic pages plus the existing native event boundary seam.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdint>
#include <chrono>
using DWORD=uint32_t;
struct LARGE_INTEGER {int64_t QuadPart=0;};
static void QueryPerformanceFrequency(LARGE_INTEGER*out){out->QuadPart=1000000000;}
static void QueryPerformanceCounter(LARGE_INTEGER*out){out->QuadPart=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <functional>
#include "../source/external_planner.h"
#include "../source/object_index.h"
#include "../source/unit_task_arbiter.h"
#include "../source/command_intent.h"
#ifdef NDEBUG
#error Ore handoff fixture requires active assertions (-UNDEBUG).
#endif
#undef assert
#define assert(v) ((v)?(void)0:(std::fprintf(stderr,"assert %s at %d\n",#v,__LINE__),std::_Exit(3)))
using namespace assist;
constexpr uint32_t House=0x20010000,Miner=0x20020000,MinerType=0x20030000,Refinery=0x20040000,RefineryType=0x20050000;
constexpr uint32_t Overlay=0x20060000,Tiberium=0x20070000,OverlayItems=0x20080000,TiberiumItems=0x20081000,Links=0x20082000,Docks=0x20083000;
constexpr uint32_t Cells=0x21000000,CellBase=0x30000000,Map=0x87F7E8;
static Shared*sh;static Snapshot snap;static planner::World planningWorld;
static uint32_t match=7,lastHouse=House;static bool fault=false,queueOkay=true,held=false,bursted=false,dodging=false;
static DWORD clockNow=1000;static uint32_t failRead=0;static unsigned reads=0,cellReads=0,maxCellReads=0;static uint64_t observerCellReads=0;
static bool kernelReads=false;
static uint8_t globals[0x500000]{},objects[0x100000]{};
static ManualVersions manualVersions;static ObjectIndex worldObjects;static taskarbiter::FrameArbiter<> actorFrameArbiter;
static oreconfig::Config oreConfig;
static long oreConfigRevision=0;
static std::vector<intent::EventOrder>events;static std::function<void()>queueMutation;
struct Cell {int overlay=-1,land=0;uint32_t occupation=0;uint8_t amount=0;int8_t level=0;uint8_t slope=0;};
static std::map<uint32_t,Cell>cells;static std::set<uint32_t>nullCells;
static void*memory(uint32_t at,size_t n){if(at>=0x700000&&uint64_t(at)+n<=0xC00000)return globals+at-0x700000;if(at>=0x20000000&&uint64_t(at)+n<=0x20100000)return objects+at-0x20000000;return nullptr;}
template<class T>static void set(uint32_t at,T v){void*p=memory(at,sizeof v);assert(p);memcpy(p,&v,sizeof v);}
static uint32_t cellPointer(int x,int y){return CellBase+uint32_t(y*512+x)*0x200;}
#include "fixtures/tactical_frame/runtime_sample_fixture.h"
static bool readFixtureLive(uint32_t at,void*out,size_t n){
 assert(!sampleReadDepth);
 ++reads;if(failRead&&at<=failRead&&uint64_t(at)+n>failRead){if(n)memset(out,0xCC,n-1);return false;}
 if(fixtureReadMap(at,out,n)>0)return true;
 void*p=memory(at,n);if(!p)return false;
#ifdef _WIN32
 if(kernelReads){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),p,out,n,&got)&&got==n;}
#endif
 memcpy(out,p,n);return true;
}
template<class T>static T get(uint32_t at){T v{};read(at,&v,sizeof v);return v;}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t at,int limit=16384){Vec v{};if(!read(at,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||v.items<0x10000)return {};return v;}
static uint32_t item(Vec v,int i){return i>=0&&i<v.count?get<uint32_t>(v.items+uint32_t(i)*4):0;}
static uint32_t worldByID(uint32_t id){uint32_t p=worldObjects.byId(id),actual=0;return p&&read(p+0x10,&actual,4)&&actual==id?p:0;}
static int kind(uint32_t p){uint32_t vt=get<uint32_t>(p);return vt==0x7F5C70?2:vt==0x7E3EBC?3:-1;}
static uint32_t typeOf(uint32_t p,int k){return get<uint32_t>(p+(k==3?0x520:0x6C4));}
static bool own(uint32_t p,uint32_t house){return p&&get<uint32_t>(p+0x21C)==house&&get<int>(p+0x6C)>0&&get<uint8_t>(p+0x74)&&get<uint8_t>(p+0x90)&&!get<uint8_t>(p+0x81);}
static bool refreshOreConfig(){return true;}
static uint32_t readMapCell(int x,int y){return x>0&&x<512&&y>0&&y<512&&!nullCells.count(uint32_t(y*1000+x))?cellPointer(x,y):0;}
static bool stickyOwns(uint32_t){return held;}static bool burstOwns(uint32_t){return bursted;}static bool dodgeOwns(uint32_t){return dodging;}
// This existing unload-only fixture has no active defense worker. The defense
// fixture executes that production worker and its own restoration gateway.
static bool minerDefenseOwns(uint32_t){return false;}
static void minerDefenseRememberOre(const planner::Order&,uint32_t){}
static bool rawMagnetronDrop(uint32_t,uint32_t,uint32_t){assert(false);return false;}
static bool antiTheftBuilding(uint32_t){assert(false);return false;}static bool antiTheftThreat(uint32_t,uint32_t){assert(false);return false;}
static bool rawMagnetron(uint32_t){assert(false);return false;}static bool featureObjectAllowed(int,uint32_t){assert(false);return false;}
static bool nativeEnemy(uint32_t,uint32_t){assert(false);return false;}
static void rawCombatInfo(uint32_t,uint32_t,int,planner::Actor&,bool){assert(false);}
static planner::CombatProbe probeCombatPair(uint32_t,uint32_t,const planner::Actor&,const planner::Actor&,int){assert(false);return {};}
static bool advancedTargetInRange(uint32_t,uint32_t){assert(false);return false;}
static int orderIndex(uint32_t){return -1;}
struct NativeOrder {bool global=false,attacking=false,stopped=false;uint32_t token=0,mode=0,id=0,target=0;int sentFrame=-1;};
static NativeOrder nativeOrders[1];static int nativeCount=0;
static bool planningRoom(int){return queueOkay;}static bool outgoingRoom(){return queueOkay;}static void consumePlanning(int){}
enum class OrderAuthority {Local};
static bool nativeObjectEvent(int,int,uint32_t,OrderAuthority,void*,bool){assert(false);return false;}
static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,uint32_t destination,int tag,uint32_t nearest,int nearestTag,taskarbiter::Owner owner){
 assert(!sampleReadDepth&&house==House&&owner==taskarbiter::Owner::Ore&&mission==10&&!target&&tag==11&&!nearest&&!nearestTag);
 if(queueMutation)queueMutation();if(!queueOkay)return false;
 events.push_back(intent::mega(get<int>(House+0x30),id,mission,target,0,destination,uint8_t(tag),nearest,uint8_t(nearestTag)));
 actorFrameArbiter.record(id,manualVersions.get(id),owner);return true;
}
// The runner extracts these complete production functions, not a matching
// decision model. Unused combat/device seams above fail if reached.
#include "ore_eligibility_extract.h"
#ifdef ORE_HARVEST_OBSERVER_HEADER
#include ORE_HARVEST_OBSERVER_HEADER
#else
#include "../source/ore_harvest_bridge.h"
#endif
#include "ore_admission_extract.h"
static void object(uint32_t p,uint32_t vt,uint32_t id,uint32_t type){set<uint32_t>(p,vt);set<uint32_t>(p+0x10,id);set<int>(p+0x6C,800);set<uint32_t>(p+0x21C,House);set<uint8_t>(p+0x74,1);set<uint8_t>(p+0x90,1);set<uint32_t>(p+(vt==0x7E3EBC?0x520:0x6C4),type);worldObjects.add(p,id);}
static void vectorAt(uint32_t at,uint32_t items,int count){Vec v{0,items,count,0,count,0};memcpy(memory(at,sizeof v),&v,sizeof v);}
static void resource(int x,int y,int land=0,uint32_t occupation=0){cells[uint32_t(y*1000+x)]={0,land,occupation,0,0,0};}
static void reset(){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);sh->~Shared();new(sh)Shared{};snap={};planningWorld.~World();new(&planningWorld)planner::World{};oreConfig={};manualVersions.clear();worldObjects.clear();actorFrameArbiter.clear();oreHarvestReset();events.clear();cells.clear();nullCells.clear();failRead=0;fixtureResetWorld();queueMutation={};fault=held=bursted=dodging=false;queueOkay=true;kernelReads=false;reads=cellReads=maxCellReads=0;observerCellReads=0;
 match=7;lastHouse=House;clockNow=1000;sh->flags=AutoOreReturn;sh->computeApplied=compute::All;snap.status=snap.commandStatus=1;snap.session=3;snap.match=7;snap.frame=300;
 set<uint32_t>(0xA83D4C,House);set<uint32_t>(House+0x10,101);set<int>(House+0x30,3);set<uint32_t>(0xA8B230,0x20090000);set<int>(0xA8B238,3);set<int>(0xA8ED84,300);
 set<uint32_t>(MinerType+0x10,301);set<uint8_t>(MinerType+0xE0E,1);set<int>(MinerType+0x800,20);set<int>(MinerType+0x67C,1);set<uint32_t>(RefineryType+0x10,501);vectorAt(MinerType+0x3E8,Docks,1);set<uint32_t>(Docks,RefineryType);
 object(Miner,0x7F5C70,11,MinerType);object(Refinery,0x7E3EBC,21,RefineryType);set<float>(Miner+0x33C,20);set<uint8_t>(Miner+0x6D1,1);set<int>(Miner+0xAC,16);set<int>(Miner+0xB4,-1);set<uint32_t>(Miner+0x5A4,Refinery);
 set<int>(Refinery+0xE8,1);set<uint32_t>(Refinery+0xE4,Links);set<uint32_t>(Links,Miner);
 set<int>(Miner+0x9C,51*256);set<int>(Miner+0xA0,62*256);set<int>(Refinery+0x9C,50*256);set<int>(Refinery+0xA0,60*256);
 int bounds[]={100,100,0,0,100,100};memcpy(memory(Map+0xF4,sizeof bounds),bounds,sizeof bounds);set<uint32_t>(0x87F924,Cells);set<int>(0x87F928,512*512);
 vectorAt(0xA83D80,OverlayItems,1);set<uint32_t>(OverlayItems,Overlay);set<uint8_t>(Overlay+0x2A9,1);set<int>(Overlay+0x294,0);
 vectorAt(0xB0F4E8,TiberiumItems,1);set<uint32_t>(TiberiumItems,Tiberium);set<uint32_t>(Tiberium+0xE0,Overlay);set<int>(Tiberium+0xE8,1);set<int>(Tiberium+0xEC,0);set<int>(Tiberium+0xB8,25);
 vectorAt(0x8B4108,0x20084000,1);set<uint32_t>(0x20084000,Miner);vectorAt(0xA8EB40,0x20085000,1);set<uint32_t>(0x20085000,Refinery);
 vectorAt(0xA8EC78,0x20086000,2);set<uint32_t>(0x20086000,Miner);set<uint32_t>(0x20086004,Refinery);
 for(int land=0;land<12;++land)for(int speed=0;speed<8;++speed)set<float>(0x89EA40+uint32_t(land*9+speed)*4,land==2?0.f:1.f);
 planningWorld.match=7;planningWorld.frame=300;planningWorld.tick=1000;planningWorld.count=2;planningWorld.refineryCount=1;
 auto&a=planningWorld.actors[0];a.id=11;a.type=MinerType;a.kind=2;a.flags=planner::Own|planner::Harvester|planner::Unloading;a.x=51*256;a.y=62*256;a.destination=21;a.mission=16;a.queued=-1;a.storage[0]=20;a.storageLimit=20;a.dockTypes[0]=RefineryType;
 auto&r=planningWorld.actors[1];r.id=21;r.type=RefineryType;r.kind=3;r.flags=planner::Own;r.x=50*256;r.y=60*256;r.radioIDs[0]=11;
 auto&area=planningWorld.refineries[0];area.id=21;area.cellX=50;area.cellY=60;
 resource(56,62);resource(80,62);
}
static int pending(){int n=0;for(const auto&j:oreHarvestJobs)n+=j.id!=0;return n;}
static void step(){++snap.frame;set<int>(0xA8ED84,snap.frame);clockNow+=16;planningWorld.frame=snap.frame;planningWorld.tick=clockNow;unsigned before=cellReads;fixtureCaptureWorld(clockNow);oreHarvestTick(House,snap.frame,clockNow);maxCellReads=std::max(maxCellReads,cellReads-before);}
static void observeFrame(uint32_t house,int frame,DWORD now){snap.frame=frame;fixtureCaptureWorld(now);const uint64_t before=cellReads+fixtureSampleCellReads;oreHarvestObserve(house,frame,now);observerCellReads+=cellReads+fixtureSampleCellReads-before;}
static void unloadDone(){set<float>(Miner+0x33C,0);set<uint8_t>(Miner+0x6D1,0);set<int>(Miner+0xAC,10);set<int>(Refinery+0xE8,0);set<uint32_t>(Miner+0x5A4,cellPointer(80,62));planningWorld.actors[0].flags&=~planner::Unloading;planningWorld.actors[0].storage[0]=0;planningWorld.actors[0].mission=10;planningWorld.actors[1].radioIDs[0]=0;}
static void finish(int frames=80){for(int n=0;n<frames&&events.empty()&&pending();++n)step();}
int main(){sh=new Shared{};unsigned checks=0;auto check=[&](bool b){if(!b)std::fprintf(stderr,"check ordinal=%u pending=%d events=%zu miners=%d frame=%d\n",checks+1,pending(),events.size(),oreHarvestMinerCount,snap.frame);assert(b);++checks;};
 reset();step();check(pending()==1&&events.empty());set<float>(Miner+0x33C,0);step();check(events.empty());unloadDone();finish();check(events.size()==1&&!pending());
 check(events[0].mission==10&&events[0].b==0&&events[0].tagB==0&&events[0].c==62056&&events[0].tagC==11&&events[0].d==0&&!events[0].nearestTag);
 for(int n=0;n<20;++n)step();check(events.size()==1);const unsigned normalReads=reads,normalMaxCellReads=maxCellReads;
 // Native and tool-started unloading share this observer; calculation location
 // does not change handoff semantics and no pre-existing tool OreJob is needed.
 for(bool runtime:{false,true}){reset();sh->computeApplied=runtime?compute::All:0;step();unloadDone();finish();check(events.size()==1&&events[0].c==62056);}
 reset();planningWorld.actors[0].flags&=~planner::Unloading;set<uint8_t>(Miner+0x6D1,0);step();unloadDone();for(int n=0;n<5;++n)step();check(events.empty()&&!pending());
 reset();set<float>(Miner+0x33C,0);planningWorld.actors[0].storage[0]=0;step();unloadDone();finish();check(events.size()==1); // observed final unload frame still counts
 reset();step();unloadDone();set<int>(Refinery+0xE8,1);step();check(events.empty()&&pending()==1);set<int>(Refinery+0xE8,0);finish();check(events.size()==1);
 reset();step();set<uint8_t>(Miner+0x6D1,0);planningWorld.actors[0].flags&=~planner::Unloading;step();check(events.empty()&&pending()==1);unloadDone();finish();check(events.size()==1);
 // The origin is the frozen unloading dock, not the truck already travelling
 // towards the native distant resource when the Host's next sample arrives.
 reset();step();unloadDone();set<int>(Miner+0x9C,79*256);planningWorld.actors[0].x=79*256;finish();check(events.size()==1&&events[0].c==62056);
 reset();planningWorld.refineries[0].dockStyleB=1;set<uint8_t>(RefineryType+0x16E5,1);resource(51,62);resource(53,62);step();unloadDone();finish();check(events.size()==1&&events[0].c==62051);
 reset();nullCells.insert(62052);step();unloadDone();finish();check(events.size()==1&&events[0].c==62056);
 reset();cells.erase(62056);resource(53,62,2);resource(54,62,0,0x80);resource(55,62,0,0x20);step();unloadDone();finish();check(events.size()==1&&events[0].c==62055); // transient vehicles do not forbid native pathfinding
 reset();step();unloadDone();set<uint32_t>(Miner+0x5A4,cellPointer(56,62));finish();check(events.empty()&&!pending());
 // Later manual orders, pointer reuse, world replacement and feature OFF win.
 for(int k=0;k<13;++k){reset();step();unloadDone();switch(k){case 0:manualVersions.change(11,Miner,true);break;case 1:set<uint32_t>(Miner+0x10,12);break;case 2:set<uint32_t>(MinerType+0x10,302);break;case 3:set<uint32_t>(Miner+0x21C,House+0x100);break;case 4:set<uint32_t>(House+0x10,102);break;case 5:set<uint32_t>(0xA8B230,0x20090100);break;case 6:++match;planningWorld.match=match;snap.match=match;break;case 7:set<int>(0xA8B238,0);snap.session=0;break;case 8:set<uint32_t>(Refinery+0x10,22);break;case 9:sh->flags=0;break;case 10:set<int>(Miner+0xAC,2);break;case 11:set<int>(Miner+0xB4,1);break;case 12:set<int>(Miner+0x6C,0);break;}step();check(events.empty());}
 reset();step();unloadDone();--snap.frame;--snap.frame;step();check(events.empty()&&!pending());
 reset();set<uint8_t>(Miner+0x74,0);set<uint8_t>(Miner+0x81,1);planningWorld.actors[0].flags|=planner::OffMap|planner::Limbo;step();check(pending()==1);unloadDone();step();check(events.empty());set<uint8_t>(Miner+0x74,1);set<uint8_t>(Miner+0x81,0);finish();check(events.size()==1);
 reset();step();unloadDone();failRead=Miner+0x74;step();check(events.empty()&&pending()==1);failRead=0;finish();check(events.size()==1);
 reset();step();unloadDone();failRead=cellPointer(52,62)+0x44;step();check(events.empty());failRead=0;finish();check(events.size()==1);
 reset();step();unloadDone();queueOkay=false;for(int n=0;n<6;++n)step();check(events.empty()&&pending()==1);queueOkay=true;finish();check(events.size()==1);
 reset();step();unloadDone();snap.chat=1;for(int n=0;n<6;++n)step();check(events.empty()&&pending()==1);snap.chat=0;finish();check(events.size()==1);
 reset();step();unloadDone();held=true;for(int n=0;n<4;++n)step();check(events.empty());
 // Explicit Harvest/Return also increments the native input version, but the
 // existing gateway intentionally resumes ore assistance for that click. It
 // must still cancel THIS older unloading episode, including inventory rebuild.
 reset();step();manualVersions.change(11,Miner,true);manualVersions.resumeOre(11);step();check(!pending()&&events.empty());
 vectorAt(0x8B4108,0x20084000,2);set<uint32_t>(0x20084004,Refinery);step();check(!pending()&&events.empty());unloadDone();for(int n=0;n<4;++n)step();check(events.empty());
 reset();cells.clear();resource(151,62);step();unloadDone();step();check(pending()==1&&events.empty());manualVersions.change(11,Miner,true);manualVersions.resumeOre(11);step();check(!pending()&&events.empty());
 for(int queued:{5,12,16}){reset();step();unloadDone();set<int>(Miner+0xB4,queued);step();check(events.empty()&&!pending());}
 // Bound work and prove exact Euclidean ordering rather than first square-ring
 // hit. A diagonal at radius 4 loses to an axial resource at radius 5.
 reset();cells.clear();resource(55,66);resource(56,62);step();unloadDone();finish();check(events.size()==1&&events[0].c==62056);
 reset();cells.clear();resource(151,62);step();unloadDone();finish(200);check(events.size()==1&&events[0].c==62151);check(maxCellReads<4000);
 // Read-only observation precedes the heavy world's cadence. Stale planner
 // frames do not suppress unload detection, and it never scans map cells or
 // sends an order. Only the proven post-unload state asks for a full tick.
 reset();set<uint8_t>(Miner+0x6D1,0);set<float>(Miner+0x33C,20);planningWorld.frame=1;
 for(int n=0;n<100;++n){int frame=301+n;set<int>(0xA8ED84,frame);observeFrame(House,frame,clockNow+n);}
 check(!oreHarvestPending()&&!oreHarvestNeedsWork()&&observerCellReads==0&&events.empty());
 const unsigned idleReads=reads;set<uint8_t>(Miner+0x6D1,1);set<int>(0xA8ED84,401);observeFrame(House,401,1101);check(oreHarvestPending()&&!oreHarvestNeedsWork()&&observerCellReads==0&&events.empty());
 unloadDone();set<int>(0xA8ED84,402);observeFrame(House,402,1102);check(oreHarvestNeedsWork()&&observerCellReads==0&&events.empty());snap.frame=402;planningWorld.frame=402;clockNow=1102;finish();check(events.size()==1);
 // Inventory replacement discovers a new miner immediately, without waiting
 // for the next slow planningWorld scan or requiring a prior toolbox order.
 reset();vectorAt(0x8B4108,0x20084000,0);observeFrame(House,300,1000);check(!oreHarvestPending());vectorAt(0x8B4108,0x20084000,1);set<int>(0xA8ED84,301);observeFrame(House,301,1016);check(oreHarvestPending());
 // A last non-miner can be destroyed and replaced at the same allocation and
 // Unit-array slot between polls. Pointer/count/hash are unchanged, but a new
 // UniqueID/type must be discovered within the metadata revalidation bound.
 reset();set<int>(MinerType+0x800,0);observeFrame(House,300,1000);check(oreHarvestMinerCount==0&&!pending());
 set<uint32_t>(Miner+0x10,12);set<int>(MinerType+0x800,20);worldObjects.clear();worldObjects.add(Miner,12);
 for(int n=1;n<=30;++n){set<int>(0xA8ED84,300+n);observeFrame(House,300+n,DWORD(1000+n*16));}
 check(oreHarvestMinerCount==1&&pending()==1&&oreHarvestJobs[0].id==12&&events.empty());
 reset();set<int>(MinerType+0x800,0);observeFrame(House,300,1000);set<uint32_t>(Miner+0x10,12);set<int>(MinerType+0x800,20);worldObjects.clear();worldObjects.add(Miner,12);
 set<int>(0xA8ED84,301);observeFrame(House,301,1500);check(oreHarvestMinerCount==1&&pending()==1&&events.empty()); // slow-frame wall-clock bound
 reset();step();manualVersions.change(11,Miner,true);manualVersions.resumeOre(11);
 for(int n=0;n<32;++n)step();check(!pending()&&events.empty()); // periodic rebuild still cannot adopt the later token
 // Search-only counterexamples: uncertainty retries, budget exhaustion resumes,
 // and every coordinate in a ring is unique before reaching the nearest proof.
 oreharvest::Search search;search.begin(100,100);std::set<uint32_t>visited;int budget=3000;bool uncertain=true;
 search.advance(budget,[&](int x,int y){uint32_t id=uint32_t(y*1000+x);if(id==100101&&uncertain)return -1;assert(visited.insert(id).second);return x==105&&y==100?1:0;});check(!search.done);uncertain=false;budget=3000;search.advance(budget,[&](int x,int y){assert(visited.insert(uint32_t(y*1000+x)).second);return x==105&&y==100?1:0;});check(search.done&&search.best==100105);
 // Capture plus observer timing in private synthetic buffers. Windows builds
 // use same-process RPM for backed fields; Linux uses memcpy. The sparse map
 // remains synthetic in both modes. These costs do not measure game frame time.
 reset();set<uint8_t>(Miner+0x6D1,0);for(int n=0;n<128;++n){uint32_t p=0x200A0000+uint32_t(n)*0x800;object(p,0x7F5C70,uint32_t(1000+n),MinerType);set<uint32_t>(0x20084000+uint32_t(n+1)*4,p);set<uint32_t>(0x20086000+uint32_t(n+2)*4,p);}vectorAt(0x8B4108,0x20084000,129);vectorAt(0xA8EC78,0x20086000,130);
 observeFrame(House,300,1000);kernelReads=true;const unsigned benchmarkReads=reads;LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
 for(int n=0;n<100;++n){set<int>(0xA8ED84,301+n);observeFrame(House,301+n,DWORD(1016+n*16));}
 QueryPerformanceCounter(&end);double idleUs=double(end.QuadPart-begin.QuadPart)*1000000/frequency.QuadPart/100;
 check(events.empty()&&observerCellReads==0&&!oreHarvestNeedsWork()&&oreHarvestMinerCount==129);const unsigned rpmReads=reads-benchmarkReads;
 #ifdef _WIN32
 const char*transport="Win32_same_process_RPM_backed_fields_and_synthetic_map";
#else
 const char*transport="Linux_memcpy_synthetic_buffers_and_map";
#endif
 std::printf("{\"checks\":%u,\"assertions_active\":true,\"normal_reads\":%u,\"normal_max_cell_reads_per_frame\":%u,\"idle_100_frames_reads\":%u,\"idle_observer_map_cell_reads\":%llu,\"capture_and_observer_fixture\":{\"transport\":\"%s\",\"miners\":129,\"frames\":100,\"reads\":%u,\"microseconds_per_frame\":%.3f},\"sealed_frame_captures\":%llu,\"capture_reads\":%llu,\"capture_bytes\":%llu,\"capture_cell_reads\":%llu,\"sample_reads\":%llu,\"live_guard_reads\":%llu,\"pass\":true}\n",checks,normalReads,normalMaxCellReads,idleReads,(unsigned long long)observerCellReads,transport,rpmReads,idleUs,(unsigned long long)fixtureCaptureCount,(unsigned long long)fixtureCaptureReads,(unsigned long long)fixtureCaptureBytes,(unsigned long long)fixtureCaptureCellReads,(unsigned long long)fixtureSampleReads,(unsigned long long)fixtureLiveReads);delete sh;
}

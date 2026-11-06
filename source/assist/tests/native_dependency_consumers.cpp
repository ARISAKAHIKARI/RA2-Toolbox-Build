// Reuse the real native admission and synthetic Win32 boundary from its
// focused fixture. The consumer functions below are extracted, not copied.
#define snap startupSnap
#define main nativeEntryFixtureMain
#include "native_entry_runtime.cpp"
#undef main
#undef snap
#include <algorithm>
#include <type_traits>
#include "../source/constructor_adapter.h"
#include "../source/entry_gate.h"
#include "../source/order_policy.h"
#include "../source/command_gateway.h"
#include "../source/sidebar_policy.h"
#include "../source/technology_mode_policy.h"
using namespace assist;
constexpr uint32_t House=0x110000,Unit=0x120000,Type=0x130000,Tactical=0x140000,
 Super=0x150000,SuperType=0x160000,Factory=0x170000,TypeTable=0x180000;
constexpr unsigned Capacity=1,AllTransports=2,Iron=4,Psychic=8,Crates=16,Production=32;
constexpr int CombatSelection=1,EmptyTransports=2,IdleHarvester=10;
constexpr int MaxLabels=16,MaxThreats=16,MaxCrates=16,MaxSW=16,MaxTypes=16;
struct POINT {int x=0,y=0;};
struct TypeInfo {int kind=40,index=0;bool naval=false;};
struct Label {uint32_t id=0;int x=0,y=0,used=0,capacity=0,iron=0;bool temporal=false;int advanced=0;};
struct Threat {uint32_t source=0,target=0;int x=0,y=0,tx=0,ty=0;};
struct Crate {int cellX=0,cellY=0,x=0,y=0,preview=0;};
struct SuperInfo {char id[24]{};wchar_t name[40]{};int index=0,ready=0,frames=0;};
struct Snapshot {
 int status=1,lastSubmitError=0,commandRejected=0,labels=0,selected=0,transports=0,used=0,capacity=0,
 threatCount=0,crateCount=0,frame=0,width=0,height=0,credits=0,swCount=0,typeCount=1;
 Label label[MaxLabels];Threat threats[MaxThreats];Crate crates[MaxCrates];SuperInfo supers[MaxSW];TypeInfo types[MaxTypes];
}snap;
struct Shared {
 bool stop=false,externalDisplay=false;unsigned flags=0;
 int productionRequest=0,productionAck=0,productionCount=0;uint32_t productionMatch=0,productionTick=0;int productionTypes[4]{};
}storage,*sh=&storage;
static bool fault=false,missileCleanup=false,iconPresent=false;
static uint32_t match=1,slow=0,lastIdle[3]{},typePointers[MaxTypes]{};
static uint8_t techAllowed[MaxTypes]{};
static unsigned techImplementation=0,nativeCount=0;
static int unselectCalls=0,selectionCalls=0,enqueued=0,receipts=0,productionScans=0,redraws=0;
static std::vector<uint32_t> nativeCalls;
static std::map<uint32_t,uint32_t> nativeResults;
static std::map<uint32_t,std::vector<uint32_t>> vectors;
static std::map<uint32_t,int> kinds;
template<class T>static T get(uint32_t at){T result{};read(at,&result,sizeof result);return result;}
struct Vec {uint32_t address=0;int count=0;};
static Vec vec(uint32_t at,int limit=16384){auto&v=vectors[at];return {at,std::min(limit,int(v.size()))};}
static uint32_t item(Vec v,int i){return i>=0&&i<v.count?vectors[v.address][i]:0;}
static bool own(uint32_t p,uint32_t house){return p&&get<uint32_t>(p+0x21C)==house;}
static int kind(uint32_t p){auto i=kinds.find(p);return i==kinds.end()?-1:i->second;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
static bool live(uint32_t p){return kind(p)>=0;}
static bool idleMission(int,int,bool,bool){return true;}
template<class R,class... Args>static R call(uint32_t address,uint32_t,Args...){
 nativeCalls.push_back(address);if(address==0x6A6300)iconPresent=true;
 if constexpr(!std::is_void_v<R>)return R(nativeResults[address]);
}
template<class R,class... Args>static R callFast(uint32_t address,Args...args){return call<R>(address,0,args...);}
static bool selection(uint32_t){++selectionCalls;return true;}
static void unselect(){++unselectCalls;}
static void MemoryBarrier(){}
static int remaining(int,int duration,int frame){return std::max(0,duration-frame);}
static uint32_t sampleClock(Shared*){return clockMs;}
static void name(uint32_t,wchar_t*dst,int size){if(size)dst[0]=0;}
static void clearFlashes(){}
static void flashCameo(int,int){}
static void scanProduction(uint32_t,int,uint32_t){++productionScans;}
static int orderIndex(uint32_t){return -1;}
struct {int stopped=0;}nativeOrders[1];
struct CrateCache {int cellX=0,cellY=0,coords[3]{},preview=0;};
static CrateCache crateCache[MaxCrates];static int crateCount=0;static uint32_t crateTick=0;static bool cratesOn=false;
enum class OrderAuthority {Local};
struct NativePlacementSubmission{};
using Event=vanilla::Packet;
static bool validateMoSubmission(const intent::EventOrder&){return true;}
static bool authorizePlayerOrder(const intent::EventOrder&,OrderAuthority){return true;}
static bool validateNativePlacement(const intent::EventOrder&,const NativePlacementSubmission*){return true;}
static entrygate::Snapshot currentEntryBoundary(){return {1,1,0,true,false,true,true};}
static bool enqueue(const Event&,const intent::EventOrder&,OrderAuthority,const NativePlacementSubmission*){++enqueued;return true;}
static taskarbiter::FrameArbiter<16> actorFrameArbiter;
static gateway::MissionObserver<16> missionObserver;
static void recordReceipt(const gateway::Receipt&){++receipts;}
#include CONSUMER_SUBMIT
#include CONSUMER_SELECTION
#include CONSUMER_SUPER
#include "../source/superweapon_sidebar_bridge.h"
#include CONSUMER_DISPLAY
static constexpr uint32_t Sidebar=0x87F7E8,Tabs=Sidebar+0x1544;
static bool techGate(uint32_t,uint32_t){return false;}
static bool techParameterReservationAllowed(uint32_t,uint32_t){return false;}
static bool hasCameo(int,int){return iconPresent;}
static void sidebarRedraw(int){++redraws;}
#include CONSUMER_SIDEBAR

static void setup(){
 seed(false);snap={};storage={};match=1;slow=0;fault=missileCleanup=iconPresent=false;
 nativeCalls.clear();nativeResults.clear();vectors.clear();kinds.clear();
 unselectCalls=selectionCalls=enqueued=receipts=productionScans=redraws=0;
 actorFrameArbiter.clear();missionObserver.clear();lastIdle[0]=77;lastIdle[1]=lastIdle[2]=0;
 techImplementation=nativeCount=0;for(auto&v:typePointers)v=0;for(auto&v:techAllowed)v=0;
 typePointers[0]=Type;techAllowed[0]=1;cratesOn=false;crateCount=0;crateTick=0;
 vectors[0xA8EC78]={Unit};vectors[0xA8ECB8]={Unit};vectors[House+0x254]={Super};kinds[Unit]=2;
 put<uint32_t>(Unit+0x21C,House);put<uint32_t>(Unit+0x6C4,Type);put<uint32_t>(Unit+0x10,10);
 put<bool>(Unit+0x83,true);put<int>(Unit+0x6C,50);put<int>(Type+0xA0,100);put<bool>(Type+0xDBC,true);
 put<int>(Type+0x5E0,8);put<int>(Unit+0x194,20);put<int>(House+0x30C,1234);
 put<uint32_t>(0x887324,Tactical);put<int>(0xA8ED84,10);put<int>(0x8A00A4,1280);put<int>(0x8A00A8,720);
 put<uint32_t>(Super+0x2C,House);put<bool>(Super+0x6D,true);put<uint32_t>(Super+0x28,SuperType);
 put<int>(SuperType+0xBC,1);put<int>(SuperType+0x98,0);put<int>(0x8809A0,3);
 put<uint32_t>(0x88098C,0x1234);put<uint32_t>(0x880990,0x5678);put<int>(0x880994,2);
 put<uint32_t>(Type,TypeTable);put<uint32_t>(TypeTable+0x94,0x5F7900);
 for(const auto&s:Signatures)nativeResults[s.address]=1;
 nativeResults[0x473460]=3;nativeResults[0x6ABC60]=0;nativeResults[0x5F7900]=Factory;
}
static void rejectEntry(uint32_t address){memory[address]^=1;}
static bool called(uint32_t address){return std::find(nativeCalls.begin(),nativeCalls.end(),address)!=nativeCalls.end();}
static bool cursorCalled(){return called(0x4A8BF0)||called(0x4AC8C0)||called(0x4AC660);}
static void constructorChecks(){
 const intent::EventOrder orders[]={intent::object(0,0x15,10),intent::mega(0,10,1,20,0x34,0,0,0,0),
  intent::produce(0,0x0E,40,0,0),intent::superWeapon(0,0x12,0,{0,0})};
 for(const auto&order:orders){
  setup();const uint32_t address=vanilla::address(order.domain);rejectEntry(address);const auto before=memory;
  check(!submitConstructed(order));check(snap.lastSubmitError==8&&snap.commandRejected==1&&!enqueued&&nativeCalls.empty());check(memory==before);
  for(const auto&other:orders)if(other.domain!=order.domain){check(submitConstructed(other));check(called(vanilla::address(other.domain)));}
  check(enqueued==3);
  setup();check(submitConstructed(order)&&enqueued==1&&called(address)&&snap.lastSubmitError==0);
 }
}
static void superChecks(){
 for(uint32_t address:{0x6CC360u,0x4A8BF0u,0x4AC8C0u,0x4AC660u,0x48DC90u}){
  setup();rejectEntry(address);const auto before=memory;
  check(superSelect(House,0)==-12);check(memory==before&&!unselectCalls&&!cursorCalled()&&!enqueued&&!receipts);
  if(address==0x6CC360)check(nativeCalls.empty());else check(called(0x6CC360));
 }
 setup();check(superSelect(House,0)==1);check(unselectCalls==1&&cursorCalled()&&get<int>(0x8809A0)==0&&!enqueued);
 // A non-targeted SW needs no cursor helpers or deselection.
 for(uint32_t address:{0x4A8BF0u,0x4AC8C0u,0x4AC660u,0x48DC90u}){
  setup();rejectEntry(address);put<int>(SuperType+0xBC,0);const auto before=memory;
  check(superSelect(House,0)==1);check(enqueued==1&&receipts==1&&!cursorCalled()&&!unselectCalls&&memory==before);
 }
 setup();put<int>(SuperType+0xBC,0);rejectEntry(0x4C6B60);const auto before=memory;
 check(superSelect(House,0)==-6);check(!enqueued&&!receipts&&!called(0x4C6B60)&&memory==before);
 setup();rejectEntry(0x4A8BF0);const auto armed=memory;
 check(cancelSuperSidebarSelection()==-12);check(memory==armed&&nativeCalls.empty());
 put<int>(0x8809A0,-1);check(cancelSuperSidebarSelection()==0&&nativeCalls.empty());
 setup();check(cancelSuperSidebarSelection()==1&&get<int>(0x8809A0)==-1&&called(0x4A8BF0));
}
static void selectionChecks(){
 for(uint32_t address:{0x48DC90u,0x6FBFA0u}){
  setup();rejectEntry(address);check(selectFilter(House,CombatSelection)==-12);check(!unselectCalls&&!selectionCalls&&nativeCalls.empty());
 }
 setup();check(selectFilter(House,CombatSelection)==1&&unselectCalls==1&&selectionCalls==1);
 for(uint32_t address:{0x48DC90u,0x6FBFA0u,0x6D2420u}){
  setup();put<bool>(Type+0xE0E,true);rejectEntry(address);const auto before=memory;
  check(findIdle(House,IdleHarvester)==-12);check(lastIdle[0]==77&&!unselectCalls&&!selectionCalls&&nativeCalls.empty()&&memory==before);
 }
 setup();put<bool>(Type+0xE0E,true);check(findIdle(House,IdleHarvester)==1);
 check(lastIdle[0]==10&&unselectCalls==1&&selectionCalls==1&&called(0x6D2420));
 setup();put<bool>(Type+0xE0E,true);put<uint32_t>(0x887324,0);rejectEntry(0x6D2420);
 check(findIdle(House,IdleHarvester)==1&&unselectCalls==1&&selectionCalls==1&&!called(0x6D2420));
}
static void displayChecks(){
 setup();storage.flags=Capacity|Iron;scan(House,10,clockMs);
 check(snap.transports==1&&snap.used==3&&snap.capacity==8&&snap.labels==1&&snap.label[0].capacity==8);
 setup();storage.flags=Capacity|Iron;rejectEntry(0x473460);scan(House,10,clockMs);
 check(!called(0x473460)&&snap.transports==0&&snap.labels==1&&snap.label[0].capacity==0&&snap.label[0].iron==10);
 check(snap.credits==1234&&snap.frame==10&&snap.width==1280);
 setup();storage.flags=Capacity|Iron;rejectEntry(0x6D2140);scan(House,10,clockMs);
 check(!called(0x6D2140)&&snap.labels==0&&snap.transports==1&&snap.used==3&&snap.credits==1234);
 setup();rejectEntry(0x473460);rejectEntry(0x6D2140);scan(House,10,clockMs);
 check(nativeCalls.empty()&&snap.selected==1&&snap.credits==1234);
 setup();rejectEntry(0x6CC360);check(scanSummary(House,10,clockMs,0));
 check(snap.swCount==0&&productionScans==1&&!called(0x6CC360));
 setup();storage.externalDisplay=true;rejectEntry(0x6CC360);const unsigned before=readCalls;
 check(scanSummary(House,10,clockMs,0)&&readCalls==before&&nativeCalls.empty());
 for(uint32_t address:{0x578080u,0x6D2140u}){
  setup();rejectEntry(address);crateCount=2;cratesOn=true;scanCrates(Tactical,10,clockMs,Crates);
  check(!crateCount&&!cratesOn&&!snap.crateCount&&nativeCalls.empty());
 }
}
static void sidebarChecks(){
 setup();check(reservationCanBuild(Type,House)&&called(0x4F7870));
 setup();rejectEntry(0x4F7870);check(!reservationCanBuild(Type,House)&&nativeCalls.empty());
 setup();rejectEntry(0x6ABC60);check(typeTab(0)==-1&&nativeCalls.empty());
 setup();rejectEntry(0x5F7900);check(!techFactory(Type,House)&&nativeCalls.empty());
 setup();check(ensureTechCameo(0,House)&&iconPresent&&redraws==1&&called(0x5F7900)&&called(0x6ABC60)&&called(0x6A6300));
 for(uint32_t address:{0x5F7900u,0x6ABC60u,0x6A6300u}){
  setup();rejectEntry(address);const auto before=memory;
  check(!ensureTechCameo(0,House)&&!iconPresent&&!redraws&&!called(address));check(memory==before);
  check(!techNativeDependencies(0));check(techNativeDependencies(2));
 }
 setup();techImplementation=3;rejectEntry(0x4F7870);check(!ensureTechCameo(0,House)&&nativeCalls.empty()&&!techNativeDependencies(3));
 // An already present cameo does not need AddCameo or tab lookup.
 setup();iconPresent=true;rejectEntry(0x6A6300);check(ensureTechCameo(0,House)&&!called(0x6A6300)&&!redraws);
}
int main(){
 checks=0;constructorChecks();superChecks();selectionChecks();displayChecks();sidebarChecks();
 std::printf("PASS: %u extracted consumer checks; constructor pre-fill admission, atomic SW/selection rejection, independent overlays and sidebar; synthetic calls/memory only\n",checks);
 return 0;
}

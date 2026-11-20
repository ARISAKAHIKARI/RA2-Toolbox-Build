// Source-extracted smartUnloadTick with controlled native read failures.
// Tests real cancellation/progression code, not only the observation helper.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "../source/native_orders.h"
#include "../source/unload_compute.h"
#include "../source/unload_policy.h"
#include "../source/unload_followup_state.h"
using namespace assist;
static std::unordered_map<uint32_t,uint8_t> memory;
static uint32_t bad=0,match=1,House=0x770000,Type=0x440000;
static int checked=0,doneCount=0,observedCount=0,frameNow=100,indexedFrame=100,unloadCursor=0;
static bool commandRosterReady(){return indexedFrame==frameNow;}
static int unloadCount=0;
static unsigned rawCalls=0;
constexpr uint32_t SmartUnloadOrders=1, EjectAssist=2;
struct Shared {uint32_t flags=SmartUnloadOrders|EjectAssist;int computeApplied=0;};
static Shared shared,*sh=&shared;
struct Snapshot {int ejectWatched=0;};static Snapshot snap;
struct Versions {
 std::unordered_map<uint32_t,uint32_t> versions;
 uint32_t get(uint32_t id)const{const auto f=versions.find(id);return f==versions.end()?0:f->second;}
}manualVersions;
struct Index {
 std::unordered_map<uint32_t,uint32_t> idToPointer;
 bool contains(uint32_t p)const{for(const auto&v:idToPointer)if(v.second==p)return true;return false;}
 uint32_t byId(uint32_t id)const{const auto f=idToPointer.find(id);return f==idToPointer.end()?0:f->second;}
}worldObjects;
struct FeatureFilter {bool custom=false;};
struct FeatureConfiguration {FeatureFilter filters[12];};
static FeatureConfiguration featureConfig;
namespace featuretypes {enum Feature{UnloadPassenger=1,UnloadTransport=2,EscapePassenger=3,EscapeTransport=4};}
static bool refreshFeatureTypes(){return true;}
static bool featureTypeAllowed(featuretypes::Feature,uint32_t){return true;}
template<class T>static void put(uint32_t at,T v){
 const uint8_t*src=reinterpret_cast<const uint8_t*>(&v);
 for(size_t n=0;n<sizeof v;++n)memory[at+uint32_t(n)]=src[n];
}
static bool read(uint32_t at,void*out,size_t n){
 ++rawCalls;
 if(bad&&uint64_t(at)<=bad&&uint64_t(at)+n>bad){std::memset(out,0xCC,n);return false;}
 auto*dest=static_cast<uint8_t*>(out);
 for(size_t i=0;i<n;++i){
  const auto f=memory.find(at+uint32_t(i));
  if(f==memory.end())return false;
  dest[i]=f->second;
 }
 return true;
}
static uint32_t sampleClock(Shared*){return uint32_t(frameNow*16);}
static int outgoingRoom(){return 8;}
namespace taskarbiter {enum class Owner {Unload};}
template<class... Args>static bool nativeMission(Args...){return true;}
template<class T,class...Args>static T call(uint32_t,Args...){return T{};}
struct UnloadPassenger : unloadcompute::State {
 uint32_t p=0,id=0,carrier=0,carrierID=0,carrierType=0,followTarget=0,followTargetPointer=0,job=0;
 int frame=0,mode=0,landedFrame=0;
 uint32_t followTicket=0,followQueuedAt=0,manualToken=0;
 bool cancelled=false,followObserved=false;
};
static UnloadPassenger unloadPassengers[16];
static int budgetUsed=0;
static void finishUnloadFollowup(const UnloadPassenger&r,int){
 ++doneCount;if(r.followObserved)++observedCount;
}
static bool externalInputFrame=false;
static unloadcompute::Input unloadExternalInput,unloadLocalInput;
namespace compute{enum Lane{Unload,Escape};static bool inside(int,Lane){return false;}}
struct Vec {};
static Vec vec(uint32_t){return {};}
static bool nativeMember(Vec,uint32_t p){return worldObjects.contains(p);}
static void watchEjections(uint32_t,int,Vec){}
static void serviceUnloadCalculation(uint32_t,int,int&){}
static void publishUnloadCalculation(){}
static bool collectUnloadSource(unloadcompute::Input&,const UnloadPassenger&,uint32_t,int){return false;}
static bool applyUnloadDecision(UnloadPassenger&,const unloadcompute::Input&,int,const unloadcompute::Decision&,uint32_t,int,int&,bool){return false;}
#include UNLOAD_TICK_HEADER
static void check(bool pass,const char*label){
 ++checked;if(!pass){std::fprintf(stderr,"FAIL %s\n",label);std::abort();}
}
static void spawn(uint32_t pointer,uint32_t uid,int mission,bool loaded=false){
 for(unsigned n=0;n<0x6C8;++n)memory[pointer+n]=0;
 for(unsigned n=0;n<0xEC9;++n)memory[Type+n]=0;
 put<uint32_t>(pointer,0x7EB058);
 put<uint32_t>(pointer+0x10,uid);put<int>(pointer+0x6C,100);
 put<uint8_t>(pointer+0x74,!loaded);put<uint8_t>(pointer+0x81,loaded);put<uint8_t>(pointer+0x90,!loaded);
 put<int>(pointer+0x9C,1000+int(uid));put<int>(pointer+0xA0,2000+int(uid));
 put<int>(pointer+0xAC,mission);put<int>(pointer+0xB4,-1);
 put<uint32_t>(pointer+0x11C,loaded?0x600000:0);
 put<uint32_t>(pointer+0x21C,House);put<uint32_t>(pointer+0x2B4,0);
 put<uint32_t>(pointer+0x5A4,0);put<uint32_t>(pointer+0x6C0,Type);
 worldObjects.idToPointer[uid]=pointer;
}
static void push(uint32_t pointer,uint32_t uid,int stage,int mode=0){
 auto&r=unloadPassengers[unloadCount++];r={};r.p=pointer;r.id=uid;r.job=uid;
 r.carrier=0x600000;r.carrierID=8;r.carrierType=Type;r.manualToken=manualVersions.get(uid);
 r.frame=10;r.landedFrame=20;r.mode=mode;r.stage=stage;r.sentFrame=20;r.stableSince=20;
 r.x=700;r.y=800;
}
static void reset(){
 memory.clear();worldObjects.idToPointer.clear();manualVersions.versions.clear();
 shared={};featureConfig={};bad=0;frameNow=indexedFrame=100;
 unloadCount=unloadCursor=doneCount=observedCount=budgetUsed=0;rawCalls=0;
}
static void tick(int frame){
 frameNow=indexedFrame=frame;int budget=8;smartUnloadTick(House,frame,budget);
 budgetUsed+=8-budget;
}
int main(){
 reset();spawn(0x210000,101,5);spawn(0x220000,102,13);
 push(0x210000,101,3);push(0x220000,102,3);
 bad=0x210000+0xAC;tick(100);
 check(unloadCount==1&&unloadPassengers[0].id==101&&doneCount==1,
       "missing first passenger should not stop second passenger's own followup");
 check(unloadPassengers[0].stage==3&&unloadPassengers[0].retries==0&&
       unloadPassengers[0].stableSince==20&&unloadPassengers[0].x==700,
       "unreadable mission advanced the first followup checkpoint");
 bad=0;put<int>(0x210000+0xAC,13);tick(101);
 check(unloadCount==0&&doneCount==2,"recovered first passenger did not retire separately");

 reset();spawn(0x210000,101,5);push(0x210000,101,3);
 const uint32_t target=0x310000;worldObjects.idToPointer[901]=target;
 unloadPassengers[0].followTarget=901;unloadPassengers[0].followTargetPointer=target;
 put<uint32_t>(target+0x21C,House);
 bad=target+0x21C;tick(100);
 check(unloadCount==1&&doneCount==0&&unloadPassengers[0].retries==0&&
       unloadPassengers[0].x==700&&!unloadPassengers[0].followObserved,
       "missing target owner advanced follow receipt or motion");
 bad=0;tick(101);
 check(unloadCount==0&&doneCount==1&&observedCount==1,
       "confirmed same-owner follow target did not observe and finish");

 reset();spawn(0x210000,101,0);push(0x210000,101,1);
 bad=0x210000+0x9C;tick(100);
 check(unloadCount==1&&!unloadPassengers[0].sleepObserved,
       "missing position incorrectly observed native Sleep");
 bad=0;tick(101);
 check(unloadCount==1&&unloadPassengers[0].sleepObserved,
       "confirmed mission 0 did not update native Sleep checkpoint");
 manualVersions.versions[101]=1;bad=0x210000+0xAC;tick(102);
 check(unloadCount==0&&doneCount==1,"manual command must supersede even during missing memory");

 reset();spawn(0x210000,101,5);push(0x210000,101,3);
 for(int f=100;f<114;++f){bad=0x210000+0xB4;tick(f);}
 check(unloadCount==1&&unloadPassengers[0].retries==0,
       "14 consecutive missing queued-mission reads consumed follow retries");
 bad=0;put<int>(0x210000+0xAC,13);tick(114);
 check(unloadCount==0&&doneCount==1,"resumed followup could not complete after consecutive missing pages");
 std::printf("PASS %d source-extracted smartUnloadTick multi-passenger, target, Sleep, manual and retry checks\n",checked);
}

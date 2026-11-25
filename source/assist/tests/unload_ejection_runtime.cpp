// Production EjectAssist enrollment in a controlled synthetic x86 world.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include "../source/unload_compute.h"
#include "../source/native_orders.h"
#include "../source/object_index.h"
using namespace assist;
constexpr uint32_t House=0x310000,Transport=0x440000,P1=0x550000,P2=0x560000,Type=0x770000;
constexpr uint32_t EjectAssist=1;
struct Shared{uint32_t flags=EjectAssist;}shared,*sh=&shared;
struct Snapshot{int ejectWatched=0;}snap;
struct UnloadPassenger:unloadcompute::State {
 uint32_t p=0,id=0,carrier=0,carrierID=0,carrierType=0,followTarget=0,followTargetPointer=0,job=0;
 int frame=0,mode=0,landedFrame=0;uint32_t followTicket=0,followQueuedAt=0,manualToken=0;
 bool cancelled=false,followObserved=false;
};
static UnloadPassenger unloadPassengers[32];
static int unloadCount=0;static uint32_t unloadJobSerial=0;
static std::unordered_map<uint32_t,uint8_t> memory;
static uint32_t bad=0;static int assertions=0;
static ObjectIndex worldObjects;
struct Watched{
 std::unordered_map<uint32_t,uint32_t> values;
 void clear(){values.clear();}
 void add(uint32_t p,uint32_t id){values[p]=id;}
 bool contains(uint32_t p)const{return values.count(p)!=0;}
}watchedPassengers;
struct Versions{uint32_t get(uint32_t)const{return 9;}}manualVersions;
struct Vec{int count=0;};
static std::vector<uint32_t> infantry;
static bool sampleComplete=true;
static bool runtimeDirectActions=false;
struct SampleReadScope {
 SampleReadScope(){sampleComplete=true;}
 explicit operator bool()const{return true;}
 bool complete()const{return sampleComplete;}
 void finish(){}
};
static bool sampleRoster(uint32_t,Vec&out){out.count=int(infantry.size());return true;}
static uint32_t item(Vec,int at){return infantry.at(size_t(at));}
static bool nativeMember(uint32_t p){return worldObjects.contains(p);}
namespace featuretypes{enum Feature{EscapePassenger,EscapeTransport};}
static bool featureObjectAllowed(featuretypes::Feature,uint32_t){return true;}
template<class T>static void put(uint32_t at,T value){
 const uint8_t*bytes=reinterpret_cast<const uint8_t*>(&value);
 for(size_t i=0;i<sizeof value;++i)memory[at+uint32_t(i)]=bytes[i];
}
static bool read(uint32_t at,void*out,size_t n){
 if(bad&&uint64_t(at)<=bad&&uint64_t(at)+n>bad){sampleComplete=false;std::memset(out,0xCC,n);return false;}
 auto*bytes=static_cast<uint8_t*>(out);
 for(size_t i=0;i<n;++i){const auto it=memory.find(at+uint32_t(i));if(it==memory.end())return false;bytes[i]=it->second;}
 return true;
}
#include EJECTION_SOURCE
static void check(bool value,const char*name){
 ++assertions;if(!value){std::fprintf(stderr,"FAIL %s\n",name);std::abort();}
}
static void setup(){
 memory.clear();worldObjects.clear();watchedPassengers.clear();infantry={P1,P2};
 unloadCount=0;unloadJobSerial=0;bad=0;shared={};snap={};
 for(uint32_t p:{P1,P2,Transport}){
  for(unsigned i=0;i<0x6C8;++i)memory[p+i]=0;
  put<uint32_t>(p+0x21C,House);
  put<uint32_t>(p+0x6C4,Type);
  put<uint32_t>(p+0x11C,p==Transport?0:Transport);
  worldObjects.add(p,p==Transport?88:p==P1?101:102);
 }
 put<uint32_t>(Transport,0x7F5C70);put<uint32_t>(Transport+0x10,88);
 put<int>(Transport+0x6C,100);put<uint8_t>(Transport+0x74,1);
 put<uint8_t>(Transport+0x90,1);put<uint8_t>(Transport+0x81,0);
 put<uint8_t>(Transport+0x83,1);
 for(uint32_t p:{P1,P2}){put<uint32_t>(p,0x7EB058);put<uint32_t>(p+0x10,p==P1?101:102);}
}
int main(){
 setup();bad=P2+0x10;watchEjections(House,10);
 check(unloadCount==0,"incomplete ejection sample must not partially enroll a passenger");
 bad=0;watchEjections(House,11);
 check(unloadCount==2&&unloadPassengers[0].id==101&&unloadPassengers[1].id==102,
       "complete recovered sample did not enroll both passengers atomically");
 watchEjections(House,12);
 check(unloadCount==2,"repeated watcher duplicated passenger jobs");

 setup();bad=Transport+0x10;watchEjections(House,10);
 check(unloadCount==0,"missing carrier UID created an invalid ejection ticket");
 bad=0;watchEjections(House,11);
 check(unloadCount==2,"restored carrier ID did not resume valid ejections");

 setup();bad=Transport+0x6C4;watchEjections(House,10);
 check(unloadCount==0,"missing carrier type created a task without type identity");
 bad=0;watchEjections(House,11);
 check(unloadCount==2&&unloadPassengers[0].carrierType==Type,
       "restored carrier type did not preserve typed ejections");

 setup();bad=P1+0x21C;watchEjections(House,10);
 check(unloadCount==0,"incomplete owner sample must not partially enroll passengers");
 bad=0;watchEjections(House,11);
 check(unloadCount==2&&unloadPassengers[0].id==101&&unloadPassengers[1].id==102,
       "recovered complete owner sample did not atomically enroll passengers");

 setup();runtimeDirectActions=true;watchEjections(House,10);
 check(unloadCount==2&&unloadPassengers[0].id==101&&unloadPassengers[1].id==102,"direct watcher lost passenger cohort");
 runtimeDirectActions=false;
 setup();sh->flags=0;watchEjections(House,10);
 check(unloadCount==0,"explicitly disabled EjectAssist must not enroll");
 std::printf("PASS %d exact EjectAssist native enrollment assertions\n",assertions);
}

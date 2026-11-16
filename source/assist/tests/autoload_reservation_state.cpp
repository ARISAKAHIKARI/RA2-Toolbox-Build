// Execute the production four-state boarding observer with controlled live reads.
// No native game process, packet queue, or Windows hook is involved.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <set>
#include "../source/autoload.h"
using namespace assist;
constexpr uint32_t Passenger=0x20010000, Carrier=0x20020000, Other=0x20030000, House=0x20040000;
static std::unordered_map<uint32_t,uint8_t> bytes;
static std::set<uint32_t> present;
static uint32_t bad=0;
static int allowedPassenger=1,allowedTransport=1;
static int reads=0,checks=0;
template<class T>static void put(uint32_t at,T value){
 const auto*raw=reinterpret_cast<const uint8_t*>(&value);
 for(size_t n=0;n<sizeof value;++n)bytes[at+uint32_t(n)]=raw[n];
}
static bool readBytes(uint32_t at,void*out,size_t n){
 ++reads;
 if(bad&&uint64_t(at)<=bad&&uint64_t(at)+n>bad){memset(out,0xCC,n);return false;}
 auto*dest=static_cast<uint8_t*>(out);
 for(size_t i=0;i<n;++i){auto it=bytes.find(at+uint32_t(i));if(it==bytes.end())return false;dest[i]=it->second;}
 return true;
}
static int allowed(uint32_t p,int kind,bool transport){
 assert((p==Carrier&&transport&&kind==2)||(p==Passenger&&!transport&&(kind==1||kind==2)));
 return transport?allowedTransport:allowedPassenger;
}
static void test(bool ok,const char*description){++checks;if(!ok){std::fprintf(stderr,"FAILED %s\n",description);std::abort();}}
static loading::Reservation make(int kind=1){
 bytes.clear();present={Passenger,Carrier};bad=0;allowedPassenger=allowedTransport=1;reads=0;
 put<uint32_t>(Passenger,kind==1?0x7EB058u:0x7F5C70u);
 put<uint32_t>(Carrier,0x7F5C70u);
 put<uint32_t>(Passenger+0x10,11);put<uint32_t>(Carrier+0x10,22);
 put<uint32_t>(Passenger+0x21C,House);put<uint32_t>(Carrier+0x21C,House);
 put<int>(Passenger+0x6C,100);put<int>(Carrier+0x6C,200);
 for(auto p:{Passenger,Carrier}){
  put<uint8_t>(p+0x90,1);put<uint8_t>(p+0x74,1);
  put<uint8_t>(p+0x81,0);put<uint8_t>(p+0x1C8,0);
 }
 put<uint32_t>(Passenger+0x11C,0);put<int>(Passenger+0xAC,2);put<int>(Passenger+0xB4,-1);
 put<uint32_t>(Passenger+0x2B4,0);put<uint32_t>(Passenger+0x5A4,Other);
 put<int>(Passenger+0x9C,1234);put<int>(Passenger+0xA0,5678);
 put<uint32_t>(Passenger+0x500,0);
 return {Passenger,11,Carrier,22,1,100};
}
static loading::ReservationView sample(const loading::Reservation&r,int frame=101){
 return loading::inspectReservation(r,House,frame,readBytes,
  [](uint32_t p){return present.count(p)!=0;},allowed);
}
int main(){
 auto r=make();auto v=sample(r);
 test(v.state==loading::ReservationState::Active&&v.x==1234&&v.y==5678&&v.mission.mission==2,"complete source");
 test(v.incoming==0,"no invented Enter destination");
 for(uint32_t offset:{0u,0x10u,0x21Cu,0x6Cu,0x90u,0x74u,0x81u,0x1C8u,
       0x11Cu,0xACu,0xB4u,0x2B4u,0x5A4u,0x9Cu,0xA0u}){
  bad=Passenger+offset;v=sample(r);
  test(v.state==loading::ReservationState::Unknown,"unreadable passenger field is unknown");
 }
 for(uint32_t offset:{0u,0x10u,0x21Cu,0x6Cu,0x90u,0x74u,0x81u,0x1C8u}){
  bad=Carrier+offset;v=sample(r);
  test(v.state==loading::ReservationState::Unknown,"unreadable carrier field is unknown");
 }
 bad=0;put<int>(Passenger+0xAC,7);put<int>(Passenger+0xB4,7);
 put<uint32_t>(Passenger+0x500,Carrier);
 v=sample(r);test(v.state==loading::ReservationState::Active&&v.incoming==Carrier,"complete queued Enter");
 bad=Passenger+0x500;v=sample(r);
 test(v.state==loading::ReservationState::Unknown,"missing queued target cannot become absent Enter");
 bad=0;put<uint32_t>(Passenger+0x11C,Carrier);present.erase(Passenger);
 v=sample(r);test(v.state==loading::ReservationState::Boarded,"boarded passenger may leave live roster");
 bad=Carrier+0x10;v=sample(r);test(v.state==loading::ReservationState::Unknown,"boarded confirmation needs carrier identity");
 r=make();put<uint32_t>(Passenger+0x10,99);
 test(sample(r).state==loading::ReservationState::Invalid,"source UID change retires old reservation");
 r=make();put<uint32_t>(Carrier+0x10,99);
 test(sample(r).state==loading::ReservationState::Invalid,"carrier UID reuse retires old reservation");
 r=make();put<uint32_t>(Passenger+0x11C,Other);
 test(sample(r).state==loading::ReservationState::Invalid,"passenger boarded other transport");
 r=make();put<uint32_t>(Passenger+0x21C,Other);
 test(sample(r).state==loading::ReservationState::Invalid,"explicitly different owner");
 r=make();present.erase(Passenger);
 test(sample(r).state==loading::ReservationState::Invalid,"complete roster proves disappearance");
 r=make();allowedPassenger=-1;
 test(sample(r).state==loading::ReservationState::Unknown,"filter loading is not explicit exclusion");
 r=make();allowedPassenger=0;
 test(sample(r).state==loading::ReservationState::Invalid,"explicit exclusion can retire");
 r=make(2);
 test(sample(r).state==loading::ReservationState::Active,"vehicle passenger uses passenger filter, not carrier filter");
 r=make();test(sample(r,99).state==loading::ReservationState::Unknown,"older frame cannot retire newer task");
 std::printf("PASS %d production boarding state-machine assertions (no live game)\n",checks);
}

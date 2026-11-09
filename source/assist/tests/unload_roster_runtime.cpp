// Execute the production observation function, with an explicit synthetic
// object-read boundary; compare every history byte with the pre-fix function.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <chrono>
#include <algorithm>
#include "../source/native_orders.h"
#include "../source/unload_policy.h"
using namespace assist;
using DWORD=uint32_t;
constexpr uint32_t SmartUnloadOrders=16384;
struct Shared {uint32_t flags=SmartUnloadOrders;};static Shared shared,*sh=&shared;
struct Snapshot {bool chat=false;};static Snapshot snap;
struct RecentPassenger {uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
struct Object {uint32_t p,id,carrier=0,owner=1,type=42;int kind=1;bool selected=false,allowed=true,alive=true;};
static std::unordered_map<uint32_t,Object> objects;
static std::vector<uint32_t> infantry;
struct Vec {int count=0;};static Vec vec(uint32_t at){if(at!=0xA83DE8)throw std::runtime_error("unexpected roster");return {int(infantry.size())};}
static uint32_t item(Vec,int i){return infantry.at(i);}
static Object* object(uint32_t p){auto at=objects.find(p);return at==objects.end()?nullptr:&at->second;}
template<class T>static T get(uint32_t at){uint32_t p=at&~0xFFFu;auto*o=object(p);if(!o)return T{};switch(at-p){case 0x10:return T(o->id);case 0x11C:return T(o->carrier);case 0x21C:return T(o->owner);case 0x83:return T(o->selected);default:throw std::runtime_error("unexpected field");}}
struct World {bool contains(uint32_t p){auto*o=object(p);return o&&o->alive;}};static World worldObjects;
struct Versions {std::unordered_map<uint32_t,uint32_t> values;uint32_t get(uint32_t id){return values[id];}};static Versions manualVersions;
static bool own(uint32_t p,uint32_t house){auto*o=object(p);return o&&o->alive&&o->owner==house;}
static int kind(uint32_t p){auto*o=object(p);return o?o->kind:-1;}
static uint32_t typeOf(uint32_t p,int){auto*o=object(p);return o?o->type:0;}
namespace featuretypes {constexpr int UnloadTransport=8,UnloadPassenger=9;}
static bool featureObjectAllowed(int,uint32_t p){auto*o=object(p);return o&&o->allowed;}
#include ROSTER_BEFORE
#include ROSTER_CURRENT
static uint32_t passenger(unsigned n){return 0x100000+n*0x1000;}
constexpr uint32_t carrier=0x80000000;
static void reset(unsigned n){objects.clear();infantry.clear();manualVersions.values.clear();recentPassengerCount=0;std::memset(recentPassengers,0,sizeof recentPassengers);snap={};shared={};objects[carrier]={carrier,50000,0,1,42,2,true};for(unsigned i=0;i<n;++i){uint32_t p=passenger(i);objects[p]={p,100+i,carrier};infantry.push_back(p);}}
using Observe=void(*)(uint32_t,int,DWORD);
struct History {std::vector<RecentPassenger> values;};
static History capture(){return {std::vector<RecentPassenger>(recentPassengers,recentPassengers+recentPassengerCount)};}
static void restore(const History&h){recentPassengerCount=int(h.values.size());std::copy(h.values.begin(),h.values.end(),recentPassengers);}
static unsigned cases=0,checks=0;
static void expect(bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);}
static void compare(History&before,History&after,int frame,DWORD now){restore(before);observeUnloadRosterBefore(1,frame,now);before=capture();restore(after);observeUnloadRoster(1,frame,now);after=capture();expect(before.values.size()==after.values.size(),"history count changed");expect(before.values.empty()||!std::memcmp(before.values.data(),after.values.data(),before.values.size()*sizeof(RecentPassenger)),"history identity/order/timestamp changed");}
template<class F>void run(const char*name,F f){f();++cases;std::printf("PASS\t%s\n",name);}
int main(){try{
 run("same_frame_selection_native_first_and_duplicate_roster",[]{reset(3);objects[carrier].selected=false;History b,a;compare(b,a,1,1000);expect(a.values.empty(),"not selected");objects[carrier].selected=true;compare(b,a,1,1001);expect(a.values.size()==3,"same frame selection must be observed");infantry.push_back(passenger(1));compare(b,a,1,1002);expect(a.values.size()==3,"duplicate vector entry does not append history");for(unsigned i=0;i<3;++i)objects[passenger(i)].carrier=0;compare(b,a,1,1003);expect(a.values.size()==3,"native unload before command preserves roster");});
 run("expiry_swap_deletion_identity_and_manual_token",[]{reset(4);History b,a;compare(b,a,10,1000);objects[passenger(0)].alive=false;objects[passenger(1)].id=900;manualVersions.values[102]=3;compare(b,a,11,1010);expect(a.values.size()==4,"loaded objects can refresh after identity/token revalidation");for(auto p:infantry)objects[p].carrier=0;compare(b,a,12,2511);expect(a.values.empty(),"time expiry prunes landed history");reset(4);b={};a={};compare(b,a,100,1000);for(auto p:infantry)objects[p].carrier=0;compare(b,a,191,1001);expect(a.values.empty(),"frame expiry");reset(4);b={};a={};compare(b,a,100,1000);for(auto p:infantry)objects[p].carrier=0;compare(b,a,99,1001);expect(a.values.empty(),"frame retreat");});
 run("disable_chat_and_type_owner_guards",[]{reset(5);objects[passenger(0)].allowed=false;objects[passenger(1)].owner=2;History b,a;compare(b,a,1,1000);expect(a.values.size()==3,"passenger filters");sh->flags=0;compare(b,a,1,1001);expect(a.values.empty(),"disable clears history");sh->flags=SmartUnloadOrders;compare(b,a,1,1002);snap.chat=true;compare(b,a,1,1003);expect(a.values.empty(),"chat clears history");snap.chat=false;objects[carrier].allowed=false;compare(b,a,1,1004);expect(a.values.empty(),"carrier filter");objects[carrier].allowed=true;objects[carrier].owner=2;compare(b,a,1,1005);expect(a.values.empty(),"carrier owner");});
 run("capacity_preserves_oldest_order_and_rebuilt_swapped_indices",[]{reset(nativeorders::MaxPassengers+1);History b,a;compare(b,a,1,1000);expect(a.values.size()==nativeorders::MaxPassengers&&a.values.back().p==passenger(nativeorders::MaxPassengers-1),"bounded overflow retains original order");for(unsigned i=0;i<nativeorders::MaxPassengers;i+=3){objects[passenger(i)].carrier=0;manualVersions.values[100+i]=1;}compare(b,a,2,1010);compare(b,a,2,1011);expect(a.values.size()<nativeorders::MaxPassengers,"swapped history rebuilt without stale slots");});
 run("lookup_collision_capacity_clear",[]{unloadpolicy::RosterLookup<8> index;std::vector<uint32_t> keys;for(uint32_t p=1;keys.size()<9;++p)if(index.slot(p)==0)keys.push_back(p);for(int n=0;n<8;++n)expect(index.add(keys[n],n),"collision insert");expect(!index.add(keys[8],8),"overflow rejected");for(int n=0;n<8;++n)expect(index.find(keys[n])==n,"collision find");expect(index.add(keys[3],99)&&index.find(keys[3])==99,"existing update at capacity");index.clear();for(auto p:keys)expect(index.find(p)==-1,"clear occupied slots");expect(index.add(keys[8],8)&&index.find(keys[8])==8,"reuse after clear");});
 reset(8192);observeUnloadRosterBefore(1,1,1000);auto start=capture();
 auto time=[&](Observe fn){restore(start);auto at=std::chrono::steady_clock::now();for(int n=0;n<7;++n)fn(1,1,1000+n);return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-at).count();};
 double old=time(observeUnloadRosterBefore),fresh=time(observeUnloadRoster);std::printf("BENCHMARK_MS\t%.3f\t%.3f\t8192\t7\n",old,fresh);
 std::printf("SUMMARY\t%u\t%u\n",cases,checks);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

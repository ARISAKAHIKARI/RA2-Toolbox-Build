#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "../source/native_orders.h"
#include "../source/unload_policy.h"
#include "../source/unload_admission.h"
using namespace assist;using DWORD=uint32_t;
constexpr uint32_t SmartUnloadOrders=1;
struct Shared{uint32_t flags=SmartUnloadOrders;}shared,*sh=&shared;
static uint32_t match=1,clockNow=100;static bool ready=true,interfaces=true;static int eventRoom=24,voices=0,cancelled=0;
static std::unordered_map<uint32_t,uint32_t> memory,ids,kinds,types,byID;
static std::vector<uint32_t> selectedObjects,infantry,allObjects,selectedResult,sent;
struct Versions{std::unordered_map<uint32_t,uint32_t> values;uint32_t get(uint32_t id){return values[id];}}manualVersions;
struct Objects{bool contains(uint32_t p){return ids.count(p)!=0;}}worldObjects;
static unloadadmission::Queue queuedUnload;
struct RecentPassenger{uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
struct UnloadPassenger{uint32_t job,p,id,carrier,carrierID,carrierType;int frame;uint32_t manualToken;};
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount;static uint32_t unloadJobSerial;
struct Vec{int count;const std::vector<uint32_t>*values;};
static Vec vec(uint32_t p,int maximum=16384){auto*v=p==0xA8ECB8?&selectedObjects:p==0xA83DE8?&infantry:&allObjects;return {v->size()<=unsigned(maximum)?int(v->size()):0,v};}
static uint32_t item(Vec v,int n){return v.values->at(n);}
template<class T>static T get(uint32_t p){return T(memory[p]);}
static bool own(uint32_t p,uint32_t house){return get<uint32_t>(p+0x21C)==house;}
static bool live(uint32_t p){return get<int>(p+0x6C)>0;}
static int kind(uint32_t p){return kinds.count(p)?int(kinds[p]):-1;}
static uint32_t typeOf(uint32_t p,int){return types[p];}
static uint32_t worldByID(uint32_t id){return byID[id];}
namespace featuretypes{constexpr int UnloadPassenger=1,UnloadTransport=2;}
static bool featureObjectAllowed(int,uint32_t){return true;}
static bool loadInterfaces(){return interfaces;}static bool simulationReady(){return ready;}
static uint32_t sampleClock(Shared*){return clockNow;}static int outgoingRoom(){return eventRoom;}
namespace taskarbiter{enum class Owner{Unload};}
static bool nativeMission(uint32_t,uint32_t id,int mission,uint32_t,uint32_t,int,uint32_t,int,taskarbiter::Owner){if(eventRoom<=0)return false;--eventRoom;sent.push_back(id);memory[byID[id]+0xAC]=uint32_t(mission);return true;}
static void cancelNative(uint32_t p){++cancelled;queuedUnload.cancel(p,get<uint32_t>(p+0x10));for(int i=0;i<unloadCount;)if(unloadPassengers[i].carrier==p||unloadPassengers[i].p==p)unloadPassengers[i]=unloadPassengers[--unloadCount];else ++i;}
#include CAPACITY_UNLOAD_FUNCTIONS
struct POINT{int x,y;};static int weaponCalls=0;
static bool nativeDependenciesReady(uint32_t,uint32_t){return true;}static bool nativeDependencyReady(uint32_t){return true;}
static void*GetModuleHandleW(const wchar_t*){return reinterpret_cast<void*>(1);}
template<class T,class...Args>static T call(uint32_t address,Args...){if(address==0x7177C0)++weaponCalls;return T(1);}
static void unselect(){selectedResult.clear();}static bool selection(uint32_t p){selectedResult.push_back(p);return true;}
#include CAPACITY_VARIANT_FUNCTIONS
static int checks=0,failures=0;
static void check(bool v,const char*label){++checks;if(!v){++failures;std::fprintf(stderr,"FAIL %s\n",label);}}
static void reset(){queuedUnload.clear();queuedUnload.epoch=queuedUnload.house=queuedUnload.lastProgress=0;memory.clear();ids.clear();kinds.clear();types.clear();byID.clear();manualVersions.values.clear();selectedObjects.clear();infantry.clear();allObjects.clear();selectedResult.clear();sent.clear();recentPassengerCount=unloadCount=0;match=1;clockNow=100;shared={};ready=interfaces=true;eventRoom=24;cancelled=weaponCalls=0;memory[0xA8ED84]=1;}
static uint32_t carrier(int n,bool passenger=true){uint32_t p=0x100000+n*0x1000,id=n+1;ids[p]=id;byID[id]=p;kinds[p]=2;types[p]=0x80000;memory[p+0x10]=id;memory[p+0x21C]=9;memory[p+0x6C]=100;memory[p+0x114]=passenger?1:0;selectedObjects.push_back(p);allObjects.push_back(p);
 if(passenger){uint32_t q=0x5000000+n*0x1000,qid=100000+n;ids[q]=qid;byID[qid]=q;kinds[q]=1;types[q]=0x90000;memory[q+0x10]=qid;memory[q+0x21C]=9;memory[q+0x6C]=100;memory[q+0x11C]=p;infantry.push_back(q);allObjects.push_back(q);}return p;}
static void tick(int frame,int room=24){memory[0xA8ED84]=frame;clockNow=100+uint32_t(frame)*16;eventRoom=room;int budget=64;smartUnloadAdmissionTick(9,frame,budget);}
int main(int argc,char**argv){std::string mode=argc>1?argv[1]:"all";
 if(mode!="variant"){
 reset();for(int n=0;n<1000;++n)carrier(n);eventRoom=0;
 check(smartUnload(9)==1000&&queuedUnload.count==1000&&sent.empty(),"full 1000-carrier cohort accepted while outgoing queue is full");
 selectedObjects.clear();carrier(4000); // later selection cannot join the original cohort
 manualVersions.values[51]=1;memory[0x100000+49*0x1000+0x6C]=0;
 tick(2);check(sent.size()==nativeorders::EventBudget&&eventRoom==24-nativeorders::EventBudget,"carrier admission leaves room for passenger follow-up");tick(2);check(sent.size()==nativeorders::EventBudget,"one admission pass per game frame");
 for(int f=3;f<300&&queuedUnload.count;++f)tick(f);
 std::unordered_set<uint32_t>unique(sent.begin(),sent.end());check(!queuedUnload.count&&sent.size()==998&&unique.size()==998,"all surviving queued carriers eventually submitted exactly once");
 check(!unique.count(4001)&&!unique.count(50)&&!unique.count(51),"frozen cohort, death and manual takeover respected");
 check(unloadCount==998,"passenger continuation installed for every accepted carrier");
 reset();auto p=carrier(0);smartUnload(9);cancelNative(p);tick(2);check(sent.empty()&&!queuedUnload.count,"manual cancellation removes queued carrier");
 reset();carrier(0);smartUnload(9);shared.flags=0;tick(2);check(sent.empty()&&!queuedUnload.count,"feature disable cancels unsent tasks");
 reset();carrier(0);smartUnload(9);++match;tick(2);check(sent.empty()&&!queuedUnload.count,"world change retires queued identities");
 reset();p=carrier(0);smartUnload(9);memory[p+0x10]=9999;tick(2);check(sent.empty()&&!queuedUnload.count,"pointer identity reuse cannot inherit queued task");
 reset();carrier(0);smartUnload(9);interfaces=false;tick(2);check(sent.empty()&&queuedUnload.count==1,"temporary interface loss retains intent");interfaces=true;tick(3);check(sent.size()==1&&!queuedUnload.count,"interface recovery delivers retained intent");
 reset();carrier(0);smartUnload(9);clockNow+=12001;int budget=64;smartUnloadAdmissionTick(9,2,budget);check(sent.empty()&&!queuedUnload.count,"stalled queue has bounded idle expiry");
 reset();p=carrier(0);smartUnload(9);check(smartUnload(9)==1&&queuedUnload.count==1,"repeated explicit request replaces duplicate identity");tick(2);check(sent.size()==1,"repeated request does not double Deploy");
 reset();p=carrier(0);memory[p+0xAC]=16;smartUnload(9);tick(2);check(sent.empty()&&unloadCount==1,"active native unload gains follow-up without another Deploy");
 reset();p=carrier(0,false);uint32_t q=0x5000000;ids[q]=100000;byID[100000]=q;kinds[q]=1;memory[q+0x10]=100000;memory[q+0x21C]=9;memory[q+0x6C]=100;recentPassengers[0]={q,100000,p,1,0x80000,0,100,1};recentPassengerCount=1;smartUnload(9);tick(2);check(sent.empty()&&unloadCount==1,"already unloaded roster gains follow-up without empty-carrier Deploy");
 reset();for(int n=0;n<unloadadmission::Capacity;++n)carrier(n);check(smartUnload(9)==unloadadmission::Capacity,"8192 carriers fit initial admission capacity");selectedObjects.clear();carrier(9000);check(smartUnload(9)==-42&&queuedUnload.count==unloadadmission::Capacity,"full queue rejects whole additional request without truncation");
 reset();for(int n=0;n<300;++n)carrier(n);smartUnload(9);tick(2);clockNow=5000;int cap=1;eventRoom=1;int waiting=queuedUnload.count;smartUnloadAdmissionTick(9,99,cap);check(queuedUnload.count==waiting&&eventRoom==1,"last event slot remains available to passenger follow-up");clockNow=10000;cap=1;eventRoom=2;smartUnloadAdmissionTick(9,100,cap);check(queuedUnload.count>0,"accepted progress refreshes queue idle horizon");clockNow=20001;eventRoom=2;cap=1;smartUnloadAdmissionTick(9,101,cap);check(queuedUnload.count>0,"large active queue survives more than original idle duration");
 }
 if(mode!="unload"){
 reset();for(int n=0;n<600;++n){auto p=carrier(n,false);types[p]=0x800000+uint32_t(n)*0x1000;}
 check(selectVariant(9,true)==600&&selectedResult.size()==600,"all 600 selected variants retained beyond old 512 cap");
 reset();for(int n=0;n<5000;++n)carrier(n,false);check(selectVariant(9,true)==5000,"live selection beyond old 4096 bound supported");
 reset();auto p=carrier(0,false);selectedObjects.clear();selectedObjects.push_back(p);auto other=carrier(1,false);selectedObjects.pop_back();types[other]=0x999000;memory[0x999000+0x805]=1;memory[0x999000+0x80C]=1;
 check(selectVariant(9,true)==1&&weaponCalls==0,"unselected types do not issue extra weapon queries");
 }
 std::printf("%s capacity admission: %d checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}

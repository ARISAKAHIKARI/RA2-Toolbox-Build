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
#include "../source/variant_keys.h"
#include "../source/type_hotkey.h"
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
static void finishUnloadFollowup(const UnloadPassenger&,int){}
static int indexedFrame=-1;
struct Vec{int count;const std::vector<uint32_t>*values;};
static Vec vec(uint32_t p,int maximum=16384){auto*v=p==0xA8ECB8?&selectedObjects:p==0xA83DE8?&infantry:&allObjects;return {v->size()<=unsigned(maximum)?int(v->size()):0,v};}
static uint32_t item(Vec v,int n){return v.values->at(n);}
// Extracted function fixture: the synthetic roster is already a complete
// immutable vector; production uses SampleReadScope + sampleRoster from
// tactical_frame_read.h and the real sealed Runtime world sample.
struct SampleReadScope {
 explicit operator bool() const{return true;}
 bool complete() const{return true;}
};
static bool sampleRoster(uint32_t address,Vec&out){out=vec(address);return out.count>=0;}

template<class T>static T get(uint32_t p){return T(memory[p]);}
static uint32_t unreadableField=0;
static bool read(uint32_t p,void*out,size_t size){
 if(unreadableField&&p==unreadableField)return false;
 if(size==1){const uint8_t v=get<uint8_t>(p);std::memcpy(out,&v,size);return true;}
 if(size==4){const uint32_t v=get<uint32_t>(p);std::memcpy(out,&v,size);return true;}
 return false;
}
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
static void reset(){queuedUnload.clear();queuedUnload.epoch=queuedUnload.house=queuedUnload.lastProgress=0;memory.clear();ids.clear();kinds.clear();types.clear();byID.clear();manualVersions.values.clear();selectedObjects.clear();infantry.clear();allObjects.clear();selectedResult.clear();sent.clear();recentPassengerCount=unloadCount=0;match=1;clockNow=100;shared={};ready=interfaces=true;eventRoom=24;cancelled=weaponCalls=0;unreadableField=0;indexedFrame=-1;memory[0xA8ED84]=1;}
static uint32_t carrier(int n,bool passenger=true){uint32_t p=0x100000+n*0x1000,id=n+1;ids[p]=id;byID[id]=p;kinds[p]=2;types[p]=0x80000;memory[p]=0x7F5C70;memory[p+0x10]=id;memory[p+0x21C]=9;memory[p+0x6C]=100;memory[p+0x74]=1;memory[p+0x90]=1;memory[p+0x81]=0;memory[p+0x1C8]=0;memory[p+0x114]=passenger?1:0;selectedObjects.push_back(p);allObjects.push_back(p);
 if(passenger){uint32_t q=0x5000000+n*0x1000,qid=100000+n;ids[q]=qid;byID[qid]=q;kinds[q]=1;types[q]=0x90000;memory[q]=0x7EB058;memory[q+0x10]=qid;memory[q+0x21C]=9;memory[q+0x6C]=100;memory[q+0x74]=1;memory[q+0x90]=1;memory[q+0x81]=0;memory[q+0x11C]=p;memory[p+0x118]=q;memory[q+0x30]=0;infantry.push_back(q);allObjects.push_back(q);}return p;}
static void tick(int frame,int room=24,bool candidate=false){indexedFrame=frame;memory[0xA8ED84]=frame;clockNow=100+uint32_t(frame)*16;eventRoom=room;int budget=64;smartUnloadAdmissionTick(9,frame,budget,candidate);}
int main(int argc,char**argv){std::string mode=argc>1?argv[1]:"all";
 if(mode!="variant"){
 reset();const uint32_t localCarrier=carrier(0),localPassenger=memory[localCarrier+0x118];
 check(smartUnload(9,true)==1&&queuedUnload.count==1,"candidate click freezes selected carrier without scanning infantry");
 unreadableField=localPassenger+0x30;
 tick(2,24,true);
 check(sent.empty()&&!unloadCount&&queuedUnload.count==1,"partial native cargo chain never deploys a partial cohort");
 unreadableField=0;tick(3,24,true);
 check(sent.size()==1&&unloadCount==1&&!queuedUnload.count,"complete native cargo chain admits between captures");
 tick(3);check(sent.size()==1,"strict path never replays candidate deploy");
 reset();const uint32_t alreadyUnloading=carrier(0);memory[alreadyUnloading+0xAC]=16;
 check(smartUnload(9,true)==1,"candidate keeps selected already-unloading carrier pending");
 tick(4,24,true);
 check(sent.empty()&&!unloadCount&&queuedUnload.count==1,"candidate does not treat a shrinking chain as the original cohort");
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
 reset();p=carrier(0);smartUnload(9);unreadableField=p+0x10;tick(2);
 check(sent.empty()&&queuedUnload.count==1,"unreadable carrier UID must not erase Ctrl+D");
 unreadableField=0;tick(3);check(sent.size()==1&&!queuedUnload.count,"recovered UID did not resume original Ctrl+D");
 reset();p=carrier(0);smartUnload(9);unreadableField=p+0x114;tick(2);
 check(sent.empty()&&queuedUnload.count==1,"unreadable cargo count must not erase Ctrl+D");
 unreadableField=0;tick(3);check(sent.size()==1&&!queuedUnload.count,"recovered cargo state did not resume original Ctrl+D");
 reset();p=carrier(0);smartUnload(9);uint32_t probePassenger=memory[p+0x118];
 unreadableField=probePassenger+0x10;tick(2);
 check(sent.empty()&&queuedUnload.count==1&&unloadCount==0&&!queuedUnload.carriers[0].attempted,
       "missing passenger UID burned native Deploy or committed partial Ctrl+D");
 tick(300);check(sent.empty()&&queuedUnload.count==1&&!queuedUnload.carriers[0].attempted,
       "unreadable passenger for 4.8 seconds exhausted an unstarted Ctrl+D");
 unreadableField=0;tick(301);check(sent.size()==1&&unloadCount==1,
       "recovered passenger UID did not resume original first Deploy");
 reset();p=carrier(0);smartUnload(9);probePassenger=memory[p+0x118];
 unreadableField=probePassenger+0x30;tick(2);
 check(sent.empty()&&queuedUnload.count==1&&unloadCount==0,"missing cargo next link committed incomplete roster");
 unreadableField=0;tick(3);check(sent.size()==1&&unloadCount==1,"recovered cargo link did not resume Ctrl+D");
 reset();carrier(0);smartUnload(9);interfaces=false;clockNow+=12001;int budget=64;smartUnloadAdmissionTick(9,2,budget);
 check(sent.empty()&&queuedUnload.count==1,"temporarily unavailable entry must preserve original Ctrl+D despite elapsed time");
 interfaces=true;tick(3);check(sent.size()==1&&unloadCount==1,"recovered entry must deliver retained Ctrl+D");
 reset();p=carrier(0);smartUnload(9);check(smartUnload(9)==1&&queuedUnload.count==1,"repeated explicit request replaces duplicate identity");tick(2);check(sent.size()==1,"repeated request does not double Deploy");
 reset();p=carrier(0);memory[p+0xAC]=16;memory[p+0x1C8]=1;smartUnload(9);tick(2);check(sent.empty()&&unloadCount==1,"active native unload gains follow-up without another Deploy");
 reset();p=carrier(0,false);uint32_t q=0x5000000;ids[q]=100000;byID[100000]=q;kinds[q]=1;memory[q]=0x7EB058;memory[q+0x10]=100000;memory[q+0x21C]=9;memory[q+0x6C]=100;memory[q+0x74]=1;memory[q+0x90]=1;recentPassengers[0]={q,100000,p,1,0x80000,0,100,1};recentPassengerCount=1;smartUnload(9);tick(2);check(sent.empty()&&unloadCount==1,"already unloaded roster gains follow-up without empty-carrier Deploy");
 reset();for(int n=0;n<unloadadmission::Capacity;++n)carrier(n);check(smartUnload(9)==unloadadmission::Capacity,"8192 carriers fit initial admission capacity");selectedObjects.clear();carrier(9000);check(smartUnload(9)==-42&&queuedUnload.count==unloadadmission::Capacity,"full queue rejects whole additional request without truncation");
 reset();for(int n=0;n<300;++n)carrier(n);smartUnload(9);tick(2);clockNow=5000;int cap=1;eventRoom=1;int waiting=queuedUnload.count;indexedFrame=99;smartUnloadAdmissionTick(9,99,cap);check(queuedUnload.count==waiting&&eventRoom==1,"last event slot remains available to passenger follow-up");clockNow=10000;cap=1;eventRoom=2;indexedFrame=100;smartUnloadAdmissionTick(9,100,cap);check(queuedUnload.count>0,"accepted progress refreshes queue idle horizon");clockNow=20001;eventRoom=2;cap=1;indexedFrame=101;smartUnloadAdmissionTick(9,101,cap);check(queuedUnload.count>0,"large active queue survives more than original idle duration");
 }
 if(mode!="unload"){
 static typekeys::VariantKeys large;large.clear();for(uint32_t n=1;n<=16384;++n)large.add(n*16,n%57);bool complete=true;for(uint32_t n=1;n<=16384;++n)complete&=large.hasType(n*16)&&large.contains(n*16,n%57)&&!large.contains(n*16,99);check(complete,"all 16384 fixed-memory variant keys remain distinct");large.clear();check(large.empty()&&!large.contains(16,1),"fixed index resets across selections");large.epoch=UINT32_MAX;large.clear();large.add(16,2);check(large.contains(16,2)&&!large.contains(16,1),"fixed key generation wrap clears stale entries");
 check(selectVariant(9,true)==-12&&weaponCalls==0,"variant selection is explicitly suspended until Worker-backed variant projection exists");
 }
 std::printf("%s capacity admission: %d checks, %d failures\n",failures?"FAIL":"PASS",checks,failures);return failures?1:0;
}

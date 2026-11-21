// Synthetic memory and game queue only. The three Enter seat functions are
// extracted verbatim from the current cooperative_bridge.h by the Python test.
// This does not execute a running game or multiplayer network.
#include "autoload.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>
using namespace assist;
using DWORD=uint32_t;
static constexpr uint32_t house=0x20000, target=0x50000, actorA=0x70000,actorB=0x71000;
static std::map<uint32_t,uint32_t> cells,identities;
template<class T>static T get(uint32_t at){auto it=cells.find(at);return it==cells.end()?T{}:static_cast<T>(it->second);}
static uint32_t worldByID(uint32_t id){auto it=identities.find(id);return it==identities.end()?0:it->second;}
static bool own(uint32_t p,uint32_t h){return get<uint32_t>(p+0x21C)==h;}
static int kind(uint32_t){return 1;}
struct Versions {std::map<uint32_t,uint32_t> versions;uint32_t get(uint32_t id)const{
 auto it=versions.find(id);return it==versions.end()?0:it->second;
}};static Versions manualVersions;
static loading::Ledger<> loadLedger;
struct EnterReservation{uint32_t id,target,type,token;DWORD queuedAt;int slots;};
static EnterReservation entryReservations[128];static int entryReservationCount=0;
struct Member{uint32_t pointer,id;};
struct {std::vector<Member>members;} worldFrameCommandRoster;
static bool pageComplete=true;
struct SampleReadScope{
 explicit operator bool()const{return pageComplete;}
 bool complete()const{return pageComplete;}
 void finish()const{}
};
static bool commandRosterReady(){return pageComplete;}
struct EntryView{
 struct Capacity{bool singleVehicle=false;};
 Capacity capacity(uint32_t)const{return {};}
 uint32_t entryDestination(uint32_t p)const{return get<uint32_t>(p+0x500);}
 bool radioLinked(uint32_t,uint32_t)const{return false;}
 int entrySlots(uint32_t,uint32_t)const{return 1;}
};static EntryView clickView;
/* INSERT_EXTRACTED */
static int checks=0;
static void verify(bool good,const char*what){++checks;if(!good){std::fprintf(stderr,"FAILED %d: %s\n",checks,what);std::abort();}}
static void reset(){
 cells.clear();identities.clear();manualVersions.versions.clear();loadLedger.clear();
 entryReservationCount=0;worldFrameCommandRoster.members.clear();pageComplete=true;
 identities[900]=target;cells[target+0x10]=900;
}
static void actor(uint32_t p,uint32_t id){
 identities[id]=p;cells[p+0x10]=id;cells[p+0x21C]=house;
 worldFrameCommandRoster.members.push_back({p,id});
}
static void loadingSeat(uint32_t p,uint32_t id,int size=1){
 loading::Reservation job{p,id,target,900,size,10};
 job.manualToken=manualVersions.get(id);
 verify(loadLedger.add(job),"source ticket entered AutoLoad ledger");
}
static void cooperativeSeat(uint32_t id,int size=1){
 entryReservations[entryReservationCount++]={id,900,0,manualVersions.get(id),100,size};
}
static int observedSlots(){
 bool ready=false;int n=entryReserved(target,0,house,&ready);
 verify(ready,"complete synthetic world must produce valid capacity evidence");return n;
}
int main(){
 reset();actor(actorA,101);loadingSeat(actorA,101);
 verify(observedSlots()==1,"AutoLoad packet reserves before native Enter is visible");
 verify(!cooperativePendingActor(101),"AutoLoad ticket does not forge cooperative ticket");
 cells[actorA+0x500]=target;
 verify(observedSlots()==1,"native destination and AutoLoad ledger count once");
 reset();actor(actorA,101);actor(actorB,102);loadingSeat(actorA,101);cooperativeSeat(102);
 verify(cooperativePendingActor(102),"cooperative ticket owns actor before native event");
 verify(cooperativePendingSlots(900,target)==1,"cooperative ticket reserves a slot");
 verify(observedSlots()==2,"both producers occupy one physical seat each");
 cells[actorB+0x500]=target;
 verify(cooperativePendingSlots(900,target)==0,"visible native destination replaces cooperative pending");
 verify(observedSlots()==2,"visible native destination stays counted exactly once");
 reset();actor(actorA,101);loadingSeat(actorA,101);cooperativeSeat(101);
 verify(observedSlots()==1,"same actor in two ledgers may consume only one slot");
 cells[actorA+0x500]=target;
 verify(observedSlots()==1,"same actor in both ledgers with native destination counts once");
 reset();actor(actorA,101);loadingSeat(actorA,101);cooperativeSeat(101);
 cells[actorA+0x11C]=target;
 verify(cooperativePendingSlots(900,target)==0,"boarded cooperative actor releases pending seat");
 verify(observedSlots()==0,"boarded actor counted by native cargo instead of pending");
 reset();actor(actorA,101);loadingSeat(actorA,101);
 identities[101]=actorB;
 verify(observedSlots()==0,"known recycled source pointer cannot reserve stale seat");
 reset();actor(actorA,101);loadingSeat(actorA,101);
 identities.erase(101); // observer has not published the current identity page
 verify(observedSlots()==1,"unknown identity remains pending instead of freeing seat");
 reset();actor(actorA,101);loadingSeat(actorA,101);
 manualVersions.versions[101]=1;
 verify(observedSlots()==0,"manual override releases old AutoLoad seat");
 reset();actor(actorA,101);cooperativeSeat(101);
 manualVersions.versions[101]=1;
 verify(!cooperativePendingActor(101)&&cooperativePendingSlots(900,target)==0,
        "manual override revokes cooperative permission");
 reset();actor(actorA,101);loadingSeat(actorA,101);pageComplete=false;
 bool ready=true;verify(entryReserved(target,0,house,&ready)==1024&&!ready,
        "incomplete membership is UNKNOWN not a false room-available finding");
 std::printf("PASS %d source-extracted native/cooperative/AutoLoad seat cases\n",checks);
}

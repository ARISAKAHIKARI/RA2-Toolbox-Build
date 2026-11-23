// Synthetic memory and game queue only. The Enter seat functions are
// extracted verbatim from the current cooperative_bridge.h by the Python test.
// This does not execute a running game or multiplayer network.
#include "autoload.h"
#include "entry_cohort.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
struct CandidateVector {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static constexpr uint32_t vectorItems=0x60000;
static CandidateVector liveVector{0,vectorItems,64,0,0,0};
static entrycohort::Members liveMembers{},frozenMembers{};
static uint32_t unreadable=0;
static bool changedHeader=false,localSession=true,candidateReady=true;
static unsigned headerReads=0;
static struct{int frame=77,session=0;}snap;
static bool readLiveWorld(uint32_t at,void*out,size_t bytes){
 if(at==unreadable)return false;
 if(at==0xA8EC78&&bytes==sizeof liveVector){
  auto v=liveVector;if(++headerReads==2&&changedHeader)++v.count;
  std::memcpy(out,&v,bytes);return true;
 }
 if(at==vectorItems&&bytes<=sizeof liveMembers){std::memcpy(out,liveMembers.data(),bytes);return true;}
 if(bytes!=4)return false;
 const uint32_t v=at==0xA8ED84?uint32_t(snap.frame):get<uint32_t>(at);
 std::memcpy(out,&v,bytes);return true;
}
static struct {bool read(uint32_t at,void*out,size_t bytes)const{
 if(at!=vectorItems||bytes>sizeof frozenMembers)return false;
 std::memcpy(out,frozenMembers.data(),bytes);return true;
}} worldFrameCandidate;
static struct CandidateIndex{uint32_t byId(uint32_t id)const{return worldByID(id);}} candidateIndex;
static struct {int count=0;uint32_t items=vectorItems;CandidateIndex*objects=&candidateIndex;} worldFrameCandidateRoster;
namespace eventdelivery {bool isSingleplayer(int){return localSession;}}
namespace preciseclock {uint64_t time=0,step=0;uint64_t micros(){return time+=step;}}
static bool candidateRosterReady(){return candidateReady;}
namespace external {
struct EntryCapacity{int maximum=0,used=0;bool singleVehicle=false;};
template<class Read>struct GameView {
 Read read;
 explicit GameView(Read reader):read(reader){}
 template<class T>T get(uint32_t p,T fallback={})const{T v{};return read(p,&v,sizeof v)?v:fallback;}
 int kind(uint32_t)const{return 1;}
 EntryCapacity capacity(uint32_t target)const{return {8,get<int>(target+0x114),false};}
 int entrySlots(uint32_t,uint32_t)const{return 1;}
 uint32_t entryDestination(uint32_t p)const{return get<uint32_t>(p+0x500);}
 bool radioLinked(uint32_t,uint32_t)const{return false;}
};
}
/* INSERT_EXTRACTED */
static int checks=0;
static void verify(bool good,const char*what){++checks;if(!good){std::fprintf(stderr,"FAILED %d: %s\n",checks,what);std::abort();}}
static void reset(){
 cells.clear();identities.clear();manualVersions.versions.clear();loadLedger.clear();
 entryReservationCount=0;worldFrameCommandRoster.members.clear();pageComplete=true;
 liveMembers={};frozenMembers={};liveVector.count=worldFrameCandidateRoster.count=0;
 unreadable=0;headerReads=0;changedHeader=false;localSession=candidateReady=true;
 preciseclock::time=preciseclock::step=0;
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
static void snapshotCohort(){
 liveVector.count=worldFrameCandidateRoster.count=int(worldFrameCommandRoster.members.size());
 for(int i=0;i<liveVector.count;++i)liveMembers[i]=worldFrameCommandRoster.members[i].pointer;
 frozenMembers=liveMembers;
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
 reset();actor(actorA,101);actor(actorB,102);actor(target,900);
 snapshotCohort();pageComplete=false;
 external::EntryCapacity capacity{};int slots=0,reserved=0;
 auto current=[&](){headerReads=0;return candidateEntryCapacity(target,actorA,101,house,capacity,slots,reserved);};
 loadingSeat(actorB,102);cooperativeSeat(102);
 verify(current()&&reserved==1&&slots==1,"candidate cohort counts a shared pending seat only once");
 cells[actorB+0x500]=target;
 verify(current()&&reserved==1,"new live arrival is recognized after the original sample");
 cells[target+0x114]=8;
 verify(current()&&capacity.used+reserved+slots>capacity.maximum,"fresh occupied seats can veto the old plan");
 unreadable=actorB+0x500;
 verify(!current(),"an unreadable live destination defers admission");
 unreadable=0;std::swap(liveMembers[0],liveMembers[1]);
 verify(!current(),"changed roster membership cannot inherit candidate authority");
 liveMembers=frozenMembers;cells[actorB+0x10]=103;
 verify(!current(),"same-pointer identity reuse is not a verified cohort");
 cells[actorB+0x10]=102;changedHeader=true;
 verify(!current(),"roster changes during proof defer admission");
 changedHeader=false;preciseclock::step=1000;
 verify(!current(),"an over-budget live proof defers admission");
 preciseclock::step=0;localSession=false;
 verify(!candidateEntryCohortReady(),"online entry keeps its strict path");
 localSession=true;worldFrameCandidateRoster.count=65;
 verify(!candidateEntryCohortReady(),"large cohorts keep the strict path");
 std::printf("PASS %d source-extracted native/cooperative/AutoLoad seat cases\n",checks);
}

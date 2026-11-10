#pragma once
// ObjectClass::AttachedBomb +0x38 is an existing documented reader field.
// Read-only detection, ordinary Enter events, and the shared boarding ledger.
namespace bombboard {
struct Fact {uint32_t p,id,bomb;int x,y;};
struct Job {Fact unit{};uint32_t token=0,target=0;int sent=-1;bool cancelled=false;};
static std::vector<Fact> armed,vehicles;static std::vector<Job> jobs;
static uint32_t epoch=0,owner=0;static int sampled=-1;static size_t cursor=0;
static const Job*submitting=nullptr;static uint32_t destination=0;static int destinationSlots=0;
static void reset(){jobs.clear();armed.clear();vehicles.clear();epoch=owner=0;sampled=-1;cursor=0;submitting=nullptr;
 if(sh)sh->bombBoardActive=sh->bombBoardWaiting=0;}
static bool sourceValid(const Job&j){return sh&&sh->bombBoardEnabled&&worldByID(j.unit.id)==j.unit.p&&own(j.unit.p,lastHouse)&&kind(j.unit.p)==1&&live(j.unit.p)&&
 get<uint32_t>(j.unit.p+0x38)==j.unit.bomb&&j.unit.bomb>=0x10000&&!get<uint32_t>(j.unit.p+0x11C)&&!get<bool>(j.unit.p+0x1C8)&&manualVersions.get(j.unit.id)==j.token&&!j.cancelled;}
static int reserved(uint32_t target,uint32_t ignore){
 int n=entryReserved(target,ignore,lastHouse);
 // Ordinary AutoBoard's pending events also own seats before the receiver
 // makes them visible as incoming passengers. Count each actor only once.
 for(int i=0;i<loadLedger.count;++i){const auto&r=loadLedger.records[i];
  if(r.id==ignore||r.vehicle!=target||!worldByID(r.id)||incomingVehicle(r.object))continue;
  bool covered=false;for(int k=0;k<entryReservationCount;++k)covered|=entryReservations[k].id==r.id;
  if(!covered)n+=r.slots;
 }return n;
}
static bool targetValid(const Job&j,uint32_t target,int slots){
 if(!sourceValid(j)||!target||!worldObjects.contains(target)||!own(target,lastHouse)||kind(target)!=2||!live(target)||get<bool>(target+0x1C8)||get<uint32_t>(target+0x11C))return false;
 const int radius=std::clamp(int(sh->bombBoardRangeCells),1,64);
 int64_t dx=int64_t(get<int>(j.unit.p+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(j.unit.p+0xA0))-get<int>(target+0xA0),limit=int64_t(radius)*256;
 if(dx*dx+dy*dy>limit*limit||!clickView.entryCompatible(j.unit.p,target,lastHouse)||slots<=0||slots!=clickView.entrySlots(j.unit.p,target))return false;
 const auto capacity=clickView.capacity(target);
 return capacity.maximum>0&&capacity.used>=0&&int64_t(capacity.used)+reserved(target,j.unit.id)+slots<=capacity.maximum;
}
static bool validate(const intent::EventOrder&o){
 if(!submitting)return true;const auto&j=*submitting;
 return sh&&sh->bombBoardEnabled&&o.domain==vanilla::Domain::MegaMission&&!o.appendWaypoint&&o.player==get<int>(lastHouse+0x30)&&
 uint32_t(o.a)==j.unit.id&&o.mission==7&&!o.b&&o.tagC==0x34&&worldByID(uint32_t(o.c))==destination&&targetValid(j,destination,destinationSlots);
}
static bool collect(){
 SampleReadScope sample;if(!sample)return false;Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
 armed.clear();vehicles.clear();
 for(int i=0;i<all.count;++i){uint32_t p=item(all,i);if(!p||!own(p,lastHouse)||!live(p))continue;int k=kind(p);
  if(k!=1&&k!=2)continue;if(get<uint32_t>(p+0x11C)||get<bool>(p+0x1C8))continue;
  uint32_t bomb=k==1?get<uint32_t>(p+0x38):0;
  const uint32_t type=k==2?typeOf(p,k):0;
  if((k==1&&bomb>=0x10000)||(k==2&&type>=0x10000&&get<int>(type+0x5E0)>0)){
   Fact f{p,get<uint32_t>(p+0x10),bomb,get<int>(p+0x9C),get<int>(p+0xA0)};(k==1?armed:vehicles).push_back(f);
  }
 }return sample.complete();
}
static void tick(uint32_t house,int frame){
 if(!sh||!sh->bombBoardEnabled){if(!jobs.empty()||epoch)reset();return;}
 if(epoch!=match||owner!=house||frame<sampled){reset();epoch=match;owner=house;sh->bombBoardIssued=0;}
 if(sampled==frame||!nativeReady()||!collect())return;sampled=frame;
 std::sort(armed.begin(),armed.end(),[](const Fact&a,const Fact&b){return a.id<b.id;});
 for(size_t i=0;i<jobs.size();){auto at=std::lower_bound(armed.begin(),armed.end(),jobs[i].unit.id,[](const Fact&a,uint32_t id){return a.id<id;});
  if(at==armed.end()||at->id!=jobs[i].unit.id||at->p!=jobs[i].unit.p||at->bomb!=jobs[i].unit.bomb)jobs.erase(jobs.begin()+i);
  else {jobs[i].unit=*at;if(manualVersions.get(at->id)!=jobs[i].token)jobs[i].cancelled=true;++i;}
 }
 std::sort(jobs.begin(),jobs.end(),[](const Job&a,const Job&b){return a.unit.id<b.unit.id;});
 for(const auto&f:armed){auto at=std::lower_bound(jobs.begin(),jobs.end(),f.id,[](const Job&j,uint32_t id){return j.unit.id<id;});
  if(at==jobs.end()||at->unit.id!=f.id)jobs.insert(at,{f,manualVersions.get(f.id)});
 }
 sh->bombBoardActive=uint32_t(jobs.size());sh->bombBoardWaiting=0;
 for(const auto&j:jobs)if(!j.cancelled&&!j.target)++sh->bombBoardWaiting;
 if(jobs.empty())return;
 // One shared roster projection, with a rotating work bound. No per-object
 // timers, extra threads, or display-frequency rereads of the whole world.
 const size_t work=std::min<size_t>(jobs.size(),32);int sends=0;loadQueryFailed=false;
 for(size_t i=0;i<work;++i){if(cursor>=jobs.size())cursor=0;auto&j=jobs[cursor++];
  const uint32_t incoming=incomingVehicle(j.unit.p);
  if(!sourceValid(j)||(incoming&&worldObjects.contains(incoming)&&live(incoming))||loadLedger.contains(j.unit.id)||(j.sent>=0&&frame-j.sent<30))continue;
  if(outgoingRoom()<=0||sends>=8)break;
  const int64_t limit=int64_t(std::clamp(int(sh->bombBoardRangeCells),1,64))*256;
  std::vector<std::pair<int64_t,uint32_t>> nearby;nearby.reserve(vehicles.size());
  for(const auto&v:vehicles){int64_t dx=int64_t(j.unit.x)-v.x,dy=int64_t(j.unit.y)-v.y,d=dx*dx+dy*dy;if(d<=limit*limit)nearby.push_back({d,v.p});}
  std::sort(nearby.begin(),nearby.end());
  for(const auto&v:nearby){uint32_t target=v.second;int slots=clickView.entrySlots(j.unit.p,target);
   if(entryReservationCount>=cooperative::MaxSources||!targetValid(j,target,slots)||!canEnter(j.unit.p,target))continue;
   bool sent=false;runtimeunwind::run([&]{submitting=&j;destination=target;destinationSlots=slots;
    sent=nativeMission(house,j.unit.id,7,0,get<uint32_t>(target+0x10),0x34,0,0,taskarbiter::Owner::BombBoard,nullptr,-1,OrderAuthority::Local,false,false,true);
    if(sent){for(int n=0;n<entryReservationCount;)if(entryReservations[n].id==j.unit.id)entryReservations[n]=entryReservations[--entryReservationCount];else ++n;
     entryReservations[entryReservationCount++]={j.unit.id,get<uint32_t>(target+0x10),typeOf(target,2),j.token,frame,slots};
     j.target=get<uint32_t>(target+0x10);j.sent=frame;++sh->bombBoardIssued;++sends;
    }
   },[&]{submitting=nullptr;destination=0;destinationSlots=0;});
   break; // A legal nearest target waits for gateway room, never a farther substitute.
  }
 }
}
}
static bool validateBombBoardSubmission(const intent::EventOrder&o){return bombboard::validate(o);}
static void resetBombBoard(){bombboard::reset();}

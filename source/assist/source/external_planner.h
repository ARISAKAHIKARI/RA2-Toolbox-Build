#pragma once
#include "protocol.h"
#include "native_orders.h"
#include "ore_queue.h"
#include "business_effects.h"
#include "planner_index.h"
#include "weapon_policy.h"
#include "magnetron_cycle.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace assist { namespace planner {
// Controller-only snapshot -> orders. No game callback, pointer write or RNG.
class Planner {
 struct Advanced {
  uint32_t token=0,cell=0,target=0,mode=0,skipTarget=0;int sent=-100000,shotStart=-1,observedStart=-1,skipUntil=0,observedX=0,observedY=0,attackProgress=0,progressX=0,progressY=0,lastFireStamp=-1,moveFailures=0;
  uint32_t scannedAt=0,pursuit=0,moveCell=0;bool scanned=false;
  bool attacking=false,stopped=false,pendingShot=false,urgent=true;nativeorders::Progress progress;
 };
 struct OreJob {
  uint32_t id,refinery,token,waitCell=0;int stage=ore::Waiting,sent=-100000,x=0,y=0,progressFrame=0,repaths=0,dockAttempts=0;
  double initialCargo=0;bool dockingObserved=false,bypassUsed=false,priority=false;int readySince=-1;
 };
 struct OreTurn {
  uint32_t winner=0,displaced=0,winnerToken=0,displacedToken=0;int started=0,sent=-100000,attempts=0;
  bool releaseAccepted=false,released=false;
 };
 std::unordered_map<uint32_t,OreTurn> oreTurns_;
 uint32_t match_=0,inputSerial_=0,flags_=0;int frame_=-1,cursor_=0;
 std::unordered_map<uint32_t,Advanced> advanced_;
 struct Drop {uint32_t victim,token;int sent;};std::unordered_map<uint32_t,Drop> drops_;bool dropEnabled_=false;
 std::unordered_map<uint32_t,uint32_t> defenseSent_;int defenseCursor_=0;
 ActorLookup actors_;EnemyGrid enemies_;std::vector<const Actor*> refineries_,trucks_;
 std::unordered_map<uint32_t,const Refinery*> areas_;
 std::unordered_set<uint32_t> oreIDs_,waitingCells_,magneticReservations_;
 std::vector<OreJob> ore_;
 const Actor* actor(uint32_t id)const{return actors_.find(id);}
 static bool positionValid(const Actor& a){return a.x>=-1048576&&a.x<=1048576&&a.y>=-1048576&&a.y<=1048576;}
 static bool truck(const Actor* a){return a&&positionValid(*a)&&(a->flags&Own)&&(a->flags&Harvester)&&!(a->flags&(Disabled|Transported))&&(!(a->flags&(Limbo|OffMap))||(a->flags&Unloading));}
 static double load(const Actor& a){return ore::storageAmount(a.storage);}
 static bool full(const Actor& a){return a.storageLimit>0&&load(a)>=a.storageLimit;}
 static bool compatible(const Actor& a,const Actor& refinery){for(uint32_t type:a.dockTypes)if(type&&type==refinery.type)return true;return false;}
 static bool refineryValid(const Actor* r){return r&&(r->flags&Own)&&r->kind==3&&positionValid(*r)&&!(r->flags&(Disabled|Limbo|OffMap));}
 static bool occupied(const Actor& refinery,uint32_t self){for(uint32_t id:refinery.radioIDs)if(id&&id!=self)return true;return false;}
 OreJob* oreJob(uint32_t id){for(auto&j:ore_)if(j.id==id)return &j;return nullptr;}
 static bool linked(const Actor&r,uint32_t id){for(uint32_t p:r.radioIDs)if(p==id)return true;return false;}
 int64_t dockDistance(const Actor&a,const Actor&r)const{
  auto i=areas_.find(r.id);if(i==areas_.end())return INT64_MAX;
  return ore::dockDistance(a.x,a.y,r.x,r.y,i->second->dockStyleB!=0);
 }
 static bool add(std::vector<Order>& out,Order order){if(out.size()>=MaxOrders)return false;out.push_back(order);return true;}
 static std::vector<Order> merge(const std::vector<Order>& combat,const std::vector<Order>& ore){
  std::vector<Order> result;result.reserve(MaxOrders);size_t c=0,o=0;
  // Interleave both families before the bounded native event allowance runs
  // out. Per-item masks acknowledge successful and rejected orders separately.
  while(result.size()<MaxOrders&&(c<combat.size()||o<ore.size())){
   if(o<ore.size())result.push_back(ore[o++]);
   if(c<combat.size()&&result.size()<MaxOrders)result.push_back(combat[c++]);
  }return result;
 }
 const Actor* nearest(const Actor& a)const{
  const Actor* best=nullptr;int64_t distance=INT64_MAX;
  for(const Actor* candidate:refineries_){if(!refineryValid(candidate)||!compatible(a,*candidate))continue;
   int64_t dx=int64_t(a.x)-candidate->x,dy=int64_t(a.y)-candidate->y,d=dx*dx+dy*dy;
   if(d<distance||(d==distance&&(!best||candidate->id<best->id))){distance=d;best=candidate;}
  }return best;
 }
 const WaitingCell* sample(const OreJob& job,uint32_t cell)const{
  auto i=areas_.find(job.refinery);if(i==areas_.end())return nullptr;
  const auto&r=*i->second;for(int n=0;n<r.count;++n)if(r.waiting[n].cell==cell)return &r.waiting[n];return nullptr;
 }
 uint32_t waitingCell(const OreJob& job)const{
  auto i=areas_.find(job.refinery);if(i==areas_.end())return 0;const auto&r=*i->second;
  for(int n=0;n<r.count;++n){const auto&cell=r.waiting[n];
   // Native Move decides pathability. Never call the game's pathfinder or radio
   // decision routines on only one peer. Occupied foundations/water/rock excluded.
   if(cell.cell&&cell.cell!=job.waitCell&&!cell.occupation&&cell.land!=2&&cell.land!=7&&!waitingCells_.count(cell.cell))return cell.cell;
  }return 0;
 }
#include "advanced_decision.h"
public:
 uint32_t oreBypasses=0,oreYieldAttempts=0,oreTurnTimeouts=0;
 uint32_t oreDelivered=0,oreUnverified=0,magnetShots=0,magnetUnverified=0;
 int queued=0,docking=0,issued=0,yielded=0;uint32_t targetChecks=0;
 bool hasJobs()const{return !ore_.empty()||!advanced_.empty();}
 std::vector<uint32_t> stoppedUnits()const{std::vector<uint32_t> ids;for(const auto&entry:advanced_)if(entry.second.stopped)ids.push_back(entry.first);return ids;}
 uint32_t trackingFlags()const{uint32_t f=ore_.empty()?0:AutoOreReturn;for(const auto&i:advanced_)f|=i.second.mode==MagnetronMove?MagnetronInput:AdvancedInput;return f;}
 void reset(){drops_.clear();dropEnabled_=false;defenseSent_.clear();defenseCursor_=0;advanced_.clear();actors_.clear();enemies_.clear();refineries_.clear();trucks_.clear();areas_.clear();ore_.clear();oreTurns_.clear();oreIDs_.clear();waitingCells_.clear();magneticReservations_.clear();match_=inputSerial_=0;frame_=-1;cursor_=0;queued=docking=issued=yielded=0;oreDelivered=oreUnverified=magnetShots=magnetUnverified=0;oreBypasses=oreYieldAttempts=oreTurnTimeouts=0;}
 void accepted(const Order& order,int frame){
  if(order.purpose==MagnetronDropOrder){drops_[order.unit]={order.refinery,order.token,frame};auto it=advanced_.find(order.unit);if(it!=advanced_.end()){auto&t=it->second;t.pendingShot=t.attacking=false;t.target=0;t.sent=frame;}return;}
  if(order.purpose==1||order.purpose==3){auto it=advanced_.find(order.unit);if(it==advanced_.end()||it->second.token!=order.token)return;auto&task=it->second;
   task.sent=frame;task.attacking=order.mission==1;task.target=order.target;task.stopped=order.mission==6;
   task.pendingShot=order.mission==1;task.shotStart=task.observedStart;task.urgent=false;
   if(order.mission==1){task.attackProgress=frame;task.progressX=task.observedX;task.progressY=task.observedY;task.lastFireStamp=task.observedStart;}
   task.pursuit=order.pursuit;task.moveCell=order.destination;
   if(order.mission==2){task.progress.begin(task.observedX,task.observedY,frame);task.moveFailures=0;}
  }else for(size_t i=0;i<ore_.size();++i)if(ore_[i].id==order.unit&&ore_[i].token==order.token){
   if(order.purpose==2){ore_.erase(ore_.begin()+i);break;}
   auto&job=ore_[i];job.sent=frame;
   if(order.purpose==OreYield){auto t=oreTurns_.find(job.refinery);if(t!=oreTurns_.end()&&t->second.displaced==job.id)t->second.releaseAccepted=true;job.stage=ore::Approaching;job.progressFrame=frame;break;}
   if(order.mission==7){if(job.stage!=ore::Docking)job.progressFrame=frame;job.stage=ore::Docking;job.waitCell=0;++job.dockAttempts;++issued;}else job.stage=ore::Approaching;break;
  }
 }
 void expired(const Order&order){
  if(order.purpose==DefenseOrder)defenseSent_.erase(order.unit);
  if(order.purpose==1||order.purpose==3){auto it=advanced_.find(order.unit);if(it!=advanced_.end()&&it->second.token==order.token)it->second.urgent=true;}
  // Permit a fresh snapshot in the same simulation frame after a pause.
  frame_=-1;
 }
 void rejected(const Order&order,int frame){
  if(order.purpose!=1&&order.purpose!=3)return;auto it=advanced_.find(order.unit);if(it==advanced_.end()||it->second.token!=order.token)return;auto&t=it->second;
  t.urgent=true;
  // Ordinary combat is based on current native range facts. A rejected old
  // Move/Attack may simply mean the target or weapon changed in transit.
  // Re-evaluate next world without blacklisting an in-range enemy or counting
  // the superseded Move as a path failure. Progress still bounds real stalls.
  if(order.purpose==CombatOrder){t.pendingShot=false;return;}
  if(order.mission==1){t.skipTarget=order.target;t.skipUntil=frame+30;if(t.target==order.target)t.pendingShot=false;}
  else if(order.mission==2&&++t.moveFailures>=2){t.progress.hold=true;t.stopped=false;}
 }
 std::vector<Order> step(const World& world,uint32_t flags,int veteran,int elite,int pathTimeout,int scanHz=0,int targetPriority=0,int rangeCells=2,bool autoDrop=false){
  const int rangePadding=std::clamp(rangeCells,0,30)*256;
  if(!validWorld(world)){reset();return {};}
  if(world.match!=match_){reset();match_=world.match;}
  std::vector<Order> combat,out;combat.reserve(16);out.reserve(MaxOrders);
  if(world.frame==frame_&&world.inputSerial==inputSerial_&&flags==flags_&&dropEnabled_==autoDrop)return out;frame_=world.frame;inputSerial_=world.inputSerial;flags_=flags;dropEnabled_=autoDrop;queued=docking=yielded=0;targetChecks=0;
  actors_.clear();enemies_.clear();refineries_.clear();trucks_.clear();areas_.clear();bool enemiesBuilt=false;
  bool useOre=(flags&AutoOreReturn)||!ore_.empty(),useCombat=(flags&(AdvancedInput|MagnetronInput))&&world.intentCount;
  for(int i=0;i<world.count;++i){const auto&a=world.actors[i];if(!a.id)continue;actors_.add(a);
   if(useOre){if(a.kind==3&&(a.flags&Own))refineries_.push_back(&a);if(truck(&a))trucks_.push_back(&a);}
  }
  for(int i=0;i<world.refineryCount;++i)areas_[world.refineries[i].id]=&world.refineries[i];
  std::unordered_set<uint32_t> dropping;
  for(auto it=drops_.begin();it!=drops_.end();){const Actor*a=actor(it->first);if(!autoDrop||!a||a->manualVersion!=it->second.token||a->liftTarget!=it->second.victim)it=drops_.erase(it);else ++it;}
  if(autoDrop)for(int n=0;n<world.count;++n){const auto&a=world.actors[n];if(!(a.flags&Own)||!a.liftTarget||!magnetron(a)||(a.flags&(Disabled|Limbo|OffMap|Transported)))continue;
   auto pending=drops_.find(a.id);if(pending!=drops_.end()){dropping.insert(a.id);if(world.frame-pending->second.sent<30)continue;}
   if(!(a.flags&DropHazard))continue;
   dropping.insert(a.id);add(combat,{a.id,a.manualVersion,0,0,13,0,MagnetronDropOrder,a.liftTarget});
  }
  if(flags&AntiTheft){
   for(auto it=defenseSent_.begin();it!=defenseSent_.end();)if(!actor(it->first))it=defenseSent_.erase(it);else ++it;
   for(int n=0;n<world.count;++n){const auto&e=world.actors[n];if(positionValid(e)&&(e.flags&Enemy)&&!(e.flags&(Cloaked|Transported|Limbo|OffMap|Protected)))enemies_.add(n,e);}enemiesBuilt=true;
   int visited=0,checked=0,sent=0;
   for(;visited<world.count&&checked<128&&sent<6;++visited){int n=(defenseCursor_+visited)%world.count;const auto&a=world.actors[n];
    if(!(a.flags&PointDefense)||!(a.flags&Own)||!positionValid(a)||(a.flags&(Disabled|Limbo|OffMap|Transported)))continue;++checked;
    const Actor* current=actor(a.target);if(current&&(current->flags&Enemy)&&(current->flags&Saboteur))continue;
    auto pending=defenseSent_.find(a.id);if(pending!=defenseSent_.end()&&uint32_t(world.tick-pending->second)<350)continue;
    const Actor* best=nullptr;int64_t bestDistance=INT64_MAX;
    enemies_.nearby(a.x,a.y,a.range,[&](int index){const auto&t=world.actors[index];if(!(t.flags&Saboteur))return;
     int64_t dx=int64_t(a.x)-t.x,dy=int64_t(a.y)-t.y,d=dx*dx+dy*dy;if(!canTarget(a,t,d,false))return;
     if(d<bestDistance||(d==bestDistance&&(!best||t.id<best->id))){best=&t;bestDistance=d;}
    });
    if(best&&add(combat,{a.id,a.manualVersion,best->id,0,1,0,DefenseOrder,0})){defenseSent_[a.id]=world.tick;++sent;}
   }if(world.count)defenseCursor_=(defenseCursor_+visited)%world.count;
  }else defenseSent_.clear();

  std::unordered_set<uint32_t> active;
  magneticReservations_.clear();
  if(useCombat){
   for(int i=0;i<world.intentCount;++i){const auto&intent=world.intents[i];const Actor* a=actor(intent.id);if(!intent.id||intent.mode>MagnetronMove||intent.cell%1000<=0||intent.cell%1000>=512||intent.cell/1000<=0||intent.cell/1000>=512)continue;bool mag=intent.mode==MagnetronMove;
    if(!(flags&(mag?MagnetronInput:AdvancedInput))||!a||!positionValid(*a)||!(a->flags&Own)||a->manualVersion!=intent.token||(a->flags&(Disabled|Transported|Limbo|OffMap|Manipulated))||(mag&&!magnetron(*a)))continue;
    active.insert(intent.id);auto&task=advanced_[intent.id];
    if(task.token!=intent.token||task.cell!=intent.cell||task.mode!=intent.mode){task={};task.token=intent.token;task.cell=intent.cell;task.mode=intent.mode;task.progress.begin(a->x,a->y,world.frame);}
    if(intent.global)task.progress.hold=true;
    task.observedStart=a->rearmStart;task.observedX=a->x;task.observedY=a->y;
    if(mag&&task.pendingShot){const Actor* t=actor(task.target);
     const bool hit=t&&((t->flags&Manipulated)||a->liftTarget==task.target);
     if(magnetcycle::shotObserved(world.frame,task.sent,cooldown(*a,world.frame),a->rearmStart,task.shotStart,hit)){
      // Critical to rapid magnetic juggling: as soon as the previous shot is
      // visible, reserve its target against another shot from the SAME unit.
      // The next event is allowed on the FIRST ready frame, not a fixed delay.
      ++magnetShots;task.pendingShot=false;task.skipTarget=task.target;
      task.skipUntil=magnetcycle::avoidUntil(world.frame,a->rearmDuration);
      if(task.target)magneticReservations_.insert(task.target);
     }else if(t&&(t->flags&Enemy)&&magneticGroundTarget(*t)&&
              !magnetcycle::shotStalled(world.frame,task.sent)&&
              (a->target==task.target||world.frame-task.sent<12)){
      magneticReservations_.insert(task.target);
     }else if(magnetcycle::shotStalled(world.frame,task.sent)||a->target!=task.target){
      // An unobserved shot may mean the event was dropped or the unit acquired
      // another target.  Retry elsewhere, but do not pretend the shot fired.
      ++magnetUnverified;task.skipTarget=task.target;task.skipUntil=world.frame+30;task.pendingShot=false;
     }
    }
   }
   int count=world.intentCount,sent=0;std::vector<int> work;work.reserve(128);uint8_t scheduled[MaxIntents]{};
   // New clicks and explicit rejected orders precede the round-robin cursor.
   // A new task at the end of a large selection need not wait a whole cycle.
   for(int i=0;i<count&&work.size()<128;++i){auto t=advanced_.find(world.intents[i].id);if(t!=advanced_.end()&&active.count(t->first)&&t->second.urgent){work.push_back(i);scheduled[i]=1;}}
   int traversed=0;for(;traversed<count&&work.size()<128;++traversed){int i=(cursor_+traversed)%count;if(!scheduled[i])work.push_back(i);}
   if(count)cursor_=(cursor_+traversed)%count;
   for(int i:work){const auto&intent=world.intents[i];auto it=advanced_.find(intent.id);const Actor* a=actor(intent.id);
    if(!a||it==advanced_.end()||!active.count(intent.id)||dropping.count(intent.id))continue;auto&task=it->second;bool mag=task.mode==MagnetronMove;int purpose=mag?3:1;
    if(!mag){stepAdvanced(world,*a,task,combat,veteran,elite,pathTimeout,scanHz,targetPriority);continue;}
    task.urgent=false;task.scanned=true;task.scannedAt=world.tick;
    const Actor* current=task.attacking?actor(task.target):nullptr;if(current&&!positionValid(*current))current=nullptr;
    int64_t currentDistance=0;if(current){int64_t dx=int64_t(a->x)-current->x,dy=int64_t(a->y)-current->y;currentDistance=dx*dx+dy*dy;}
    bool currentValid=current&&(current->flags&Enemy)&&canTarget(*a,*current,currentDistance,mag,mag?0:rangePadding)&&!(current->id==task.skipTarget&&world.frame<task.skipUntil);
    // Don't continuously reset a shot waiting for its native event/animation.
    // A reload timer is ammo replenishment, not the weapon's rearm timer.
    // Once a shot/lift is observed, the next native Attack must be allowed to
    // release the beam even while its rearm timer is running or suspended.
    // Native execution still owns ROF; this never edits a weapon or timer.
    if(task.pendingShot){task.progress.combat(a->x,a->y,world.frame);continue;}
    if(!enemiesBuilt){for(int j=0;j<world.count;++j){const auto&e=world.actors[j];if(positionValid(e)&&(e.flags&Enemy)&&!(e.flags&(Cloaked|Transported|Limbo|OffMap|Protected)))enemies_.add(j,e);}enemiesBuilt=true;}
    const Actor* best=nullptr;double score=-1,bestValue=-1;int64_t nearestDistance=INT64_MAX;bool bestSkipped=true,rangeTarget=false;
    enemies_.nearby(a->x,a->y,a->range+(mag?0:rangePadding),[&](int j){const auto&enemy=world.actors[j];++targetChecks;
     int64_t dx=int64_t(a->x)-enemy.x,dy=int64_t(a->y)-enemy.y,distance=dx*dx+dy*dy;
     if(!canTarget(*a,enemy,distance,mag,mag?0:rangePadding))return;
     rangeTarget=true;if(magneticReservations_.count(enemy.id))return;
     const bool skipped=enemy.id==task.skipTarget&&world.frame<task.skipUntil;
     double value=nativeorders::value(enemy.cost,enemy.veterancy,veteran,elite);
     double rank=!mag&&targetPriority==1?double(a->weapons[weaponIndex(*a,enemy)].effectiveness[enemy.armor]):value;
     bool keep=currentValid&&enemy.id==task.target,kept=currentValid&&best&&best->id==task.target;
     const bool closer=distance<nearestDistance||(distance==nearestDistance&&(!best||enemy.id<best->id));
     const bool tie=(keep&&!kept)||(keep==kept&&closer);
     const bool higher=rank>score||(rank==score&&(value>bestValue||(value==bestValue&&tie)));
     if(!best||(bestSkipped&&!skipped)||(bestSkipped==skipped&&higher)){best=&enemy;bestSkipped=skipped;score=rank;bestValue=value;nearestDistance=distance;}
    });
    if(best){task.progress.combat(a->x,a->y,world.frame);
     bool attack=!task.attacking||task.target!=best->id||((a->target!=best->id||a->mission!=1)&&world.frame-task.sent>=12);
     if(sent<MaxOrders&&world.frame>task.sent&&attack&&add(combat,{a->id,task.token,best->id,0,1,0,purpose})){++sent;if(mag)magneticReservations_.insert(best->id);}
     continue;
    }
    // A pending shot by another magnet is temporary contention, not proof
    // that combat ended. Keep scanning instead of resuming the old move.
    if(rangeTarget){task.progress.combat(a->x,a->y,world.frame);continue;}
    if(task.attacking){if(sent<MaxOrders&&add(combat,{a->id,task.token,0,task.progress.hold?0:task.cell,task.progress.hold?6:2,task.progress.hold?0:11,purpose}))++sent;continue;}
    task.progress.update(a->x,a->y,world.frame,a->mission,int(task.cell%1000)*256+128,int(task.cell/1000)*256+128,std::clamp(pathTimeout,30,900));
    if(task.progress.hold&&!task.stopped&&sent<MaxOrders&&add(combat,{a->id,task.token,0,0,6,0,purpose}))++sent;
   }
  }
  for(auto i=advanced_.begin();i!=advanced_.end();)if(!active.count(i->first))i=advanced_.erase(i);else ++i;
  if(!useOre)return combat;
  for(auto i=ore_.begin();i!=ore_.end();){const Actor*a=actor(i->id),*refinery=actor(i->refinery);
   // A disappearing/invalid miner is NOT evidence that it unloaded.
   double cargo=a?load(*a):-1;
   if(!truck(a)||cargo<=0||(a->flags&OrePaused)||a->manualVersion!=i->token||active.count(a->id)){
    if(a&&cargo<=0){auto status=effects::cargo(i->initialCargo,cargo,i->dockingObserved||(a->flags&Unloading));
     if(status==effects::Cargo::Delivered)++oreDelivered;
     else ++oreUnverified;}
    i=ore_.erase(i);continue;
   }
   if(a->flags&Unloading){i->dockingObserved=true;i->stage=ore::Docking;i->waitCell=0;}
   if(!refineryValid(refinery)||!compatible(*a,*refinery)){
    const Actor* replacement=nearest(*a);uint32_t id=replacement?replacement->id:0;
    if(i->refinery!=id){i->refinery=id;i->stage=ore::Waiting;i->waitCell=0;i->repaths=i->dockAttempts=0;i->progressFrame=world.frame;i->sent=-100000;i->bypassUsed=i->priority=false;i->readySince=-1;}
   }++i;
  }
  if(!(flags&AutoOreReturn)){
   oreTurns_.clear();
   for(auto i=ore_.begin();i!=ore_.end();){if(i->stage==ore::Docking||i->stage==ore::Yielding||!i->refinery){i=ore_.erase(i);continue;}
    if(add(out,{i->id,i->token,0,i->refinery,7,0x34,2,i->refinery}))++i;else break;
   }return merge(combat,out);
  }
  oreIDs_.clear();for(const auto&job:ore_)oreIDs_.insert(job.id);
  std::vector<const Actor*> newTrucks;
  for(const auto*a:trucks_)if(full(*a)&&!(a->flags&OrePaused)&&!active.count(a->id)&&ore::automaticMission(a->mission)&&!oreIDs_.count(a->id))newTrucks.push_back(a);
  std::sort(newTrucks.begin(),newTrucks.end(),[](auto*a,auto*b){return a->id<b->id;});
  for(const auto*a:newTrucks){if(ore_.size()>=ore::MaxJobs)break;const Actor* refinery=nearest(*a);if(!refinery)continue;
   OreJob job{a->id,refinery->id,a->manualVersion};job.x=a->x;job.y=a->y;job.progressFrame=world.frame;job.initialCargo=load(*a);job.dockingObserved=(a->flags&Unloading)!=0;
   if(a->flags&Unloading)job.stage=ore::Docking;ore_.push_back(job);
  }
  // A stalled native Enter must not reserve this dock forever. Yield only
  // our reservation; retain the game's existing task and never pull a truck
  // out of an observed radio handshake or active unloading.
  for(auto&job:ore_){const Actor*a=actor(job.id),*r=actor(job.refinery);if(!a||!r)continue;
   bool moved=std::abs(int64_t(a->x)-job.x)+std::abs(int64_t(a->y)-job.y)>=32;
   if(moved){job.x=a->x;job.y=a->y;job.progressFrame=world.frame;if(job.stage==ore::Yielding)job.stage=ore::Docking;}
   bool linked=false;for(uint32_t id:r->radioIDs)if(id==job.id)linked=true;
   if((a->flags&Unloading)||linked){job.dockingObserved=true;job.stage=ore::Docking;job.progressFrame=world.frame;}
   else if(job.stage==ore::Docking&&world.frame-job.sent>=ore::DockStallFrames&&world.frame-job.progressFrame>=ore::DockStallFrames){job.stage=ore::Yielding;job.waitCell=0;}
  }
  // Arrival is physical, never inferred from an accepted OutList event.
  for(auto&job:ore_){const Actor*a=actor(job.id),*r=actor(job.refinery);
   if(a&&r&&dockDistance(*a,*r)<=ore::NearDockCells){if(job.readySince<0)job.readySince=world.frame;}else job.readySince=-1;
  }
  // End a turn on completion, cancellation, refinery change or bounded failure.
  // A linked/unloading unit is still protected by native occupancy afterwards.
  for(auto it=oreTurns_.begin();it!=oreTurns_.end();){auto&t=it->second;auto*w=oreJob(t.winner),*d=oreJob(t.displaced);const Actor*a=actor(t.winner),*b=actor(t.displaced),*r=actor(it->first);
   bool invalid=!w||!d||w->refinery!=it->first||d->refinery!=it->first||!refineryValid(r)||!a||!b||w->token!=t.winnerToken||d->token!=t.displacedToken;
   bool timeout=world.frame-t.started>=(t.released?ore::TurnTimeoutFrames:ore::YieldTimeoutFrames);
   bool cancel=!invalid&&((!t.released&&((b->flags&Unloading)||dockDistance(*b,*r)<=ore::FarDockCells||w->readySince<0))||w->stage==ore::Yielding);
   if(invalid||timeout||cancel){
    if(timeout)++oreTurnTimeouts;
    if(d&&d->refinery==it->first&&t.releaseAccepted&&b&&r&&!(b->flags&Unloading)&&!linked(*r,d->id))d->stage=ore::Waiting;
    it=oreTurns_.erase(it);continue;
   }
   // Admission is not execution: wait for both the native Move to be observed
   // and the old radio reservation to disappear before issuing the new Enter.
   if(t.releaseAccepted&&!t.released&&!linked(*r,d->id)&&b->destination!=r->id&&b->mission==2){t.released=true;d->stage=ore::Approaching;w->progressFrame=world.frame;}
   ++it;
  }
  waitingCells_.clear();for(const auto&job:ore_)if(job.waitCell&&job.stage!=ore::Docking)waitingCells_.insert(job.waitCell);
  std::unordered_map<uint32_t,uint32_t> heads;
  auto rank=[&](const OreJob&j){const Actor*a=actor(j.id),*r=actor(j.refinery);
   if(a&&r&&((a->flags&Unloading)||linked(*r,j.id)))return 0;
   if(j.stage==ore::Docking)return 1;return j.priority?2:3;
  };
  for(const auto&job:ore_){if(job.stage==ore::Yielding)continue;auto h=heads.find(job.refinery);
   if(h==heads.end()||rank(job)<rank(*oreJob(h->second)))heads[job.refinery]=job.id;
  }
  // One bypass per load cycle. The displaced FIFO owner gets the next turn;
  // the winner is also non-preemptible. No repeated proximity-based swapping.
  for(auto&headJob:ore_){uint32_t refineryID=headJob.refinery;if(heads[refineryID]!=headJob.id||oreTurns_.count(refineryID))continue;auto*d=&headJob;const Actor*b=actor(d->id),*r=actor(refineryID);
   if(!b||!r||d->bypassUsed||(b->flags&Unloading)||dockDistance(*b,*r)<=ore::FarDockCells||occupied(*r,d->id))continue;
   OreJob*w=nullptr;
   for(auto&j:ore_)if(j.refinery==refineryID&&j.id!=d->id&&j.readySince>=0&&j.stage!=ore::Yielding){const Actor*a=actor(j.id);
    if(a&&!(a->flags&Unloading)&&(!w||j.readySince<w->readySince))w=&j;
   }
   if(!w)continue;
   uint32_t cell=waitingCell(*d);if(!cell)continue; // No safe native Move, no takeover.
   if(d->waitCell)waitingCells_.erase(d->waitCell);d->waitCell=cell;waitingCells_.insert(cell);
   d->bypassUsed=d->priority=w->bypassUsed=true;++oreBypasses;oreTurns_[refineryID]={w->id,d->id,w->token,d->token,world.frame};
  }
  // Submit releases before regular waiting moves so a large queue cannot
  // consume the entire order budget. Retrying is bounded even on rejection.
  for(auto&job:ore_){auto it=oreTurns_.find(job.refinery);if(it==oreTurns_.end()||it->second.displaced!=job.id)continue;auto&t=it->second;
   if(t.released||t.attempts>=ore::MaxDockAttempts||world.frame-t.sent<ore::YieldRetryFrames)continue;
   const auto*c=sample(job,job.waitCell);if(!c||c->occupation||c->land==2||c->land==7){waitingCells_.erase(job.waitCell);job.waitCell=waitingCell(job);if(job.waitCell)waitingCells_.insert(job.waitCell);}
   if(job.waitCell&&add(out,{job.id,job.token,0,job.waitCell,2,11,OreYield,job.refinery})){++t.attempts;++oreYieldAttempts;t.sent=world.frame;}
  }
  for(auto&job:ore_){const Actor* a=actor(job.id),*refinery=actor(job.refinery);if(!a||!refinery){++queued;continue;}
   auto turn=oreTurns_.find(job.refinery);
   if(turn!=oreTurns_.end()){
    const auto&t=turn->second;
    if(job.id==t.displaced||(!t.released&&job.id==t.winner)){++queued;continue;}
   }
   if(job.stage==ore::Yielding){++queued;++yielded;continue;}
   bool first=turn!=oreTurns_.end()?turn->second.winner==job.id:heads[job.refinery]==job.id;
   bool busy=occupied(*refinery,job.id);
   if(job.stage==ore::Docking){++docking;
    if(first&&a->destination!=job.refinery&&!(a->flags&Unloading)&&!busy&&job.dockAttempts<ore::MaxDockAttempts&&world.frame-job.sent>=90)add(out,{job.id,job.token,0,job.refinery,7,0x34,0,job.refinery});
    continue;
   }
   if(first&&!busy){if(add(out,{job.id,job.token,0,job.refinery,7,0x34,0,job.refinery}))++docking;else ++queued;continue;}
   ++queued;
   if(job.stage==ore::Waiting&&job.waitCell){const auto*c=sample(job,job.waitCell);
    if(!c||c->occupation||c->land==2||c->land==7){waitingCells_.erase(job.waitCell);job.waitCell=0;}
   }
   if(std::abs(a->x-job.x)+std::abs(a->y-job.y)>=32){job.x=a->x;job.y=a->y;job.progressFrame=world.frame;}
   if(job.stage==ore::Approaching){
    uint32_t here=uint32_t((a->y/256)*1000+a->x/256);if(here==job.waitCell)continue;
    const auto*cell=sample(job,job.waitCell);bool blocked=!cell||cell->occupation||cell->land==2||cell->land==7;
    if(world.frame-job.sent<30||(!blocked&&world.frame-job.progressFrame<90)||job.repaths>=3)continue;
    uint32_t replacement=waitingCell(job);if(!replacement)continue;
    waitingCells_.erase(job.waitCell);job.waitCell=replacement;waitingCells_.insert(replacement);job.stage=ore::Waiting;job.progressFrame=world.frame;++job.repaths;
   }
   if(!job.waitCell){job.waitCell=waitingCell(job);if(job.waitCell)waitingCells_.insert(job.waitCell);}
   if(job.waitCell)add(out,{job.id,job.token,0,job.waitCell,2,11,0,job.refinery});
  }
  return merge(combat,out);
 }
};
}}


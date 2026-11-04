// Explicit middle-button orders captured by the existing Runtime message
// boundary. All native events use nativeMission / submitConstructed; no queue
// copies, mission setters, native RNG, or private receiver hooks live here.
#include "mo_orders.h"
#include "mo_patrol_graph.h"
struct MoActor {uint32_t pointer=0,id=0,token=0;};
struct MoTarget {uint32_t pointer=0,id=0,type=0;int assigned=0,reserved=0,pending=0;};
struct MoQueued {MoActor actor;uint32_t target=0,targetPointer=0,targetType=0;int mission=0,entrySlots=0;};
static MoActor moActors[moorders::Actors];static int moActorCount=0;
static MoTarget moTargets[moorders::Targets];static int moTargetCount=0;
static MoQueued moQueue[moorders::Actors];static int moQueueCount=0,moQueueAt=0;
static moorders::Point moStroke[moorders::Nodes],moRoute[moorders::Waypoints];
static moorders::Point moCurrent;
static int moStrokeCount=0,moRouteCount=0,moMode=0,moStage=0,moNodeAt=0,moActorAt=0;
static uint32_t moEpoch=0,moHouse=0,moStarted=0,moPreviewAt=0,moOutlineAt=0,moHover=0;
static bool moOutlineDirty=false;
static int moStageFrame=0,moStartFrame=0;static bool moHolding=false,moFeedback=false;
static bool moWaypointSubmitted=false;static mopatrol::Path moPaths[moorders::Actors];
static int moSubmitting=0,moSubmittingActor=-1;
template<class Work>static void moSubmission(int kind,int index,Work&& work){
 const int previous=moSubmitting,actor=moSubmittingActor;
 runtimeunwind::run([&]{moSubmitting=kind;moSubmittingActor=index;work();},[&]{moSubmitting=previous;moSubmittingActor=actor;});
}
static uint32_t moGestureButton=0,moSuppressedButton=0;static moorders::GesturePolicy moCapturedGestures;
static moorders::Options moCapturedOptions;
static bool moCapturedPolicy();
#include "mo_patrol_planning_bridge.h"
static moorders::Preview moPreview;
static bool moOrdersPending(){return moHolding||moStage||moQueueAt<moQueueCount||moPlanningUI.draining;}
static void moOrdersPublish(){
 if(!sh)return;moPreview.match=match;moPreview.tick=sampleClock(sh);double scale=0;if(read(0xB0CD48,&scale,8)&&std::isfinite(scale)&&scale>0&&scale<=1)moPreview.zScale=scale;
 InterlockedIncrement(&sh->moOrdersPreviewSeq);MemoryBarrier();sh->moOrdersPreview=moPreview;MemoryBarrier();InterlockedIncrement(&sh->moOrdersPreviewSeq);notifyHost();
}
static void resetMoOrders(){
 if(moWaypointSubmitted&&sh)sh->moOrderStatus=uint32_t(-16);moWaypointSubmitted=false;
 moHolding=false;moStage=moQueueCount=moQueueAt=moActorCount=moStrokeCount=moTargetCount=moRouteCount=0;moPreview={};moCurrent={};moOutlineDirty=false;moMode=0;moFeedback=false;moGestureButton=0;if(moEpoch!=match||moHouse!=lastHouse)moSuppressedButton=0;moPlanningReset();moOrdersPublish();
}
static bool moOrdersContext(bool preview=false){
 // A brief host render stall must not destroy a read-only captured preview.
 // Capture, release and every event submission retain the strict 150ms gate.
 if(!sh||fault||sh->stop||!nativeReady()||!sh->foreground||DWORD(sampleClock(sh)-sh->heartbeat)>(preview?1000u:150u)||get<bool>(0x87F7E8+0x11B1))return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return foreground==GetCurrentProcessId();
}
static bool moActorValid(const MoActor&a){
 uint32_t p=worldByID(a.id);int k=p?kind(p):-1;
 return p==a.pointer&&p&&own(p,lastHouse)&&k>=0&&k<3&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C)&&!get<uint32_t>(p+0x2B0)&&manualVersions.get(a.id)==a.token;
}
static bool moCapture(){
 moActorCount=0;Vec selected=vec(0xA8ECB8);
 if(selected.count<=0||selected.count>moorders::Actors)return false;
 for(int n=0;n<selected.count;++n){uint32_t p=item(selected,n);int k=p&&worldObjects.contains(p)?kind(p):-1;
  if(k<0||k>=3||!own(p,lastHouse)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2B0)||stickyOwns(p)||get<int>(p+0xAC)==6||get<int>(p+0xB4)==6)return false;
  uint32_t id=get<uint32_t>(p+0x10);if(!manualVersions.trackable(id))return false;
  for(int j=0;j<moActorCount;++j)if(moActors[j].id==id)return false;
  moActors[moActorCount++]={p,id,manualVersions.get(id)};
 }return moActorCount>0;
}
static bool moSameSelection(){
 Vec selected=vec(0xA8ECB8);if(selected.count!=moActorCount)return false;
 for(int i=0;i<moActorCount;++i){if(!moActorValid(moActors[i]))return false;bool found=false;for(int j=0;j<selected.count;++j)found|=item(selected,j)==moActors[i].pointer;if(!found)return false;}return true;
}
static bool moPoint(MSG*msg,moorders::Point&cell,uint32_t*hover=nullptr){
 int x=0,y=0,px=0,py=0;if(!formationPoint(msg,x,y,px,py,nullptr,hover))return false;cell={x/256,y/256,0};return true;
}
static bool moNeutral(uint32_t p){uint32_t owner=get<uint32_t>(p+0x21C),country=owner>=0x10000?get<uint32_t>(owner+0x34):0;return !owner||(country>=0x10000&&get<bool>(country+0x1A6));}
static bool moEngineer(uint32_t p){return kind(p)==1&&get<bool>(typeOf(p,1)+0xEC3);}
static bool moCivilBuilding(uint32_t p){return kind(p)==3&&get<int>(typeOf(p,3)+0x634)==-1&&moNeutral(p);}
static bool moDamaged(uint32_t p){uint32_t t=typeOf(p,kind(p));int hp=get<int>(p+0x6C),maximum=t?get<int>(t+0xA0):0;return hp>0&&maximum>hp&&maximum<=10000000;}
// Behaviour reference: MO_ExperienceEnhancements@80850c7a SmartArea.cpp
// SA_BldFix / SA_GarrQuota. These are original building flags; the native
// compatibility query and the normal Enter event retain their authority.
static bool moOwnInfantryAbsorb(uint32_t p){
 if(kind(p)!=3)return false;uint32_t owner=0,t=typeOf(p,3);uint8_t absorb=0;
 return t&&read(p+0x21C,&owner,4)&&owner==lastHouse&&read(t+0x16AF,&absorb,1)&&absorb;
}
static bool moBuildingEnterFallback(uint32_t p,uint32_t q,unsigned action,bool engineer){
 if((action!=1&&action!=2)||engineer||kind(q)!=3)return false;
 uint32_t owner=0,t=typeOf(q,3);uint8_t a=0,b=0;const int sk=kind(p);
 if(!t||!read(q+0x21C,&owner,4)||owner!=lastHouse)return false;
 if(sk==1){if(!read(t+0x16AF,&a,1)||!a)return false;}
 else if(sk==2){if(!read(t+0x16AB,&a,1)||!read(t+0x16AE,&b,1)||(!a&&!b))return false;}
 else return false;
 return clickView.entryCompatible(p,q,lastHouse);
}
static int moActionMission(uint32_t p,uint32_t q){
 unsigned action=0;if(!GuardedEnterAction(get<uint32_t>(get<uint32_t>(p)+0x74),p,q,&action))return -1;
 const int sk=kind(p),tk=kind(q);const bool engineer=moEngineer(p);
 // Select/Move on our bunker/absorber is a UI omission, not a cell Move.
 // Enter must carry the building object as Destination. Engineers keep the
 // original repair/capture path, and allied/foreign buildings do not opt in.
 if(moBuildingEnterFallback(p,q,action,engineer))return 7;
 // Tank bunkers / repair docks use the already verified native radio/Enter
 // route. Their building destination must not be treated as infantry sabotage.
 if((action==3||action==9||action==16||action==26)&&clickView.entryCompatible(p,q,lastHouse)){
  auto capacity=clickView.capacity(q);if(!capacity.occupants)return 7;
 }
 // Reproduce the per-class native action remap before mapping an event.
 if(sk==1){if(action==28||action==29)action=9;else if(action==27||action==57)action=5;}
 if(sk==2){if(action==3)action=1;else if(action==9||action==16||action==27||action==29)action=5;}
 int mission=-1;
 if(action==5||action==10||action==28||action==53||action==55||action==56||action==62||action==63||action==64||action==71)mission=1;
 else if(action==3)mission=tk==3?17:7;
 else if(action==9)mission=8;
 else if(action==16||(action==26&&tk==3))mission=17;
 else if((action==11||action==29)&&tk==3&&engineer)mission=8;
 // An engineer repairing a neutral civilian or own building is consumed;
 // never waste one on a full-health building, or direct a guessed attack.
 if(engineer&&tk==3&&(mission==7||mission==8||mission==17)&&
    (get<uint32_t>(q+0x21C)==lastHouse||moCivilBuilding(q))&&!moDamaged(q))return -1;
 return mission;
}
static void moAddTarget(uint32_t p){
 if(!p||!worldObjects.contains(p)||!live(p)||kind(p)<0||get<uint32_t>(p+0x11C)||get<int>(p+0x220)!=0)return;
 if(moMode==int(moorders::Fuzzy)&&!nativeEnemy(p,lastHouse))return;
 uint32_t id=get<uint32_t>(p+0x10);for(int i=0;i<moTargetCount;++i)if(moTargets[i].id==id)return;
 if(moTargetCount<moorders::Targets)moTargets[moTargetCount++]={p,id,typeOf(p,kind(p)),0,0};
}
static void moFindTargets(){
 moTargetCount=0;moAddTarget(moHover);
 // Same cell-content chains used by native clicking. Walking the whole chain
 // keeps all three infantry subcells and multi-cell buildings discoverable.
 for(int node=moStrokeCount;node>=0&&moTargetCount<moorders::Targets;--node){
  if(moMode==int(moorders::Fuzzy)&&node!=moStrokeCount)break;
  if(node<moStrokeCount&&moStroke[node].x==moCurrent.x&&moStroke[node].y==moCurrent.y)continue;
  auto center=node==moStrokeCount?moCurrent:moStroke[node];
  const int half=int(moCapturedOptions.range/2),end=int(moCapturedOptions.range)-half;
  for(int radius=0;radius<=std::max(half,end-1);++radius)for(int y=-half;y<end;++y)for(int x=-half;x<end;++x){if(std::max(std::abs(x),std::abs(y))!=radius)continue;uint32_t cell=readMapCell(center.x+x,center.y+y);if(!cell)continue;
   uint32_t p=get<uint32_t>(cell+0xE4);for(int steps=0;p>=0x10000&&steps<256;++steps){
    uint32_t next=0;if(!read(p+0x30,&next,sizeof next))break;moAddTarget(p);if(next==p)break;p=next;
   }
  }
 }
}
static moorders::Point moCoords(uint32_t p){return {get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4)};}
static moorders::Point moGround(moorders::Point p){bool valid=readMapCell(p.x,p.y)!=0;p.x=p.x*256+128;p.y=p.y*256+128;p.z=valid?call<int>(0x578080,0x87F7E8,&p):0;return p;}
static void moLine(moorders::Point a,moorders::Point b){if(moPreview.count<moorders::Lines)moPreview.lines[moPreview.count++]={a,b};}
static int moPendingEntry(const MoActor&a,const MoTarget&t){
 // Most candidates have no prior entry order. Scan the world once per target,
 // not once per candidate pair; only an actor replacing its own reservation
 // needs the existing exact ignore-ID query.
 if(clickView.entryDestination(a.pointer)==t.pointer)return entryReserved(t.pointer,a.id,lastHouse);
 for(int i=0;i<entryReservationCount;++i)if(entryReservations[i].id==a.id&&entryReservations[i].target==t.id)return entryReserved(t.pointer,a.id,lastHouse);
 return t.pending;
}
static bool moAssign(int actor,int target,bool commit,int*missionOut=nullptr){
 auto&a=moActors[actor];auto&t=moTargets[target];if(!moActorValid(a)||worldByID(t.id)!=t.pointer||!live(t.pointer))return false;
 int mission=moActionMission(a.pointer,t.pointer);if(moMode==int(moorders::Fuzzy)&&mission!=1)return false;if(mission<0)return false;
 bool entry=mission==7||mission==8||mission==17;bool engineer=moEngineer(a.pointer);int tk=kind(t.pointer),slots=0;
 bool single=entry&&((engineer&&tk==3&&(get<uint32_t>(t.pointer+0x21C)==lastHouse||moNeutral(t.pointer)))||
                       (tk==2&&(mission==8||(mission==7&&get<uint32_t>(t.pointer+0x21C)!=lastHouse))));
 if(single&&t.assigned)return false;
 if(entry&&!engineer){auto capacity=clickView.capacity(t.pointer);if(capacity.maximum>0){slots=clickView.entrySlots(a.pointer,t.pointer);int pending=moPendingEntry(a,t);if(slots<=0||int64_t(capacity.used)+pending+t.reserved+slots>capacity.maximum)return false;}else if(moOwnInfantryAbsorb(t.pointer))return false;}
 if(missionOut)*missionOut=mission;if(!commit)return true;
 ++t.assigned;t.reserved+=slots;moLine(moCoords(a.pointer),moCoords(t.pointer));
 if(!moHolding&&moQueueCount<moorders::Actors)moQueue[moQueueCount++]={a,t.id,t.pointer,t.type,mission,slots};return true;
}
static void moAssignments(bool preview){
 moFindTargets();moPreview.count=0;if(!preview)moQueueCount=moQueueAt=0;
 const int count=std::min(moActorCount,preview?64:moorders::Actors),targets=std::min(moTargetCount,preview?96:moorders::Targets);
 for(int j=0;j<targets;++j){auto&t=moTargets[j];t.pending=moMode==int(moorders::Area)&&clickView.capacity(t.pointer).maximum>0?entryReserved(t.pointer,0,lastHouse):0;}
 bool assigned[moorders::Actors]{};
 if(moMode==int(moorders::Fuzzy)){
  int groups=moorders::groups(count,targets,int(moCapturedOptions.squad));for(int i=0;i<count&&groups;++i){int first=moorders::groupTarget(i,count,groups);
   for(int offset=0;offset<groups;++offset){int j=(first+offset)%groups;if(moAssign(i,j,true)){assigned[i]=true;break;}}
  }
 }else{
  // Civilian garrisons get a round-robin prepass before attack/capture squads.
  for(int pass=0;pass<count;++pass){bool progress=false;for(int j=0;j<targets;++j)if(moCivilBuilding(moTargets[j].pointer))for(int i=0;i<count;++i){if(assigned[i]||moEngineer(moActors[i].pointer))continue;int mission=-1;
    if(moAssign(i,j,false,&mission)&&(mission==8||mission==17)&&moAssign(i,j,true)){assigned[i]=true;progress=true;break;}}
   if(!progress)break;
  }
  // Once civilian garrisons have taken their prepass, spread the remaining
  // infantry over our absorbers one per building per round. A nearer building
  // must not consume the remainder before its equally available neighbour.
  for(int pass=0;pass<count;++pass){bool progress=false;
   for(int j=0;j<targets;++j)if(moOwnInfantryAbsorb(moTargets[j].pointer)){
    for(int i=0;i<count;++i)if(!assigned[i]&&kind(moActors[i].pointer)==1&&!moEngineer(moActors[i].pointer)){
     int mission=-1;if(moAssign(i,j,false,&mission)&&mission==7&&moAssign(i,j,true)){assigned[i]=true;progress=true;break;}
    }
   }if(!progress)break;
  }
  for(int j=0;j<targets;++j)for(int i=0;i<count&&moTargets[j].assigned<int(moCapturedOptions.squad);++i){if(!assigned[i]&&moAssign(i,j,true))assigned[i]=true;}
  for(int i=0;i<count;++i)if(!assigned[i]){
   // Remaining units select the closest compatible target, preserving quotas.
   int best=-1;int64_t nearest=INT64_MAX;auto a=moCoords(moActors[i].pointer);
   for(int j=0;j<targets;++j)if(moAssign(i,j,false)){auto b=moCoords(moTargets[j].pointer);int64_t dx=int64_t(a.x)-b.x,dy=int64_t(a.y)-b.y,d=dx*dx+dy*dy;if(d<nearest){nearest=d;best=j;}}
   if(best>=0&&moAssign(i,best,true))assigned[i]=true;
  }
 }
 if(!preview)for(int i=0;i<count;++i)if(!assigned[i])++sh->moOrderRejected;
}
static bool moGraphEmpty(){
 mopatrol::Actor actors[moorders::Actors];for(int i=0;i<moActorCount;++i)actors[i]={moActors[i].pointer,moActors[i].id};
 return mopatrol::empty([](uint32_t at,void*out,size_t n){return read(at,out,n);},lastHouse,actors,moActorCount);
}
static bool moGraphRoute(int index,bool closed,bool bind=false){
 auto&a=moActors[index];if(!moActorValid(a))return false;uint32_t cells[moorders::Waypoints];for(int n=0;n<moRouteCount;++n)cells[n]=moRoute[n].y*1000+moRoute[n].x;
 mopatrol::Path found;const auto*bound=bind?nullptr:&moPaths[index];
 if(!mopatrol::route([](uint32_t at,void*out,size_t n){return read(at,out,n);},lastHouse,{a.pointer,a.id},cells,moRouteCount,found,closed,bound)||found.pending!=0)return false;
 if(bind)moPaths[index]=std::move(found);return true;
}
static bool moGraphCommitReady(){
 mopatrol::Actor actors[moorders::Actors];for(int i=0;i<moActorCount;++i){auto&a=moActors[i];if(!actorLeaseArbiter.canIssue(match,get<int>(0xA8ED84),a.id,a.pointer,a.token,taskarbiter::Owner::Generic)||!moGraphRoute(i,true))return false;actors[i]={a.pointer,a.id};}
 return mopatrol::exclusive([](uint32_t at,void*out,size_t n){return read(at,out,n);},lastHouse,actors,moActorCount);
}
static bool moEntryReservationRoom(uint32_t actor){
 if(entryReservationCount<cooperative::MaxSources)return true;
 for(int i=0;i<entryReservationCount;++i)if(entryReservations[i].id==actor)return true;return false;
}
static bool moEntryDispatchable(const MoQueued&q){
 if(q.entrySlots<=0||q.mission!=7||!moActorValid(q.actor)||!moEntryReservationRoom(q.actor.id)||
    worldByID(q.target)!=q.targetPointer||!live(q.targetPointer)||typeOf(q.targetPointer,kind(q.targetPointer))!=q.targetType||
    moActionMission(q.actor.pointer,q.targetPointer)!=q.mission)return false;
 const auto c=clickView.capacity(q.targetPointer);const int slots=clickView.entrySlots(q.actor.pointer,q.targetPointer);
 return c.maximum>0&&c.used>=0&&slots==q.entrySlots&&
  int64_t(c.used)+entryReserved(q.targetPointer,q.actor.id,lastHouse)+slots<=c.maximum;
}
static void moRememberEntry(const MoQueued&q,int frame){
 // Share pending reservations with the existing boarding path: orders already
 // in OutList are not yet reflected in Passengers. Replace this actor's prior
 // reservation only AFTER the replacement event actually queued.
 for(int i=0;i<entryReservationCount;)if(entryReservations[i].id==q.actor.id)entryReservations[i]=entryReservations[--entryReservationCount];else ++i;
 if(entryReservationCount<cooperative::MaxSources)entryReservations[entryReservationCount++]={q.actor.id,q.target,q.targetType,q.actor.token,frame,q.entrySlots};
}
static bool validateMoSubmission(const intent::EventOrder&order){
 const bool connect=order.domain==vanilla::Domain::Waypoints;
 if(!moSubmitting)return !connect;
 if(!moOrdersContext()||moEpoch!=match||moHouse!=lastHouse||!moCapturedPolicy())return false;
 if(moSubmitting==4){
  if(moSubmittingActor<0||moSubmittingActor>=moQueueCount)return false;const auto&q=moQueue[moSubmittingActor];
  return order.domain==vanilla::Domain::MegaMission&&!order.appendWaypoint&&order.player==get<int>(lastHouse+0x30)&&
   uint32_t(order.a)==q.actor.id&&order.mission==7&&!order.b&&uint32_t(order.c)==q.target&&order.tagC==0x34&&moEntryDispatchable(q);
 }
 if(moSubmitting==3)return order.domain==vanilla::Domain::NoOptions&&order.eventKind==vanilla::EventType::PlanCommit&&moGraphCommitReady();
 if(moSubmittingActor<0||moSubmittingActor>=moActorCount)return false;const auto&a=moActors[moSubmittingActor];
 if(!moActorValid(a)||uint32_t(order.a)!=a.id||order.player!=get<int>(lastHouse+0x30))return false;
 if(moSubmitting==1)return order.domain==vanilla::Domain::MegaMission&&order.appendWaypoint;
 return connect&&order.eventKind==vanilla::EventType::PlanConnect&&actorLeaseArbiter.canIssue(match,get<int>(0xA8ED84),a.id,a.pointer,a.token,taskarbiter::Owner::Generic)&&moGraphRoute(moSubmittingActor,false);
}
// The upstream draws its current-cell outline every render while throttling
// target assignment independently. A sparse route node is never the cursor.
static void moUpdateOutline(){
 if(moMode==int(moorders::Patrol)){
  moorders::Point stroke[moorders::Nodes],route[moorders::Waypoints];int count=moStrokeCount;
  std::copy(moStroke,moStroke+count,stroke);moorders::sample(stroke,count,moCurrent,true);
  int n=moorders::patrol(stroke,count,route,moCapturedOptions);bool valid=n>=4;moPreview.count=0;
  for(int i=0;i<n;++i)valid&=readMapCell(route[i].x,route[i].y)!=0;
  for(int i=0;i<n;++i){moPreview.nodes[i]=moGround(route[i]);if(valid)moLine(moPreview.nodes[i],moGround(route[(i+1)%n]));}moPreview.nodeCount=n;
 }else{
  auto center=moCurrent;const int half=int(moCapturedOptions.range/2),end=int(moCapturedOptions.range)-half;moorders::Point corners[]={{center.x-half,center.y-half},{center.x+end,center.y-half},{center.x+end,center.y+end},{center.x-half,center.y+end}};
  for(int i=0;i<4;++i)moPreview.nodes[i]=moGround(corners[i]);moPreview.nodeCount=4;
 }
 moOutlineDirty=false;moOutlineAt=sampleClock(sh);
}
static void moBuildPreview(){
 moPreview={};moPreview.active=moHolding;moPreview.mode=moMode;
 if(moMode!=int(moorders::Patrol))moAssignments(true);
 moUpdateOutline();moPreviewAt=sampleClock(sh);moOrdersPublish();
}
static void moSamplePreview(moorders::Point point){
 moOutlineDirty|=moCurrent.x!=point.x||moCurrent.y!=point.y;moCurrent=point;moorders::sample(moStroke,moStrokeCount,point,false,moMode==int(moorders::Patrol)?int(moCapturedOptions.nodeStep):2);
 const DWORD now=sampleClock(sh);
 // Target legality/assignment is bounded separately from visual tracking.
 if(moMode!=int(moorders::Patrol)&&DWORD(now-moPreviewAt)>=80)moBuildPreview();
 else if(moOutlineDirty&&DWORD(now-moOutlineAt)>=16){moUpdateOutline();moOrdersPublish();}
}
static void moCommit(){
 if(!moSameSelection()){resetMoOrders();return;}
 sh->moOrderAccepted=sh->moOrderRejected=0;moHolding=false;moFeedback=true;moStarted=sampleClock(sh);moStartFrame=get<int>(0xA8ED84);
 if(moMode==int(moorders::Patrol)){
  if(!moGraphEmpty()){sh->moOrderStatus=uint32_t(-17);resetMoOrders();return;}
  moRouteCount=moorders::patrol(moStroke,moStrokeCount,moRoute,moCapturedOptions);
  for(int i=0;i<moRouteCount;++i)if(!readMapCell(moRoute[i].x,moRoute[i].y)){sh->moOrderStatus=uint32_t(-4);sh->moOrderRejected=moActorCount;resetMoOrders();return;}
 }
 // Explicit player intent cancels the same automatic jobs as a native click.
 // Subsequent manual input advances these tokens and cancels the pending tail.
 for(int i=0;i<moActorCount;++i){cancelNative(moActors[i].pointer,false);moActors[i].token=manualVersions.get(moActors[i].id);}
 if(moMode==int(moorders::Patrol)){
  for(int i=0;i<moActorCount;++i)moPaths[i]={};
  moStage=1;moNodeAt=moActorAt=0;
 }else moAssignments(false);
 moPreview.active=0;moOrdersPublish();sh->moOrderStatus=1;
}
static bool moCapturedPolicy(){return moReadGestures()&&moorders::sameGestures(moCapturedGestures,moRuntimeGestures)&&moorders::sameOptions(moCapturedOptions,moRuntimeOptions)&&(moGestureEnabled()&uint32_t(moMode))&&!(moorders::gestureConflicts(moRuntimeGestures,moGestureEnabled())&uint32_t(moMode));}
static bool moCapturedHold(){
 if(!moCapturedPolicy())return false;const int index=moorders::gestureIndex(uint32_t(moMode));if(index<0)return false;
 uint8_t keys[256]{};moGestureKeyboard(keys);return moorders::gestureHeld(keys,moCapturedGestures.keys[index]);
}
static bool moOrdersMessage(MSG*msg){
 auto swallow=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(moPlanningMessage(msg))return true;
 if(moPlanningUI.draining)return false;
 const auto edge=moorders::gestureMouseEdge(msg->message,uintptr_t(msg->wParam));
 if(edge.up&&edge.button==moSuppressedButton&&!moHolding){moSuppressedButton=0;return swallow();}
 const bool keyDown=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN;
 if(moStage&&keyDown&&msg->wParam==VK_ESCAPE){resetMoOrders();return swallow();}
 if(moHolding&&((keyDown&&msg->wParam==VK_ESCAPE)||(edge.down&&edge.button!=moGestureButton))){bool escape=keyDown&&msg->wParam==VK_ESCAPE;resetMoOrders();return escape?swallow():false;}
 if(edge.up&&edge.button==moGestureButton&&moHolding){
  moSuppressedButton=0;moorders::Point p;if(moOrdersContext()&&moCapturedHold()&&moPoint(msg,p,&moHover)){moCurrent=p;moorders::sample(moStroke,moStrokeCount,p,true);moCommit();}else resetMoOrders();return swallow();
 }
 if(moHolding&&(!moOrdersContext(true)||!moCapturedHold())){resetMoOrders();return false;}
 if(msg->message==WM_MOUSEMOVE&&moHolding){
  moorders::Point p;if(moPoint(msg,p,&moHover))moSamplePreview(p);return swallow();
 }
 if(!edge.down||!moOrdersContext()||moStage||!moReadGestures())return false;
 if(formationView.active&&(!formationHolding||formationModal||formationButton))return false;
 uint8_t keys[256]{};moGestureKeyboard(keys);const auto resolved=moGestureResolve(edge.button,keys);
 if(!resolved.matched)return false;
 if(!resolved.mode){sh->moOrderStatus=uint32_t(-14);moSuppressedButton=edge.button;return swallow();}
 if(resolved.mode==moorders::QuickFormation)return false;
 if(formationView.active)formationCancel();
 moorders::Point point;uint32_t hover=0;if(!moPoint(msg,point,&hover))return false;
 indexWorld(get<int>(0xA8ED84));if(!moCapture()){sh->moOrderStatus=uint32_t(-4);moSuppressedButton=edge.button;return swallow();}
 // The configured keyboard authorizes this capture, but never substitutes for
 // the game's own PlanningMode binding or mutates its local editor state.
 moPlanningReset();moEpoch=match;moHouse=lastHouse;
 if(resolved.mode==moorders::Patrol&&!moGraphEmpty()){sh->moOrderStatus=uint32_t(-17);moSuppressedButton=edge.button;return swallow();}
 if(resolved.mode==moorders::Patrol&&!moPlanningCapture(msg)){sh->moOrderStatus=uint32_t(-15);moSuppressedButton=edge.button;return swallow();}
 moQueueCount=moQueueAt=moStage=0;moMode=int(resolved.mode);moHover=hover;moStrokeCount=0;moCapturedGestures=moRuntimeGestures;moCapturedOptions=moRuntimeOptions;moGestureButton=moSuppressedButton=edge.button;
 moCurrent=point;moorders::sample(moStroke,moStrokeCount,point);moHolding=true;moBuildPreview();return swallow();
}
static void moOrdersTick(uint32_t house,int frame,DWORD now){
 if(!moOrdersPending())return;
 if(moPlanningUI.draining){moPlanningReset();return;}
 if(moEpoch!=match||moHouse!=house||!sh||(sh->moFeatures&moMode)==0||!moOrdersContext(moHolding)||!moCapturedPolicy()){resetMoOrders();return;}
 if(moHolding){if(!moCapturedHold()){resetMoOrders();return;}if(DWORD(now-moPreviewAt)>=400)moBuildPreview();else if(moOutlineDirty&&DWORD(now-moOutlineAt)>=16){moUpdateOutline();moOrdersPublish();}return;}
 if(frame<moStartFrame||frame-moStartFrame>1800||DWORD(now-moStarted)>30000){sh->moOrderStatus=uint32_t(-7);resetMoOrders();return;}
 // Standard-object commands each get one final native capability/identity
 // check immediately before submission. Accepted commands are never retried.
 int visited=0;
 while(moQueueAt<moQueueCount&&outgoingRoom()>0&&visited++<64){auto&q=moQueue[moQueueAt];
  if(!moActorValid(q.actor)||worldByID(q.target)!=q.targetPointer||!live(q.targetPointer)||typeOf(q.targetPointer,kind(q.targetPointer))!=q.targetType||moActionMission(q.actor.pointer,q.targetPointer)!=q.mission){++sh->moOrderRejected;++moQueueAt;continue;}
  const bool guardedEntry=q.entrySlots>0&&q.mission==7;
  if(guardedEntry&&!moEntryDispatchable(q)){++sh->moOrderRejected;++moQueueAt;continue;}
  bool attack=q.mission==1,queued=false;
  moSubmission(guardedEntry?4:0,moQueueAt,[&]{
   // The optional native high-level MP backend bypasses our post-clock guard.
   // Capacity-bound Enter always selects the ordinary constructed transport.
   queued=nativeMission(house,q.actor.id,q.mission,attack?q.target:0,attack?0:q.target,attack?0:0x34,0,0,taskarbiter::Owner::Generic,nullptr,-1,OrderAuthority::Local,false,false,guardedEntry);
   if(!queued)return;if(guardedEntry)moRememberEntry(q,frame);++sh->moOrderAccepted;++moQueueAt;if(moFeedback){moFeedback=false;inputFeedback(q.actor.pointer,q.targetPointer,!attack);}
  });
  if(!queued)break;
 }
 if(!moStage){if(moQueueAt==moQueueCount){sh->moOrderStatus=2;}return;}
 // Native patrol graph: every point for every actor, wait for token material-
 // ization, Connect to the first new node, observe loop, then PlanCommit.
 if(moStage==1){
  while(moNodeAt<moRouteCount&&outgoingRoom()>0){auto&a=moActors[moActorAt];
   if(!moActorValid(a)){++sh->moOrderRejected;resetMoOrders();return;}
   else {uint32_t cell=uint32_t(moRoute[moNodeAt].y*1000+moRoute[moNodeAt].x);bool queued=false;moSubmission(1,moActorAt,[&]{queued=nativeMission(house,a.id,2,0,cell,11,cell,11,taskarbiter::Owner::Generic,nullptr,-1,OrderAuthority::Local,true);if(queued)moWaypointSubmitted=true;});if(!queued)break;}
   if(++moActorAt==moActorCount){moActorAt=0;++moNodeAt;}
  }
  if(moNodeAt==moRouteCount){moStage=2;moStageFrame=frame;moActorAt=0;}
 }
 if(moStage==2&&frame>moStageFrame){bool ready=true;
  for(int i=0;i<moActorCount;++i){if(!moActorValid(moActors[i])){++sh->moOrderRejected;resetMoOrders();return;}ready&=moGraphRoute(i,false,true);}
  if(ready){moStage=3;moActorAt=0;}
 }
 if(moStage==3){while(moActorAt<moActorCount&&outgoingRoom()>0){auto&a=moActors[moActorAt];
   if(!moActorValid(a)){++sh->moOrderRejected;resetMoOrders();return;}
   actorFrameArbiter.begin(match,frame);if(!actorFrameArbiter.canIssue(a.id,a.token))break;
   intent::EventOrder connect{};connect.domain=vanilla::Domain::Waypoints;connect.player=get<int>(house+0x30);connect.eventKind=vanilla::EventType::PlanConnect;connect.a=a.id;connect.tagA=0x34;connect.b=moRoute[0].y*1000+moRoute[0].x;connect.tagB=11;
   bool queued=false;moSubmission(2,moActorAt,[&]{queued=submitConstructed(connect);if(queued){actorFrameArbiter.record(a.id,a.token,taskarbiter::Owner::Generic);++moActorAt;}});if(!queued)break;
  }if(moActorAt==moActorCount){moStage=4;moStageFrame=frame;}
 }
 if(moStage==4&&frame>moStageFrame){bool closed=true;
  for(int i=0;i<moActorCount;++i){if(!moActorValid(moActors[i])){++sh->moOrderRejected;resetMoOrders();return;}closed&=moGraphRoute(i,true);}
  if(closed&&!moCapturedOptions.commit){moPlanningReset();sh->moOrderAccepted+=moActorCount;moStage=0;moWaypointSubmitted=false;sh->moOrderStatus=2;return;}
  moSubmission(3,-1,[&]{if(closed&&outgoingRoom()>0&&moGraphCommitReady()&&moPlanningFinish(house)){
   sh->moOrderAccepted+=moActorCount;moStage=0;moWaypointSubmitted=false;sh->moOrderStatus=2;
  }});
 }
}

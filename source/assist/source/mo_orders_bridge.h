// Explicit middle-button orders captured by the existing Runtime message
// boundary. All native events use nativeMission / submitConstructed; no queue
// copies, mission setters, native RNG, or private receiver hooks live here.
#include "mo_orders.h"
#include "mo_patrol_graph.h"
struct MoActor {uint32_t pointer=0,id=0,token=0;};
struct MoTarget {uint32_t pointer=0,id=0,type=0;int assigned=0,reserved=0;};
struct MoQueued {MoActor actor;uint32_t target=0,targetPointer=0,targetType=0;int mission=0;};
static MoActor moActors[moorders::Actors];static int moActorCount=0;
static MoTarget moTargets[moorders::Targets];static int moTargetCount=0;
static MoQueued moQueue[moorders::Actors];static int moQueueCount=0,moQueueAt=0;
static moorders::Point moStroke[moorders::Nodes],moRoute[moorders::Waypoints];
static int moStrokeCount=0,moRouteCount=0,moMode=0,moStage=0,moNodeAt=0,moActorAt=0;
static uint32_t moEpoch=0,moHouse=0,moStarted=0,moPreviewAt=0,moHover=0;
static int moStageFrame=0,moStartFrame=0;static bool moHolding=false,moFeedback=false;
static bool moWaypointSubmitted=false;static mopatrol::Path moPaths[moorders::Actors];
static int moSubmitting=0,moSubmittingActor=-1;
struct MoSubmission {int previous,actor;MoSubmission(int kind,int index=-1):previous(moSubmitting),actor(moSubmittingActor){moSubmitting=kind;moSubmittingActor=index;}~MoSubmission(){moSubmitting=previous;moSubmittingActor=actor;}};
static uint32_t moGestureButton=0,moSuppressedButton=0;static moorders::GesturePolicy moCapturedGestures;
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
 moHolding=false;moStage=moQueueCount=moQueueAt=moActorCount=moStrokeCount=moTargetCount=moRouteCount=0;moPreview={};moMode=0;moFeedback=false;moGestureButton=0;if(moEpoch!=match||moHouse!=lastHouse)moSuppressedButton=0;moPlanningReset();moOrdersPublish();
}
static bool moOrdersContext(){
 if(!sh||fault||sh->stop||!nativeReady()||!sh->foreground||DWORD(sampleClock(sh)-sh->heartbeat)>150||get<bool>(0x87F7E8+0x11B1))return false;
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
 int x=0,y=0,px=0,py=0;if(!formationPoint(msg,x,y,px,py))return false;cell={x/256,y/256,0};
 if(hover){
  uint32_t tactical=get<uint32_t>(0x887324);RECT r{};if(!GetClientRect(msg->hwnd,&r)||r.right<=0||r.bottom<=0)return false;
  pointerinput::Intent point{};point.operation=pointerinput::AirWaypoint;point.width=get<int>(0x8A00A4);point.height=get<int>(0x8A00A8);point.cameraX=get<int>(tactical+0xB0);point.cameraY=get<int>(tactical+0xB4);
  point.x=int(int64_t(short(LOWORD(msg->lParam)))*point.width/r.right);point.y=int(int64_t(short(HIWORD(msg->lParam)))*point.height/r.bottom);
  const uint32_t oldCell=pointerCell,oldTarget=pointerTarget,oldID=pointerTargetID;const int oldZ=pointerGroundZ;
  bool ok=pointerLocation(point);*hover=ok?pointerTarget:0;pointerCell=oldCell;pointerTarget=oldTarget;pointerTargetID=oldID;pointerGroundZ=oldZ;
 }return true;
}
static bool moNeutral(uint32_t p){uint32_t owner=get<uint32_t>(p+0x21C),country=owner>=0x10000?get<uint32_t>(owner+0x34):0;return !owner||(country>=0x10000&&get<bool>(country+0x1A6));}
static bool moEngineer(uint32_t p){return kind(p)==1&&get<bool>(typeOf(p,1)+0xEC3);}
static bool moCivilBuilding(uint32_t p){return kind(p)==3&&get<int>(typeOf(p,3)+0x634)==-1&&moNeutral(p);}
static bool moDamaged(uint32_t p){uint32_t t=typeOf(p,kind(p));int hp=get<int>(p+0x6C),maximum=t?get<int>(t+0xA0):0;return hp>0&&maximum>hp&&maximum<=10000000;}
static int moActionMission(uint32_t p,uint32_t q){
 unsigned action=0;if(!GuardedEnterAction(get<uint32_t>(get<uint32_t>(p)+0x74),p,q,&action))return -1;
 const int sk=kind(p),tk=kind(q);const bool engineer=moEngineer(p);
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
 for(int node=moStrokeCount-1;node>=0&&moTargetCount<moorders::Targets;--node){
  if(moMode==int(moorders::Fuzzy)&&node!=moStrokeCount-1)continue;
  auto center=moStroke[node];
  for(int radius=0;radius<=2;++radius)for(int y=-2;y<2;++y)for(int x=-2;x<2;++x){if(std::max(std::abs(x),std::abs(y))!=radius)continue;uint32_t cell=readMapCell(center.x+x,center.y+y);if(!cell)continue;
   uint32_t p=get<uint32_t>(cell+0xE4);for(int steps=0;p>=0x10000&&steps<256;++steps){
    uint32_t next=0;if(!read(p+0x30,&next,sizeof next))break;moAddTarget(p);if(next==p)break;p=next;
   }
  }
 }
}
static moorders::Point moCoords(uint32_t p){return {get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4)};}
static moorders::Point moGround(moorders::Point p){bool valid=readMapCell(p.x,p.y)!=0;p.x=p.x*256+128;p.y=p.y*256+128;p.z=valid?call<int>(0x578080,0x87F7E8,&p):0;return p;}
static void moLine(moorders::Point a,moorders::Point b){if(moPreview.count<moorders::Lines)moPreview.lines[moPreview.count++]={a,b};}
static bool moAssign(int actor,int target,bool commit,int*missionOut=nullptr){
 auto&a=moActors[actor];auto&t=moTargets[target];if(!moActorValid(a)||worldByID(t.id)!=t.pointer||!live(t.pointer))return false;
 int mission=moActionMission(a.pointer,t.pointer);if(moMode==int(moorders::Fuzzy)&&mission!=1)return false;if(mission<0)return false;
 bool entry=mission==7||mission==8||mission==17;bool engineer=moEngineer(a.pointer);int tk=kind(t.pointer),slots=0;
 bool single=entry&&((engineer&&tk==3&&(get<uint32_t>(t.pointer+0x21C)==lastHouse||moNeutral(t.pointer)))||
                       (tk==2&&(mission==8||(mission==7&&get<uint32_t>(t.pointer+0x21C)!=lastHouse))));
 if(single&&t.assigned)return false;
 if(entry&&!engineer){auto capacity=clickView.capacity(t.pointer);if(capacity.maximum>0){slots=clickView.entrySlots(a.pointer,t.pointer);if(slots<=0||capacity.used+t.reserved+slots>capacity.maximum)return false;}}
 if(missionOut)*missionOut=mission;if(!commit)return true;
 ++t.assigned;t.reserved+=slots;moLine(moCoords(a.pointer),moCoords(t.pointer));
 if(!moHolding&&moQueueCount<moorders::Actors)moQueue[moQueueCount++]={a,t.id,t.pointer,t.type,mission};return true;
}
static void moAssignments(bool preview){
 moFindTargets();moPreview.count=0;if(!preview)moQueueCount=moQueueAt=0;
 const int count=std::min(moActorCount,preview?64:moorders::Actors),targets=std::min(moTargetCount,preview?96:moorders::Targets);
 bool assigned[moorders::Actors]{};
 if(moMode==int(moorders::Fuzzy)){
  int groups=moorders::groups(count,targets);for(int i=0;i<count&&groups;++i){int first=moorders::groupTarget(i,count,groups);
   for(int offset=0;offset<groups;++offset){int j=(first+offset)%groups;if(moAssign(i,j,true)){assigned[i]=true;break;}}
  }
 }else{
  // Civilian garrisons get a round-robin prepass before attack/capture squads.
  for(int pass=0;pass<count;++pass){bool progress=false;for(int j=0;j<targets;++j)if(moCivilBuilding(moTargets[j].pointer))for(int i=0;i<count;++i){if(assigned[i]||moEngineer(moActors[i].pointer))continue;int mission=-1;
    if(moAssign(i,j,false,&mission)&&(mission==8||mission==17)&&moAssign(i,j,true)){assigned[i]=true;progress=true;break;}}
   if(!progress)break;
  }
  for(int j=0;j<targets;++j)for(int i=0;i<count&&moTargets[j].assigned<4;++i){if(!assigned[i]&&moAssign(i,j,true))assigned[i]=true;}
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
static bool validateMoSubmission(const intent::EventOrder&order){
 const bool connect=order.domain==vanilla::Domain::Waypoints;
 if(!moSubmitting)return !connect;
 if(!moOrdersContext()||moEpoch!=match||moHouse!=lastHouse||!moCapturedPolicy())return false;
 if(moSubmitting==3)return order.domain==vanilla::Domain::NoOptions&&order.eventKind==vanilla::EventType::PlanCommit&&moGraphCommitReady();
 if(moSubmittingActor<0||moSubmittingActor>=moActorCount)return false;const auto&a=moActors[moSubmittingActor];
 if(!moActorValid(a)||uint32_t(order.a)!=a.id||order.player!=get<int>(lastHouse+0x30))return false;
 if(moSubmitting==1)return order.domain==vanilla::Domain::MegaMission&&order.appendWaypoint;
 return connect&&order.eventKind==vanilla::EventType::PlanConnect&&actorLeaseArbiter.canIssue(match,get<int>(0xA8ED84),a.id,a.pointer,a.token,taskarbiter::Owner::Generic)&&moGraphRoute(moSubmittingActor,false);
}
static void moBuildPreview(){
 moPreview={};moPreview.active=moHolding;moPreview.mode=moMode;
 if(moMode==int(moorders::Patrol)){
  moorders::Point route[moorders::Waypoints];int n=moorders::patrol(moStroke,moStrokeCount,route);bool valid=n>=4;
  for(int i=0;i<n;++i)valid&=readMapCell(route[i].x,route[i].y)!=0;
  for(int i=0;i<n;++i){moPreview.nodes[i]=moGround(route[i]);if(valid)moLine(moPreview.nodes[i],moGround(route[(i+1)%n]));}moPreview.nodeCount=n;
 }else{
  moAssignments(true);auto center=moStroke[moStrokeCount-1];moorders::Point corners[]={{center.x-2,center.y-2},{center.x+2,center.y-2},{center.x+2,center.y+2},{center.x-2,center.y+2}};
  for(int i=0;i<4;++i)moPreview.nodes[i]=moGround(corners[i]);moPreview.nodeCount=4;
 }
 moPreviewAt=sampleClock(sh);moOrdersPublish();
}
static void moCommit(){
 if(!moSameSelection()){resetMoOrders();return;}
 sh->moOrderAccepted=sh->moOrderRejected=0;moHolding=false;moFeedback=true;moStarted=sampleClock(sh);moStartFrame=get<int>(0xA8ED84);
 if(moMode==int(moorders::Patrol)){
  if(!moGraphEmpty()){sh->moOrderStatus=uint32_t(-17);resetMoOrders();return;}
  moRouteCount=moorders::patrol(moStroke,moStrokeCount,moRoute);
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
static bool moCapturedPolicy(){return moReadGestures()&&moorders::sameGestures(moCapturedGestures,moRuntimeGestures)&&(moGestureEnabled()&uint32_t(moMode))&&!(moorders::gestureConflicts(moRuntimeGestures,moGestureEnabled())&uint32_t(moMode));}
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
  moSuppressedButton=0;moorders::Point p;if(moOrdersContext()&&moCapturedHold()&&moPoint(msg,p,&moHover)){moorders::sample(moStroke,moStrokeCount,p,true);moCommit();}else resetMoOrders();return swallow();
 }
 if(moHolding&&(!moOrdersContext()||!moCapturedHold())){resetMoOrders();return false;}
 if(msg->message==WM_MOUSEMOVE&&moHolding){
  moorders::Point p;if(moPoint(msg,p,&moHover)){moorders::sample(moStroke,moStrokeCount,p);if(DWORD(sampleClock(sh)-moPreviewAt)>=80)moBuildPreview();}return swallow();
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
 moQueueCount=moQueueAt=moStage=0;moMode=int(resolved.mode);moHover=hover;moStrokeCount=0;moCapturedGestures=moRuntimeGestures;moGestureButton=moSuppressedButton=edge.button;
 moorders::sample(moStroke,moStrokeCount,point);moHolding=true;moBuildPreview();return swallow();
}
static void moOrdersTick(uint32_t house,int frame,DWORD now){
 if(!moOrdersPending())return;
 if(moPlanningUI.draining){moPlanningReset();return;}
 if(moEpoch!=match||moHouse!=house||!sh||(sh->moFeatures&moMode)==0||!moOrdersContext()||!moCapturedPolicy()){resetMoOrders();return;}
 if(moHolding){if(!moCapturedHold()){resetMoOrders();return;}if(DWORD(now-moPreviewAt)>=400)moBuildPreview();return;}
 if(frame<moStartFrame||frame-moStartFrame>1800||DWORD(now-moStarted)>30000){sh->moOrderStatus=uint32_t(-7);resetMoOrders();return;}
 // Standard-object commands each get one final native capability/identity
 // check immediately before submission. Accepted commands are never retried.
 int visited=0;
 while(moQueueAt<moQueueCount&&outgoingRoom()>0&&visited++<24){auto&q=moQueue[moQueueAt];
  if(!moActorValid(q.actor)||worldByID(q.target)!=q.targetPointer||!live(q.targetPointer)||typeOf(q.targetPointer,kind(q.targetPointer))!=q.targetType||moActionMission(q.actor.pointer,q.targetPointer)!=q.mission){++sh->moOrderRejected;++moQueueAt;continue;}
  bool attack=q.mission==1;bool queued=nativeMission(house,q.actor.id,q.mission,attack?q.target:0,attack?0:q.target,attack?0:0x34,0,0,taskarbiter::Owner::Generic);
  if(!queued)break;++sh->moOrderAccepted;++moQueueAt;if(moFeedback){moFeedback=false;inputFeedback(q.actor.pointer,q.targetPointer,!attack);}
 }
 if(!moStage){if(moQueueAt==moQueueCount){sh->moOrderStatus=2;}return;}
 // Native patrol graph: every point for every actor, wait for token material-
 // ization, Connect to the first new node, observe loop, then PlanCommit.
 if(moStage==1){
  while(moNodeAt<moRouteCount&&outgoingRoom()>0){auto&a=moActors[moActorAt];
   if(!moActorValid(a)){++sh->moOrderRejected;resetMoOrders();return;}
   else {uint32_t cell=uint32_t(moRoute[moNodeAt].y*1000+moRoute[moNodeAt].x);MoSubmission scope(1,moActorAt);if(!nativeMission(house,a.id,2,0,cell,11,cell,11,taskarbiter::Owner::Generic,nullptr,-1,OrderAuthority::Local,true))break;moWaypointSubmitted=true;}
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
   MoSubmission scope(2,moActorAt);if(!submitConstructed(connect))break;actorFrameArbiter.record(a.id,a.token,taskarbiter::Owner::Generic);++moActorAt;
  }if(moActorAt==moActorCount){moStage=4;moStageFrame=frame;}
 }
 if(moStage==4&&frame>moStageFrame){bool closed=true;
  for(int i=0;i<moActorCount;++i){if(!moActorValid(moActors[i])){++sh->moOrderRejected;resetMoOrders();return;}closed&=moGraphRoute(i,true);}
  MoSubmission scope(3);if(closed&&outgoingRoom()>0&&moGraphCommitReady()&&moPlanningFinish(house)){
   sh->moOrderAccepted+=moActorCount;moStage=0;moWaypointSubmitted=false;sh->moOrderStatus=2;
  }
 }
}

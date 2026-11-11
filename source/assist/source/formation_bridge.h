#include "formation_quick.h"
#include "input_ownership.h"
#include "mo_gesture_bridge.h"
// Input and vanilla event submission run on the existing Runtime boundary.
// Voxel decoding/rasterization lives exclusively in the external controller.
static formation::Preview formationView;
static formation::Policy formationPolicy;
static bool formationModal=false,formationHolding=false,formationButton=false;
static uint32_t formationHoldingKey=0;
static bool formationSuppressed[256]{};
static uint32_t formationAltAt=0,formationEpoch=0,formationLayoutTick=0;
static int formationStartX=0,formationStartY=0,formationLastX=0,formationLastY=0,formationPixelX=0,formationPixelY=0;
static bool formationDirected=false,formationHoldConsumed=false,formationFeedback=false;
static UINT formationButtonUp=WM_RBUTTONUP;
static bool formationQuick=false,formationQuickCaptured=false;static int formationSavedShape=0;
static uint32_t formationQuickMouse=0;static moorders::GesturePolicy formationQuickPolicy;
static formation::quick::Path formationQuickPath;
static bool formationQuickPending=false;static int formationQuickX=0,formationQuickY=0,formationQuickPX=0,formationQuickPY=0;
static void formationEndQuick(){if(formationQuick){formationQuick=false;formationView.shape=formationSavedShape;}formationQuickPath=formation::quick::Path{};formationQuickPending=false;}
struct FormationMemory {int count=0,ax=0,ay=0;double facing=0;bool directed=false,quick=false;formation::Unit units[formation::Maximum]{};formation::Slot offsets[formation::Maximum]{};};
static FormationMemory formationMemory;
struct FormationFollow {uint32_t cell=0,tick=0;int frame=0;bool down=false,dragged=false;UINT up=0;int x=0,y=0;};
static FormationFollow formationFollow;
static inputownership::ReleaseWatch formationReleaseWatch,formationFollowReleaseWatch;
static bool formationSameSelection(){
 if(!formationMemory.count)return false;Vec selected=vec(0xA8ECB8);if(selected.count!=formationMemory.count)return false;
 bool found[formation::Maximum]{};
 for(int n=0;n<selected.count;++n){uint32_t p=item(selected,n);int j=0;for(;j<formationMemory.count;++j)if(!found[j]&&p==formationMemory.units[j].pointer&&get<uint32_t>(p+0x10)==formationMemory.units[j].id)break;if(j==formationMemory.count)return false;found[j]=true;}return true;
}
static void formationForget(){formationMemory.count=0;formationFollow={};}

struct FormationJob {formation::Unit unit;formation::Slot goal;uint32_t approach=0,epoch=0,issued=0;int stage=0,frame=0;bool observed=false,quick=false;};
static FormationJob formationJobs[formation::Maximum];static int formationJobCursor;
static bool formationOwns(uint32_t id){for(int n=0;n<formationJobCount;++n)if(formationJobs[n].unit.id==id&&formationJobs[n].unit.token==manualVersions.get(id))return true;return false;}
static void formationPublish(){if(!sh)return;formationView.tick=sampleClock(sh);formationView.match=match;++formationView.revision;
 InterlockedIncrement(&sh->formationSeq);MemoryBarrier();sh->formationPreview=formationView;MemoryBarrier();InterlockedIncrement(&sh->formationSeq);sh->formationJobs=formationJobCount;notifyHost();}
static void formationCancel(){if(formationQuick)formationButton=false;formationEndQuick();formationView.active=formationView.dragging=0;formationView.count=0;formationModal=formationHolding=false;formationHoldingKey=0;formationPublish();}
static void resetFormation(){formationReleaseWatch.clear();formationFollowReleaseWatch.clear();formationForget();formationEndQuick();formationQuickCaptured=false;formationQuickMouse=0;formationFeedback=false;formationView={};formationModal=formationHolding=formationButton=false;formationHoldingKey=0;formationAltAt=0;formationHoldConsumed=false;formationJobCount=formationJobCursor=0;memset(formationSuppressed,0,sizeof formationSuppressed);if(sh){sh->formationJobs=0;formationPublish();}}
static bool formationContext(bool preview=false){
 if(!sh||fault||sh->stop||!sh->foreground||!gameentry::active(gameentry::Client::Assist)||snap.status!=1||!match||!lastHouse||snap.commandStatus!=1||!get<bool>(0xA8E9A0)||get<uint32_t>(0xA83D4C)!=lastHouse||get<bool>(0xA8BC60+25)||DWORD(sampleClock(sh)-sh->heartbeat)>(preview?1000u:150u))return false;
 int frame=get<int>(0xA8ED84);if(frame<snap.frame||int64_t(frame)-snap.frame>6||frame<0||frame>=INT32_MAX||get<int>(0xA8B238)!=snap.session||!eventdelivery::mayQueue(snap.session)||get<bool>(lastHouse+0x1F5)||get<bool>(lastHouse+0x1F6)||get<bool>(lastHouse+0x1F8))return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return false;
 if(!external::display::inspect([](uint32_t at,void*out,size_t n){return read(at,out,n);}).visible())return false;
 long seq=sh->formationPolicySeq;if(seq&1)return formationPolicy.enabled!=0;MemoryBarrier();auto p=sh->formationPolicy;MemoryBarrier();if(seq!=sh->formationPolicySeq)return formationPolicy.enabled!=0;formationPolicy=p;return p.enabled!=0;
}
static bool formationActor(uint32_t p,bool quick=false){int air=0,k=kind(p);return p&&worldObjects.contains(p)&&own(p,lastHouse)&&(k==1||k==2||(quick&&k==0))&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C)&&!get<uint32_t>(p+0x2B0)&&!get<bool>(p+0x427)&&(quick||(objectAirQuery(p,&air)&&!air));}
static bool formationAerial(const formation::Unit&u){
 // Aircraft are always in the air placement group, including while landed.
 // Existing type definitions identify jumpjet/flying infantry and vehicles.
 if(u.kind==0)return true;
 return u.type&&(get<int>(u.type+0x5B4)==9||get<int>(u.type+0x67C)==4);
}
static bool formationSelectionMatchesPreview(){
 Vec selected=vec(0xA8ECB8);bool found[formation::Maximum]{};int count=0;
 for(int n=0;n<selected.count;++n){uint32_t p=item(selected,n);if(!formationActor(p,formationQuick))continue;++count;int j=0;
  for(;j<formationView.count;++j)if(!found[j]&&p==formationView.units[j].pointer&&get<uint32_t>(p+0x10)==formationView.units[j].id)break;
  if(j==formationView.count)return false;found[j]=true;
 }return count==formationView.count&&count>0;
}
static void formationSelect(){
 indexWorld(get<int>(0xA8ED84));formationView.count=0;Vec selected=vec(0xA8ECB8);
 for(int n=0;n<selected.count&&formationView.count<formation::Maximum;++n){uint32_t p=item(selected,n);if(!formationActor(p,formationQuick))continue;
  auto&u=formationView.units[formationView.count++];u={};u.pointer=p;u.id=get<uint32_t>(p+0x10);u.type=typeOf(p,kind(p));u.token=manualVersions.get(u.id);u.x=get<int>(p+0x9C);u.y=get<int>(p+0xA0);u.z=get<int>(p+0xA4);u.kind=kind(p);
 }
}
static bool formationPoint(MSG*msg,int&x,int&y,int&px,int&py,bool*ground=nullptr,uint32_t*hover=nullptr){
 RECT r{};if(!GetClientRect(msg->hwnd,&r)||r.right<=0||r.bottom<=0)return false;uint32_t tactical=get<uint32_t>(0x887324);if(!tactical)return false;
 pointerinput::Intent point{};point.width=get<int>(0x8A00A4);point.height=get<int>(0x8A00A8);point.cameraX=get<int>(tactical+0xB0);point.cameraY=get<int>(tactical+0xB4);point.operation=pointerinput::AirWaypoint;
 px=point.x=int(int64_t(short(LOWORD(msg->lParam)))*point.width/r.right);py=point.y=int(int64_t(short(HIWORD(msg->lParam)))*point.height/r.bottom);
 uint32_t c=pointerCell,t=pointerTarget,id=pointerTargetID;int z=pointerGroundZ;bool ok=pointerLocation(point);if(ground)*ground=ok&&!pointerTarget&&!get<bool>(0x87F7E8+0x11B3);if(hover)*hover=ok?pointerTarget:0;if(ok){x=int(pointerCell%1000)*256+128;y=int(pointerCell/1000)*256+128;}
 pointerCell=c;pointerTarget=t;pointerTargetID=id;pointerGroundZ=z;return ok;
}
static bool formationCell(int x,int y){
 if(x<=0||y<=0||x>=512||y>=512)return false;uint32_t cell=readMapCell(x,y);if(!cell)return false;
 // Preview omits permanent obstacles, but native movement owns terrain,
 // locomotor, congestion and reachability decisions. No pathfinder or RNG probe.
 uint32_t q=get<uint32_t>(cell+0xE4);for(int n=0;q>=0x10000&&n<128;++n){if(get<uint32_t>(q)==0x7F522C&&!get<bool>(q+0xCD))return false;if(worldObjects.contains(q)&&kind(q)==3&&live(q))return false;uint32_t next=get<uint32_t>(q+0x30);if(q==next)return false;q=next;}
 return q<0x10000;
}
static bool formationQuickCell(int x,int y,bool air){
 if(x<=0||y<=0||x>=512||y>=512)return false;
 return air?readMapCell(x,y)!=0:formationCell(x,y);
}
static void formationQuickLayout(int x,int y,int px,int py){
 if(!nativeDependencyReady(0x578080)){formationCancel();return;}
 formationQuickPending=false;
 bool air[formation::Maximum]{};for(int n=0;n<formationView.count;++n)air[n]=formationAerial(formationView.units[n]);
 formation::quick::layout(formationView.units,formationView.count,air,formationQuickPath,x/256,y/256,formationView.slots,formationQuickCell);
 for(int n=0;n<formationView.count;++n)if(formationView.slots[n].valid){auto&s=formationView.slots[n];struct Coord{int x,y,z;}c{s.x,s.y,0};s.z=call<int>(0x578080,0x87F7E8,&c);if(air[n])s.z=std::max(s.z,formationView.units[n].z);}
 formationDirected=false;formationLastX=x;formationLastY=y;formationPixelX=px;formationPixelY=py;formationLayoutTick=sampleClock(sh);formationPublish();
}
static void formationLayout(int x,int y,int px,int py){
 if(!nativeDependencyReady(0x578080)){formationCancel();return;}
 formationLayoutTick=sampleClock(sh);
 const int count=formationView.count;if(count<=0)return;
 if(formationQuick){formationQuickLayout(x,y,px,py);return;}
 double pixels=std::hypot(double(px-formationStartX),double(py-formationStartY));bool click=!formationView.dragging;
 formationDirected=!click;
 if(!click&&pixels>=8)formationView.facing=formation::dragFacing(double(px)-formationStartX,double(py)-formationStartY,formationView.facing);
 double stretch=click?0:std::clamp((pixels-8)/312.,0.,1.);formation::Point points[formation::Maximum];formation::layout(count,formationView.shape,stretch,click,points);
 double fx=std::cos(formationView.facing),fy=std::sin(formationView.facing);
 for(int n=0;n<count;++n){int cx=int(std::lround((formationView.ax-128)/256.+points[n].x*(-fy)-points[n].y*fx));int cy=int(std::lround((formationView.ay-128)/256.+points[n].x*fx-points[n].y*fy));
  formation::Slot result{};
  // Quantization of rotated ranks must not put two tanks in the same cell.
  for(int radius=0;radius<=4&&!result.valid;++radius)for(int oy=-radius;oy<=radius&&!result.valid;++oy)for(int ox=-radius;ox<=radius&&!result.valid;++ox){if(std::max(std::abs(ox),std::abs(oy))!=radius)continue;int tx=cx+ox,ty=cy+oy;if(!formationCell(tx,ty))continue;bool used=false;
   for(int j=0;j<n;++j)if(formationView.slots[j].valid&&formationView.slots[j].x==tx*256+128&&formationView.slots[j].y==ty*256+128){used=true;break;}if(used)continue;
   struct Coord{int x,y,z;}c{tx*256+128,ty*256+128,0};result={c.x,c.y,call<int>(0x578080,0x87F7E8,&c),1};
  }formationView.slots[n]=result;
 }
 formation::assign(formationView.units,count,formationView.slots);formationLastX=x;formationLastY=y;formationPixelX=px;formationPixelY=py;formationPublish();
}
static void formationAtCursor(MSG*msg){POINT pt{};if(!GetCursorPos(&pt)||!ScreenToClient(msg->hwnd,&pt))return;MSG pointer=*msg;pointer.lParam=MAKELPARAM(short(pt.x),short(pt.y));int x,y,px,py;if(!formationPoint(&pointer,x,y,px,py))return;formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;formationLayout(x,y,px,py);}
static void formationCommit(bool remember=true,bool feedback=true){
 if(!nativeDependencyReady(0x578080)){formationCancel();return;}
 formationFeedback=feedback;formationFollow={};if(remember){formationMemory={};formationMemory.ax=formationView.ax;formationMemory.ay=formationView.ay;formationMemory.facing=formationView.facing;formationMemory.directed=formationDirected;formationMemory.quick=formationQuick;}
 sh->formationAccepted=sh->formationRejected=0;
 for(int n=0;n<formationView.count;++n){auto u=formationView.units[n];auto goal=formationView.slots[n];
  if(!goal.valid||!formationActor(u.pointer,formationQuick)||get<uint32_t>(u.pointer+0x10)!=u.id||manualVersions.get(u.id)!=u.token){++sh->formationRejected;continue;}
  for(int j=0;j<formationJobCount;)if(formationJobs[j].unit.id==u.id)formationJobs[j]=formationJobs[--formationJobCount];else ++j;
  if(formationJobCount>=formation::Maximum){++sh->formationRejected;continue;}
  // This is an explicit player order: invalidate older automation leases.
  cancelNative(u.pointer);u.token=manualVersions.get(u.id);
  int dx=int(std::lround(std::cos(formationView.facing))),dy=int(std::lround(std::sin(formationView.facing)));
  uint32_t approach=0;int cx=goal.x/256-dx,cy=goal.y/256-dy;
  // Only a newly dragged layout asks for a common final heading. Continuation
  // translates the saved slots directly, without another facing detour.
  if(remember&&formationDirected&&formationCell(cx,cy))approach=cy*1000+cx;
  auto&j=formationJobs[formationJobCount++];j={};j.unit=u;j.goal=goal;j.quick=formationQuick;j.approach=approach;j.epoch=match;j.frame=get<int>(0xA8ED84);++sh->formationAccepted;
  if(remember){int at=formationMemory.count++;formationMemory.units[at]=u;formationMemory.offsets[at]=goal;formationMemory.offsets[at].x-=formationView.ax;formationMemory.offsets[at].y-=formationView.ay;}
 }
 if(remember&&!formationSameSelection())formationForget();
 formationEndQuick();formationView.active=formationView.dragging=0;formationHoldConsumed=formationHolding;formationModal=formationHolding=false;formationHoldingKey=0;formationAltAt=0;sh->formationConsumedUntil=inputownership::afterGesture(sampleClock(sh));formationPublish();
}
static void formationWatchNative(MSG*msg){
 if(!formationMemory.count)return;
 if(!formationSameSelection()){formationForget();return;}
 // Right-button panning is not a selection change. Keep the remembered
 // member set and in-flight jobs until the actual selection changes.
 // Native left messages (including box selection and its one voice) pass on.
 if(msg->message==WM_LBUTTONDOWN){
  formationFollowReleaseWatch.clear();formationFollow={};formationFollow.down=true;formationFollow.up=WM_LBUTTONUP;formationFollow.x=short(LOWORD(msg->lParam));formationFollow.y=short(HIWORD(msg->lParam));
  int x,y,px,py;bool ground=false;
  if(!(GetKeyState(VK_SHIFT)&0x8000)&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)&&formationPoint(msg,x,y,px,py,&ground)&&ground)
   sh->formationConsumedUntil=inputownership::afterGesture(sampleClock(sh));
 }else if(msg->message==WM_MOUSEMOVE&&formationFollow.down){
  formationFollow.dragged|=std::abs(short(LOWORD(msg->lParam))-formationFollow.x)>5||std::abs(short(HIWORD(msg->lParam))-formationFollow.y)>5;
 }else if(msg->message==WM_LBUTTONUP&&formationFollow.down){
  formationFollow.down=false;int x,y,px,py;bool ground=false;
  if(formationFollow.dragged||(GetKeyState(VK_SHIFT)&0x8000)||(GetKeyState(VK_CONTROL)&0x8000)||(GetKeyState(VK_MENU)&0x8000)||!formationPoint(msg,x,y,px,py,&ground)){formationFollow.cell=0;return;}
  formationJobCount=0;formationFeedback=false;
  if(ground){formationFollow.cell=y/256*1000+x/256;formationFollow.tick=sampleClock(sh);formationFollow.frame=get<int>(0xA8ED84);sh->formationConsumedUntil=inputownership::afterGesture(sampleClock(sh));}
 }
}
static void formationContinue(DWORD now){
 if(!formationMemory.count)return;
 if(!nativeDependencyReady(0x578080)){formationFollow.cell=0;return;}
 if(!formationSameSelection()){formationForget();return;}
 if(!formationFollow.cell)return;
 // GETMESSAGE sees button-up before the game consumes it. Queue our per-unit
 // moves on a later simulation turn, after the native common move/voice.
 if(get<int>(0xA8ED84)<=formationFollow.frame)return;
 if(DWORD(now-formationFollow.tick)>1500){formationFollow.cell=0;return;}
 uint32_t cell=formationFollow.cell,mapCell=readMapCell(cell%1000,cell/1000);if(!mapCell)return;
 // The click is the new intent. Native moves may have distinct scatter cells,
 // arrive at different times, or still be queued; none is grounds to reject it.
 for(int n=0;n<formationMemory.count;++n){auto&u=formationMemory.units[n];if(!formationActor(u.pointer,formationMemory.quick)){formationForget();return;}}
 formationView.count=formationMemory.count;formationView.ax=int(cell%1000)*256+128;formationView.ay=int(cell/1000)*256+128;formationView.facing=formationMemory.facing;formationDirected=formationMemory.directed;
 formationEndQuick();formationQuick=formationMemory.quick;formationSavedShape=formationView.shape;if(formationQuick)formationView.shape=formation::quick::Shape;
 bool air[formation::Maximum]{};for(int n=0;n<formationMemory.count;++n)air[n]=formationAerial(formationMemory.units[n]);
 for(int n=0;n<formationMemory.count;++n){auto u=formationMemory.units[n];u.token=manualVersions.get(u.id);formationView.units[n]=u;auto goal=formationMemory.offsets[n];goal.x+=formationView.ax;goal.y+=formationView.ay;goal.valid=0;
  int cx=goal.x/256,cy=goal.y/256;
  for(int radius=0;radius<=4&&!goal.valid;++radius)for(int dy=-radius;dy<=radius&&!goal.valid;++dy)for(int dx=-radius;dx<=radius&&!goal.valid;++dx){if(std::max(std::abs(dx),std::abs(dy))!=radius||!(formationQuick?formationQuickCell(cx+dx,cy+dy,air[n]):formationCell(cx+dx,cy+dy)))continue;bool used=false;if(formationQuick)used=!formation::quick::room(formationView.units,air,formationView.slots,n,n,cx+dx,cy+dy);else for(int j=0;j<n;++j)if(formationView.slots[j].valid&&formationView.slots[j].x==(cx+dx)*256+128&&formationView.slots[j].y==(cy+dy)*256+128)used=true;if(!used){goal.x=(cx+dx)*256+128;goal.y=(cy+dy)*256+128;goal.valid=1;}}
  if(goal.valid){struct Coord{int x,y,z;}c{goal.x,goal.y,0};goal.z=call<int>(0x578080,0x87F7E8,&c);if(formationQuick&&air[n])goal.z=std::max(goal.z,u.z);}
  formationView.slots[n]=goal;
 }
 formationCommit(false,false);
}
static bool formationSwitch(uint32_t key){if(formationQuick)return false;key=formation::shapeKey(key);bool found=false;for(auto k:formationPolicy.keys)found|=key&&formation::shapeKey(k)==key;if(!found)return false;formationView.shape=formation::cycle(formationView.shape,key,formationPolicy.keys);if(formationView.count)formationLayout(formationLastX,formationLastY,formationPixelX,formationPixelY);return true;}
// Modifier prefixes are part of an active hold, not just its entry chord.
// Once a mouse gesture owns the preview, its release still commits the last
// published layout even if the keyboard authorization was released first.
static void formationRefreshHold(){
 if(!formationHolding)return;
 uint8_t keys[256]{};const unsigned vk=formationHoldingKey&255;
 keys[vk]=(GetAsyncKeyState(int(vk))&0x8000)!=0;
 for(unsigned key:{16u,17u,18u})keys[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;
 runtimeKeys.decorate(keys);
 const bool same=keygesture::normalize(formationHoldingKey,keygesture::Mode::Hold)==keygesture::normalize(formationPolicy.hold,keygesture::Mode::Hold);
 if(same&&keygesture::matchesHold(keys,formationHoldingKey))return;
 formationHolding=false;formationHoldingKey=0;
 if(!formationModal&&!formationButton)formationCancel();else formationPublish();
}
static bool formationQuickAuthorized(bool captured=false){
 if(!formationQuick||!moReadGestures(captured)||!moorders::sameGestures(formationQuickPolicy,moRuntimeGestures)||!(moGestureEnabled()&moorders::QuickFormation)||(moorders::gestureConflicts(moRuntimeGestures,moGestureEnabled())&moorders::QuickFormation))return false;
 uint8_t keys[256]{};moGestureKeyboard(keys);return moorders::gestureHeld(keys,formationQuickPolicy.keys[3]);
}
static bool formationMessage(MSG*msg){
 const auto gestureMouse=moorders::gestureMouseEdge(msg->message,uintptr_t(msg->wParam));
 const bool down=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN,up=msg->message==WM_KEYUP||msg->message==WM_SYSKEYUP;
 uint32_t key=uint32_t(msg->wParam)&255;if(up)formationRefreshHold();auto swallow=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(up&&formationSuppressed[key]){formationSuppressed[key]=false;if(key==(formationPolicy.hold&255))formationHoldConsumed=false;if(key==(formationPolicy.hold&255)){formationHolding=false;formationHoldingKey=0;if(!formationModal&&!formationButton)formationCancel();}return swallow();}
 // Release commits the last published layout, never an unseen extra solve.
 if(gestureMouse.up&&gestureMouse.button==formationQuickMouse&&formationQuickCaptured&&!formationQuick){formationQuickCaptured=false;formationQuickMouse=0;return swallow();}
 if(msg->message==formationButtonUp&&formationButton&&(!formationQuick||(gestureMouse.up&&gestureMouse.button==formationQuickMouse))){bool authorized=!formationQuick||formationQuickAuthorized();formationButton=false;formationQuickCaptured=false;formationQuickMouse=0;if(authorized&&formationContext()&&formationView.active&&formationSelectionMatchesPreview()){int x,y,px,py;if(formationPoint(msg,x,y,px,py)){if(formationQuick){if(formationView.dragging)formationQuickPath.push(x/256,y/256,true);formationQuickLayout(x,y,px,py);}formationCommit();}else formationCancel();}else formationCancel();return swallow();}
 if(!formationContext(formationQuick&&formationButton)){if(formationView.active)formationCancel();formationFollow={};if(sh&&!sh->formationPolicy.enabled)formationForget();return false;}
 formationRefreshHold();
 // All four gestures resolve through the same configured policy. The
 // captured button and keyboard hold remain authoritative until release.
 if(formationQuick&&down&&key==VK_ESCAPE){formationCancel();formationSuppressed[key]=true;return swallow();}
 if(formationQuick&&!formationQuickAuthorized(true)){formationCancel();formationButton=false;return false;}
 if(formationQuick&&gestureMouse.down&&gestureMouse.button!=formationQuickMouse){formationCancel();return false;}
 if(gestureMouse.down&&moReadGestures()){
  uint8_t keys[256]{};moGestureKeyboard(keys);auto resolved=moGestureResolve(gestureMouse.button,keys);
  if(resolved.mode==moorders::QuickFormation){
   int x,y,px,py;if(!formationPoint(msg,x,y,px,py))return false;
   formationCancel();formationSavedShape=formationView.shape;formationQuick=true;formationQuickCaptured=true;formationQuickMouse=gestureMouse.button;formationQuickPolicy=moRuntimeGestures;formationView.shape=formation::quick::Shape;formationView.active=1;formationView.dragging=0;formationModal=false;formationButton=true;formationReleaseWatch.clear();formationButtonUp=moorders::gestureUpMessage(gestureMouse.button);
   formationSelect();if(!formationView.count){formationCancel();formationButton=false;formationQuickCaptured=false;formationQuickMouse=0;return false;}
   formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;formationQuickPath.push(x/256,y/256);formationQuickLayout(x,y,px,py);sh->formationConsumedUntil=inputownership::afterGesture(sampleClock(sh));return swallow();
  }
 }
 if(down){bool repeated=(msg->lParam&(1u<<30))!=0;
  uint32_t mods=((GetKeyState(VK_SHIFT)&0x8000)?1:0)|((GetKeyState(VK_CONTROL)&0x8000)?2:0)|((GetKeyState(VK_MENU)&0x8000)?4:0);
  uint32_t chord=key|(mods<<8);if(key==VK_MENU)chord&=~(4u<<8);if(key==VK_SHIFT)chord&=~(1u<<8);if(key==VK_CONTROL)chord&=~(2u<<8);
  if(key==VK_MENU&&!repeated){formationEndQuick();uint32_t now=sampleClock(sh);if(formationAltAt&&DWORD(now-formationAltAt)<=350){formationModal=true;formationView.active=1;formationSelect();formationAtCursor(msg);formationAltAt=0;formationSuppressed[key]=true;formationPublish();return swallow();}else formationAltAt=now;}
  if(formationPolicy.hold&&chord==keygesture::physical(formationPolicy.hold)&&runtimeKeys.qualifies(uint16_t(formationPolicy.hold))&&(!repeated||formationHolding)){if(formationHoldConsumed)return swallow();formationEndQuick();formationHolding=true;formationHoldingKey=formationPolicy.hold;formationView.active=1;formationSuppressed[key]=true;if(!repeated&&!formationButton){formationSelect();formationAtCursor(msg);}formationPublish();return swallow();}
  if(formationView.active&&key==VK_ESCAPE){formationCancel();formationSuppressed[key]=true;return swallow();}
  // Ignore the mode's hold modifier while interpreting a shape binding.
  if(formationView.active){uint32_t shapeChord=chord;uint32_t hold=formationPolicy.hold&255;if(formationHolding){if(hold==VK_MENU)shapeChord&=~(4u<<8);if(hold==VK_CONTROL)shapeChord&=~(2u<<8);if(hold==VK_SHIFT)shapeChord&=~(1u<<8);}
   bool matches=false;for(auto k:formationPolicy.keys)if(auto binding=formation::shapeKey(k);binding)matches|=keygesture::physical(binding)==shapeChord;if(matches){if(!repeated){bool single=false,twice=false;for(auto k:formationPolicy.keys)if(auto binding=formation::shapeKey(k);binding&&keygesture::physical(binding)==shapeChord){single|=!(binding&keygesture::Double);twice|=(binding&keygesture::Double)!=0;}if(twice&&(runtimeKeys.held[key]&2))formationSwitch(shapeChord|0x8000);else if(single&&!twice)formationSwitch(shapeChord);}formationSuppressed[key]=true;formationPublish();return swallow();}
  }
 }
 if(!formationView.active){formationWatchNative(msg);return false;}
 // Record every quick-placement stroke point before the expensive layout
 // cadence. Previously an early return dropped corners within each 16ms
 // window, so a fast curve became a chord although its endpoint was kept.
 if(formationQuick&&msg->message==WM_MOUSEMOVE){
  int x,y,px,py;if(formationPoint(msg,x,y,px,py)){
   if(!formationView.dragging&&std::hypot(double(px-formationStartX),double(py-formationStartY))<8)return swallow();
   formationView.dragging=1;const int count=formationQuickPath.count;const auto last=count?formationQuickPath.nodes[count-1]:formation::Point{};formationQuickPath.push(x/256,y/256);
   const auto tail=formationQuickPath.nodes[formationQuickPath.count-1];
   formationQuickPending|=count!=formationQuickPath.count||last.x!=tail.x||last.y!=tail.y||x!=formationLastX||y!=formationLastY;
   formationQuickX=x;formationQuickY=y;formationQuickPX=px;formationQuickPY=py;
   if(formationQuickPending&&DWORD(sampleClock(sh)-formationLayoutTick)>=16){if(!formationSelectionMatchesPreview()){formationCancel();return swallow();}formationQuickLayout(x,y,px,py);}
  }return swallow();
 }
 if(msg->message==(formationPolicy.mouse==1?WM_LBUTTONDOWN:WM_RBUTTONDOWN)){int x,y,px,py;if(!formationPoint(msg,x,y,px,py))return false;
  if(!formationSelectionMatchesPreview()){formationCancel();return swallow();}
  formationButton=true;formationReleaseWatch.clear();formationButtonUp=formationPolicy.mouse==1?WM_LBUTTONUP:WM_RBUTTONUP;formationStartX=formationPixelX=px;formationStartY=formationPixelY=py;
  sh->formationConsumedUntil=inputownership::afterGesture(sampleClock(sh));return swallow();
 }
 if(msg->message==WM_MOUSEMOVE){if(DWORD(sampleClock(sh)-formationLayoutTick)<16)return formationButton?swallow():false;int x,y,px,py;if(formationPoint(msg,x,y,px,py)){
   if(formationButton){
    if(!formationSelectionMatchesPreview()){formationCancel();return swallow();}
    if(!formationView.dragging&&std::hypot(double(px-formationStartX),double(py-formationStartY))<8)return swallow();
    if(formationView.dragging&&std::hypot(double(px-formationPixelX),double(py-formationPixelY))<2)return swallow();
    formationView.dragging=1;
   }else{formationSelect();formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;}
   if(formationQuick&&formationView.dragging)formationQuickPath.push(x/256,y/256);formationLayout(x,y,px,py);
  }if(formationButton)return swallow();}
 return false;
}
static void formationTick(uint32_t house,int frame,DWORD now){
 if(formationEpoch!=match){resetFormation();formationEpoch=match;}
 if(!formationContext(formationQuick&&formationButton)){if(formationView.active)formationCancel();formationFollow={};if(!sh->formationPolicy.enabled){formationJobCount=0;formationForget();}}
 // The native mouse-up can be lost when focus/capture changes. Do not
 // infer a click or commit the preview from polling; retire only its input lease.
 const unsigned button=formationQuick?moorders::gestureVK(formationQuickMouse):(formationButtonUp==WM_LBUTTONUP?VK_LBUTTON:VK_RBUTTON);
 if(formationReleaseWatch.expired(formationButton,button&&(GetAsyncKeyState(button)&0x8000),now)){
  formationButton=false;formationQuickCaptured=false;formationQuickMouse=0;formationCancel();formationReleaseWatch.clear();
 }
 if(formationFollowReleaseWatch.expired(formationFollow.down,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,now)){
  formationFollow={};sh->formationConsumedUntil=0;formationFollowReleaseWatch.clear();
 }
 formationRefreshHold();if(formationQuick&&!formationQuickAuthorized(true))formationCancel();
 if(formationQuick&&formationQuickPending&&DWORD(now-formationLayoutTick)>=16){
  if(formationSelectionMatchesPreview())formationQuickLayout(formationQuickX,formationQuickY,formationQuickPX,formationQuickPY);else formationCancel();
 }
 if(sh->formationKey!=sh->formationKeyAck){if(formationView.active)formationSwitch(sh->formationKeyCode);sh->formationKeyAck=sh->formationKey;formationPublish();}
 if(!simulationReady()||snap.chat)return;
 if(formationFollow.down&&!formationFollow.dragged&&int32_t(sh->formationConsumedUntil-now)>0)sh->formationConsumedUntil=inputownership::afterGesture(now);
 formationContinue(now);
 int visited=0;while(formationJobCount&&visited++<32){if(formationJobCursor>=formationJobCount)formationJobCursor=0;int at=formationJobCursor++;auto&j=formationJobs[at];uint32_t p=worldByID(j.unit.id);
  bool remove=j.epoch!=match||p!=j.unit.pointer||!formationActor(p,j.quick)||manualVersions.get(j.unit.id)!=j.unit.token||frame<j.frame||frame-j.frame>1800;
  uint32_t cell=j.stage<2&&j.approach?j.approach:uint32_t(j.goal.y/256*1000+j.goal.x/256);int gx=int(cell%1000)*256+128,gy=int(cell/1000)*256+128;
  if(!remove){double dx=double(get<int>(p+0x9C))-gx,dy=double(get<int>(p+0xA0))-gy;bool arrived=dx*dx+dy*dy<=96.*96.;
   if(j.stage==1||j.stage==3){bool going=get<int>(p+0xAC)==2&&get<uint32_t>(p+0x5A4)==readMapCell(cell%1000,cell/1000);j.observed|=going;
    if(arrived){if(j.stage==1&&j.approach){j.stage=2;j.observed=false;}else remove=true;}
    else if((j.observed&&!going)||DWORD(now-j.issued)>15000)remove=true;
   }
   if(!remove&&(j.stage==0||j.stage==2)&&outgoingRoom()>0){uint32_t destination=j.stage==2?uint32_t(j.goal.y/256*1000+j.goal.x/256):cell;
    if(nativeMission(house,j.unit.id,2,0,destination,11,0,0,taskarbiter::Owner::Formation)){j.stage=j.stage==2?3:1;j.issued=now;if(formationFeedback){formationFeedback=false;inputFeedback(p,0,false);}}
   }
  }
  if(remove){formationJobs[at]=formationJobs[--formationJobCount];formationJobCursor=at;}
 }sh->formationJobs=formationJobCount;
}


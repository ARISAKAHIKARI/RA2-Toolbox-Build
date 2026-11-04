// An explicit toolbox modal cursor, entirely outside native cursor state.
// Only this mode's matched down/up pair is consumed. Sidebar and native tools
// keep their messages; an unrelated native cursor cancels this mode first.
#include "native_placement_geometry.h"
#include "../../shared/source/native_viewport_projection.h"
#include "native_placement_completion_bridge.h"
#include "../../external_reader/source/projection.h"
static nativeplacement::Preview placementView;
static uint32_t placementPreviewSerial=0,placementCapturedButtons=0;
static uint32_t placementScenario=0;
static uint32_t placementCapturedSerial[2]{};
static void nativePlacementPublish(){
 if(!sh)return;placementView.tick=sampleClock(sh);placementView.match=match;
 InterlockedIncrement(&sh->placementPreviewSeq);MemoryBarrier();sh->placementPreview=placementView;MemoryBarrier();InterlockedIncrement(&sh->placementPreviewSeq);notifyHost();
}
static void nativePlacementReset(){placementView={};placementScenario=0;++placementPreviewSerial;nativePlacementPublish();}
static bool nativePlacementCursorFree(){
 uint8_t tools[5]{};uint32_t state[4]{};int super=-1;
 // Map::CurrentBuilding/Type and their copy, the five native tool modes,
 // and the selected superweapon are read only. Never clear someone else's UI.
 return read(0x87F7E8+0x1190,state,8)&&!state[0]&&!state[1]&&
  read(0x87F7E8+0x11A4,state+2,8)&&!state[2]&&!state[3]&&
  read(0x87F7E8+0x11B0,tools,sizeof tools)&&!tools[0]&&!tools[1]&&!tools[2]&&!tools[3]&&!tools[4]&&
  read(0x8809A0,&super,4)&&super<0&&
  !sh->formationPreview.active&&!sh->moOrdersPreview.active&&!sh->viewZoomBusy;
}
static bool nativePlacementInputContext(bool preview=false){
 if(!nativePlacementAvailable(preview)||DWORD(sampleClock(sh)-sh->heartbeat)>(preview?1000u:150u)||!nativePlacementCursorFree())return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return foreground==GetCurrentProcessId();
}
static int nativePlacementArm(uint32_t house,DWORD now){
 if(placementView.active){nativePlacementReset();return 2;}
 if(!nativePlacementInputContext())return -4;
 nativeviewportprojection::Transform transform{};if(!nativeviewportprojection::sample(sh,transform))return -7;
 int tab=-1,after=-1;nativeplacement::Offer offer{};
 uint32_t scenario=0,verifyScenario=0;
 if(!read(0xA8B230,&scenario,4)||scenario<0x10000||!read(0x87F7E8+0x539C,&tab,4)||tab<0||tab>1||!nativePlacementCapture(house,tab,now,offer)||
    nativePlacementCompletionOwns(offer)||!read(0xA8B230,&verifyScenario,4)||scenario!=verifyScenario||
    !read(0x87F7E8+0x539C,&after,4)||after!=tab)return -7;
 placementView={};placementView.offer=offer;placementView.active=1;placementView.viewportGeneration=transform.generation;placementScenario=scenario;++placementPreviewSerial;
 int foundation=-1;
 if(read(offer.type+0xEF0,&foundation,4)&&foundation>=0&&foundation<21){
  read(0x8192B8+uint32_t(foundation)*4,&placementView.width,4);read(0x819310+uint32_t(foundation)*4,&placementView.height,4);
  if(placementView.width<1||placementView.width>32||placementView.height<1||placementView.height>32)placementView.width=placementView.height=0;
 }
 nativePlacementPublish();return 2; // ARMED; no Place event has been constructed.
}
static bool nativePlacementRenew(DWORD now,bool preview=false){
 if(!placementView.active||!nativePlacementInputContext(preview))return false;
 nativeviewportprojection::Transform transform{};if(!nativeviewportprojection::sample(sh,transform)||transform.generation!=placementView.viewportGeneration)return false;
 uint32_t scenario=0;if(!read(0xA8B230,&scenario,4)||scenario!=placementScenario)return false;
 int tab=-1;nativeplacement::Offer current{};
 if(!read(0x87F7E8+0x539C,&tab,4)||tab!=placementView.offer.tab||
    !nativePlacementCapture(lastHouse,tab,now,current)||!nativeplacement::sameIdentity(current,placementView.offer))return false;
 placementView.offer=current;return true;
}
static bool nativePlacementWindow(HWND window){
 if(!window||uintptr_t(window)!=get<uint32_t>(0xB73550))return false;
 DWORD owner=0;return GetWindowThreadProcessId(window,&owner)&&owner==GetCurrentProcessId();
}
// Physical placement input must not depend on the next Host-ready pulse.
// The manager samples through the ordinary dispatcher before strict admission.
static bool nativePlacementWantsInputSample(const MSG*msg){
 return msg&&sh&&!sh->stop&&!fault&&(placementView.active||nativePlacementCompletionPending())&&nativePlacementWindow(msg->hwnd);
}
static bool nativePlacementPoint(HWND window,POINT point,nativeplacement::Request&r,NativePlacementViewport&view,bool*inside=nullptr){
 if(inside)*inside=false;
 r={};r.offer=placementView.offer;RECT client{};uint32_t tactical=0;
 if(!nativePlacementWindow(window)||!GetClientRect(window,&client)||client.right<=0||client.bottom<=0||point.x<0||point.y<0||point.x>=client.right||point.y>=client.bottom||
    !read(0x887324,&tactical,4)||tactical<0x10000||!read(tactical+0xB0,&r.cameraX,4)||!read(tactical+0xB4,&r.cameraY,4)||
    !read(0x8A00A4,&r.width,4)||!read(0x8A00A8,&r.height,4))return false;
 r.screenX=int(int64_t(point.x)*r.width/client.right);r.screenY=int(int64_t(point.y)*r.height/client.bottom);
 nativeviewportprojection::Transform transform{};
 if(!nativeViewTransform(transform)||!nativePlacementViewport(r,view)||view.generation!=transform.generation||
    transform.generation!=placementView.viewportGeneration||!nativeViewQueryCompatible())return false;
 if(inside)*inside=true;
 NativeViewPoint p{r.screenX,r.screenY};if(!nativeViewMapPoint(p)||!nativeViewTransformUnchanged(transform))return false;
 p.x-=transform.logical.x;p.y-=transform.logical.y;vanilla::Cell cell{};struct Coord{int x,y,z;}coord{};uint32_t object=0;uint8_t a=0,b=0;
 if(!nativeViewQuery(&p,&cell,&coord,&object,&a,&b)||!nativeViewTransformUnchanged(transform))return false;
 NativePlacementViewport after{};if(!nativePlacementViewport(r,after)||!(after==view)||!nativeplacement::cell(cell.x,cell.y))return false;
 auto readOnly=[](uint32_t at,void*out,size_t n){return read(at,out,n);};
 const auto origin=nativeplacement::origin(cell.x,cell.y,placementView.width,placementView.height);
 // ClientToCoords returns Z=0. Resolve the same real ground height as normal
 // native cursor projection, rather than drawing the footprint on that plane.
 uint8_t bytes[12]{};uint32_t hash=2166136261u;
 if(!read(0x578080,bytes,sizeof bytes))return false;for(auto b:bytes)hash=(hash^b)*16777619u;
 if(hash!=0x8FFFDB45u)return false;
 struct Ground{int x,y,z;}ground{int(cell.x)*256+128,int(cell.y)*256+128,0};
 const int height=call<int>(0x578080,0x87F7E8,&ground);if(height < -1048576||height>1048576)return false;
 const double zScale=external::projectionZScale(readOnly);
 const int status=nativeplacement::targetUsable(readOnly,origin.x,origin.y)?1:2;
 NativePlacementViewport finalView{};
 if(!nativeViewTransformUnchanged(transform)||!nativePlacementViewport(r,finalView)||!(finalView==view))return false;
 r.cellX=origin.x;r.cellY=origin.y;placementView.cellX=origin.x;placementView.cellY=origin.y;
 placementView.x=origin.x*256;placementView.y=origin.y*256;placementView.z=height;
 placementView.zScale=zScale;
 placementView.cameraX=view.cameraX;placementView.cameraY=view.cameraY;placementView.viewportX=transform.logical.x;placementView.viewportY=transform.logical.y;
 placementView.status=status;
 return true;
}
static bool nativePlacementInteractiveValid(const NativePlacementSubmission&s){
 return placementView.active&&s.serial==placementPreviewSerial&&nativePlacementInputContext()&&
  nativeplacement::sameIdentity(s.request.offer,placementView.offer);
}
static void nativePlacementTick(DWORD now){
 nativePlacementCompletionTick(now);
 if(!placementView.active)return;
 if(!nativePlacementRenew(now,true)){nativePlacementReset();return;}
 HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));POINT point{};
 nativeplacement::Request request{};NativePlacementViewport view{};
 if(!(GetCursorPos(&point)&&ScreenToClient(window,&point)&&nativePlacementPoint(window,point,request,view)))placementView.status=0;
 nativePlacementPublish();
}
static bool nativePlacementMessage(MSG*msg){
 const bool leftDown=msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONDBLCLK,leftUp=msg->message==WM_LBUTTONUP;
 const bool rightDown=msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONDBLCLK,rightUp=msg->message==WM_RBUTTONUP;
 const uint32_t bit=leftDown||leftUp?1u:rightDown||rightUp?2u:0u;
 const unsigned button=bit==2?1:0;
 // A new physical press proves an old release was missed outside this input
 // route. Never let that old ownership swallow the new native click's up.
 if(leftDown||rightDown){placementCapturedButtons&=~bit;placementCapturedSerial[button]=0;}
 const bool ownedUp=(leftUp||rightUp)&&(placementCapturedButtons&bit);
 auto consume=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(ownedUp){
  const uint32_t captured=placementCapturedSerial[button];placementCapturedSerial[button]=0;
  placementCapturedButtons&=~bit;
  if(leftUp&&captured==placementPreviewSerial&&nativePlacementWindow(msg->hwnd)&&nativePlacementRenew(sampleClock(sh))){
   NativePlacementSubmission submission{};submission.interactive=true;submission.serial=placementPreviewSerial;
   POINT point{short(LOWORD(msg->lParam)),short(HIWORD(msg->lParam))};
   if(nativePlacementPoint(msg->hwnd,point,submission.request,submission.view)){
    const auto&r=submission.request;const auto order=intent::placeBuilding(get<int>(lastHouse+0x30),r.offer.typeIndex,r.offer.naval,{short(r.cellX),short(r.cellY)});
    if(validateNativePlacement(order,&submission)&&submitConstructed(order,OrderAuthority::Local,&submission)){nativePlacementQueued(r.offer,sampleClock(sh));nativePlacementReset();}
   }
  }
  return consume();
 }
 if(!placementView.active)return false;
 if(!nativePlacementRenew(sampleClock(sh),true)){nativePlacementReset();return false;}
 if((msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN)&&msg->wParam==VK_ESCAPE){nativePlacementReset();return consume();}
 if(!leftDown&&!rightDown)return false;
 nativeplacement::Request request{};NativePlacementViewport view{};POINT point{short(LOWORD(msg->lParam)),short(HIWORD(msg->lParam))};bool inside=false;
 if(!nativePlacementPoint(msg->hwnd,point,request,view,&inside)){
  if(!inside){nativePlacementReset();return false;} // Sidebar click remains native.
  // Unreadable/invalid ground is a position failure, not the end of this modal
  // gesture. Own its click pair so it cannot leak a native unit command.
  placementView.status=0;nativePlacementPublish();
 }
 placementCapturedButtons|=bit;placementCapturedSerial[button]=placementPreviewSerial;if(rightDown)nativePlacementReset();return consume();
}

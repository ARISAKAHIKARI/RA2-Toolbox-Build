// Explicit one-shot native Place. Reads factory/product state and appends the
// ordinary event; never edits Type, factory, ownership or placement code.
#include "native_placement_capture.h"
#include "../../shared/source/native_viewport_projection.h"
#include "native_placement_completion_bridge.h"
#include "activation_diagnostic.h"
struct NativePlacementViewport {
 uint32_t tactical=0,generation=0;int width=0,height=0,cameraX=0,cameraY=0,x=0,y=0,viewWidth=0,viewHeight=0;
 bool operator==(const NativePlacementViewport&o)const{return tactical==o.tactical&&width==o.width&&height==o.height&&
  generation==o.generation&&cameraX==o.cameraX&&cameraY==o.cameraY&&x==o.x&&y==o.y&&viewWidth==o.viewWidth&&viewHeight==o.viewHeight;}
};
struct NativePlacementSubmission {nativeplacement::Request request{};NativePlacementViewport view{};uint32_t serial=0;bool interactive=false;};
static int nativePlacementArm(uint32_t,DWORD);
static bool nativePlacementInteractiveValid(const NativePlacementSubmission&);
static bool nativePlacementViewport(const nativeplacement::Request&r,NativePlacementViewport&v){
 auto reader=[](uint32_t at,void*out,size_t n){return read(at,out,n);};
 using nativeplacement::field;
 nativeviewportprojection::Transform transform{};if(!nativeviewportprojection::sample(sh,transform))return false;v.generation=transform.generation;
 if(nativeSidebarUIHit(r.screenX,r.screenY))return false;
 if(!nativeplacement::screen(r)||!field(reader,0x887324,v.tactical)||v.tactical<0x10000||
    !field(reader,0x8A00A4,v.width)||!field(reader,0x8A00A8,v.height)||v.width!=r.width||v.height!=r.height||
    !field(reader,v.tactical+0xB0,v.cameraX)||!field(reader,v.tactical+0xB4,v.cameraY)||v.cameraX!=r.cameraX||v.cameraY!=r.cameraY)return false;
 if(transform.active){v.x=transform.physical.x;v.y=transform.physical.y;v.viewWidth=transform.physical.w;v.viewHeight=transform.physical.h;}
 else if(!field(reader,0x886FA0,v.x)||!field(reader,0x886FA4,v.y)||!field(reader,0xB0CE30,v.viewWidth)||!field(reader,0xB0CE34,v.viewHeight))return false;
 nativeviewportprojection::Transform verify{};if(!nativeviewportprojection::sample(sh,verify)||verify!=transform)return false;
 return v.x>=0&&v.y>=0&&v.viewWidth>0&&v.viewHeight>0&&v.viewWidth<=v.width&&v.viewHeight<=v.height&&
  r.screenX>=v.x&&r.screenY>=v.y&&r.screenX-v.x<v.viewWidth&&r.screenY-v.y<v.viewHeight;
}
static bool nativePlacementLookup(uint32_t type,int&slot,int&index,int&naval){
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)if(typePointers[i]==type&&snap.types[i].kind==7){
  const auto&t=snap.types[i];Vec v=vec(0xA83C68,4096);uint32_t actual=0;
  if(t.index<0||t.index>=v.count||!read(v.items+uint32_t(t.index)*4,&actual,4)||actual!=type)return false;
  slot=i;index=t.index;naval=t.naval;return true;
 }return false;
}
static bool nativePlacementCapture(uint32_t house,int tab,DWORD now,nativeplacement::Offer&offer){
 return nativeplacement::capture([](uint32_t at,void*out,size_t n){return read(at,out,n);},nativePlacementLookup,
  house,tab,match,now,snap.frame,typeListHash(snap.types,snap.typeCount),offer);
}
static bool nativePlacementAvailable(bool preview=false){
 return sh&&!sh->stop&&sh->foreground&&(sh->flags&BuildAnywhere)&&sh->placementImplementation==2&&
  snap.status==1&&snap.commandStatus==1&&simulationReady(preview)&&!get<bool>(0xA8BC60+25)&&nativePlacementRetired();
}
static void nativePlacementOffers(DWORD now){
 snap.placementOfferCount=0;for(auto&o:snap.placementOffers)o={};
 // Read-only choices must remain available while the player focuses the
 // toolbox. Paused/hidden worlds may publish newly VERIFIED products, but
 // never gain command authority. Unknown/changed worlds publish no offers.
 if(!sh||sh->stop||!(sh->flags&BuildAnywhere)||sh->placementImplementation!=2||snap.status!=1||snap.match!=match||
    (snap.commandStatus!=activation::Ready&&snap.commandStatus!=activation::Paused&&snap.commandStatus!=activation::DisplayHidden)||
    !lastHouse||!nativePlacementRetired())return;
 uint32_t house=0;int frame=-1,session=-1;
 if(!read(0xA83D4C,&house,4)||house!=lastHouse||!read(0xA8ED84,&frame,4)||frame!=snap.frame||
    !read(0xA8B238,&session,4)||session!=snap.session||!eventdelivery::mayQueue(session))return;
 for(int tab=0;tab<nativeplacement::MaxOffers;++tab){nativeplacement::Offer offer{};
  if(nativePlacementCapture(lastHouse,tab,now,offer)&&!nativePlacementCompletionOwns(offer))snap.placementOffers[snap.placementOfferCount++]=offer;
 }
}
static bool nativePlacementSignature(uint32_t at,const uint8_t*expected,size_t n){uint8_t bytes[16]{};return n<=sizeof bytes&&read(at,bytes,n)&&!memcmp(bytes,expected,n);}
static bool validateNativePlacement(const intent::EventOrder&o,const NativePlacementSubmission*submission){
 if(o.domain!=vanilla::Domain::Place)return !submission;
 if(!submission||!nativePlacementAvailable())return false;
 const auto&r=submission->request;const auto&expected=r.offer;const DWORD now=sampleClock(sh);
 if((submission->interactive?!nativePlacementInteractiveValid(*submission):
    (sh->command!=PlaceCompletedBuilding||sh->request!=submission->serial||sh->ack==submission->serial||
    sh->requestMatch!=match||DWORD(now-sh->requestTick)>nativeplacement::FreshMs||sh->arg!=expected.slot||
    sh->requestTypeHash!=expected.typeHash))||!nativeplacement::fresh(expected,match,now,snap.frame)||expected.house!=lastHouse||
    expected.typeHash!=typeListHash(snap.types,snap.typeCount)||o.player!=get<int>(lastHouse+0x30)||
    o.eventKind!=vanilla::EventType::Place||o.a!=7||o.b!=expected.typeIndex||o.c!=expected.naval||
    o.cell.x!=r.cellX||o.cell.y!=r.cellY||!nativeplacement::cell(r.cellX,r.cellY))return false;
 NativePlacementViewport view{};nativeplacement::Offer current{};
 static const uint8_t ctor[]={0x53,0x8B,0x5C,0x24,0x0C,0x8B,0xC3,0x56};
 return nativePlacementViewport(r,view)&&view==submission->view&&
  nativePlacementCapture(lastHouse,expected.tab,now,current)&&nativeplacement::sameIdentity(expected,current)&&
  nativeplacement::targetUsable([](uint32_t at,void*out,size_t n){return read(at,out,n);},r.cellX,r.cellY)&&
  nativePlacementSignature(0x4C6AE0,ctor,sizeof ctor);
}
static bool nativePlacementPacket(const Event&e,const intent::EventOrder&o){
 if(o.domain!=vanilla::Domain::Place)return true;
 int32_t type=0,index=0,naval=0;vanilla::Cell cell{};
 memcpy(&type,e.payload,4);memcpy(&index,e.payload+4,4);memcpy(&naval,e.payload+8,4);memcpy(&cell,e.payload+12,4);
 return e.kind==vanilla::EventType::Place&&e.house==o.player&&type==7&&index==o.b&&naval==o.c&&cell.x==o.cell.x&&cell.y==o.cell.y;
}
static int nativePlacementCommand(uint32_t house,DWORD now){
 if(!nativePlacementAvailable())return nativePlacementRetired()?-3:-2;
 if(sh->arg2==1)return nativePlacementArm(house,now);
 NativePlacementSubmission submission{};submission.request=sh->placementRequest;submission.serial=sh->request;
 auto&r=submission.request;
 if(nativePlacementCompletionOwns(r.offer)||!nativeplacement::fresh(r.offer,match,now,snap.frame)||r.offer.house!=house||sh->arg!=r.offer.slot||
    sh->requestTypeHash!=r.offer.typeHash)return -7;
 nativeplacement::Offer current{};
 if(!nativePlacementCapture(house,r.offer.tab,now,current)||!nativeplacement::sameIdentity(r.offer,current))return -7;
 nativeviewportprojection::Transform transform{};
 // This legacy one-shot mailbox has no capture generation. Reject while a
 // scaled view is active rather than interpreting an old click in a new view.
 if(!nativeViewTransform(transform)||transform.active||!nativePlacementViewport(r,submission.view)||
    submission.view.generation!=transform.generation||!nativeViewQueryCompatible())return -4;
 NativeViewPoint point{r.screenX,r.screenY};
 if(!nativeViewMapPoint(point)||!nativeViewTransformUnchanged(transform))return -4;
 point.x-=transform.logical.x;point.y-=transform.logical.y;
 vanilla::Cell cell{};struct Coord{int x,y,z;}coord{};uint32_t object=0;uint8_t a=0,b=0;
 // Query only, in the same leased game-thread route as pointerLocation().
 if(!nativeViewQuery(&point,&cell,&coord,&object,&a,&b)||!nativeViewTransformUnchanged(transform))return -4;
 r.cellX=cell.x;r.cellY=cell.y;
 const auto order=intent::placeBuilding(get<int>(house+0x30),r.offer.typeIndex,r.offer.naval,cell);
 if(!validateNativePlacement(order,&submission))return -7;
 // The ordinary event owns all effects. A positive receipt means QUEUED.
 if(!submitConstructed(order,OrderAuthority::Local,&submission))return snap.lastSubmitError==43?-7:-6;
 nativePlacementQueued(r.offer,now);return 1;
}
#include "native_placement_preview_bridge.h"

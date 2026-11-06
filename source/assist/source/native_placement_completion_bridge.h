#include "../../shared/source/debug_entry_checks.h"
#pragma once
// Included inside the existing game-thread lease, after read/put/call. A Place
// receipt only means queued. Q/W caches retire on queue acceptance; native
// cursor foundations retire only after the exact product is live on the map.
// This observer sends no order.
#include "native_placement_policy.h"
struct NativePlacementCompletion {
 nativeplacement::Offer offer{};
 uint32_t scenario=0;
 DWORD queuedAt=0;
 int houseIndex=-1,session=-1,lastFrame=-1;
};
static NativePlacementCompletion placementCompletions[8];
static constexpr DWORD NativePlacementCompletionTimeout=10000;
static void nativePlacementCompletionReset(){for(auto&p:placementCompletions)p={};}
static bool nativePlacementCompletionOwns(const nativeplacement::Offer&o){
 for(const auto&p:placementCompletions)if(p.scenario&&p.offer.match==o.match&&p.offer.house==o.house&&p.offer.houseID==o.houseID&&p.offer.product==o.product&&p.offer.productID==o.productID&&p.offer.type==o.type&&p.offer.typeID==o.typeID)return true;
 return false;
}
static bool nativePlacementCompletionPending(){for(const auto&p:placementCompletions)if(p.scenario)return true;return false;}
template<class T>static bool nativePlacementCompletionField(uint32_t at,T&value){
 return at>=0x10000&&uint64_t(at)+sizeof(T)<0x80000000ull&&read(at,&value,sizeof value);
}
// Unknown reads wait without writing; a known different world retires the
// receipt. Rewinds are compared with every successfully observed native frame.
static int nativePlacementCompletionWorld(const NativePlacementCompletion&p,int&frame){
 if(p.offer.match!=match||p.offer.house!=lastHouse)return -1;
 uint32_t house=0,scenario=0,id=0;int index=-1,session=-1;
 if(!nativePlacementCompletionField(0xA83D4C,house)||!nativePlacementCompletionField(0xA8B230,scenario)||
    !nativePlacementCompletionField(0xA8B238,session)||!nativePlacementCompletionField(0xA8ED84,frame))return 0;
 if(house!=p.offer.house||scenario!=p.scenario||session!=p.session||frame<p.lastFrame)return -1;
 if(!nativePlacementCompletionField(house+0x10,id)||!nativePlacementCompletionField(house+0x30,index))return 0;
 return id==p.offer.houseID&&index==p.houseIndex?1:-1;
}
// -1 is a different/dead object, 0 is uncertain or not placed yet, 1 is the
// exact queued product now alive on the map (or still Limbo when explicitly
// permitted for Q/W cleanup). Factory consumption is no proof.
static int nativePlacementCompletionProduct(const NativePlacementCompletion&p,bool allowQueued=false){
 const auto&o=p.offer;uint32_t vt=0,id=0,type=0,typeID=0,owner=0;int hp=0;uint8_t alive=0,onMap=0,limbo=1;
 if(!nativePlacementCompletionField(o.product,vt)||!nativePlacementCompletionField(o.product+0x10,id)||
    !nativePlacementCompletionField(o.product+0x520,type)||!nativePlacementCompletionField(o.product+0x21C,owner)||
    !nativePlacementCompletionField(o.product+0x6C,hp)||!nativePlacementCompletionField(o.product+0x90,alive)||
    !nativePlacementCompletionField(o.product+0x74,onMap)||!nativePlacementCompletionField(o.product+0x81,limbo))return 0;
 if(vt!=0x7E3EBC||id!=o.productID||type!=o.type||owner!=o.house||hp<=0||!alive)return -1;
 if(!nativePlacementCompletionField(type+0x10,typeID))return 0;
 if(typeID!=o.typeID)return -1;
 return (onMap&&!limbo)||(allowQueued&&!onMap&&limbo)?1:0;
}
static int nativePlacementCompletionIdle(){
 uint8_t tools[5]{};int super=-1;
 if(!read(0x87F7E8+0x11B0,tools,sizeof tools)||!nativePlacementCompletionField(0x8809A0,super))return 0;
 return !tools[0]&&!tools[1]&&!tools[2]&&!tools[3]&&!tools[4]&&super<0?1:-1;
}
static bool nativePlacementCompletionSignature(uint32_t at,uint32_t expected){
 uint8_t bytes[12]{};const bool readable=read(at,bytes,sizeof bytes);uint32_t hash=2166136261u;
 for(auto b:bytes)hash=(hash^b)*16777619u;return debugentries::allow(debugentries::Kind::Call,at,readable,hash==expected);
}
struct NativePlacementCursorIdentity {uint32_t product=0,type=0;int owner=-1;};
static_assert(sizeof(NativePlacementCursorIdentity)==12,"native placement cursor identity");
static bool nativePlacementCompletionMatches(const NativePlacementCompletion&p,const NativePlacementCursorIdentity&s){
 return s.product==p.offer.product&&s.type==p.offer.type&&s.owner==p.houseIndex;
}
// Each cursor is independent. A different native product is already finished
// from this receipt's perspective and is never altered. Failed reads/signatures
// retain the receipt until its deadline, so a transient short read is harmless.
static bool nativePlacementCompletionCursor(const NativePlacementCompletion&p,uint32_t offset,uint32_t function,uint32_t signature){
 NativePlacementCursorIdentity before{};
 if(!read(0x87F7E8+offset,&before,sizeof before))return false;
 if(!nativePlacementCompletionMatches(p,before))return true;
 if(!nativePlacementCompletionSignature(function,signature))return false;
 int frame=-1;NativePlacementCursorIdentity verified{};
 if(nativePlacementCompletionWorld(p,frame)!=1||nativePlacementCompletionProduct(p)!=1||nativePlacementCompletionIdle()!=1||
    !read(0x87F7E8+offset,&verified,sizeof verified)||!nativePlacementCompletionMatches(p,verified))return false;
 call<void>(function,0x87F7E8,(void*)nullptr);
 // The verified setters retire only their foundation. If an interposed native
 // callback changed the product meanwhile, do not overwrite its new selection.
 NativePlacementCursorIdentity after{};
 if(!read(0x87F7E8+offset,&after,sizeof after))return false;
 if(nativePlacementCompletionMatches(p,after)){
  put<uint32_t>(0x87F7E8+offset,0);put<uint32_t>(0x87F7E8+offset+4,0);put<int>(0x87F7E8+offset+8,-1);
 }
 return true;
}
static bool nativePlacementCompletionQuick(const NativePlacementCompletion&,bool allowQueued);
static void nativePlacementQueued(const nativeplacement::Offer&offer,DWORD now){
 if(!sh||sh->stop||fault||!nativeplacement::valid(offer)||offer.match!=match||offer.house!=lastHouse)return;
 NativePlacementCompletion pending{};pending.offer=offer;pending.queuedAt=now;pending.lastFrame=offer.frame;
 if(!nativePlacementCompletionField(0xA8B230,pending.scenario)||pending.scenario<0x10000||
    !nativePlacementCompletionField(0xA8B238,pending.session)||
    !nativePlacementCompletionField(offer.house+0x30,pending.houseIndex)||pending.houseIndex<0||pending.houseIndex>=32)return;
 int frame=-1;if(nativePlacementCompletionWorld(pending,frame)!=1)return;pending.lastFrame=frame;
 NativePlacementCompletion*slot=nullptr;
 for(auto&p:placementCompletions){
  if(p.scenario&&nativeplacement::sameIdentity(p.offer,offer)&&p.scenario==pending.scenario&&p.houseIndex==pending.houseIndex){slot=&p;break;}
  if(!p.scenario||DWORD(now-p.queuedAt)>=NativePlacementCompletionTimeout){if(!slot)slot=&p;}
 }
 if(!slot){slot=&placementCompletions[0];for(auto&p:placementCompletions)if(DWORD(now-p.queuedAt)>DWORD(now-slot->queuedAt))slot=&p;}
 *slot=pending; // Bounded local observation; dropping an old receipt never writes UI.
 // Native MouseUp retires its Q/W cache immediately after queueing, before
 // the receiver places the building. Keep the same UI-only ordering.
 nativePlacementCompletionQuick(*slot,true);
}
// Q/W use independent completed-building caches, populated by the native
// sidebar. Normal MouseUp calls 734270(product); direct Place bypasses it.
static bool nativePlacementCompletionQuick(const NativePlacementCompletion&p,bool allowQueued){
 for(unsigned pass=0;pass<2;++pass){
  uint32_t q=0,w=0;if(!read(0xB0FE5C,&q,4)||!read(0xB0FE60,&w,4))return false;
  if(q!=p.offer.product&&w!=p.offer.product)return true;
  uint8_t code[45]{};const bool readable=read(0x734270,code,sizeof code);uint32_t hash=2166136261u;
  for(auto b:code)hash=(hash^b)*16777619u;if(!debugentries::allow(debugentries::Kind::Call,0x734270,readable,hash==0x61003681u))return false;
  int frame=-1;uint32_t q2=0,w2=0;
  if(nativePlacementCompletionWorld(p,frame)!=1||nativePlacementCompletionProduct(p,allowQueued)!=1||
     !read(0xB0FE5C,&q2,4)||!read(0xB0FE60,&w2,4)||q!=q2||w!=w2)return false;
  // This exact native helper clears at most one matching slot per call.
  // Two bounded calls handle both caches; null would clear unrelated entries.
  call<void>(0x734270,p.offer.product);
 }
 uint32_t q=0,w=0;return read(0xB0FE5C,&q,4)&&read(0xB0FE60,&w,4)&&q!=p.offer.product&&w!=p.offer.product;
}

static void nativePlacementCompletionTick(DWORD now){
 if(!sh||sh->stop||fault){nativePlacementCompletionReset();return;}
 for(auto&p:placementCompletions){
  if(!p.scenario)continue;
  if(DWORD(now-p.queuedAt)>=NativePlacementCompletionTimeout){p={};continue;}
  int frame=-1;const int world=nativePlacementCompletionWorld(p,frame);
  if(world<0){p={};continue;}if(!world)continue;p.lastFrame=frame;
  const int product=nativePlacementCompletionProduct(p);if(product<0){p={};continue;}
  // Retire only this accepted product's quick cache even while its event is
  // pending. A later tool owns its own cursor; keep observation until idle.
  const bool quick=nativePlacementCompletionQuick(p,true);
  if(!product)continue;
  const int idle=nativePlacementCompletionIdle();if(idle!=1)continue;
  const bool active=nativePlacementCompletionCursor(p,0x11A4,0x4A8BF0,0xC44D9D5Eu);
  const bool copy=nativePlacementCompletionCursor(p,0x1190,0x4A8D50,0xDC84E30Eu);
  if(active&&copy&&quick)p={};
 }
}

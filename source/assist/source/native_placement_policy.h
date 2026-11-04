#pragma once
#include <stdint.h>
#include <type_traits>
namespace assist { namespace nativeplacement {
constexpr int MaxOffers=2;
constexpr uint32_t FreshMs=3500;
struct alignas(8) Offer {
 uint32_t match,tick;
 int32_t frame,slot,tab;
 uint32_t house,houseID,factory,factoryID,product,productID,type,typeID;
 int32_t typeIndex,naval;
 uint64_t typeHash;
};
struct alignas(8) Request {
 Offer offer;
 // Host sends the captured full-client point. Runtime resolves cell exactly
 // once with native ProcessClickCoords, then preserves it through admission.
 int32_t cellX,cellY,screenX,screenY,width,height,cameraX,cameraY;
};
// Read-only overlay state. This is not a native placement cursor or a queued order.
struct alignas(8) Preview {
 Offer offer{};
 uint32_t match=0,tick=0;
 int32_t active=0,cellX=0,cellY=0,x=0,y=0,z=0,width=0,height=0,status=0;
 double zScale=.0866025403784;
 int32_t cameraX=0,cameraY=0,viewportX=0,viewportY=0;
 uint32_t viewportGeneration=0;
};
static_assert(std::is_standard_layout<Offer>::value&&std::is_trivial<Offer>::value,"placement offer IPC");
static_assert(std::is_standard_layout<Request>::value&&std::is_trivial<Request>::value,"placement request IPC");
static_assert(sizeof(Offer)==72&&alignof(Offer)==8&&sizeof(Request)==104&&alignof(Request)==8,"placement IPC layout");
inline bool sameIdentity(const Offer&a,const Offer&b){
 return a.match==b.match&&a.slot==b.slot&&a.tab==b.tab&&a.house==b.house&&a.houseID==b.houseID&&
  a.factory==b.factory&&a.factoryID==b.factoryID&&a.product==b.product&&a.productID==b.productID&&
  a.type==b.type&&a.typeID==b.typeID&&a.typeIndex==b.typeIndex&&a.naval==b.naval&&a.typeHash==b.typeHash;
}
inline bool valid(const Offer&o){
 return o.match&&o.frame>=0&&o.slot>=0&&o.slot<512&&o.tab>=0&&o.tab<2&&
  o.house>=0x10000&&o.houseID&&o.factory>=0x10000&&o.factoryID&&o.product>=0x10000&&o.productID&&
  o.type>=0x10000&&o.typeID&&o.typeIndex>=0&&o.typeIndex<4096&&(o.naval==0||o.naval==1)&&o.typeHash;
}
inline bool fresh(const Offer&o,uint32_t match,uint32_t now,int frame){
 return valid(o)&&o.match==match&&uint32_t(now-o.tick)<=FreshMs&&frame>=o.frame;
}
inline bool cell(int x,int y){return x>0&&y>0&&x<512&&y<512;}
inline bool screen(const Request&r){return r.width>0&&r.height>0&&r.width<=32768&&r.height<=32768&&
 r.screenX>=0&&r.screenY>=0&&r.screenX<r.width&&r.screenY<r.height;}
}} // assist::nativeplacement

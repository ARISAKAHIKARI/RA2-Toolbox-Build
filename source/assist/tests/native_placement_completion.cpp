// Production completion observer; synthetic private pages and explicit native
// setter seams. No process attachment, input injection or event API exists here.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#ifdef NDEBUG
#error Completion fixture requires active assertions (-UNDEBUG).
#endif
#undef assert
#define assert(v) ((v)?(void)0:(std::fprintf(stderr,"assert %s at %d\n",#v,__LINE__),std::_Exit(3)))
#include "../source/native_placement_policy.h"
using namespace assist;
struct Shared {uint32_t stop=0;};
static Shared shared,*sh=&shared;static bool fault=false;
static uint32_t match=7,lastHouse=0x20010000;
static uint8_t globals[0x500000]{},objects[0x100000]{};
static uint8_t activeCode[12]={0x83,0xEC,0x08,0x53,0x8B,0xD9,0x56,0x8B,0x83,0x7C,0x11,0};
static uint8_t quickCode[45]={0x33,0xc0,0x3b,0xc8,0x75,0x0b,0xa3,0x5c,0xfe,0xb0,0x00,0xa3,0x60,0xfe,0xb0,0x00,0xc3,0x3b,0x0d,0x5c,0xfe,0xb0,0x00,0x75,0x06,0xa3,0x5c,0xfe,0xb0,0x00,0xc3,0x3b,0x0d,0x60,0xfe,0xb0,0x00,0x75,0x05,0xa3,0x60,0xfe,0xb0,0x00,0xc3};
static uint8_t copyCode[12]={0x83,0xEC,0x08,0x53,0x8B,0xD9,0x56,0x8B,0x83,0x8C,0x11,0};
static uint32_t failRead=0;static bool partialFailure=true;
static int calls=0,activeCalls=0,copyCalls=0,quickCalls=0,writes=0;static std::function<void(uint32_t)> onCall;
constexpr uint32_t Map=0x87F7E8,House=0x20010000,Product=0x20020000,Type=0x20030000,Factory=0x20040000,Scenario=0x20050000;
static void* memory(uint32_t at,size_t n){
 if(at>=0x700000&&uint64_t(at)+n<=0xC00000)return globals+at-0x700000;
 if(at>=0x20000000&&uint64_t(at)+n<=0x20100000)return objects+at-0x20000000;
 return nullptr;
}
template<class T>static void set(uint32_t at,T value){void*p=memory(at,sizeof value);assert(p);memcpy(p,&value,sizeof value);}
template<class T>static T value(uint32_t at){T out{};void*p=memory(at,sizeof out);assert(p);memcpy(&out,p,sizeof out);return out;}
static bool read(uint32_t at,void*out,size_t n){
 const void*p=at==0x734270&&n==45?quickCode:at==0x4A8BF0&&n==12?activeCode:at==0x4A8D50&&n==12?copyCode:memory(at,n);
 if(!p)return false;
 if(failRead&&at<=failRead&&uint64_t(at)+n>failRead){if(partialFailure&&n)memcpy(out,p,n-1);return false;}
 memcpy(out,p,n);return true;
}
template<class T>static void put(uint32_t at,T v){
 assert((at>=Map+0x1190&&at<=Map+0x1198)||(at>=Map+0x11A4&&at<=Map+0x11AC));
 assert(sizeof(T)==4&&(at%4)==0);++writes;set(at,v);
}
template<class R,class...Args>static R call(uint32_t function,uint32_t self,Args...args){
 static_assert(std::is_void<R>::value);
 if constexpr(sizeof...(Args)==0){
  assert(function==0x734270&&self>=0x10000);++quickCalls;
  if(value<uint32_t>(0xB0FE5C)==self)set<uint32_t>(0xB0FE5C,0);
  else if(value<uint32_t>(0xB0FE60)==self)set<uint32_t>(0xB0FE60,0);
 }else{
  static_assert(sizeof...(Args)==1);assert(self==Map&&std::get<0>(std::make_tuple(args...))==nullptr);
  assert(function==0x4A8BF0||function==0x4A8D50);++calls;
  if(function==0x4A8BF0){++activeCalls;set<uint32_t>(Map+0x117C,0);}else{++copyCalls;set<uint32_t>(Map+0x118C,0);}
  if(onCall)onCall(function);
 }
}
#include "../source/native_placement_completion_bridge.h"
static nativeplacement::Offer offer;
static void cursor(uint32_t offset,uint32_t product=Product,uint32_t type=Type,int owner=3){set<uint32_t>(Map+offset,product);set<uint32_t>(Map+offset+4,type);set<int>(Map+offset+8,owner);}
static void completed(){set<uint8_t>(Product+0x74,1);set<uint8_t>(Product+0x81,0);}
static int pending(){int n=0;for(const auto&p:placementCompletions)n+=p.scenario!=0;return n;}
static void clearAssert(uint32_t offset){assert(!value<uint32_t>(Map+offset)&&!value<uint32_t>(Map+offset+4)&&value<int>(Map+offset+8)==-1);}
static void untouched(uint32_t offset,uint32_t product=Product,uint32_t type=Type,int owner=3){assert(value<uint32_t>(Map+offset)==product&&value<uint32_t>(Map+offset+4)==type&&value<int>(Map+offset+8)==owner);}
static void reset(){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);shared={};sh=&shared;fault=false;match=7;lastHouse=House;
 failRead=0;partialFailure=true;calls=activeCalls=copyCalls=quickCalls=writes=0;quickCode[0]=0x33;onCall={};activeCode[0]=copyCode[0]=0x83;nativePlacementCompletionReset();
 offer={7,1000,300,0,0,House,11,Factory,21,Product,31,Type,41,0,0,123};
 set<uint32_t>(0xA83D4C,House);set<uint32_t>(0xA8B230,Scenario);set<int>(0xA8B238,3);set<int>(0xA8ED84,300);
 set<uint32_t>(House+0x10,11);set<int>(House+0x30,3);
 set<uint32_t>(Product,0x7E3EBC);set<uint32_t>(Product+0x10,31);set<uint32_t>(Product+0x520,Type);set<uint32_t>(Product+0x21C,House);
 set<int>(Product+0x6C,800);set<uint8_t>(Product+0x90,1);set<uint8_t>(Product+0x81,1);set<uint8_t>(Product+0x74,0);set<uint32_t>(Type+0x10,41);
 set<int>(0x8809A0,-1);cursor(0x11A4);cursor(0x1190,0,0,-1);set<uint32_t>(Map+0x117C,0x22220000);set<uint32_t>(Map+0x118C,0x33330000);
}
int main(){unsigned checks=0;auto check=[&](bool b){assert(b);++checks;};
 reset();nativePlacementQueued(offer,1000);check(pending()==1&&!calls&&!writes);nativePlacementCompletionTick(1016);untouched(0x11A4);check(pending()==1&&!calls&&!writes);
 // Neither a receipt nor consumed Factory proves successful placement.
 set<uint32_t>(Factory+0x58,0);set<int>(Factory+0x24,0);nativePlacementCompletionTick(1032);check(pending()==1&&!calls&&!writes);
 completed();nativePlacementCompletionTick(1048);clearAssert(0x11A4);check(activeCalls==1&&!copyCalls&&writes==3&&!pending()&&!value<uint32_t>(Map+0x117C));
 nativePlacementCompletionTick(1064);check(calls==1&&writes==3);
 reset();cursor(0x11A4,0,0,-1);cursor(0x1190);nativePlacementQueued(offer,1000);completed();nativePlacementCompletionTick(1016);clearAssert(0x1190);check(copyCalls==1&&!activeCalls&&writes==3&&!pending()&&!value<uint32_t>(Map+0x118C));
 reset();cursor(0x1190);nativePlacementQueued(offer,1000);completed();nativePlacementCompletionTick(1016);clearAssert(0x11A4);clearAssert(0x1190);check(activeCalls==1&&copyCalls==1&&writes==6&&!pending());
 reset();cursor(0x11A4,0,0,-1);nativePlacementQueued(offer,1000);completed();nativePlacementCompletionTick(1016);check(!calls&&!writes&&!pending());
 // Exact independent triple: foreign product/type/House index remain native.
 for(int k=0;k<3;++k){reset();uint32_t product=k==0?Product+0x100:Product,type=k==1?Type+0x100:Type;int owner=k==2?4:3;cursor(0x11A4,product,type,owner);cursor(0x1190);nativePlacementQueued(offer,1000);completed();nativePlacementCompletionTick(1016);untouched(0x11A4,product,type,owner);clearAssert(0x1190);check(!activeCalls&&copyCalls==1&&writes==3&&!pending());}
 // Pointer reuse, dead products and owner changes cannot be mistaken for done.
 for(int k=0;k<7;++k){reset();nativePlacementQueued(offer,1000);completed();switch(k){case 0:set<uint32_t>(Product+0x10,32);break;case 1:set<uint32_t>(Product+0x520,Type+0x100);break;case 2:set<uint32_t>(Type+0x10,42);break;case 3:set<uint32_t>(Product+0x21C,House+0x100);break;case 4:set<uint32_t>(Product,0x7E3EBD);break;case 5:set<int>(Product+0x6C,0);break;case 6:set<uint8_t>(Product+0x90,0);break;}nativePlacementCompletionTick(1016);untouched(0x11A4);check(!pending()&&!calls&&!writes);}
 for(int k=0;k<2;++k){reset();nativePlacementQueued(offer,1000);set<uint8_t>(Product+0x74,k?0:1);set<uint8_t>(Product+0x81,k?0:1);nativePlacementCompletionTick(1016);check(pending()==1&&!calls&&!writes);}
 for(int k=0;k<8;++k){reset();nativePlacementQueued(offer,1000);completed();switch(k){case 0:++match;break;case 1:lastHouse+=0x100;break;case 2:set<uint32_t>(0xA83D4C,House+0x100);break;case 3:set<uint32_t>(0xA8B230,Scenario+0x100);break;case 4:set<int>(0xA8B238,0);break;case 5:set<uint32_t>(House+0x10,12);break;case 6:set<int>(House+0x30,4);break;case 7:set<int>(0xA8ED84,299);break;}nativePlacementCompletionTick(1016);check(!pending()&&!calls&&!writes);}
 reset();nativePlacementQueued(offer,1000);set<int>(0xA8ED84,310);nativePlacementCompletionTick(1016);set<int>(0xA8ED84,309);completed();nativePlacementCompletionTick(1032);check(!pending()&&!calls&&!writes);
 // Read failures may partially fill output. Every one still fails closed.
 const uint32_t reads[]={0xA83D4C,0xA8B230,0xA8B238,0xA8ED84,House+0x10,House+0x30,Product,Product+0x10,Product+0x520,Product+0x21C,Product+0x6C,Product+0x90,Product+0x74,Product+0x81,Type+0x10,Map+0x11B2,0x8809A0,Map+0x11A8,0x4A8BF5};
 for(auto at:reads){reset();nativePlacementQueued(offer,1000);completed();failRead=at;nativePlacementCompletionTick(1016);untouched(0x11A4);check(pending()==1&&!calls&&!writes);failRead=0;nativePlacementCompletionTick(1032);clearAssert(0x11A4);check(calls==1&&writes==3&&!pending());}
 for(int k=0;k<2;++k){reset();cursor(0x1190);nativePlacementQueued(offer,1000);completed();(k?copyCode:activeCode)[0]=0xE9;nativePlacementCompletionTick(1016);check(pending()==1&&calls==1&&writes==3&&(k?activeCalls:copyCalls)==1);untouched(k?0x1190:0x11A4);(k?copyCode:activeCode)[0]=0x83;nativePlacementCompletionTick(1032);check(!pending()&&calls==2&&writes==6);}
 for(int k=0;k<6;++k){reset();nativePlacementQueued(offer,1000);completed();if(k<5)set<uint8_t>(Map+0x11B0+k,1);else set<int>(0x8809A0,0);nativePlacementCompletionTick(1016);check(pending()==1&&!calls&&!writes);untouched(0x11A4);if(k<5)set<uint8_t>(Map+0x11B0+k,0);else set<int>(0x8809A0,-1);nativePlacementCompletionTick(1032);check(!pending()&&activeCalls==1&&writes==3);}
 for(int k=0;k<3;++k){reset();nativePlacementQueued(offer,1000);completed();if(k==0)shared.stop=1;else if(k==1)fault=true;else sh=nullptr;nativePlacementCompletionTick(1016);check(!pending()&&!calls&&!writes);}
 reset();nativePlacementQueued(offer,1000);nativePlacementCompletionTick(10999);check(pending()==1);completed();nativePlacementCompletionTick(11000);check(!pending()&&!calls&&!writes);
 reset();nativePlacementQueued(offer,0xFFFFFFF0u);completed();nativePlacementCompletionTick(0x000003D8u);check(!pending()&&calls==1&&writes==3);
 reset();nativePlacementQueued(offer,0xFFFFFFF0u);completed();nativePlacementCompletionTick(DWORD(0xFFFFFFF0u+10000u));check(!pending()&&!calls&&!writes);
 reset();for(int i=0;i<10;++i)nativePlacementQueued(offer,1000+i);check(pending()==1&&!calls&&!writes);nativePlacementCompletionReset();check(!nativePlacementCompletionPending()&&!calls&&!writes);
 reset();for(int i=0;i<9;++i){auto other=offer;other.productID+=i;nativePlacementQueued(other,1000+i);}check(pending()==8&&!calls&&!writes);
 reset();auto invalid=offer;invalid.productID=0;nativePlacementQueued(invalid,1000);check(!pending());
 reset();failRead=House+0x30;nativePlacementQueued(offer,1000);check(!pending()&&!calls&&!writes);
 // Q/W follow original MouseUp ordering, before the product leaves Limbo.
 for(int k=0;k<3;++k){reset();set<uint32_t>(0xB0FE5C,Product);set<uint32_t>(0xB0FE60,k==0?Product:Product+0x100);if(k==2)set<uint8_t>(Map+0x11B0,1);nativePlacementQueued(offer,1000);check(quickCalls==(k==0?2:1)&&pending()==1&&!calls&&!writes);check(value<uint32_t>(0xB0FE5C)==0&&value<uint32_t>(0xB0FE60)==(k==0?0:Product+0x100));untouched(0x11A4);nativePlacementCompletionTick(1016);check(pending()==1&&!calls&&!writes);}
 // A short read or unknown helper retains the exact cache and retries later.
 for(int k=0;k<3;++k){reset();set<uint32_t>(0xB0FE5C,Product);if(k==0)failRead=0x734270;else if(k==1)quickCode[0]=0x90;else failRead=Product+0x81;nativePlacementQueued(offer,1000);check(!quickCalls&&value<uint32_t>(0xB0FE5C)==Product&&pending()==1);failRead=0;quickCode[0]=0x33;nativePlacementCompletionTick(1016);check(quickCalls==1&&value<uint32_t>(0xB0FE5C)==0&&pending()==1&&!calls&&!writes);}
 // The sidebar can republish a matching cache while the event is in flight.
 reset();nativePlacementQueued(offer,1000);set<uint32_t>(0xB0FE60,Product);nativePlacementCompletionTick(1016);check(quickCalls==1&&!value<uint32_t>(0xB0FE60)&&pending()==1);completed();nativePlacementCompletionTick(1032);check(!pending()&&calls==1);
 // Invalid/replaced queued products never authorize quick-cache calls.
 reset();set<uint32_t>(0xB0FE5C,Product);set<uint32_t>(Product+0x10,99);nativePlacementQueued(offer,1000);check(!quickCalls&&value<uint32_t>(0xB0FE5C)==Product);nativePlacementCompletionTick(1016);check(!pending()&&!quickCalls&&!calls&&!writes);
 // A native callback's replacement is never overwritten after the setter.
 reset();nativePlacementQueued(offer,1000);completed();onCall=[](uint32_t){cursor(0x11A4,Product+0x100,Type+0x100,4);};nativePlacementCompletionTick(1016);untouched(0x11A4,Product+0x100,Type+0x100,4);check(calls==1&&!writes&&!pending());
 reset();nativePlacementQueued(offer,1000);completed();onCall=[](uint32_t){failRead=Map+0x11A8;};nativePlacementCompletionTick(1016);check(calls==1&&!writes&&pending()==1);failRead=0;onCall={};nativePlacementCompletionTick(1032);check(calls==2&&writes==3&&!pending());
 std::printf("{\"checks\":%u,\"assertions_active\":true,\"pass\":true}\n",checks);
}

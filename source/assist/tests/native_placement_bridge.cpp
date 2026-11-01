// Actual production Place helper + extracted production submit/enqueue.
// Synthetic private pages only; never attaches to or executes the game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <functional>
#include "../source/protocol.h"
#include "../source/constructor_adapter.h"
#include "../source/entry_gate.h"
#include "../source/event_delivery.h"
using namespace assist;
using Event=vanilla::Packet;
static Shared*sh;static Snapshot snap;
static uint32_t lastHouse,match,typePointers[MaxTypes];static int outgoingBudget=10;
static DWORD clockNow=1000;static bool ready=true,retired=true,clockOkay=true;
static uint32_t failRead=0;static std::function<void()> queryMutation,ctorMutation,clockMutation;
static int queries=0,constructorsCalled=0,clockCalls=0,reads=0;
static Event*constructedPacket=nullptr;
static uint8_t ctorCode[8]{},queryCode[8]{};
static uint8_t globals[0x500000]{},objects[0x400000]{};
static void* memory(uint32_t p,size_t n){
 if(p>=0x700000&&uint64_t(p)+n<=0xC00000)return globals+p-0x700000;
 if(p>=0x20000000&&uint64_t(p)+n<=0x20400000)return objects+p-0x20000000;
 return nullptr;
}
static void* queueCopy(void*dest,const void*src,size_t n){void*out=memory(uint32_t(uintptr_t(dest)),n);assert(out);memcpy(out,src,n);return dest;}
template<class T>static T get(uint32_t p){auto at=memory(p,sizeof(T));assert(at);return *(T*)at;}
template<class T>static void put(uint32_t p,T v){auto at=memory(p,sizeof(T));assert(at);*(T*)at=v;}
static bool read(uint32_t p,void*out,size_t n){++reads;if(failRead&&p<=failRead&&uint64_t(p)+n>failRead)return false;
 if(p==0x4C6AE0&&n==8){memcpy(out,ctorCode,8);return true;}if(p==0x692300&&n==8){memcpy(out,queryCode,8);return true;}
 auto at=memory(p,n);if(!at)return false;memcpy(out,at,n);return true;}
struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t p,int limit=16384){Vec v{};if(!read(p,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||v.items<0x10000)return {};return v;}
static DWORD sampleClock(Shared*){return clockNow;}
static bool simulationReady(){return ready&&get<uint32_t>(0xA83D4C)==lastHouse&&get<int>(0xA8ED84)==snap.frame&&eventdelivery::mayQueue(get<int>(0xA8B238));}
static bool nativePlacementRetired(){return retired;}
static int outgoingRoom(){return outgoingBudget>0&&eventdelivery::outListRoom(get<int>(0xA802C8),get<int>(0xA802D0));}
static bool gameSendTiming(uint32_t&n){++clockCalls;if(clockMutation)clockMutation();n=123;return clockOkay;}
static bool networkHousePreserveReady(){return false;}
static bool networkSenderHouse(int){return false;}
enum class OrderAuthority {Local,PlayerTest,SellPlayerTest,DestructPlayerTest};
static bool authorizePlayerOrder(const intent::EventOrder&o,OrderAuthority a){return a==OrderAuthority::Local&&o.player==get<int>(lastHouse+0x30);}
static entrygate::Snapshot currentEntryBoundary(){return {match,match,get<int>(0xA8B238),simulationReady(),false,true,outgoingRoom()>0};}
template<class R,class...Args>static R call(uint32_t address,uint32_t self,Args...args){
 auto values=std::make_tuple(args...);
 if constexpr(sizeof...(Args)==6){
  using First=typename std::tuple_element<0,decltype(values)>::type;
  if constexpr(std::is_pointer<First>::value){
   assert(address==0x692300&&self==0x87F7E8);++queries;
   auto cell=std::get<1>(values);cell->x=70;cell->y=70;if(queryMutation)queryMutation();
  }else if constexpr(std::is_pointer<typename std::tuple_element<5,decltype(values)>::type>::value){
   assert(address==0x4C6AE0);++constructorsCalled;auto*e=(Event*)(uintptr_t)self;constructedPacket=e;
   e->kind=uint8_t(std::get<1>(values));e->house=char(std::get<0>(values));e->frame=uint32_t(snap.frame);
   int fields[]={int(std::get<2>(values)),int(std::get<3>(values)),int(std::get<4>(values))};
   memcpy(e->payload,fields,12);memcpy(e->payload+12,std::get<5>(values),4);if(ctorMutation)ctorMutation();
  }else assert(false&&"unexpected six-field constructor");
 }else assert(false&&"unexpected constructor in placement test");
 if constexpr(!std::is_void<R>::value)return R{};
}
struct NativePlacementSubmission;
static bool validateNativePlacement(const intent::EventOrder&,const NativePlacementSubmission*);
static bool nativePlacementPacket(const Event&,const intent::EventOrder&);
#define memcpy queueCopy
#include "native_placement_entry_extract.h"
#undef memcpy
#include "../source/native_placement_bridge.h"
constexpr uint32_t House=0x20010000,BuildingType=0x20020000,Factory=0x20030000,Product=0x20040000,Tactical=0x20050000,Cell=0x20060000,TypeItems=0x20070000,Cells=0x20100000;
static void reset(){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);memset(sh,0,sizeof *sh);memset(&snap,0,sizeof snap);
 memset(typePointers,0,sizeof typePointers);lastHouse=House;match=7;clockNow=1000;ready=retired=clockOkay=true;failRead=0;
 outgoingBudget=10;queryMutation=ctorMutation=clockMutation={};queries=constructorsCalled=clockCalls=reads=0;constructedPacket=nullptr;
 sh->foreground=1;sh->flags=BuildAnywhere;sh->placementImplementation=2;sh->request=1;sh->command=PlaceCompletedBuilding;sh->requestMatch=match;sh->requestTick=clockNow;
 snap.status=snap.commandStatus=1;snap.frame=300;snap.match=match;snap.session=3;snap.typeCount=1;snap.types[0].kind=7;snap.types[0].index=0;
 memcpy(snap.types[0].id,"GAPOWR",7);typePointers[0]=BuildingType;
 put<uint32_t>(0xA83D4C,House);put<int>(0xA8ED84,snap.frame);put<int>(0xA8B238,3);
 put<uint32_t>(House+0x10,11);put<int>(House+0x30,3);put<uint32_t>(House+0x53BC,Factory);
 put<uint32_t>(Factory+0x10,21);put<uint32_t>(Factory+0x6C,House);put<int>(Factory+0x24,54);put<uint32_t>(Factory+0x58,Product);
 put<uint32_t>(Product,0x7E3EBC);put<uint32_t>(Product+0x10,31);put<uint32_t>(Product+0x21C,House);put<int>(Product+0x6C,800);
 put<uint8_t>(Product+0x90,1);put<uint8_t>(Product+0x81,1);put<uint32_t>(Product+0x520,BuildingType);put<uint32_t>(BuildingType+0x10,41);
 Vec types{0,TypeItems,1,0,1,0};memcpy(memory(0xA83C68,sizeof types),&types,sizeof types);put<uint32_t>(TypeItems,BuildingType);
 put<uint32_t>(0x887324,Tactical);put<int>(0x8A00A4,800);put<int>(0x8A00A8,600);put<int>(0xB0CE30,640);put<int>(0xB0CE34,480);
 int bounds[]={100,100,0,0,100,100};memcpy(memory(0x87F7E8+0xF4,sizeof bounds),bounds,sizeof bounds);
 put<uint32_t>(0x87F924,Cells);put<int>(0x87F928,512*512);put<uint32_t>(Cells+(70*512+70)*4,Cell);
 put<int16_t>(Cell+0x24,70);put<int16_t>(Cell+0x26,70);
 const uint8_t ctor[]={0x53,0x8B,0x5C,0x24,0x0C,0x8B,0xC3,0x56},query[]={0x8B,0x44,0x24,0x10,0x83,0xEC,0x18,0x8B};
 memcpy(ctorCode,ctor,8);memcpy(queryCode,query,8);
 nativePlacementOffers(clockNow);assert(snap.placementOfferCount==1);
 sh->placementRequest.offer=snap.placementOffers[0];sh->placementRequest.screenX=100;sh->placementRequest.screenY=100;sh->placementRequest.width=800;sh->placementRequest.height=600;
 sh->requestTypeHash=sh->placementRequest.offer.typeHash;
}
static int queued(){return get<int>(0xA802C8)+get<int>(0x8B41F8);}
static void unchangedFactory(){assert(get<uint32_t>(Factory+0x58)==Product&&get<int>(Factory+0x24)==54);}
int main(){
 // Game addresses are translated into this test process's private buffers.
 sh=new Shared{};int checks=0;
 auto reject=[&](const char*name){int r=nativePlacementCommand(House,clockNow);if(r>0||queued())printf("FAILED %s result=%d queued=%d\n",name,r,queued());assert(r<0&&queued()==0);unchangedFactory();++checks;};
 reset();assert(nativePlacementCommand(House,clockNow)==1&&queued()==1&&queries==1&&constructorsCalled==1&&clockCalls==1);unchangedFactory();
 {auto*e=(Event*)memory(0xA802D4,sizeof(Event));assert(e->kind==0x0B&&e->house==3&&get<int>(0xA802D4+7)==7&&get<int16_t>(0xA802D4+19)==70);}++checks;
 reset();put<int>(0xA8B238,0);snap.session=0;assert(nativePlacementCommand(House,clockNow)==1&&get<int>(0x8B41F8)==1&&get<int>(0xA802C8)==0);assert(((Event*)memory(0x8B4204,sizeof(Event)))->frame==301);unchangedFactory();++checks;
 reset();retired=false;reject("old placement ownership");
 reset();sh->placementImplementation=0;reject("legacy mode");
 reset();sh->foreground=0;nativePlacementOffers(clockNow+100);assert(snap.placementOfferCount==1&&snap.placementOffers[0].tick==clockNow+100);reject("focus denies command but observes choices");
 reset();snap.commandStatus=activation::Paused;nativePlacementOffers(clockNow+100);assert(snap.placementOfferCount==1);reject("paused denies command but observes choices");
 reset();snap.commandStatus=activation::DisplayHidden;nativePlacementOffers(clockNow+100);assert(snap.placementOfferCount==1);reject("hidden denies command but observes choices");
 reset();snap.commandStatus=activation::AuthorityUnknown;nativePlacementOffers(clockNow+100);assert(!snap.placementOfferCount);reject("unknown authority clears choices");
 reset();snap.commandStatus=-3;nativePlacementOffers(clockNow+100);assert(!snap.placementOfferCount);reject("suspended offer retired");
 reset();put<uint32_t>(Product+0x10,32);reject("product pointer reused");
 reset();put<uint32_t>(Factory+0x10,22);reject("factory pointer reused");
 reset();put<uint32_t>(Product+0x21C,House+100);reject("foreign product");
 reset();put<uint32_t>(TypeItems,BuildingType+100);reject("native type slot replaced");
 reset();put<uint32_t>(BuildingType+0x10,42);reject("type pointer reused");
 reset();put<uint8_t>(Product+0x81,0);reject("no longer in limbo");
 reset();put<uint8_t>(Product+0x74,1);reject("already on map");
 reset();put<int>(Factory+0x24,53);assert(nativePlacementCommand(House,clockNow)<0&&queued()==0&&get<int>(Factory+0x24)==53);++checks;
 reset();failRead=Factory+0x58;reject("read unavailable");
 reset();failRead=Cell+0x11B;reject("target read unavailable");
 reset();clockNow+=3501;reject("expired offer");
 reset();++match;reject("new epoch");
 reset();sh->placementRequest.screenX=700;reject("sidebar");
 reset();queryCode[0]=0xE9;reject("query entry changed");
 reset();ctorCode[0]=0xE9;reject("constructor entry changed");
 reset();queryMutation=[](){put<int>(Tactical+0xB0,1);};reject("camera changed at query");
 reset();queryMutation=[](){put<int>(0xB0CE30,600);};reject("surface changed at query");
 reset();ctorMutation=[](){put<uint32_t>(Product+0x10,32);};reject("product changed at constructor");
 reset();ctorMutation=[](){constructedPacket->house=4;};reject("constructor produced wrong player packet");
 reset();clockMutation=[](){put<uint32_t>(Factory+0x10,22);};reject("factory changed at clock");
 reset();clockMutation=[](){put<int>(Tactical+0xB4,1);};reject("camera changed at clock");
 reset();clockMutation=[](){retired=false;};reject("patch ownership changed at clock");
 reset();clockMutation=[](){++match;};reject("epoch changed at clock");
 reset();clockMutation=[](){sh->stop=1;};reject("stop changed at clock");
 reset();clockMutation=[](){++sh->request;};reject("request replaced at clock");
 reset();clockMutation=[](){put<int>(0xA8ED84,301);};reject("frame changed at clock");
 reset();clockMutation=[](){put<int>(0x87F7E8+0x100,80);};reject("usable rect changed at clock");
 reset();put<int>(0xA802C8,112);assert(nativePlacementCommand(House,clockNow)<0&&get<int>(0xA802C8)==112&&constructorsCalled==0);unchangedFactory();++checks;
 reset();auto order=intent::placeBuilding(3,0,0,{70,70});assert(!submitConstructed(order)&&queued()==0);++checks;
 auto good=intent::placeBuilding(3,0,0,{70,70});assert(constructors::admit(good)==constructors::Verdict::Ready);++checks;
 for(int n=0;n<8;++n){auto bad=good;switch(n){case 0:bad.a=40;break;case 1:bad.eventKind=0x0E;break;case 2:bad.b=-1;break;case 3:bad.c=2;break;case 4:bad.cell.x=0;break;case 5:bad.cell.y=512;break;case 6:bad.appendWaypoint=true;break;case 7:bad.tagA=1;break;}assert(constructors::admit(bad)==constructors::Verdict::InvalidFields);++checks;}
 assert(vanilla::exposure(vanilla::Domain::SellCell)==vanilla::Exposure::ProbeOnly&&vanilla::exposure(vanilla::Domain::Waypoints)==vanilla::Exposure::ProbeOnly);++checks;
 printf("native placement production admission/queue PASS checks=%d\n",checks);
 delete sh;
}

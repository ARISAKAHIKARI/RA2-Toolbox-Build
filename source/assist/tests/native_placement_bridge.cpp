// Actual production Place helper + extracted production submit/enqueue.
// Synthetic private pages only; never attaches to or executes the game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <functional>
#include <cmath>
#ifdef NDEBUG
#error Native placement fixture requires active assertions (-UNDEBUG).
#endif
#undef assert
#define assert(v) ((v)?(void)0:(std::fprintf(stderr,"assert %s at %d\n",#v,__LINE__),std::_Exit(3)))
#include "../source/protocol.h"
#include "../source/constructor_adapter.h"
#include "../source/entry_gate.h"
#include "../source/event_delivery.h"
using namespace assist;
using Event=vanilla::Packet;
static Shared*sh;static Snapshot snap;
static uint32_t lastHouse,match,typePointers[MaxTypes];static int outgoingBudget=10;
static DWORD clockNow=1000;static bool ready=true,retired=true,clockOkay=true,fault=false;
static uint32_t failRead=0;static std::function<void()> queryMutation,ctorMutation,clockMutation;
static int queries=0,constructorsCalled=0,clockCalls=0,reads=0,floorHeight=0;
static int completionActiveCalls=0,completionCopyCalls=0;
static Event*constructedPacket=nullptr;
static uint8_t ctorCode[8]{},queryCode[8]{},floorCode[12]={0x83,0xec,0x08,0x8b,0x4c,0x24,0x0c,0x56,0x8b,0x01,0x8b,0x51};
static uint8_t completionActiveCode[12]={0x83,0xEC,0x08,0x53,0x8B,0xD9,0x56,0x8B,0x83,0x7C,0x11,0};
static uint8_t completionCopyCode[12]={0x83,0xEC,0x08,0x53,0x8B,0xD9,0x56,0x8B,0x83,0x8C,0x11,0};
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
 if(p==0x578080&&n==12){memcpy(out,floorCode,12);return true;}
 if(p==0x4A8BF0&&n==12){memcpy(out,completionActiveCode,12);return true;}
 if(p==0x4A8D50&&n==12){memcpy(out,completionCopyCode,12);return true;}
 if(p==0x4C6AE0&&n==8){memcpy(out,ctorCode,8);return true;}if(p==0x692300&&n==8){memcpy(out,queryCode,8);return true;}
 auto at=memory(p,n);if(!at)return false;memcpy(out,at,n);return true;}
struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t p,int limit=16384){Vec v{};if(!read(p,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||v.items<0x10000)return {};return v;}
static DWORD sampleClock(Shared*){return clockNow;}
static bool simulationReady(bool preview=false){int frame=get<int>(0xA8ED84);return ready&&get<uint32_t>(0xA83D4C)==lastHouse&&frame>=0&&(preview?frame>=snap.frame:frame==snap.frame)&&get<int>(0xA8B238)==snap.session&&eventdelivery::mayQueue(get<int>(0xA8B238));}
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
 }else if constexpr(sizeof...(Args)==1){
  assert(self==0x87F7E8);
  if constexpr(std::is_void<R>::value){
   assert(std::get<0>(values)==nullptr&&(address==0x4A8BF0||address==0x4A8D50));
   if(address==0x4A8BF0){++completionActiveCalls;put<uint32_t>(self+0x117C,0);}else{++completionCopyCalls;put<uint32_t>(self+0x118C,0);}
  }else{assert(address==0x578080);return R(floorHeight);}
 }
 else assert(false&&"unexpected constructor in placement test");
 if constexpr(!std::is_void<R>::value)return R{};
}
struct NativePlacementSubmission;
static bool validateNativePlacement(const intent::EventOrder&,const NativePlacementSubmission*);
static bool nativePlacementPacket(const Event&,const intent::EventOrder&);
// This fixture exercises Place, never MO Waypoints. The production MO guard
// accepts every non-Waypoints order unless a MO submission is in progress.
static bool validateMoSubmission(const intent::EventOrder&o){return o.domain==vanilla::Domain::Place;}
#define memcpy queueCopy
#include "native_placement_entry_extract.h"
#undef memcpy
static int notifications=0;static void notifyHost(){++notifications;}
static HWND fixtureForeground(){return (HWND)0x1234;}
static DWORD fixtureWindowPid(HWND,DWORD*out){*out=GetCurrentProcessId();return 1;}
static BOOL fixtureRect(HWND,RECT*out){*out={0,0,800,600};return TRUE;}
static BOOL fixtureCursor(POINT*out){*out={100,100};return TRUE;}
static BOOL fixtureClient(HWND,POINT*){return TRUE;}
#define GetForegroundWindow fixtureForeground
#define GetWindowThreadProcessId fixtureWindowPid
#define GetClientRect fixtureRect
#define GetCursorPos fixtureCursor
#define ScreenToClient fixtureClient
#include "../source/native_placement_bridge.h"
#undef GetForegroundWindow
#undef GetWindowThreadProcessId
#undef GetClientRect
#undef GetCursorPos
#undef ScreenToClient
constexpr uint32_t House=0x20010000,BuildingType=0x20020000,Factory=0x20030000,Product=0x20040000,Tactical=0x20050000,Cell=0x20060000,TypeItems=0x20070000,Cells=0x20100000;
static void reset(){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);memset(sh,0,sizeof *sh);memset(&snap,0,sizeof snap);
 memset(typePointers,0,sizeof typePointers);lastHouse=House;match=7;clockNow=1000;ready=retired=clockOkay=true;failRead=0;
 outgoingBudget=10;queryMutation=ctorMutation=clockMutation={};queries=constructorsCalled=clockCalls=reads=0;constructedPacket=nullptr;
 fault=false;completionActiveCalls=completionCopyCalls=0;nativePlacementCompletionReset();
 floorHeight=0;placementView={};placementCapturedButtons=0;sh->heartbeat=clockNow;put<uint32_t>(0xB73550,0x1234);put<int>(0x8809A0,-1);put<int>(0x8192B8,2);put<int>(0x819310,3);
 sh->foreground=1;sh->flags=BuildAnywhere;sh->placementImplementation=2;sh->request=1;sh->command=PlaceCompletedBuilding;sh->requestMatch=match;sh->requestTick=clockNow;
 snap.status=snap.commandStatus=1;snap.frame=300;snap.match=match;snap.session=3;snap.typeCount=1;snap.types[0].kind=7;snap.types[0].index=0;
 memcpy(snap.types[0].id,"GAPOWR",7);typePointers[0]=BuildingType;
 put<uint32_t>(0xA8B230,0x20099000);put<uint32_t>(0xA83D4C,House);put<int>(0xA8ED84,snap.frame);put<int>(0xA8B238,3);
 put<uint32_t>(House+0x10,11);put<int>(House+0x30,3);put<uint32_t>(House+0x53BC,Factory);
 put<uint32_t>(Factory+0x10,21);put<uint32_t>(Factory+0x6C,House);put<int>(Factory+0x24,54);put<uint32_t>(Factory+0x58,Product);
 put<uint32_t>(Product,0x7E3EBC);put<uint32_t>(Product+0x10,31);put<uint32_t>(Product+0x21C,House);put<int>(Product+0x6C,800);
 put<uint8_t>(Product+0x90,1);put<uint8_t>(Product+0x81,1);put<uint32_t>(Product+0x520,BuildingType);put<uint32_t>(BuildingType+0x10,41);
 Vec types{0,TypeItems,1,0,1,0};memcpy(memory(0xA83C68,sizeof types),&types,sizeof types);put<uint32_t>(TypeItems,BuildingType);
 put<uint32_t>(0x887324,Tactical);put<int>(0x8A00A4,800);put<int>(0x8A00A8,600);put<int>(0xB0CE30,640);put<int>(0xB0CE34,480);
 int bounds[]={100,100,0,0,100,100};memcpy(memory(0x87F7E8+0xF4,sizeof bounds),bounds,sizeof bounds);
 put<uint32_t>(0x87F924,Cells);put<int>(0x87F928,512*512);put<uint32_t>(Cells+(70*512+70)*4,Cell);
 put<int16_t>(Cell+0x24,70);put<int16_t>(Cell+0x26,70);
 put<uint32_t>(Cells+(69*512+70)*4,Cell+0x1000);put<int16_t>(Cell+0x1000+0x24,70);put<int16_t>(Cell+0x1000+0x26,69);
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

 // Interactive mode chooses the native tab, previews without constructing,
 // and consumes exactly its own click pair after the arm receipt is retired.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2&&placementView.active&&queued()==0&&constructorsCalled==0);++checks;
 sh->ack=sh->request;nativePlacementTick(clockNow);assert(placementView.status==1&&placementView.width==2&&placementView.height==3&&queued()==0);++checks;
 auto mouse=[](UINT message,int x=100,int y=100){MSG m{};m.hwnd=(HWND)0x1234;m.message=message;m.lParam=MAKELPARAM(x,y);return m;};
 auto down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down)&&down.message==WM_NULL&&queued()==0);++checks;
 auto up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==1&&!placementView.active);++checks;
 auto again=mouse(WM_LBUTTONUP);assert(!nativePlacementMessage(&again)&&queued()==1);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);auto right=mouse(WM_RBUTTONDOWN);assert(nativePlacementMessage(&right)&&!placementView.active&&queued()==0);right=mouse(WM_RBUTTONUP);assert(nativePlacementMessage(&right)&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);MSG esc{};esc.message=WM_KEYDOWN;esc.wParam=VK_ESCAPE;assert(nativePlacementMessage(&esc)&&!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN,700,100);assert(!nativePlacementMessage(&down)&&!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;put<uint32_t>(0x87F7E8+0x11A4,Product);assert(nativePlacementCommand(House,clockNow)<0&&!placementView.active);down=mouse(WM_LBUTTONDOWN);assert(!nativePlacementMessage(&down)&&queued()==0);++checks;
 reset();sh->arg2=1;put<int>(0x8809A0,0);assert(nativePlacementCommand(House,clockNow)<0&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));put<uint32_t>(Product+0x10,99);up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));clockMutation=[](){put<uint32_t>(Product+0x10,99);};up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);put<int>(0x87F7E8+0x539C,1);nativePlacementTick(clockNow);assert(!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);sh->foreground=0;nativePlacementTick(clockNow);assert(!placementView.active&&queued()==0);++checks;
 // Runtime's message filter may run in the next simulation frame before
 // the next Host-ready tick publishes snap.frame. That read-only observation
 // must not permanently cancel an otherwise identical completed product.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);
 for(int frame=301;frame<=316;++frame){put<int>(0xA8ED84,frame);auto motion=mouse(WM_MOUSEMOVE);assert(!nativePlacementMessage(&motion)&&placementView.active&&queued()==0);nativePlacementTick(clockNow);assert(placementView.active);}
 assert(snap.frame==300&&!simulationReady()&&simulationReady(true));++checks;
 // Click capture can survive that read-only interval, but neither its up
 // nor the native event gateway may submit until the exact frame is sampled.
 down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down)&&queued()==0);up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0&&placementView.active);++checks;
 snap.frame=get<int>(0xA8ED84);nativePlacementTick(clockNow);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==1);++checks;
 // This permission admits forward frames only; it does not accept rewind,
 // changed session, changed House, or a replaced completed product.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);put<int>(0xA8ED84,299);auto motion=mouse(WM_MOUSEMOVE);assert(!nativePlacementMessage(&motion)&&!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);put<int>(0xA8ED84,301);put<int>(0xA8B238,4);motion=mouse(WM_MOUSEMOVE);assert(!nativePlacementMessage(&motion)&&!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);put<int>(0xA8ED84,301);put<uint32_t>(0xA83D4C,House+4);motion=mouse(WM_MOUSEMOVE);assert(!nativePlacementMessage(&motion)&&!placementView.active&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);put<int>(0xA8ED84,301);put<uint32_t>(Product+0x10,99);motion=mouse(WM_MOUSEMOVE);assert(!nativePlacementMessage(&motion)&&!placementView.active&&queued()==0);++checks;
 // Re-arming creates a new gesture; an old held button's release cannot
 // submit it without a new matching down edge in this preview generation.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));assert(nativePlacementCommand(House,clockNow)==2&&!placementView.active);assert(nativePlacementCommand(House,clockNow)==2&&placementView.active);up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0&&placementView.active);++checks;
 // If focus loss cancelled this preview and its up happened elsewhere, a
 // later native click must retain BOTH of its own down/up edges.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));sh->foreground=0;nativePlacementTick(clockNow);sh->foreground=1;down=mouse(WM_LBUTTONDOWN);assert(!nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);assert(!nativePlacementMessage(&up)&&queued()==0);++checks;
 // The matching old up remains consumed when no newer down supersedes it.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));sh->foreground=0;nativePlacementTick(clockNow);sh->foreground=1;up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 // Thread hooks can observe another same-process window; it never gets the
 // main game window's placement authority, including a captured up edge.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);down.hwnd=(HWND)0x5678;assert(!nativePlacementMessage(&down)&&queued()==0);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);up.hwnd=(HWND)0x5678;assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 reset();sh->arg2=1;put<int>(0x87F7E8+0x539C,2);assert(nativePlacementCommand(House,clockNow)<0&&queued()==0);++checks;
 reset();sh->arg2=1;
 constexpr uint32_t Factory2=Factory+0x8000,Product2=Product+0x8000,Type2=BuildingType+0x8000;
 memcpy(memory(Factory2,0x80),memory(Factory,0x80),0x80);memcpy(memory(Product2,0x600),memory(Product,0x600),0x600);memcpy(memory(Type2,0x1000),memory(BuildingType,0x1000),0x1000);
 put<uint32_t>(House+0x53CC,Factory2);put<uint32_t>(Factory2+0x10,22);put<uint32_t>(Factory2+0x58,Product2);put<uint32_t>(Product2+0x10,32);put<uint32_t>(Product2+0x520,Type2);put<uint32_t>(Type2+0x10,42);put<int>(Type2+0xE08,5);
 snap.typeCount=2;snap.types[1]=snap.types[0];snap.types[1].index=1;typePointers[1]=Type2;put<int>(0xA83C68+16,2);put<int>(0xA83C68+8,2);put<uint32_t>(TypeItems+4,Type2);
 put<int>(0x87F7E8+0x539C,1);assert(nativePlacementCommand(House,clockNow)==2&&placementView.offer.tab==1&&placementView.offer.product==Product2);++checks;
 nativePlacementReset();put<int>(0x87F7E8+0x539C,0);assert(nativePlacementCommand(House,clockNow)==2&&placementView.offer.tab==0&&placementView.offer.product==Product);++checks;

 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);clockNow+=300;nativePlacementTick(clockNow);assert(placementView.active&&queued()==0);
 down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 sh->heartbeat=clockNow;down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==1);++checks;
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);clockNow+=1001;nativePlacementTick(clockNow);assert(!placementView.active&&queued()==0);++checks;

 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);floorHeight=104;
 put<int>(0x886FA0,168);put<int>(0x886FA4,32);put<int>(0xB0CE30,600);put<int>(0xB0CE34,568);
 nativeplacement::Request aligned{};NativePlacementViewport alignedView{};
 assert(nativePlacementPoint((HWND)0x1234,{200,100},aligned,alignedView)&&aligned.cellX==70&&aligned.cellY==69);
 assert(placementView.x==70*256&&placementView.y==69*256&&placementView.z==104&&placementView.viewportX==168&&placementView.viewportY==32);
 assert(placementView.zScale==external::Projection{}.zScale);++checks;
 // A playable-area rejection is a red position, never a modal cancellation.
 reset();sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);
 put<int>(0x87F7E8+0x100,50);put<int>(0x87F7E8+0x108,50);nativePlacementTick(clockNow);
 assert(placementView.active&&placementView.status==2);down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));
 up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0&&placementView.active);++checks;
 put<int>(0x87F7E8+0x100,0);put<int>(0x87F7E8+0x108,100);nativePlacementTick(clockNow);
 assert(placementView.active&&placementView.status==1);++checks;
 // Cell read failures keep this gesture; returning to valid ground recovers.
 failRead=Cell+0x1000+0x24;nativePlacementTick(clockNow);assert(placementView.active&&placementView.status==2);
 down=mouse(WM_LBUTTONDOWN);assert(nativePlacementMessage(&down));up=mouse(WM_LBUTTONUP);assert(nativePlacementMessage(&up)&&queued()==0);++checks;
 failRead=0;nativePlacementTick(clockNow);assert(placementView.active&&placementView.status==1);++checks;
 // Loading another scenario before the ordinary sample cannot reuse this modal.
 put<uint32_t>(0xA8B230,0x200A9000);nativePlacementTick(clockNow);assert(!placementView.active&&queued()==0);++checks;
 // Both actual admission routes register a receipt only after the real
 // submit/enqueue succeeds. Modal reset does not retire that observation.
 auto queuePreview=[&](){
  sh->arg2=1;assert(nativePlacementCommand(House,clockNow)==2);
  auto press=mouse(WM_LBUTTONDOWN),release=mouse(WM_LBUTTONUP);
  assert(nativePlacementMessage(&press)&&nativePlacementMessage(&release));
  assert(!placementView.active&&queued()==1&&nativePlacementCompletionPending());
 };
 auto nativeCursor=[&](uint32_t offset,uint32_t product=Product){
  put<uint32_t>(0x87F7E8+offset,product);put<uint32_t>(0x87F7E8+offset+4,BuildingType);put<int>(0x87F7E8+offset+8,3);
 };
 auto onMap=[&](){put<uint8_t>(Product+0x74,1);put<uint8_t>(Product+0x81,0);};
 for(int route=0;route<2;++route){
  reset();if(route)queuePreview();else assert(nativePlacementCommand(House,clockNow)==1&&nativePlacementCompletionPending());
  assert(queued()==1&&!completionActiveCalls&&!completionCopyCalls);++checks;
  nativeCursor(0x11A4);nativeCursor(0x1190);nativePlacementReset();
  nativePlacementTick(clockNow);assert(nativePlacementCompletionPending()&&get<uint32_t>(0x87F7E8+0x11A4)==Product&&!completionActiveCalls&&!completionCopyCalls);++checks;
  put<uint32_t>(Factory+0x58,0);put<int>(Factory+0x24,0);nativePlacementTick(clockNow);
  assert(nativePlacementCompletionPending()&&queued()==1&&!completionActiveCalls&&!completionCopyCalls);++checks;
  // The observer keeps working after the modal closes and focus changes.
  sh->foreground=0;clockNow+=1100;onMap();nativePlacementTick(clockNow);
  assert(!nativePlacementCompletionPending()&&queued()==1&&completionActiveCalls==1&&completionCopyCalls==1);
  for(auto offset:{0x11A4,0x1190})assert(!get<uint32_t>(0x87F7E8+offset)&&!get<uint32_t>(0x87F7E8+offset+4)&&get<int>(0x87F7E8+offset+8)==-1);++checks;
  nativePlacementTick(clockNow);assert(queued()==1&&completionActiveCalls==1&&completionCopyCalls==1);++checks;
 }
 reset();queuePreview();nativeCursor(0x11A4,Product+0x100);nativeCursor(0x1190);onMap();nativePlacementTick(clockNow);
 assert(get<uint32_t>(0x87F7E8+0x11A4)==Product+0x100&&!get<uint32_t>(0x87F7E8+0x1190)&&!completionActiveCalls&&completionCopyCalls==1&&queued()==1);++checks;
 reset();queuePreview();nativeCursor(0x11A4);onMap();put<uint32_t>(Product+0x10,32);nativePlacementTick(clockNow);
 assert(!nativePlacementCompletionPending()&&!completionActiveCalls&&get<uint32_t>(0x87F7E8+0x11A4)==Product&&queued()==1);++checks;
 reset();queuePreview();nativeCursor(0x11A4);onMap();failRead=Product+0x74;nativePlacementTick(clockNow);
 assert(nativePlacementCompletionPending()&&!completionActiveCalls&&queued()==1);++checks;
 failRead=0;nativePlacementTick(clockNow);assert(!nativePlacementCompletionPending()&&completionActiveCalls==1&&queued()==1);++checks;
 reset();queuePreview();nativeCursor(0x11A4);onMap();put<uint8_t>(0x87F7E8+0x11B0,1);nativePlacementTick(clockNow);
 assert(!nativePlacementCompletionPending()&&!completionActiveCalls&&get<uint32_t>(0x87F7E8+0x11A4)==Product&&queued()==1);++checks;
 reset();outgoingBudget=0;assert(nativePlacementCommand(House,clockNow)<0&&!nativePlacementCompletionPending()&&!queued());++checks;
 reset();queuePreview();nativeCursor(0x11A4);onMap();put<int>(0xA8ED84,299);nativePlacementTick(clockNow);
 assert(!nativePlacementCompletionPending()&&!completionActiveCalls&&queued()==1);++checks;
 printf("native placement production admission/queue PASS checks=%d\n",checks);
 delete sh;
}

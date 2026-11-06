// Actual production pointerLocation and query/projection wrappers. Native
// coordinate and ground-height functions are explicit controlled boundaries.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <type_traits>
#include "../source/protocol.h"
#include "../source/object_index.h"
#ifdef NDEBUG
#error Pointer projection fixture requires active assertions (-UNDEBUG).
#endif
#undef assert
#define assert(v) ((v)?(void)0:(std::fprintf(stderr,"assert %s at %d\n",#v,__LINE__),std::_Exit(3)))
using namespace assist;
static Shared*sh;static ObjectIndex worldObjects;
static uint8_t globals[0x500000]{},objects[0x100000]{};
constexpr uint32_t Tactical=0x20010000,Cell=0x20020000,Target=0x20030000;
static uint8_t queryCode[8]={0x8B,0x44,0x24,0x10,0x83,0xEC,0x18,0x8B};
static bool lease=true,ownerOkay=false,mapCellPresent=true,radarHit=false;static int queryCount=0,floorCount=0,queryX=-1,queryY=-1;
// Explicit native-dependency boundary. The shared cache/Ares policy is tested
// by native_entry_runtime; this fixture verifies the pointer consumer's order.
static bool groundReady=true;static unsigned dependencyChecks=0;
static bool nativeDependencyReady(uint32_t address,bool=false){assert(address==0x578080);++dependencyChecks;return groundReady;}
static bool nativeSidebarUIHit(int,int){return radarHit;}
static uint32_t resolvedObject=0,failRead=0;static std::function<void()>queryMutation,floorMutation;static std::function<void(uint32_t)>readMutation;
static uint32_t pointerCell=0,pointerTarget=0,pointerTargetID=0;static int pointerGroundZ=0;
struct NativeViewRect{int x=0,y=0,w=0,h=0;};struct NativeViewPoint{int x=0,y=0;};
static void*memory(uint32_t at,size_t n){if(at>=0x700000&&uint64_t(at)+n<=0xC00000)return globals+at-0x700000;if(at>=0x20000000&&uint64_t(at)+n<=0x20100000)return objects+at-0x20000000;return nullptr;}
template<class T>static void put(uint32_t at,T v){void*p=memory(at,sizeof v);assert(p);memcpy(p,&v,sizeof v);}
template<class T>static T get(uint32_t at){T v{};void*p=memory(at,sizeof v);assert(p);memcpy(&v,p,sizeof v);return v;}
static bool read(uint32_t at,void*out,size_t n){if(readMutation)readMutation(at);if(failRead&&at<=failRead&&uint64_t(at)+n>failRead)return false;if(at==0x692300&&n==8){memcpy(out,queryCode,8);return true;}void*p=memory(at,n);if(!p)return false;memcpy(out,p,n);return true;}
static uint32_t readMapCell(int x,int y){return mapCellPresent&&x==70&&y==71?Cell:0;}
static int kind(uint32_t p){return p==Target?3:-1;}
namespace gameentry {enum class Client{Assist};static bool active(Client){return lease;}}
#include "../source/native_viewport_query.h"
struct QueryCell{short x,y;};struct QueryCoord{int x,y,z;};
extern "C" int __cdecl GuardedNativeViewQuery(void*point,void*cell,void*coord,void*object,void*,void*){
 const unsigned token=BeginNativeViewQuery();if(!token)return 0;
 assert(nativeViewQueryDepth==token&&token==1);++queryCount;auto&p=*(NativeViewPoint*)point;queryX=p.x;queryY=p.y;
 *(QueryCell*)cell={70,71};*(QueryCoord*)coord={70*256,71*256,39};*(uint32_t*)object=resolvedObject;
 if(queryMutation)queryMutation();EndNativeViewQuery(token);return 1;
}
template<class R,class...Args>static R call(uint32_t address,uint32_t self,Args...){
 static_assert(std::is_same<R,int>::value&&sizeof...(Args)==1,"only original ground-height read boundary");assert(address==0x578080&&self==0x87F7E8);++floorCount;if(floorMutation)floorMutation();return 96;
}
#include "pointer_location_extract.h"
static bool owner(){return ownerOkay;}
static void publish(bool active,int left=168,int top=16,int width=632,int height=568,int logicalX=0,int logicalY=0,int logicalWidth=948,int logicalHeight=852){
 sh->viewportSerial+=2;sh->viewportActive=active;sh->viewportGeneration=7;
 int a[]={left,top,width,height},r[]={logicalX,logicalY,logicalWidth,logicalHeight};memcpy(sh->viewportPhysical,a,sizeof a);memcpy(sh->viewportLogical,r,sizeof r);
 NativeViewRect rect=active?NativeViewRect{logicalX,logicalY,logicalWidth,logicalHeight}:NativeViewRect{left,top,width,height};memcpy(memory(0x886FA0,sizeof rect),&rect,sizeof rect);
 put<int>(0xB0CE30,rect.w);put<int>(0xB0CE34,rect.h);
}
static pointerinput::Intent reset(bool active=true){
 memset(globals,0,sizeof globals);memset(objects,0,sizeof objects);*sh={};worldObjects.clear();lease=true;ownerOkay=false;mapCellPresent=true;queryCode[0]=0x8B;queryCount=floorCount=0;queryX=queryY=-1;queryMutation=floorMutation={};readMutation={};resolvedObject=failRead=0;nativeViewQueryDepth=0;nativeViewQueryBlocked=false;nativeViewQueryOwner=owner;
 pointerGroundZ=-999;pointerCell=123;pointerTarget=Target;pointerTargetID=99;radarHit=false;groundReady=true;dependencyChecks=0;
 put<uint32_t>(0x887324,Tactical);put<int>(Tactical+0xB0,-900);put<int>(Tactical+0xB4,400);put<int>(0x8A00A4,800);put<int>(0x8A00A8,600);put<uint32_t>(Target+0x10,77);worldObjects.add(Target,77);publish(active);
 pointerinput::Intent intent{};intent.width=800;intent.height=600;intent.cameraX=-900;intent.cameraY=400;intent.x=484;intent.y=300;intent.operation=pointerinput::Enhanced;return intent;
}
int main(){sh=new Shared{};unsigned checks=0;auto check=[&](bool v){if(!v)std::fprintf(stderr,"check=%u query=%d xy=%d,%d depth=%u\n",checks+1,queryCount,queryX,queryY,nativeViewQueryDepth);assert(v);++checks;};
 auto rejected=[&](const pointerinput::Intent&i){check(!pointerLocation(i));check(pointerGroundZ==-999&&pointerCell==123&&pointerTarget==Target&&pointerTargetID==99&&nativeViewQueryDepth==0);};
 auto i=reset();check(pointerLocation(i));check(queryCount==1&&queryX==474&&queryY==426&&nativeViewQueryDepth==0);check(pointerCell==71070&&pointerTarget==0&&pointerGroundZ==39);
 i=reset();radarHit=true;rejected(i);check(queryCount==0);
 i=reset();publish(true,168,16,632,568,10,20,948,852);check(pointerLocation(i));check(queryX==474&&queryY==426); // subtract R origin exactly once
 i=reset(false);i.x=200;i.y=100;check(pointerLocation(i));check(queryX==32&&queryY==84); // native origin subtraction, no scale
 i=reset(false);publish(false,0,0,632,568);i.x=100;i.y=80;check(pointerLocation(i));check(queryX==100&&queryY==80);
 for(auto point:{POINT{100,100},POINT{800,100},POINT{484,584},POINT{484,600},POINT{-1,300},POINT{484,15}}){i=reset();i.x=point.x;i.y=point.y;rejected(i);check(queryCount==0);}
 i=reset();i.x=168;i.y=16;check(pointerLocation(i)&&queryX==0&&queryY==0);
 i=reset();queryMutation=[](){++sh->viewportGeneration;};rejected(i);check(queryCount==1);
 i=reset();queryMutation=[](){sh->viewportSerial|=1;};rejected(i);check(queryCount==1);
 i=reset();queryMutation=[](){sh->viewportActive=0;};rejected(i);
 i=reset();queryMutation=[](){put<int>(Tactical+0xB0,-899);};rejected(i);
 i=reset();queryMutation=[](){put<uint32_t>(0x887324,Tactical+0x1000);};rejected(i);
 i=reset();queryMutation=[](){put<int>(0x8A00A4,801);};rejected(i);
 i=reset();queryMutation=[](){put<int>(0x8A00A8,601);};rejected(i);
 i=reset();queryMutation=[](){put<int>(0x886FA0+8,947);};rejected(i);
 i=reset();i.operation=pointerinput::AirWaypoint;check(pointerLocation(i));check(queryCount==1&&floorCount==1&&pointerGroundZ==96);
 i=reset();i.operation=pointerinput::AirWaypoint;groundReady=false;rejected(i);check(queryCount==0&&floorCount==0&&dependencyChecks==1);
 groundReady=true;check(pointerLocation(i));check(queryCount==1&&floorCount==1&&pointerGroundZ==96&&pointerCell==71070); // same request recovers
 i=reset();groundReady=false;check(pointerLocation(i));check(queryCount==1&&floorCount==0&&dependencyChecks==0&&pointerGroundZ==39); // Enhanced does not use ground height
 i=reset();i.operation=pointerinput::AirWaypoint;floorMutation=[](){++sh->viewportGeneration;};rejected(i);check(floorCount==1);
 i=reset();i.operation=pointerinput::AirWaypoint;floorMutation=[](){put<int>(Tactical+0xB4,401);};rejected(i);check(floorCount==1);
 i=reset();i.operation=pointerinput::AirWaypoint;floorMutation=[](){put<int>(0x8A00A4,801);};rejected(i);
 i=reset();i.operation=pointerinput::AirWaypoint;floorMutation=[](){sh->viewZoomBusy=1;};rejected(i);
 i=reset();sh->viewportSerial=1;rejected(i);check(queryCount==0);
 i=reset();sh->viewZoomBusy=1;rejected(i);check(queryCount==0);
 i=reset();nativeViewQueryBlocked=true;rejected(i);check(queryCount==0);
 i=reset();lease=false;rejected(i);check(queryCount==0);
 i=reset();queryCode[0]=0xE9;rejected(i);check(queryCount==0);
 i=reset();queryCode[0]=0xE9;ownerOkay=true;check(pointerLocation(i)&&queryCount==1&&queryX==474&&queryY==426);
 i=reset();bool changed=false;readMutation=[&](uint32_t at){if(at==0x886FA0&&!changed){changed=true;++sh->viewportGeneration;}};rejected(i);check(queryCount==0);
 i=reset();mapCellPresent=false;rejected(i);i=reset();resolvedObject=Target+0x1000;rejected(i);
 i=reset();resolvedObject=Target;check(pointerLocation(i)&&pointerTarget==Target&&pointerTargetID==77);
 i=reset();failRead=0x886FA0;rejected(i);check(queryCount==0);
 std::printf("{\"schema\":\"ra2.pointer-location-projection.v1\",\"checks\":%u,\"assertions_active\":true,\"passed\":true,\"native_dependency_boundary\":\"synthetic\",\"game_process_tested\":false,\"windows_hooks_tested\":false}\n",checks);delete sh;
}

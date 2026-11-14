// Execute the production native-PiP adapter against mapped synthetic engine
// data and surface/native-call doubles. Windows additionally uses the actual
// production SEH wrapper with native access violations. No game is loaded.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <map>
#include <vector>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
using DWORD=uint32_t;using LONG=int32_t;using LONGLONG=int64_t;using HWND=void*;using WPARAM=uintptr_t;using LPARAM=intptr_t;
#define __cdecl
struct RECT{int left=0,top=0,right=0,bottom=0;};struct LARGE_INTEGER{long long QuadPart;};struct MSG{HWND hwnd;unsigned message;WPARAM wParam;LPARAM lParam;};
constexpr unsigned WM_NULL=0,WM_MOUSEMOVE=0x200,WM_LBUTTONDOWN=0x201,WM_LBUTTONUP=0x202,WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDOWN=0x204,WM_RBUTTONUP=0x205,WM_RBUTTONDBLCLK=0x206,VK_LBUTTON=1;
#define LOWORD(v) (uint16_t(uintptr_t(v)&65535))
#define HIWORD(v) (uint16_t((uintptr_t(v)>>16)&65535))
static long InterlockedExchange(volatile long*p,long n){auto old=*p;*p=n;return old;}
#endif
static uint32_t clockTick=100;static long long counter=0;static bool leftHeld=false;
static uint32_t fixtureTick(){return clockTick;}
static short fixtureAsync(int){return leftHeld?short(0x8000):0;}
static int fixtureCounter(LARGE_INTEGER*p){p->QuadPart=counter;return 1;}
static int fixtureFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;return 1;}
static int fixtureClient(HWND,RECT*r){*r={0,0,1920,1080};return 1;}
#define GetTickCount fixtureTick
#define GetAsyncKeyState fixtureAsync
#define QueryPerformanceCounter fixtureCounter
#define QueryPerformanceFrequency fixtureFrequency
#define GetClientRect fixtureClient
#include "../source/small_map_camera.h"
#include "../source/native_pip_core.h"
using namespace assist;
// The PE loader owns the arena before CRT heaps/stacks can occupy engine
// addresses. Tests clear only the verified subrange, never image code/imports.
#ifdef _WIN32
alignas(4096) static volatile uint8_t engineArena[0xD00000];
#endif
constexpr uintptr_t Memory=0x800000;constexpr size_t MemorySize=0x800000;constexpr uint32_t Tactical=0xC00000,Object=0xC10000;
static bool read(uint32_t p,void*out,size_t n){if(p<Memory||uint64_t(p)+n>Memory+MemorySize)return false;memcpy(out,(void*)(uintptr_t)p,n);return true;}
template<class T>static T get(uint32_t p){T v{};if(!read(p,&v,sizeof v))throw std::runtime_error("invalid test read");return v;}
template<class T>static void put(uint32_t p,const T&v){assert(p>=Memory&&uint64_t(p)+sizeof v<=Memory+MemorySize);memcpy((void*)(uintptr_t)p,&v,sizeof v);}
struct Shared{uint32_t nativePipVisible=1,nativePipEnabled=1,readOnly=0,stop=0,foreground=1,nativePipPinRequest=0,nativePipPinAck=0,nativePipPinMatch=0,nativePipPinTick=0;int32_t nativePipStatus=0;uint32_t nativePipMicros=0,nativePipMaxMicros=0,nativePipFrames=0;} storage,*sh=&storage;
static bool fault=false,sidebarViewTracking=true,nativeViewInRender=false,nativeViewActive=false,nativeMapPipLease=false,nativeMapZoomLease=false,trackingIntact=true;
static unsigned sidebarViewRenderSerial=1,sidebarViewRenderDepth=0;static uint32_t match=3;static volatile long hostWorkPending=0;
struct NativeViewRect{int x=0,y=0,w=0,h=0;};static NativeViewRect nativeViewA{168,0,1112,688};
static bool sidebarViewRendering(){return sidebarViewRenderDepth!=0;}
static bool sidebarViewTrackingIntact(){return trackingIntact;}
static bool nativeViewExecutable(uint32_t){return true;}
static bool nativeMapAcquire(bool){nativeMapPipLease=true;return true;}
static bool nativeMapRelease(bool){nativeMapPipLease=false;return true;}
struct Surface{int w=576,h=384;std::vector<uint16_t> pixels;};static std::map<uintptr_t,Surface> surfaces;
static uintptr_t nextSurface=0xD00000;static unsigned locks=0,unlocks=0;static bool unlockFault=false;
static void*surface(int w,int h){const auto p=nextSurface;nextSurface+=0x1000;surfaces[p]={w,h,std::vector<uint16_t>(size_t(w)*h,0xACE0)};return (void*)p;}
static void*nativeViewNewSurface(int w,int h){return surface(w,h);}
static void nativeViewFreeSurface(void*p){surfaces.erase(uintptr_t(p));}
static void*nativeViewNewBuffer(bool,NativeViewRect){return (void*)(nextSurface+=0x1000);}
static void nativeViewFreeBuffer(bool,void*){}
static bool nativeViewBufferValid(void*,int,int){return true;}
static int nativeViewSurfaceValue(void*p,unsigned member){auto it=surfaces.find(uintptr_t(p));if(it==surfaces.end())return 0;return member==0x7C?it->second.w:member==0x80?it->second.h:member==0x74?it->second.w*2:member==0x70?2:0;}
static unsigned char*nativeViewSurfaceLock(void*p){auto it=surfaces.find(uintptr_t(p));if(it==surfaces.end())return nullptr;++locks;return (unsigned char*)it->second.pixels.data();}
static void nativeViewSurfaceUnlock(void*){++unlocks;
#ifdef _WIN32
 if(unlockFault){unlockFault=false;RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);}
#endif
}
static bool live(uint32_t p){return p==Object&&get<int>(p+0x6C)>0&&get<bool>(p+0x74)&&!get<bool>(p+0x81)&&get<bool>(p+0x90);}
static int kind(uint32_t p){return p==Object?1:-1;}
struct Vec{int count;};static std::vector<uint32_t> selection;
static Vec vec(uint32_t){return {int(selection.size())};}static uint32_t item(Vec,int n){return selection.at(n);}
static smallcamera::Point navigation{};static unsigned navigations=0;
static bool radarNavigationMove(smallcamera::Point p){navigation=p;++navigations;return true;}
static std::vector<int> phases;static int throwPhase=-1;static bool nestedDone=false;static void checkNested();
template<class R,class...Args>static R call(uint32_t address,uint32_t self,void*target,int flag,int phase,Args...){
 assert(address==0x6D3D10&&self==Tactical&&flag==1&&sidebarViewRenderDepth==1);if(!nestedDone)checkNested();phases.push_back(phase);counter+=300;
 assert(get<nativepip::Rect>(nativepip::Bounds).w==nativepip::RasterWidth);
 assert(get<uint32_t>(nativepip::SurfaceSlots[0])==uint32_t(uintptr_t(target)));
 put(Tactical+0xDB0,int32_t(1));put(Tactical+0xE0,int32_t(1));put(nativepip::Selectable,uint32_t(999));put(Tactical+0xE4,uint32_t(888));put(0xD40000,uint32_t(777));
 auto&s=surfaces.at(uintptr_t(target));std::fill(s.pixels.begin(),s.pixels.end(),uint16_t(0x1230+phase));
 if(phase==throwPhase){
#ifdef _WIN32
  RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
#else
  throw std::runtime_error("controlled native renderer failure");
#endif
 }
 if constexpr(!std::is_void_v<R>)return R{};
}
extern "C" int __cdecl GuardedNativePipAction(unsigned,unsigned);
#include "../source/native_pip_bridge.h"
#ifdef _WIN32
static unsigned captureDepth=0,filters=0;
extern "C" unsigned BeginAssistCapture(){return ++captureDepth;}
extern "C" void EndAssistCapture(unsigned token){assert(token==captureDepth&&captureDepth);--captureDepth;}
extern "C" int AssistExceptionFilter(void*,unsigned boundary){assert(boundary==33);++filters;return EXCEPTION_EXECUTE_HANDLER;}
// The production MSVC SEH header is compiled in a separate object.
#else
extern "C" int __cdecl GuardedNativePipAction(unsigned action,unsigned argument){static unsigned depth=0;if(depth)return 0;++depth;int result=0;try{result=AssistNativePipAction(action,argument);}catch(...){AssistNativePipFailed();}if(!AssistNativePipRestore())AssistNativePipFailed();AssistNativePipUnlock();AssistNativePipUnlock();AssistNativePipEnd();--depth;return result;}
#endif
static void checkNested(){nestedDone=true;assert(nativePipTransaction.borrowed());assert(!GuardedNativePipAction(0,0));assert(nativePipTransaction.borrowed());}
static void setup(){
 memset((void*)Memory,0,MemorySize);storage={};clockTick=100;counter=0;phases.clear();throwPhase=-1;nestedDone=false;
 put(0xA8B230,uint32_t(0xC20000));put(0x887324,Tactical);put(0xA8E9A0,uint8_t(1));put(0xA8ED84,int32_t(1));put(0xB73550,uint32_t(0x1234));
 int32_t bounds[6]={100,100,0,0,100,100};memcpy((void*)(0x87F7E8+0xF4),bounds,sizeof bounds);
 put(0xB0CD48,double(0.0866025403784));put(nativepip::Bounds,nativepip::Rect{168,0,1112,688});put(nativepip::Cache,nativepip::Rect{168,0,1112,688});
 put(Tactical+0xDB0,int32_t(2));put(Tactical+0xE0,int32_t(2));put(nativepip::DirtyCount,int32_t(1));put(0xB0CE7C,uint32_t(0xD40000));put(0xB0CE80,int32_t(5));
 for(unsigned i=0;i<10;++i)put(Tactical+nativepip::CameraOffsets[i],int32_t(i&1?2500:1000));
 const uint8_t zoom[2]={5,7};memcpy((void*)(Tactical+0xD7C),zoom,2);
 void*comp=surface(1280,720),*map=surface(1280,720);uint32_t slots[]={uint32_t(uintptr_t(comp)),uint32_t(uintptr_t(map)),uint32_t(uintptr_t(comp)),0xD70000,0xD80000};for(unsigned i=0;i<5;++i)put(nativepip::SurfaceSlots[i],slots[i]);
 for(unsigned i=0;i<24;++i)((uint8_t*)nativepip::Selectable)[i]=uint8_t(i);for(unsigned i=0;i<20;++i)((uint8_t*)0xD40000)[i]=uint8_t(i+1);
 put(Object+0x10,uint32_t(42));put(Object+0x6C,int32_t(100));put(Object+0x74,uint8_t(1));put(Object+0x90,uint8_t(1));put(Object+0x9C,int32_t(20000));put(Object+0xA0,int32_t(16000));put(Object+0xA4,int32_t(0));selection={Object};
 sidebarViewRenderDepth=0;assert(GuardedNativePipAction(0,0)==1);
}
static std::vector<uint8_t> bytes(){return std::vector<uint8_t>((uint8_t*)Memory,(uint8_t*)(Memory+MemorySize));}
static MSG mouse(unsigned message,int x=1500,int y=30){return MSG{(HWND)uintptr_t(0x1234),message,0,LPARAM(uint32_t(uint16_t(x))|(uint32_t(uint16_t(y))<<16))};}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
#ifdef _WIN32
 AddVectoredExceptionHandler(1,[](EXCEPTION_POINTERS*p)->LONG{if(p->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION)std::printf("fixture AV at %p access=%lu target=%p\n",p->ExceptionRecord->ExceptionAddress,p->ExceptionRecord->NumberParameters?p->ExceptionRecord->ExceptionInformation[0]:99,p->ExceptionRecord->NumberParameters>1?(void*)p->ExceptionRecord->ExceptionInformation[1]:nullptr);return EXCEPTION_CONTINUE_SEARCH;});
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 assert(Memory>=uintptr_t(engineArena)&&Memory+MemorySize<=uintptr_t(engineArena)+sizeof engineArena);
 MEMORY_BASIC_INFORMATION owned{};assert(VirtualQuery((void*)Memory,&owned,sizeof owned)&&owned.Type==MEM_IMAGE&&owned.AllocationBase==GetModuleHandleW(nullptr));
#else
 assert(mmap((void*)Memory,MemorySize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)Memory);
#endif
 std::puts("stage initial setup");setup();std::puts("stage initial render");const auto before=bytes();sidebarViewRenderDepth=1;assert(GuardedNativePipAction(1,0)==1);assert((phases==std::vector<int>{0,1,2}));assert(bytes()==before);assert(sh->nativePipFrames==1&&sh->nativePipMicros==900&&locks==unlocks);
 auto comp=get<void*>(0x88731C);auto&image=surfaces.at(uintptr_t(comp));assert(image.pixels[8*1280+984]==0x1232);assert(GuardedNativePipAction(3,0)==1&&image.pixels[8*1280+984]==0xACE0);
 clockTick+=20;assert(GuardedNativePipAction(1,0)==1&&sh->nativePipFrames==1); // same logic frame, reuse pixels
 auto down=mouse(WM_LBUTTONDOWN);assert(nativePipMessage(&down)&&down.message==WM_NULL);auto up=mouse(WM_LBUTTONUP);assert(nativePipMessage(&up)&&nativePipJumpPending);sidebarViewRenderDepth=0;assert(GuardedNativePipAction(4,0)==1&&navigations==1);
 assert(GuardedNativePipAction(3,0)==1);put(0xA8EDA0,int32_t(1));sidebarViewRenderDepth=1;assert(!GuardedNativePipAction(1,0)&&!nativePipUnderlayPending());put(0xA8EDA0,int32_t(0));sidebarViewRenderDepth=0;
 assert(GuardedNativePipAction(2,0)==1&&!nativeMapPipLease);
 std::puts("stage follow/input/lifetime");setup();sh->nativePipPinRequest=1;sh->nativePipPinMatch=match;sh->nativePipPinTick=clockTick;sidebarViewRenderDepth=1;assert(GuardedNativePipAction(1,0)==1&&nativePipObject==Object);assert(GuardedNativePipAction(3,0));selection.clear();clockTick+=20;put(0xA8ED84,int32_t(2));assert(GuardedNativePipAction(1,0)&&nativePipObject==Object);assert(GuardedNativePipAction(3,0));
 sh->nativePipVisible=0;clockTick+=20;put(0xA8ED84,int32_t(3));assert(!GuardedNativePipAction(1,0)&&nativePipObject==Object&&!nativePipUnderlayPending());sh->nativePipVisible=1;assert(GuardedNativePipAction(1,0)&&nativePipObject==Object);assert(GuardedNativePipAction(3,0));
 const uint32_t priorPin=sh->nativePipPinAck;sh->nativePipPinRequest=priorPin+1;sh->nativePipPinMatch=match-1;sh->nativePipPinTick=clockTick;clockTick+=20;put(0xA8ED84,int32_t(4));assert(GuardedNativePipAction(1,0)&&nativePipObject==Object&&sh->nativePipPinAck==priorPin+1);assert(GuardedNativePipAction(3,0));
 // The consumed button pair must remain balanced when the overlay is hidden.
 assert(GuardedNativePipAction(1,0));auto hideDown=mouse(WM_RBUTTONDOWN);assert(nativePipMessage(&hideDown));sh->nativePipVisible=0;auto hideUp=mouse(WM_RBUTTONUP,0,1000);assert(nativePipMessage(&hideUp)&&nativePipMouseOwned==0);sh->nativePipVisible=1;assert(GuardedNativePipAction(3,0));
 // Reused actor address with a different UID stops following.
 put(Object+0x10,uint32_t(43));clockTick+=20;put(0xA8ED84,int32_t(5));assert(GuardedNativePipAction(1,0)&&nativePipObject==0);assert(GuardedNativePipAction(3,0));
 // Same world/surface addresses across a new match cannot replay old pixels.
 clockTick+=20;put(0xA8ED84,int32_t(6));assert(GuardedNativePipAction(1,0));auto mainSurface=get<void*>(0x88731C);auto&replacement=surfaces.at(uintptr_t(mainSurface));std::fill(replacement.pixels.begin(),replacement.pixels.end(),uint16_t(0x55AA));++match;
 assert(GuardedNativePipAction(3,0)&&replacement.pixels[8*1280+984]==0x55AA);clockTick+=20;assert(GuardedNativePipAction(1,0)&&nativePipEpoch==match&&nativePipObject==0);assert(GuardedNativePipAction(3,0));
 // A logic-frame rewind invalidates the cached camera even at identical pointers.
 put(Tactical+0xD64,int32_t(1500));put(0xA8ED84,int32_t(1));clockTick+=20;assert(GuardedNativePipAction(1,0)&&nativePipLastFrame==1&&nativePipCenterX==1500);assert(GuardedNativePipAction(3,0));

 std::puts("stage renderer fault");clockTick+=20;put(0xA8ED84,int32_t(5));const auto original=bytes();throwPhase=1;assert(!GuardedNativePipAction(1,0));assert(nativePipFailed&&!nativePipTransaction.borrowed()&&bytes()==original&&locks==unlocks);sidebarViewRenderDepth=0;assert(GuardedNativePipAction(2,0)==1);
#ifdef _WIN32
 std::puts("stage unlock fault");setup();sidebarViewRenderDepth=1;unlockFault=true;const auto beforeFilters=filters;SetLastError(0x1256);GuardedNativePipAction(1,0);assert(nativePipFailed&&locks==unlocks&&filters==beforeFilters+1&&captureDepth==0&&GetLastError()==0x1256);assert(GuardedNativePipAction(3,0));sidebarViewRenderDepth=0;assert(GuardedNativePipAction(2,0));
 std::puts("stage partial-entry memory fault");setup();const auto original2=bytes();DWORD old=0;assert(VirtualProtect((void*)Tactical,4096,PAGE_READONLY,&old));sidebarViewRenderDepth=1;assert(!GuardedNativePipAction(1,0)&&nativePipTransaction.borrowed());assert(VirtualProtect((void*)Tactical,4096,old,&old));assert(GuardedNativePipAction(3,0)&&!nativePipTransaction.borrowed()&&bytes()==original2);sidebarViewRenderDepth=0;assert(GuardedNativePipAction(2,0));
#endif
 std::puts("PASS production native PiP adapter: three phases, complete camera/dirty/selection restoration, fixed/follow camera, shared-frame reuse, underlay cleanup, scaled input ownership/navigation, menu hiding, shutdown and renderer faults; synthetic engine/surfaces; no live-game/FPS claim");
}

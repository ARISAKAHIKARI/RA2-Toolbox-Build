// Opt-in native performance adapters. No per-node breakpoint, allocation,
// logging, timing query or world scan; original search and rasterization stay.
#include "../../shared/source/single_player_lag_policy.h"
namespace lagfix {
static volatile uint8_t pathActive,mindActive;
static volatile uint32_t pathHits,mindHits;
static DWORD lastCheck;

static __declspec(naked) void pathEntry(){__asm {
 cmp byte ptr [pathActive],0
 je original
 cmp dword ptr ds:[0A8B238h],0
 je modeOK
 cmp dword ptr ds:[0A8B238h],5
 jne original
modeOK:
 // Generation wrap must retain the original full-table reset.
 cmp dword ptr [ecx+28h],-1
 je original
 sub esp,8
 push esi
 push edi
 mov esi,ecx
 mov eax,[esi+0Ch]
 mov dword ptr [eax+180000h],0
 mov eax,[esi+10h]
 mov dword ptr [eax+100000h],0
 mov edx,[esi+14h]
 mov ecx,[edx]
 test ecx,ecx
 js regularDone
 inc ecx
 mov edi,[edx+8]
 xor eax,eax
 cld
 rep stosd
regularDone:
 mov dword ptr [edx],0
 mov edx,[esi+68h]
 mov ecx,[edx]
 test ecx,ecx
 js hierDone
 inc ecx
 mov edi,[edx+8]
 xor eax,eax
 cld
 rep stosd
hierDone:
 mov dword ptr [edx],0
 mov eax,edx
 mov ecx,esi
 mov edx,[esi+28h]
 inc edx
 mov [esi+28h],edx
 inc dword ptr [pathHits]
 pop edi
 pop esi
 add esp,8
 ret
original:
 mov eax,[ecx+0Ch]
 sub esp,8
 push 42A5B6h
 ret
}}

struct Point {int32_t x,y;};
struct Coord {int32_t x,y,z;};
struct Rect {int32_t x,y,w,h;};
template<class T> static T raw(uint32_t at){return *reinterpret_cast<const T*>(uintptr_t(at));}
static Point project(const Coord&c){
 Point p;reinterpret_cast<bool(__thiscall*)(void*,const Coord*,Point*)>(0x6D2140)(
  reinterpret_cast<void*>(uintptr_t(raw<uint32_t>(0x887324))),&c,&p);
 p.y=int32_t(uint32_t(p.y)+raw<uint32_t>(0x886FA4));return p;
}
static int32_t interpolate(int32_t a,int32_t b,unsigned i){
 // Explicit x86 wrap before signed division: native negative coordinates
 // truncate toward zero, not arithmetic shift/floor.
 return int32_t(uint32_t(a)*(32-i)+uint32_t(b)*i)/32;
}
static uint32_t pixel(uint32_t rgb){
 const unsigned r=rgb&255,g=(rgb>>8)&255,b=(rgb>>16)&255;
 return ((r>>(raw<uint32_t>(0x8A0DD4)&31))<<(raw<uint32_t>(0x8A0DD0)&31))|
  ((g>>(raw<uint32_t>(0x8A0DE4)&31))<<(raw<uint32_t>(0x8A0DE0)&31))|
  ((b>>(raw<uint32_t>(0x8A0DDC)&31))<<(raw<uint32_t>(0x8A0DD8)&31));
}
static __declspec(noinline) void __cdecl mindLoop(const uint8_t*frame){
 // Called after native endpoint boxes and its ONE wall-clock sample. Keep
 // that clock, all 32 segments, their order, native clipping and native lines.
 const Coord from=*reinterpret_cast<const Coord*>(frame+0xA4);
 const Coord to=*reinterpret_cast<const Coord*>(frame+0xB0);
 const uint32_t rgb=*reinterpret_cast<const uint32_t*>(frame+0xBC);
 const uint32_t phase=*reinterpret_cast<const uint32_t*>(frame+0x30);
 Rect clip=*reinterpret_cast<const Rect*>(frame+0x70);
 Point previous=project(from);
 for(unsigned i=1;i<=32;++i){
  const int t=int(i)-16;
  Coord next={interpolate(from.x,to.x,i),interpolate(from.y,to.y,i),
   int32_t(uint32_t(interpolate(from.z,to.z,i))+uint32_t(512-2*t*t))};
  const Point projected=project(next);
  Point a=previous,b=projected;previous=projected; // clip mutates its endpoints
  if(!reinterpret_cast<bool(__fastcall*)(Point*,Point*,Rect*)>(0x7BC2B0)(&a,&b,&clip))continue;
  uint32_t color=rgb;const uint32_t pulse=(i*8-phase)&0x1F8;
  if(pulse&&pulse<=248){const uint32_t white=0xFFFFFF;
   reinterpret_cast<void(__thiscall*)(void*,int,const void*)>(0x6612C0)(&color,int(pulse),&white);
  }
  const uint32_t surface=raw<uint32_t>(0x88731C),table=raw<uint32_t>(surface);
  reinterpret_cast<bool(__thiscall*)(void*,const Point*,const Point*,int)>(raw<uint32_t>(table+0x30))(
   reinterpret_cast<void*>(uintptr_t(surface)),&a,&b,int(pixel(color)));
 }
 ++mindHits;
}
static __declspec(naked) void mindEntry(){__asm {
 cmp byte ptr [mindActive],0
 je original
 cmp dword ptr ds:[0A8B238h],0
 je accelerated
 cmp dword ptr ds:[0A8B238h],5
 jne original
accelerated:
 mov ebx,esp
 and esp,-16
 sub esp,12
 push ebx
 call mindLoop
 mov esp,ebx
 push 705256h
 ret
original:
 mov eax,[esp+0B0h]
 push 705091h
 ret
}}
static RiskSite sites[]={
 {singlelag::PathClear,6,{0x8B,0x41,0x0C,0x83,0xEC,0x08},{}},
 {singlelag::MindCurve,7,{0x8B,0x84,0x24,0xB0,0,0,0},{}}
};
static void prepare(){
 static bool ready=false;if(ready)return;ready=true;
 const uint32_t targets[]={uint32_t(uintptr_t(&pathEntry)),uint32_t(uintptr_t(&mindEntry))};
 for(unsigned i=0;i<2;++i){auto&s=sites[i];memset(s.after,0x90,s.size);s.after[0]=0xE9;
  const uint32_t displacement=targets[i]-s.address-5;memcpy(s.after+1,&displacement,4);}
}
static bool restore(){
 pathActive=mindActive=0;lastCheck=0;
 const bool p=restoreRiskSite(sites[0]),m=restoreRiskSite(sites[1]);
 if(sh){sh->lagPathStatus=p?0:-2;sh->lagMindStatus=m?0:-2;}
 return p&&m;
}
static void tick(bool install){
 if(!sh)return;
 sh->lagPathHits=pathHits;sh->lagMindHits=mindHits;
 const bool sp=singlelag::singlePlayer(get<int>(0xA8B238));
 const bool ready=sp&&!fault&&!sh->stop&&!sh->readOnly&&hookWorld.bound&&get<bool>(0xA8E9A0);
 const uint32_t wanted=sh->singlePlayerLagFix&3;
 if(!ready||!wanted){restore();if(wanted){sh->lagPathStatus=(wanted&1)?(sp?-3:-1):0;sh->lagMindStatus=(wanted&2)?(sp?-3:-1):0;}return;}
 volatile uint8_t*active[]={&pathActive,&mindActive};
 volatile int32_t*status[]={&sh->lagPathStatus,&sh->lagMindStatus};
 const DWORD now=sampleClock(sh);const bool check=!lastCheck||DWORD(now-lastCheck)>=1000;
 if(check)lastCheck=now;
 prepare();
 for(unsigned i=0;i<2;++i){
  if(!(wanted&(1u<<i))){*active[i]=0;*status[i]=restoreRiskSite(sites[i])?0:-2;continue;}
  if(!install&&!*active[i]){*status[i]=-3;continue;}
  if(*active[i]&&!check)continue;
  *active[i]=0;
  if(installRiskSite(sites[i])){*active[i]=1;*status[i]=1;}
  else{restoreRiskSite(sites[i]);*status[i]=-2;}
 }
}
}
static bool restoreSinglePlayerLag(){return lagfix::restore();}

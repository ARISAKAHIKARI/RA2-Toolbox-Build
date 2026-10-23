#include "scroll_refresh.h"
static scrollrefresh::Rate scrollRates[3];
static scrollrefresh::Cadence scrollCadence;
static uint64_t scrollMeasureAt,scrollDrawAt;static uint32_t scrollMeasureCount,scrollMaxGap;static bool scrollRefreshActive,scrollRendering;
struct ScrollCall {int direction;int*distance;bool commit;};
static bool nativeScrollMap(const ScrollCall&s){
 if(!s.distance||s.direction<0||s.direction>7)return false;
 const int amount=*s.distance;
 if(!s.commit)return amount&&call<int>(0x6DA230,get<uint32_t>(0x887324),s.direction)!=-1;
 call<void>(0x6D8530,get<uint32_t>(0x887324),s.direction,amount);return true;
}
static int highScrollAction(uint32_t address){
 if(!scrollRefreshActive||!sh||!sh->scrollRefreshMode||!sh->scrollRefreshHz||!sh->foreground)return 0;
 const auto&s=*reinterpret_cast<const ScrollCall*>(uintptr_t(address));
 if(!s.distance||s.direction<0||s.direction>7)return 1;
 if(!s.commit)return nativeScrollMap(s)?2:1;
 int distance=*s.distance;if(distance<0||distance>4096)return 0;
 const bool dragging=get<bool>(0x87F7E8+0x555A);
 // Native drag modes 1/2 consume physical displacement and recenter the
 // cursor. Only edge scroll and mode 0 (anchor-based velocity) are timed.
 const bool velocity=!dragging||call<int>(0x5FBF70,0xA8EB60)==0;
 if(velocity){const int axis=!dragging?0:(s.direction==2||s.direction==6)?1:2;
  distance=scrollrefresh::amount(scrollRates[axis],distance,s.direction,preciseclock::micros());}
 if(distance){ScrollCall scaled{s.direction,&distance,true};nativeScrollMap(scaled);}
 return 2;
}
// On a denied Capture, replay the displaced bytes and resume the ORIGINAL
// game call. The guarded call<> helper intentionally cannot run without a
// lease; using it for this fallback would silently swallow native scrolling.
static __declspec(naked) bool __stdcall originalScrollMap(int,int*,bool){__asm {
 mov eax,[esp+8]
 mov cl,byte ptr [esp+12]
 push 4A9848h
 ret
}}
// Original ScrollMap does not retain ECX and pops three arguments.
static bool __stdcall highScrollMap(int direction,int*distance,bool commit){
 ScrollCall input{direction,distance,commit};const int result=GuardedRiskAction(8,uint32_t(uintptr_t(&input)),0);
 return result?result==2:originalScrollMap(direction,distance,commit);
}
static RiskSite scrollMapSite{experimentalpatch::ScrollMap,8,{0x8B,0x44,0x24,0x08,0x8A,0x4C,0x24,0x0C},{}};
static void restoreHighScroll(){
 scrollRefreshActive=false;scrollRendering=false;restoreRiskSite(scrollMapSite);scrollCadence={};scrollMeasureAt=scrollDrawAt=0;scrollMeasureCount=scrollMaxGap=0;for(auto&r:scrollRates)r={};
 if(sh){sh->scrollRefreshStatus=0;sh->scrollRefreshRateMilli=sh->scrollRefreshMaxGapUs=0;}
}
static bool highScrollInterfaces(){
 // FlagRedraw(2) schedules a full scene rebuild; Render alone may reuse the
 // last composed surface. This extra work is explicitly opt-in, including LAN/Internet.
 static const uint8_t redraw[]={0xA1,0x24,0x73,0x88,0x00,0x85,0xC0,0x74,0x07,0xC6,0x80,0x7D,0x0D,0x00,0x00,0x01,0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x13,0x83,0x79,0x0C,0x02,0x74,0x03,0x89,0x41,0x0C,0xB9,0xE8,0xF7,0x87,0x00,0xE8,0xA5,0x47,0x08,0x00,0xC2,0x04,0x00};
 if(sh->scrollRefreshMode==3&&memcmp((void*)0x4F42F0,redraw,sizeof redraw))return false;
 // Query/render interfaces from YRpp, checked against both supplied platforms.
 static const uint8_t render[]={0x83,0xEC,8,0xA1,0x1C,0x73,0x88,0,0x55};
 // Supplied LBW image redirects the displaced load through its startup code.
 // Call its ORIGINAL entry, preserving that platform behavior. Playable-view
 // gates above require startup to have completed; the payload is not on disk
 // and its rendering effects still need real-platform validation.
 static const uint8_t renderLBW[]={0x83,0xEC,8,0xE9,0x29,0xA0,0x68,0,0x55};
 static const uint8_t setPosition[]={0x83,0xEC,8,0x56,0x57,0x8B,0x7C,0x24,0x14};
 static const uint8_t edge[]={0x83,0xEC,0x18,0x53,0x55,0x56,0x57,0x8B,0xF9};
 static const uint8_t drag[]={0xA0,0xA0,0xE9,0xA8,0,0x83,0xEC,0x14,0x84};
 return (!memcmp((void*)0x4F4480,render,sizeof render)||!memcmp((void*)0x4F4480,renderLBW,sizeof renderLBW))&&
        !memcmp((void*)0x6D6000,setPosition,sizeof setPosition)&&
        !memcmp((void*)0x692B60,edge,sizeof edge)&&!memcmp((void*)0x693440,drag,sizeof drag);
}
static bool zoomClientPoint(HWND,POINT&);
static void highScrollTick(uint64_t now){
 if(!sh->scrollRefreshMode||sh->scrollRefreshMode>3||!sh->scrollRefreshHz||!sh->foreground||snap.chat||!riskReady()||!simulationReady()){
  if(scrollRefreshActive)restoreHighScroll();return;
 }
 if(!scrollMapSite.after[0]){scrollMapSite.after[0]=0xE9;uint32_t offset=uint32_t(uintptr_t(&highScrollMap))-scrollMapSite.address-5;memcpy(scrollMapSite.after+1,&offset,4);memset(scrollMapSite.after+5,0x90,3);}
 if(!highScrollInterfaces()||!installRiskSite(scrollMapSite)){restoreHighScroll();sh->scrollRefreshStatus=-2;return;}
 scrollRefreshActive=true;sh->scrollRefreshStatus=1;
 if(scrollRendering)return;
 if(!scrollCadence.take(now,sh->scrollRefreshHz))return;
 const uint32_t tactical=get<uint32_t>(0x887324),mouse=get<uint32_t>(0x887640);
 if(!tactical||!mouse||!get<uint32_t>(0x88731C)||!get<uint32_t>(0x887308))return;
 // WWMouse::GetCoords only returns a cached logic-frame position. Sample
 // the current client point for extra camera updates without changing that
 // cache or synthesizing mouse input. Native button/drag state remains owner.
 HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));DWORD owner=0;
 POINT point{};const bool current=window&&GetWindowThreadProcessId(window,&owner)&&owner==GetCurrentProcessId()&&zoomClientPoint(window,point);
 point.x-=get<int>(0x886FA0);point.y-=get<int>(0x886FA4);
 const int width=get<int>(0x886FB8)-get<int>(0x886FA0),height=get<int>(0x886FBC)-get<int>(0x886FA4);
 const bool inside=current&&width>0&&height>0&&point.x>=0&&point.y>=0&&point.x<width&&point.y<height;
 const bool held=get<bool>(0x87F7E8+0x555A);
 bool scrolling=false;
 if(inside&&held){
  if(get<bool>(0x87F7E8+0x5558)&&get<bool>(0x87F7E8+0x554C)&&call<bool>(0x54F5C0,get<uint32_t>(0x87F770),2)){
   call<void>(0x693440,0x87F7E8,&point);scrolling=true;
  }
 }else if(inside&&!held&&get<bool>(0x87F7E8+0x5559)&&(point.x<=1||point.y<=1||point.x>=width-2||point.y>=height-2)){
  call<void>(0x692B60,0x87F7E8,&point);scrolling=true;
 }
 smallcamera::Point position{get<int>(tactical+0xD74),get<int>(tactical+0xD78)};
 const bool moved=scrolling&&(position.x!=get<int>(tactical+0xD64)||position.y!=get<int>(tactical+0xD68));
 if(moved)call<void>(0x6D6000,tactical,&position);
 if(!moved&&sh->scrollRefreshMode<2)return;
 scrollRendering=true;
 if(sh->scrollRefreshMode==3)call<void>(0x4F42F0,0x87F7E8,2);
 call<void>(0x4F4480,0x87F7E8); // native composition + presentation
 scrollRendering=false;
 ++sh->scrollRefreshDraws;
 if(scrollDrawAt&&now>=scrollDrawAt){uint64_t gap=now-scrollDrawAt;if(gap>scrollMaxGap)scrollMaxGap=uint32_t(gap>UINT32_MAX?UINT32_MAX:gap);}
 scrollDrawAt=now;
 if(!scrollMeasureAt)scrollMeasureAt=now;else ++scrollMeasureCount;
 if(now-scrollMeasureAt>=1000000){sh->scrollRefreshRateMilli=uint32_t(uint64_t(scrollMeasureCount)*1000000000/(now-scrollMeasureAt));sh->scrollRefreshMaxGapUs=scrollMaxGap;scrollMeasureAt=now;scrollMeasureCount=scrollMaxGap=0;}
}


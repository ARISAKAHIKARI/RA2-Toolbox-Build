#include "view_zoom.h"
// Presentation-only adapter executed under the existing Assist dispatcher lease.
// No new hook, unit event, simulation tick, or game INI write.
extern "C" int GuardedViewResize(unsigned,int,int);
static viewzoom::Size zoomBase{},zoomOwned{};
static int zoomApplied=100,zoomAttempt=0;
static bool zoomFaulted=false;
using ZoomEnable=void(__cdecl*)();
using ZoomResolve=FARPROC(WINAPI*)(HMODULE,LPCSTR);
using ZoomRect=BOOL(WINAPI*)(HWND,LPRECT);
using ZoomCursor=BOOL(WINAPI*)(LPPOINT);
using ZoomClient=BOOL(WINAPI*)(HWND,LPPOINT);
static ZoomRect zoomRealRect=nullptr;
static ZoomCursor zoomRealCursor=nullptr;
static ZoomClient zoomRealClient=nullptr;
static viewzoom::Size zoomCurrent(){return {get<int>(0x8A00A4),get<int>(0x8A00A8)};}
static bool zoomNativeCompatible(){
 // Entire ChangeResolution body, including the existing platform trampolines.
 // See research/2026-10-23-experimental-view-zoom.md for sample provenance.
 uint8_t bytes[0x58A];if(!read(0x560BF0,bytes,sizeof bytes))return false;
 uint32_t h=2166136261u;for(auto b:bytes)h=(h^b)*16777619u;
 return h==0x858162CFu||h==0xC0FFC84Eu;
}
static bool zoomRenderer(ZoomEnable&enable,ZoomRect&rect){
 uint32_t dd=get<uint32_t>(0x8A0094),vt=0,entry=0;
 if(!read(dd,&vt,4)||!read(vt+0x54,&entry,4))return false;
 HMODULE module=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
     reinterpret_cast<LPCWSTR>(uintptr_t(entry)),&module))return false;
 enable=reinterpret_cast<ZoomEnable>(GetProcAddress(module,"DDEnableZoom"));
 auto resolve=reinterpret_cast<ZoomResolve>(GetProcAddress(module,"DDGetProcAddress"));
 if(!enable||!resolve)return false;
 rect=reinterpret_cast<ZoomRect>(resolve(GetModuleHandleW(L"user32.dll"),"GetClientRect"));
 zoomRealRect=rect;
 zoomRealCursor=reinterpret_cast<ZoomCursor>(resolve(GetModuleHandleW(L"user32.dll"),"GetCursorPos"));
 zoomRealClient=reinterpret_cast<ZoomClient>(resolve(GetModuleHandleW(L"user32.dll"),"ScreenToClient"));
 return rect&&zoomRealCursor&&zoomRealClient;
}
static bool zoomClientPoint(HWND window,POINT&point){
 if(zoomApplied==100)return GetCursorPos(&point)&&ScreenToClient(window,&point);
 RECT r{};auto size=zoomCurrent();
 if(!zoomRealRect||!zoomRealCursor||!zoomRealClient||!zoomRealRect(window,&r)||r.right<=0||r.bottom<=0||
    !zoomRealCursor(&point)||!zoomRealClient(window,&point))return false;
 // Real APIs bypass cnc-ddraw's logical-coordinate hooks, so scale exactly once.
 point.x=point.x<0?-1:int64_t(point.x)*size.width/r.right;point.y=point.y<0?-1:int64_t(point.y)*size.height/r.bottom;
 return true;
}
static void zoomPublish(int status){if(sh){sh->viewZoomStatus=status;sh->viewZoomApplied=zoomApplied;auto s=zoomCurrent();sh->viewZoomWidth=s.width;sh->viewZoomHeight=s.height;}}
static int zoomResize(viewzoom::Size size,ZoomEnable enable){
 if(!gameentry::active(gameentry::Client::Assist)||zoomFaulted)return -1;
 if(sh)InterlockedExchange(&sh->viewZoomBusy,1);
 restoreHighScroll();resetPointerInput();
 int result=GuardedViewResize(uint32_t(uintptr_t(enable)),size.width,size.height);
 if(result<0)zoomFaulted=true;
 sceneFrame=-2147483647;sceneTick=0;
 if(sh){MemoryBarrier();InterlockedExchange(&sh->viewZoomBusy,0);}
 return result;
}
static void forgetViewZoom(){zoomBase={};zoomOwned={};zoomApplied=100;zoomAttempt=0;}
static void restoreViewZoom(){
 if(!zoomBase.width)return;
 // A menu/new match or another display owner already replaced our mode.
 if(zoomCurrent()!=zoomOwned){forgetViewZoom();zoomPublish(viewzoom::Off);return;}
 if(zoomFaulted){zoomPublish(viewzoom::Fault);return;}
 ZoomEnable enable=nullptr;ZoomRect rect=nullptr;
 if(!get<uint32_t>(0x887640)||!get<uint32_t>(0x887308)||!zoomNativeCompatible()||!zoomRenderer(enable,rect)){
  zoomPublish(viewzoom::RestoreFailed);return;
 }
 const uint32_t tactical=get<uint32_t>(0x887324);
 smallcamera::Point center{get<int>(tactical+0xD64),get<int>(tactical+0xD68)};
 int result=zoomResize(zoomBase,enable);
 if(result>0&&zoomCurrent()==zoomBase){forgetViewZoom();smallMapCameraTick();if(tactical)call<void>(0x6D6000,tactical,&center);zoomPublish(viewzoom::Off);}
 else zoomPublish(result<0?viewzoom::Fault:viewzoom::RestoreFailed);
}
static void viewZoomTick(){
 if(!sh)return;
 const int desired=int(sh->viewZoomPercent);
 if(zoomFaulted){zoomPublish(viewzoom::Fault);return;}
 auto current=zoomCurrent();
 if(zoomBase.width&&current!=zoomOwned)forgetViewZoom();
 if(desired==100&&!zoomBase.width){zoomAttempt=0;zoomPublish(viewzoom::Off);return;}
 if(desired!=100&&desired==zoomApplied&&zoomBase.width){zoomAttempt=desired;zoomPublish(viewzoom::Applied);return;}
 if(!sh->foreground||snap.chat||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000)){
  zoomPublish(viewzoom::Waiting);return;
 }
 if(desired==100){if(zoomAttempt!=100){zoomAttempt=100;restoreViewZoom();}return;}
 if(zoomAttempt==desired)return; // failed transitions are never retried every frame
 zoomAttempt=desired;
 if(!viewzoom::valid(desired)){zoomPublish(viewzoom::Dimensions);return;}
 const auto base=zoomBase.width?zoomBase:current,target=viewzoom::target(base,desired);
 if(!target.width){zoomPublish(viewzoom::Dimensions);return;}
 if(get<bool>(0x89F978)||!zoomNativeCompatible()||!get<uint32_t>(0x887640)||!get<uint32_t>(0x887308)){
  zoomPublish(viewzoom::Native);return;
 }
 ZoomEnable enable=nullptr;ZoomRect realRect=nullptr;
 if(!zoomRenderer(enable,realRect)){zoomPublish(viewzoom::Renderer);return;}
 HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));RECT before{};
 if(!realRect(window,&before)||!viewzoom::sameAspect(current,{before.right,before.bottom})){
  zoomPublish(viewzoom::Presentation);return;
 }
 const uint32_t tactical=get<uint32_t>(0x887324);
 smallcamera::Point center{get<int>(tactical+0xD64),get<int>(tactical+0xD68)};
 int result=zoomResize(target,enable);RECT after{};
 bool presented=result>0&&zoomCurrent()==target&&realRect(window,&after)&&before.right==after.right&&before.bottom==after.bottom;
 if(presented){
  zoomBase=base;zoomOwned=target;zoomApplied=desired;smallMapCameraTick();
  call<void>(0x6D6000,tactical,&center);zoomPublish(viewzoom::Applied);return;
 }
 if(result<0){zoomPublish(viewzoom::Fault);return;}
 // A driver may accept the mode but resize the physical window. Roll back.
 const int rollback=zoomResize(current,enable);
 if(rollback<=0||zoomCurrent()!=current){zoomBase=base;zoomOwned=zoomCurrent();zoomPublish(rollback<0?viewzoom::Fault:viewzoom::RestoreFailed);return;}
 zoomPublish(result>0?viewzoom::Presentation:viewzoom::ResizeFailed);
}

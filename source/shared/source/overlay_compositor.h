#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <map>
#include <chrono>
#include <cstring>
#include "overlay_composition.h"
#include "overlay_stack.h"
// One EXE-owned passive window for multiple logical layers. No game hooks,
// engine functions, screen capture, window activation or input forwarding.
// Producers retain their immutable pixels; this host owns only composition.
namespace overlaycompositor {
using overlaycomposition::Context;
using overlaycomposition::Surface;
constexpr uint64_t World=1,Statistics=2,RenderUpgrade=3;
struct DIB {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;void*bits=nullptr;int w=0,h=0;
 void clear(){if(dc){if(old)SelectObject(dc,old);DeleteDC(dc);}if(bitmap)DeleteObject(bitmap);dc=nullptr;bitmap=nullptr;old=nullptr;bits=nullptr;w=h=0;}
 ~DIB(){clear();}
 bool resize(int width,int height){
  if(dc&&w==width&&h==height)return true;clear();
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  dc=CreateCompatibleDC(nullptr);if(!dc)return false;
  bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
  if(!bitmap){clear();return false;}old=SelectObject(dc,bitmap);if(!old||old==HGDI_ERROR){old=nullptr;clear();return false;}
  w=width;h=height;return true;
 }
};
struct Node {Surface surface;uint64_t lastSeen=0;};
class Host {
 std::mutex mutex;std::condition_variable cv;std::thread thread;
 std::map<uint64_t,Node> nodes;Context context{};
 uint64_t generation=0,revision=0,heartbeat=0;bool enabled=false,stop=false,top=false;
 int rate=60;DWORD error=0;std::atomic<HWND> ownedWindow{nullptr};
 static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
  if(m==WM_NCHITTEST)return HTTRANSPARENT;
  if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  if(m==WM_ERASEBKGND)return 1;
  return DefWindowProcW(w,m,a,b);
 }
 void run(){
  WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2UnifiedOverlay139Render";
  RegisterClassW(&wc);
  HWND window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
   wc.lpszClassName,L"统一覆盖层 · 渲染实验",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window){std::lock_guard<std::mutex>lock(mutex);error=GetLastError();return;}
  ownedWindow=window;
  DIB dib;std::vector<uint32_t> output;uint64_t presented=~uint64_t(0),token=0;bool visible=false;POINT lastOrigin{LONG_MIN,LONG_MIN};bool wasTop=false;
  for(;;){
   MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
   Context current;std::vector<Surface> layers;uint64_t version;bool active,highest;int hz;
   {
    std::unique_lock<std::mutex>lock(mutex);if(stop)break;
    cv.wait_for(lock,std::chrono::milliseconds(4),[&]{return stop||revision!=presented;});if(stop)break;
    const auto now=GetTickCount64();
    for(auto i=nodes.begin();i!=nodes.end();)if(now-i->second.lastSeen>750){i=nodes.erase(i);++revision;}else ++i;
    current=context;version=revision;active=enabled&&now-heartbeat<500;highest=top;hz=rate;
    for(const auto&n:nodes)if(overlaycomposition::valid(n.second.surface,current))layers.push_back(n.second.surface);
   }
   const HWND target=reinterpret_cast<HWND>(current.window);DWORD fg=0;GetWindowThreadProcessId(GetForegroundWindow(),&fg);
   RECT client{};POINT origin{};
   active=active&&fg==current.pid&&target&&IsWindow(target)&&!IsIconic(target)&&GetClientRect(target,&client)&&
    client.right==current.width&&client.bottom==current.height&&ClientToScreen(target,&origin)&&!layers.empty();
   if(!active){if(visible)ShowWindow(window,SW_HIDE);visible=false;presented=version;std::this_thread::sleep_for(std::chrono::milliseconds(8));continue;}
   bool updated=true;
   if(!visible||version!=presented||token!=current.token){
    if(!overlaycomposition::compose(current,std::move(layers),output)||!dib.resize(current.width,current.height)){updated=false;}
    else{
     {std::lock_guard<std::mutex>lock(mutex);if(!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY)continue;}
     memcpy(dib.bits,output.data(),output.size()*4);SIZE size{current.width,current.height};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
     updated=UpdateLayeredWindow(window,nullptr,&origin,&size,dib.dc,&zero,0,&blend,ULW_ALPHA)!=FALSE;
     if(updated){presented=version;token=current.token;}
    }
   }
   if(updated){
    if(!visible||wasTop!=highest||origin.x!=lastOrigin.x||origin.y!=lastOrigin.y)
     updated=overlaystack::place(window,highest?overlaystack::Layer::RenderUpgrade:overlaystack::Layer::Statistics,origin.x,origin.y,0,0,SWP_NOSIZE|SWP_SHOWWINDOW)!=FALSE;
    else overlaystack::keepAbove(GetForegroundWindow());
    if(updated){visible=true;lastOrigin=origin;wasTop=highest;}
   }
   {std::lock_guard<std::mutex>lock(mutex);error=updated?0:GetLastError();}
   std::this_thread::sleep_for(std::chrono::microseconds(1000000/std::clamp(hz,5,500)));
  }
  overlaystack::forget(window);DestroyWindow(window);ownedWindow=nullptr;
 }
public:
 ~Host(){shutdown();}
 Context configure(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,int cameraX,int cameraY,bool active,bool highest,int hz){
  std::lock_guard<std::mutex>lock(mutex);if(stop)return {};
  active=active&&window&&pid&&match&&width>0&&height>0&&width<=8192&&height<=8192&&uint64_t(width)*height<=16*1024*1024;
  bool changed=enabled!=active||context.window!=window||context.pid!=pid||context.match!=match||context.width!=width||context.height!=height;
  if(changed){nodes.clear();++generation;++revision;}
  if(top!=highest){top=highest;++revision;}
  if(context.cameraX!=cameraX||context.cameraY!=cameraY)++revision;
  context={generation,window,pid,match,width,height,cameraX,cameraY};enabled=active;heartbeat=GetTickCount64();rate=hz;
  if(active&&!thread.joinable())thread=std::thread([this]{run();});cv.notify_one();return active?context:Context{};
 }
 Context capture(uintptr_t window,uint32_t pid){std::lock_guard<std::mutex>lock(mutex);return enabled&&!error&&context.window==window&&context.pid==pid?context:Context{};}
 // true = compositor owns presentation, including deliberately rejected stale
 // data. false = disabled; producer should use its established legacy window.
 bool publish(Surface surface){
  std::lock_guard<std::mutex>lock(mutex);if(!enabled||error)return false;
  if(!overlaycomposition::valid(surface,context))return true;
  auto&i=nodes[surface.id];auto&old=i.surface;
  bool changed=old.pixels!=surface.pixels||old.rect!=surface.rect||old.order!=surface.order||old.opacity!=surface.opacity||old.context.token!=surface.context.token||old.context.cameraX!=surface.context.cameraX||old.context.cameraY!=surface.context.cameraY;
  i={std::move(surface),GetTickCount64()};if(changed){++revision;cv.notify_one();}return true;
 }
 bool touch(uint64_t id,uintptr_t window,uint32_t pid){std::lock_guard<std::mutex>lock(mutex);if(!enabled||context.window!=window||context.pid!=pid)return false;auto i=nodes.find(id);if(i!=nodes.end()&&overlaycomposition::valid(i->second.surface,context)){i->second.lastSeen=GetTickCount64();return true;}return false;}
 void hide(uint64_t id){std::lock_guard<std::mutex>lock(mutex);if(nodes.erase(id)){++revision;cv.notify_one();}}
 void suspend(){std::lock_guard<std::mutex>lock(mutex);if(enabled){enabled=false;nodes.clear();++generation;++revision;cv.notify_one();}}
 HWND window()const{return ownedWindow.load();}
 size_t layerCount(){std::lock_guard<std::mutex>lock(mutex);return nodes.size();}
 DWORD lastError(){std::lock_guard<std::mutex>lock(mutex);return error;}
 void shutdown(){{std::lock_guard<std::mutex>lock(mutex);stop=true;enabled=false;nodes.clear();cv.notify_one();}if(thread.joinable())thread.join();}
};
inline Host&host(){static Host value;return value;}
}

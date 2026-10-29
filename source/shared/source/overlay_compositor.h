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
#include "overlay_group.h"
#include "overlay_present_monitor.h"
#include <atomic>
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
// Successful HWND submissions, not DWM/vblank/displayed-frame measurements.
struct UploadStatistics {uint64_t count=0,lastTick=0,intervalMs=0,maxGapMs=0,failures=0;};
class Host {
 std::mutex mutex;std::condition_variable cv;std::thread thread;
 std::map<uint64_t,Node> nodes;Context context{};
 uint64_t generation=0,revision=0,heartbeat=0;bool enabled=false,stop=false,top=false;
 int rate=60;DWORD error=0;bool startupFailed=false;std::atomic<HWND> ownedWindow{nullptr};
 UploadStatistics uploadStatistics_{};bool uploadContinuous_=false;
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
  if(!window){std::lock_guard<std::mutex>lock(mutex);error=GetLastError();startupFailed=true;return;}
  ownedWindow=window;
  DIB dib;overlaycomposition::Incremental composition;overlaypresentrate::Monitor displayRate;uint64_t presented=~uint64_t(0),token=0;bool visible=false,dibNeedsFullCopy=true;POINT lastOrigin{LONG_MIN,LONG_MIN};bool wasTop=false;
  overlaypresentrate::Cadence uploadCadence;
  std::chrono::steady_clock::time_point lastUploadComplete{};
  for(;;){
   MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_DISPLAYCHANGE)displayRate.invalidate();TranslateMessage(&message);DispatchMessageW(&message);}
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
   if(!active){if(visible)ShowWindow(window,SW_HIDE);visible=false;presented=version;
    std::unique_lock<std::mutex>lock(mutex);uploadContinuous_=false;uploadStatistics_.intervalMs=uploadStatistics_.maxGapMs=0;
    cv.wait_for(lock,std::chrono::milliseconds(8),[&]{return stop||revision!=version;});continue;}
   // Uploading a full layered HWND faster than its confirmed display mode
   // cannot reveal more frames. Sampling/command clocks keep their own rate.
   const int requestedHz=hz;
   hz=overlaypresentrate::limit(hz,displayRate.update(target,GetTickCount()));
   bool updated=true;
   if(!visible||version!=presented||token!=current.token){
    const bool resize=dib.w!=current.width||dib.h!=current.height||!dib.dc;
    if(!composition.update(current,std::move(layers))||!dib.resize(current.width,current.height)){updated=false;}
    else{
     {std::lock_guard<std::mutex>lock(mutex);if(!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY){dibNeedsFullCopy=true;continue;}}
     const auto&output=composition.pixels();
     auto copy=[&](const overlaycomposition::Rect&dirty){for(int y=dirty.y;y<dirty.y+dirty.h;++y)memcpy(static_cast<uint32_t*>(dib.bits)+size_t(y)*current.width+dirty.x,output.data()+size_t(y)*current.width+dirty.x,size_t(dirty.w)*4);};
     if(resize||dibNeedsFullCopy)copy({0,0,current.width,current.height});else for(const auto&dirty:composition.dirtyRegions())copy(dirty);
     dibNeedsFullCopy=false;
     SIZE size{current.width,current.height};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
     // Composition may start immediately. Only submitting to the HWND is
     // rate-limited, so variable render costs cannot append a whole idle period
     // or produce a short catch-up upload burst after a slow frame.
     {
      std::unique_lock<std::mutex>lock(mutex);
      auto obsolete=[&]{return stop||!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY||rate!=requestedHz;};
      cv.wait_until(lock,uploadCadence.deadline(std::chrono::steady_clock::now(),hz),obsolete);
      if(obsolete()){dibNeedsFullCopy=true;continue;}
     }
     uploadCadence.submitted(std::chrono::steady_clock::now());
     updated=UpdateLayeredWindow(window,nullptr,&origin,&size,dib.dc,&zero,0,&blend,ULW_ALPHA)!=FALSE;
     const auto completed=std::chrono::steady_clock::now();
     {std::lock_guard<std::mutex>lock(mutex);if(context.token==current.token){
      if(updated){
       auto&statistics=uploadStatistics_;statistics.intervalMs=uploadContinuous_?uint64_t(std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(completed-lastUploadComplete).count())):0;
       statistics.maxGapMs=std::max(statistics.maxGapMs,statistics.intervalMs);++statistics.count;statistics.lastTick=GetTickCount64();uploadContinuous_=true;lastUploadComplete=completed;
      }else ++uploadStatistics_.failures;
     }}
     if(updated){presented=version;token=current.token;}
    }
   }else{std::lock_guard<std::mutex>lock(mutex);uploadContinuous_=false;uploadStatistics_.intervalMs=0;}
   if(updated){
    if(!visible||wasTop!=highest||origin.x!=lastOrigin.x||origin.y!=lastOrigin.y)
     updated=overlaystack::place(window,overlaystack::Layer::RenderUpgrade,origin.x,origin.y,0,0,SWP_NOSIZE|SWP_SHOWWINDOW)!=FALSE;
    else overlaystack::keepAbove(GetForegroundWindow());
    if(updated){visible=true;lastOrigin=origin;wasTop=highest;}
   }
   {std::lock_guard<std::mutex>lock(mutex);error=updated?0:GetLastError();}
  }
  overlaystack::forget(window);DestroyWindow(window);ownedWindow=nullptr;
 }
public:
 ~Host(){shutdown();}
 Context configure(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,int cameraX,int cameraY,bool active,bool highest,int hz,bool deferCamera=false){
  std::lock_guard<std::mutex>lock(mutex);if(stop)return {};
  active=active&&window&&pid&&match&&width>0&&height>0&&width<=8192&&height<=8192&&uint64_t(width)*height<=16*1024*1024;
  bool changed=enabled!=active||context.window!=window||context.pid!=pid||context.match!=match||context.width!=width||context.height!=height;
  if(changed){nodes.clear();++generation;++revision;uploadStatistics_={};uploadContinuous_=false;}
  if(top!=highest){top=highest;++revision;}
  const int committedX=deferCamera&&!changed?context.cameraX:cameraX,committedY=deferCamera&&!changed?context.cameraY:cameraY;
  if(context.cameraX!=committedX||context.cameraY!=committedY)++revision;
  context={generation,window,pid,match,width,height,committedX,committedY};enabled=active;heartbeat=GetTickCount64();rate=hz;
  if(active&&!thread.joinable())thread=std::thread([this]{run();});cv.notify_one();
  auto desired=context;desired.cameraX=cameraX;desired.cameraY=cameraY;return active?desired:Context{};
 }
private:
 bool replaceGroupLocked(const std::vector<uint64_t>&ids,std::vector<Surface>layers){
  if(!overlaycomposition::groupIds(ids)||!enabled||startupFailed)return false;
  if(!overlaycomposition::groupValid(ids,layers,context)){
   bool changed=false;for(auto id:ids)changed|=nodes.erase(id)!=0;
   if(changed){++revision;cv.notify_one();}return false;
  }
  bool changed=false;const auto now=GetTickCount64();
  for(auto id:ids){
   auto next=std::find_if(layers.begin(),layers.end(),[&](const Surface&s){return s.id==id;});
   if(next==layers.end()){changed|=nodes.erase(id)!=0;continue;}
   auto&node=nodes[id];const auto&old=node.surface;
   changed|=old.pixels!=next->pixels||old.rect!=next->rect||old.clip!=next->clip||old.cutouts!=next->cutouts||old.order!=next->order||old.opacity!=next->opacity||old.cameraBound!=next->cameraBound||
    old.context.token!=next->context.token||old.context.cameraX!=next->context.cameraX||old.context.cameraY!=next->context.cameraY;
   node={std::move(*next),now};
  }
  if(changed){++revision;cv.notify_one();}return true;
 }
public:
 // A single lock publishes/removes a complete scene/input group.
 bool replaceGroup(const std::vector<uint64_t>&ids,std::vector<Surface>layers){
  std::lock_guard<std::mutex>lock(mutex);return replaceGroupLocked(ids,std::move(layers));
 }
 // A scrolling producer prepares pixels and feedback for the desired camera
 // first. Commit camera and layers together, so the compositor cannot observe
 // a new camera with only the old camera-bound scene and hide between them.
 bool replaceGroupAt(const Context&desired,const std::vector<uint64_t>&ids,std::vector<Surface>layers){
  std::lock_guard<std::mutex>lock(mutex);
  if(!enabled||startupFailed||!overlaycomposition::groupIds(ids)||desired.token!=context.token||desired.window!=context.window||
    desired.pid!=context.pid||desired.match!=context.match||desired.width!=context.width||desired.height!=context.height||
    !overlaycomposition::groupValid(ids,layers,desired))return false;
  const bool cameraChanged=desired.cameraX!=context.cameraX||desired.cameraY!=context.cameraY;
  context=desired;const bool ok=replaceGroupLocked(ids,std::move(layers));
  if(cameraChanged){++revision;cv.notify_one();}return ok;
 }
 Context capture(uintptr_t window,uint32_t pid){std::lock_guard<std::mutex>lock(mutex);return enabled&&!startupFailed&&context.window==window&&context.pid==pid?context:Context{};}
 // true = compositor owns presentation, including deliberately rejected stale
 // data. false = disabled; producer should use its established legacy window.
 bool publish(Surface surface){
  // Runtime presentation errors are diagnostics, not an ownership transfer.
  // Keep accepting current immutable frames while this same HWND retries;
  // otherwise one failed upload makes every producer flash to its old HWND.
  std::lock_guard<std::mutex>lock(mutex);if(!enabled||startupFailed)return false;
  if(!overlaycomposition::valid(surface,context))return true;
  auto&i=nodes[surface.id];auto&old=i.surface;
  bool changed=old.pixels!=surface.pixels||old.rect!=surface.rect||old.clip!=surface.clip||old.cutouts!=surface.cutouts||old.order!=surface.order||old.opacity!=surface.opacity||old.context.token!=surface.context.token||old.context.cameraX!=surface.context.cameraX||old.context.cameraY!=surface.context.cameraY;
  i={std::move(surface),GetTickCount64()};if(changed){++revision;cv.notify_one();}return true;
 }
 bool touch(uint64_t id,uintptr_t window,uint32_t pid){std::lock_guard<std::mutex>lock(mutex);if(!enabled||startupFailed||context.window!=window||context.pid!=pid)return false;auto i=nodes.find(id);if(i!=nodes.end()&&overlaycomposition::valid(i->second.surface,context)){i->second.lastSeen=GetTickCount64();return true;}return false;}
 void hide(uint64_t id){std::lock_guard<std::mutex>lock(mutex);if(nodes.erase(id)){++revision;cv.notify_one();}}
 void suspend(){std::lock_guard<std::mutex>lock(mutex);if(enabled){enabled=false;nodes.clear();++generation;++revision;cv.notify_one();}}
 HWND window()const{return ownedWindow.load();}
 size_t layerCount(){std::lock_guard<std::mutex>lock(mutex);return nodes.size();}
 DWORD lastError(){std::lock_guard<std::mutex>lock(mutex);return error;}
 UploadStatistics uploadStatistics(){std::lock_guard<std::mutex>lock(mutex);return uploadStatistics_;}
 void shutdown(){{std::lock_guard<std::mutex>lock(mutex);stop=true;enabled=false;nodes.clear();cv.notify_one();}if(thread.joinable())thread.join();}
};
inline Host&host(){static Host value;return value;}
}

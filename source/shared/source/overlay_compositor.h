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
#include <new>
#include "overlay_stack.h"
#include "gpu_compositor.h"
#include "overlay_submit_policy.h"
#include "pipeline_timing.h"
#include "deadline_wake.h"
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
struct UploadStatistics {uint64_t count=0,lastTick=0,intervalMs=0,maxGapMs=0,failures=0,exceptions=0;};
class Host {
 std::mutex mutex;asyncwork::DeadlineWake cv;std::thread thread;
 std::map<uint64_t,Node> nodes;Context context{};overlaycomposition::Rect mapExclusion{};
 uint64_t generation=0,revision=0,heartbeat=0;bool enabled=false,stop=false,top=false;
 int rate=60;DWORD error=0;bool startupFailed=false;std::atomic<HWND> ownedWindow{nullptr};
 UploadStatistics uploadStatistics_{};bool uploadContinuous_=false;
 std::atomic<pipelineperf::Sink> diagnosticsSink_{nullptr};
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
  DIB dib;gpupresent::Compositor gpuComposition;bool gpuWindow=false;overlaypublication::CpuStage cpu;overlaypublication::Submission submission;overlaypresentrate::Monitor displayRate;uint64_t presented=~uint64_t(0),token=0;bool visible=false;POINT lastOrigin{LONG_MIN,LONG_MIN};bool wasTop=false;
  pipelineperf::Window timings(L"compositor",{L"compose",L"dib-copy",L"ulw",L"gpu-present"});
  // A native window region enforces the same scissor boundary as the world
  // pass. Uncovered sidebar pixels never belong to this composition HWND.
  std::vector<overlaycomposition::Rect> windowRegion;
  auto clipWindow=[&](const Context&view,const std::vector<Surface>&surfaces){
   std::vector<overlaycomposition::Rect> rects;for(const auto&s:surfaces){auto r=overlaycomposition::bounds(s,view);if(r.w>0&&r.h>0)rects.push_back(r);}
   std::sort(rects.begin(),rects.end(),[](const auto&a,const auto&b){return std::tie(a.x,a.y,a.w,a.h)<std::tie(b.x,b.y,b.w,b.h);});
   rects.erase(std::unique(rects.begin(),rects.end()),rects.end());if(rects==windowRegion)return true;
   HRGN region=CreateRectRgn(0,0,0,0);if(!region)return false;
   for(const auto&r:rects){HRGN part=CreateRectRgn(r.x,r.y,r.x+r.w,r.y+r.h);if(!part){DeleteObject(region);return false;}
    const int combined=CombineRgn(region,region,part,RGN_OR);DeleteObject(part);if(combined==ERROR){DeleteObject(region);return false;}}
   if(!SetWindowRgn(window,region,FALSE)){DeleteObject(region);return false;}windowRegion=std::move(rects);return true;
  };
  overlaypresentrate::Cadence uploadCadence;
  std::chrono::steady_clock::time_point lastUploadComplete{},wakeAt{};bool wakeScheduled=false,cpuPrepared=false;uint64_t cpuVersion=0,cpuToken=0;
  std::chrono::steady_clock::time_point exceptionRetryAt{};uint64_t exceptionCount=0,lastExceptionLog=0;bool exceptionLogged=false;
  auto recover=[&](DWORD code,const wchar_t*kind,const wchar_t*stage)noexcept{
   ++exceptionCount;exceptionRetryAt=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
   // Only this presentation thread owns these resources. Drop partial output;
   // retain the producers' latest immutable layers for a bounded retry.
   try{cpu.reset();dib.clear();gpuComposition.reset();
    if(gpuWindow){auto style=GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_NOREDIRECTIONBITMAP;
     SetWindowLongPtrW(window,GWL_EXSTYLE,style&~WS_EX_LAYERED);SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);gpuWindow=false;}
    ShowWindow(window,SW_HIDE);visible=false;cpuPrepared=false;presented=~uint64_t(0);token=0;windowRegion.clear();submission.failed();
   }catch(...){} // Recovery diagnostics must not replace the original failure.
   try{std::lock_guard<std::mutex>lock(mutex);error=code;++uploadStatistics_.failures;++uploadStatistics_.exceptions;
    uploadContinuous_=false;uploadStatistics_.intervalMs=uploadStatistics_.maxGapMs=0;
   }catch(...){}
   const auto now=GetTickCount64();if(!exceptionLogged||now-lastExceptionLog>=1000){
    exceptionLogged=true;lastExceptionLog=now;
    try{if(auto sink=diagnosticsSink_.load(std::memory_order_relaxed))sink(std::wstring(L"compositor-exception type=")+kind+L" stage="+stage+L" count="+std::to_wstring(exceptionCount)+L" retryMs=100 code="+std::to_wstring(code));}catch(...){}
   }
  };
  for(;;){
   const wchar_t*stage=L"wait";
   try{
    {std::unique_lock<std::mutex>lock(mutex);if(stop)break;
     // New frames/heartbeats cannot bypass exception backoff. Stop can.
     if(exceptionRetryAt>std::chrono::steady_clock::now())cv.wait_until(lock,exceptionRetryAt,[&]{return stop;});if(stop)break;
    }
   timings.report(diagnosticsSink_.load(std::memory_order_relaxed));
   MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_DISPLAYCHANGE){displayRate.invalidate();cpu.lost();submission.failed();}TranslateMessage(&message);DispatchMessageW(&message);}
   Context current;std::vector<Surface> layers;uint64_t version;bool active,highest;int hz;
   {
    std::unique_lock<std::mutex>lock(mutex);if(stop)break;
    const auto wait=overlaypublication::pollDelay(std::chrono::steady_clock::now(),wakeAt,wakeScheduled);
    // No periodic work when disabled/empty. New content, activation and stop
    // carry revisions; live layers retain the existing focus/TTL poll bound.
    auto ready=[&]{return stop||submission.changed(revision);};
    if(!enabled||nodes.empty())cv.wait(lock,ready);
    else cv.wait_until(lock,std::chrono::steady_clock::now()+wait,ready);
    wakeScheduled=false;if(stop)break;
    const auto now=GetTickCount64();
    for(auto i=nodes.begin();i!=nodes.end();)if(now-i->second.lastSeen>750){i=nodes.erase(i);++revision;}else ++i;
    current=context;version=revision;submission.observe(version);active=enabled&&now-heartbeat<500;highest=top;hz=rate;
    stage=L"layers";for(const auto&n:nodes)if(overlaycomposition::valid(n.second.surface,current)){layers.push_back(n.second.surface);overlaycomposition::excludeMapRect(layers.back(),mapExclusion);}
   }
   const HWND target=reinterpret_cast<HWND>(current.window);DWORD fg=0;GetWindowThreadProcessId(GetForegroundWindow(),&fg);
   RECT client{};POINT origin{};
   active=active&&fg==current.pid&&target&&IsWindow(target)&&!IsIconic(target)&&GetClientRect(target,&client)&&
    client.right==current.width&&client.bottom==current.height&&ClientToScreen(target,&origin)&&!layers.empty();
   if(!active){if(visible)ShowWindow(window,SW_HIDE);visible=false;presented=version;
    std::unique_lock<std::mutex>lock(mutex);uploadContinuous_=false;uploadStatistics_.intervalMs=uploadStatistics_.maxGapMs=0;
    continue;}
   const bool continuous=std::any_of(layers.begin(),layers.end(),[](const Surface&s){return s.id==RenderUpgrade;});
   // New enhanced scenes retain the requested upload cadence. The completed
   // HWND already retains identical pixels while the next render computes.
   const int requestedHz=hz;
   hz=continuous?std::clamp(hz,5,500):overlaypresentrate::limit(hz,displayRate.update(target,GetTickCount()));
   bool updated=true,deferred=false;
   auto maySubmit=[&]{const auto now=std::chrono::steady_clock::now(),deadline=uploadCadence.deadline(now,hz);if(deadline<=now)return true;
    wakeAt=deadline;wakeScheduled=true;submission.defer();deferred=true;return false;};
   const bool repair=submission.repair(GetTickCount64());
   const bool changed=version!=presented||token!=current.token;
   const bool gpuFrame=std::any_of(layers.begin(),layers.end(),[](const Surface&s){return bool(s.gpu);});
   const bool needsFrame=submission.needs(GetTickCount64(),changed,visible,!gpuFrame&&cpu.lostContent());
   if(needsFrame&&gpuFrame){
    if(maySubmit()){
    if(!gpuWindow){
     SetWindowLongPtrW(window,GWL_EXSTYLE,(GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_LAYERED)|WS_EX_NOREDIRECTIONBITMAP);
     gpuWindow=true;dib.clear();cpu.reset();cpuPrepared=false;
    }
    SetWindowPos(window,nullptr,origin.x,origin.y,current.width,current.height,SWP_NOZORDER|SWP_NOACTIVATE);
    {
     std::unique_lock<std::mutex>lock(mutex);if(stop||!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY||rate!=requestedHz)continue;
     // A worker may finish while the output cadence is waiting. Present the
     // newest complete layer set, without holding an older frame one interval.
     if(version!=revision){layers.clear();for(const auto&n:nodes)if(overlaycomposition::valid(n.second.surface,current)){layers.push_back(n.second.surface);overlaycomposition::excludeMapRect(layers.back(),mapExclusion);}version=revision;
      if(std::none_of(layers.begin(),layers.end(),[](const Surface&s){return bool(s.gpu);}))continue;
     }
    }
    stage=L"gpu-clipping";if(!clipWindow(current,layers)){submission.failed();continue;}
    uploadCadence.submitted(std::chrono::steady_clock::now());{pipelineperf::Scope timing(timings,3);stage=L"gpu-present";updated=gpuComposition.present(window,current,layers);}
    submission.completed(GetTickCount64(),updated);
    const auto completed=std::chrono::steady_clock::now();
    {std::lock_guard<std::mutex>lock(mutex);if(context.token==current.token){if(updated){auto&statistics=uploadStatistics_;statistics.intervalMs=uploadContinuous_?uint64_t(std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(completed-lastUploadComplete).count())):0;statistics.maxGapMs=std::max(statistics.maxGapMs,statistics.intervalMs);++statistics.count;statistics.lastTick=GetTickCount64();uploadContinuous_=true;lastUploadComplete=completed;}else ++uploadStatistics_.failures;}}
    if(updated){presented=version;token=current.token;}
    else if(gpuComposition.reason()!="GPU frame not ready")gpupresent::available()=false;
    }
   }else if(needsFrame){
   if(gpuWindow){
    gpuComposition.reset();auto style=GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_NOREDIRECTIONBITMAP;
    SetWindowLongPtrW(window,GWL_EXSTYLE,style&~WS_EX_LAYERED);SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);gpuWindow=false;cpu.lost();
   }
   stage=L"cpu-clipping";if(!clipWindow(current,layers)){submission.failed();continue;}
   {
    bool composed=cpuPrepared&&cpuVersion==version&&cpuToken==current.token;
    if(!composed){pipelineperf::Scope timing(timings,0);stage=L"cpu-compose";composed=cpu.compose(current,std::move(layers),dib);cpuPrepared=composed;if(composed){cpuVersion=version;cpuToken=current.token;}}
    if(!composed){updated=false;submission.failed();}
    else{
     {std::lock_guard<std::mutex>lock(mutex);if(!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY){cpu.lost();continue;}}
     const bool needsUpload=cpu.needsUpload(repair||submission.pending()||!visible||token!=current.token);
     bool copied=true;if(cpu.needsCopy()){pipelineperf::Scope timing(timings,1);stage=L"dib-copy";copied=cpu.copy(dib);}if(!copied){submission.failed();continue;}
     if(!needsUpload){
      // A camera/group revision can leave the same clipped pixels. Consume
      // it without copying the full HWND to DWM; placement still runs below.
      presented=version;token=current.token;
      std::lock_guard<std::mutex>lock(mutex);uploadContinuous_=false;uploadStatistics_.intervalMs=0;
     }else if(maySubmit()){
     SIZE size{current.width,current.height};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
     // Composition may start immediately. Only submitting to the HWND is
     // rate-limited, so variable render costs cannot append a whole idle period
     // or produce a short catch-up upload burst after a slow frame.
     {
      std::unique_lock<std::mutex>lock(mutex);
      if(stop||!enabled||context.token!=current.token||context.cameraX!=current.cameraX||context.cameraY!=current.cameraY||rate!=requestedHz||revision!=version){cpu.lost();continue;}
     }
     uploadCadence.submitted(std::chrono::steady_clock::now());
     {pipelineperf::Scope timing(timings,2);stage=L"layered-upload";updated=UpdateLayeredWindow(window,nullptr,&origin,&size,dib.dc,&zero,0,&blend,ULW_ALPHA)!=FALSE;}
     submission.completed(GetTickCount64(),updated);
     const auto completed=std::chrono::steady_clock::now();
     {std::lock_guard<std::mutex>lock(mutex);if(context.token==current.token){
      if(updated){
       auto&statistics=uploadStatistics_;statistics.intervalMs=uploadContinuous_?uint64_t(std::max<int64_t>(0,std::chrono::duration_cast<std::chrono::milliseconds>(completed-lastUploadComplete).count())):0;
       statistics.maxGapMs=std::max(statistics.maxGapMs,statistics.intervalMs);++statistics.count;statistics.lastTick=GetTickCount64();uploadContinuous_=true;lastUploadComplete=completed;
      }else ++uploadStatistics_.failures;
     }}
     if(updated){presented=version;token=current.token;}
     else cpu.lost();
     }
    }
   }
   }else{std::lock_guard<std::mutex>lock(mutex);uploadContinuous_=false;uploadStatistics_.intervalMs=0;
   }
   if(updated&&(!deferred||visible)){
    if(repair||!visible||wasTop!=highest||origin.x!=lastOrigin.x||origin.y!=lastOrigin.y)
     updated=overlaystack::place(window,overlaystack::Layer::RenderUpgrade,origin.x,origin.y,0,0,SWP_NOSIZE|SWP_SHOWWINDOW)!=FALSE;
    else overlaystack::keepAbove(GetForegroundWindow());
    if(updated){visible=true;lastOrigin=origin;wasTop=highest;}
   }
   {std::lock_guard<std::mutex>lock(mutex);error=updated?0:GetLastError();}
   }catch(const std::bad_alloc&){recover(ERROR_NOT_ENOUGH_MEMORY,L"allocation",stage);}
    catch(...){recover(ERROR_UNHANDLED_EXCEPTION,L"exception",stage);}
  }
  gpuComposition.reset();overlaystack::forget(window);DestroyWindow(window);ownedWindow=nullptr;
 }
public:
 ~Host(){shutdown();}
 void setMapExclusion(overlaycomposition::Rect area){std::lock_guard<std::mutex>lock(mutex);if(area!=mapExclusion){mapExclusion=area;++revision;cv.notify_one();}}
 void setDiagnosticsSink(pipelineperf::Sink sink){diagnosticsSink_.store(sink,std::memory_order_relaxed);}
 Context configure(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,int cameraX,int cameraY,bool active,bool highest,int hz,bool deferCamera=false,uint64_t projection=0){
  std::lock_guard<std::mutex>lock(mutex);if(stop)return {};
  const auto priorRevision=revision,now=GetTickCount64();
  active=active&&window&&pid&&match&&width>0&&height>0&&width<=8192&&height<=8192&&uint64_t(width)*height<=16*1024*1024;
  bool changed=enabled!=active||context.window!=window||context.pid!=pid||context.match!=match||context.width!=width||context.height!=height||context.projection!=projection;
  if(changed){nodes.clear();++generation;++revision;uploadStatistics_={};uploadContinuous_=false;}
  if(top!=highest){top=highest;++revision;}
  const int committedX=deferCamera&&!changed?context.cameraX:cameraX,committedY=deferCamera&&!changed?context.cameraY:cameraY;
  if(context.cameraX!=committedX||context.cameraY!=committedY)++revision;
  // Heartbeat renewals alone need no wake; rate changes and stale-to-live
  // recovery must interrupt a pending deadline even with identical pixels.
  if(rate!=hz||(enabled&&active&&now-heartbeat>=500))++revision;
  context={generation,window,pid,match,width,height,committedX,committedY,projection};enabled=active;heartbeat=now;rate=hz;
  if(active&&!thread.joinable())thread=std::thread([this]{run();});if(revision!=priorRevision)cv.notify_one();
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
   changed|=old.pixels!=next->pixels||old.gpu!=next->gpu||old.rect!=next->rect||old.clip!=next->clip||!overlaycomposition::sameCutouts(old.cutouts,next->cutouts)||old.order!=next->order||old.opacity!=next->opacity||old.cameraBound!=next->cameraBound||
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
    desired.pid!=context.pid||desired.match!=context.match||desired.width!=context.width||desired.height!=context.height||desired.projection!=context.projection||
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
  bool changed=old.pixels!=surface.pixels||old.gpu!=surface.gpu||old.rect!=surface.rect||old.clip!=surface.clip||!overlaycomposition::sameCutouts(old.cutouts,surface.cutouts)||old.order!=surface.order||old.opacity!=surface.opacity||old.context.token!=surface.context.token||old.context.cameraX!=surface.context.cameraX||old.context.cameraY!=surface.context.cameraY;
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

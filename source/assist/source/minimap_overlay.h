#include "radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
#include "construction_buffer.h"
#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
namespace minimap {
struct Request {DWORD pid=0;uint32_t match=0;int rate=60;uint32_t reveal=0;int mode=0,width=320,height=160;};
// Separate task cache, shared central read-only connection. Terrain traversal
// never blocks the UI or the game thread. Publish complete immutable frames.
class Worker {
 std::mutex mutex;std::condition_variable cv;std::thread thread;bool stop=false,paused=false;Request request{};uint64_t generation=0;
 std::shared_ptr<const external::radar::Frame> latest;
 void run(){
  external::ReadSession reader;external::cameraassets::Context<external::ReadSession> assets(reader);external::radar::SceneRenderer<decltype(assets)> renderer;external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});uint64_t active=~uint64_t(0);int previousFrame=-1;unsigned previousRevision=0;Request activeRequest{};
  for(;;){Request r;uint64_t token;{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return stop||(!paused&&request.pid);});if(stop)return;r=request;token=generation;}
   if(token!=active){if(r.pid!=activeRequest.pid||r.match!=activeRequest.match){reader.close();sampler.reset();renderer.reset();}else if(r.mode!=activeRequest.mode)sampler.reset();activeRequest=r;active=token;previousFrame=-1;previousRevision=0;}
   std::shared_ptr<const external::radar::Frame> next;
   if(reader.open(r.pid)){reader.begin();bool ok=sampler.step(r.reveal,4096,r.mode==1);int frame=reader.get<int>(0xA8ED84,-1);
    if(ok&&(r.mode==1||frame!=previousFrame||sampler.revision()!=previousRevision)){
     auto data=sampler.snapshot(GetTickCount());if(data.terrain){auto map=std::make_shared<external::radar::Scene>();map->width=r.width;map->height=r.height;external::radar::raster(*data.terrain,r.width,r.height,map->pixels);data.map=std::move(map);}auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
     auto sampled=external::radar::publicationContext(fresh);
     if(external::radar::sameWorld(data,sampled)){
      if(!external::radar::freshMarkers(data,sampled))data.markers.clear();
      // Publish terrain immediately; retain the last completed scene while the
      // next one loads. Initial MIX scans must not keep the whole window hidden.
      {std::lock_guard<std::mutex> lock(mutex);if(token==generation){if(r.mode==1&&latest&&latest->scene&&latest->terrain->geo.same(data.terrain->geo)&&latest->scene->width==r.width&&latest->scene->height==r.height)data.scene=latest->scene;latest=std::make_shared<external::radar::Frame>(data);}}
      if(r.mode==1){assets.identity(r.pid,r.match);auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets);if(scene)data.scene=std::move(scene);}
      next=std::make_shared<external::radar::Frame>(std::move(data));
     }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
     previousFrame=frame;previousRevision=sampler.revision();
    }
    if(!ok){std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
   }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
   auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto completed=external::radar::publicationContext(fresh);
   {std::unique_lock<std::mutex> lock(mutex);if(token==generation&&next){if(external::radar::sameWorld(*next,completed))latest=std::move(next);else latest.reset();}cv.wait_for(lock,std::chrono::milliseconds(std::max(2,1000/std::clamp(r.rate,5,500))),[&]{return stop||paused||generation!=token;});if(stop)return;}
  }
 }
public:
 ~Worker(){shutdown();}
 void set(Request r){std::lock_guard<std::mutex> lock(mutex);if(r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode||r.width!=request.width||r.height!=request.height){++generation;latest.reset();}request=r;paused=false;if(r.pid&&!thread.joinable()){stop=false;thread=std::thread([this]{run();});}cv.notify_one();}
 void pause(){std::lock_guard<std::mutex> lock(mutex);paused=true;cv.notify_one();}
 std::shared_ptr<const external::radar::Frame> get(){std::lock_guard<std::mutex> lock(mutex);return latest;}
 void shutdown(){{std::lock_guard<std::mutex> lock(mutex);stop=true;request={};latest.reset();++generation;cv.notify_one();}if(thread.joinable())thread.join();}
};
static assist::construction::PanelBuffer paintBuffer;
static Worker worker;static HWND window;static std::shared_ptr<const external::radar::Frame> frame;
static std::shared_ptr<const external::radar::Terrain> paintedTerrain;static std::vector<uint32_t> pixels;static int imageW,imageH;
static external::radar::Geometry lastGeometry;static bool dragging=false;static uint32_t dragMatch=0;
static void cancelDrag(){dragging=false;dragMatch=0;if(window&&GetCapture()==window)ReleaseCapture();}
static void hide(){cancelDrag();worker.pause();frame.reset();if(window)ShowWindow(window,SW_HIDE);}
static void navigate(HWND w,LPARAM lp){
 DWORD now=GetTickCount();if(!shared||!enabled||!minimapEnabled||!foregroundGame()||snap.chat||!commandFresh(now)||!frame||!frame->terrain||DWORD(now-frame->tick)>500||dragMatch!=commandMatch()){cancelDrag();return;}
 RECT r{};GetClientRect(w,&r);int x=0,y=0;if(!external::radar::navigate(frame->terrain->geo,short(LOWORD(lp)),short(HIWORD(lp)),r.right,r.bottom,std::max(1,snap.width-160),snap.height,x,y))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);
}
static void clear(){hide();worker.set({});lastGeometry={};paintBuffer.clear();paintedTerrain.reset();pixels.clear();imageW=imageH=0;}
static void destroy(){clear();worker.shutdown();if(window)DestroyWindow(window);window=nullptr;}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_SETCURSOR){
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE; // Never install a second Windows arrow/hand over the game cursor.
 }
 if(msg==WM_LBUTTONDOWN){dragging=true;dragMatch=commandMatch();SetCapture(w);navigate(w,lp);return 0;}
 if(msg==WM_MOUSEMOVE&&dragging){if(!(wp&MK_LBUTTON)){cancelDrag();return 0;}navigate(w,lp);return 0;}
 if(msg==WM_LBUTTONUP){if(dragging)navigate(w,lp);cancelDrag();return 0;}
 if(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE){cancelDrag();return 0;}
 if(msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK)return 0;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);int width=r.right,height=r.bottom;
  if(frame&&frame->terrain&&width>2&&height>2){int mw=width-2,mh=height-2;
   bool real=minimapMode==1&&frame->scene&&frame->scene->width==mw&&frame->scene->height==mh;
   if(!real&&(paintedTerrain!=frame->terrain||imageW!=mw||imageH!=mh)){if(frame->map&&frame->map->width==mw&&frame->map->height==mh)pixels=frame->map->pixels;else pixels.clear();paintedTerrain=frame->terrain;imageW=mw;imageH=mh;}
   HDC mem=paintBuffer.acquire(dc,width,height);
   const auto&image=real?frame->scene->pixels:pixels;
   if(mem&&!image.empty()){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=mw;info.bmiHeader.biHeight=-mh;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    SetDIBitsToDevice(mem,1,1,mw,mh,0,0,0,mh,image.data(),&info,DIB_RGB_COLORS);
    if(!real)for(const auto&m:frame->markers){int x=1+int(m.u*mw),y=1+int(m.v*mh),radius=m.building?2:1;RECT mark{std::max(1,x-radius),std::max(1,y-radius),std::min(width-1,x+radius+1),std::min(height-1,y+radius+1)};HBRUSH b=CreateSolidBrush(RGB(m.color>>16,(m.color>>8)&255,m.color&255));FillRect(mem,&mark,b);DeleteObject(b);}
    const auto&g=frame->terrain->geo;double left=(snap.cameraX/30.0+g.offsetX)*g.factor/g.rw,top=(snap.cameraY/15.0-g.offsetY)*g.factor/g.rh;
    double right=left+std::max(0,snap.width-160)/30.0*g.factor/g.rw,bottom=top+snap.height/15.0*g.factor/g.rh;
    RECT view{1+int(std::clamp(left,0.,1.)*(mw-1)),1+int(std::clamp(top,0.,1.)*(mh-1)),1+int(std::clamp(right,0.,1.)*(mw-1)),1+int(std::clamp(bottom,0.,1.)*(mh-1))};
    HBRUSH white=CreateSolidBrush(RGB(230,234,205));if(view.right>view.left&&view.bottom>view.top)FrameRect(mem,&view,white);DeleteObject(white);
    HBRUSH border=CreateSolidBrush(RGB(157,161,115));FrameRect(mem,&r,border);DeleteObject(border);paintBuffer.present(dc);
   }
  }EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void update(POINT origin,const RECT&client){
 if(!minimapEnabled||!enabled||!foregroundGame()||snap.renderStatus!=1||snap.chat||!pid||snap.width<320||snap.height<200||client.right<320||client.bottom<200){hide();return;}
 auto latest=worker.get();if(latest&&latest->terrain)lastGeometry=latest->terrain->geo;
 double rw=lastGeometry.valid()?lastGeometry.rw:1,rh=lastGeometry.valid()?lastGeometry.rh:1;if(minimapMode==1)rw*=2;
 auto layout=radarlayout::place(snap.width,snap.height,client.right,client.bottom,minimapScale,minimapPosition,minimapX,minimapY,aircraftButtonBox,rw/rh,minimapAlignX,minimapAlignY);
 if(!layout.valid){hide();return;}auto destination=layout.destination;
 worker.set({pid,snap.match,fps,extra.renderReveal,minimapMode,std::min(2048,destination.w-2),std::min(2048,destination.h-2)});
 frame=worker.get();if(!frame||!frame->terrain){cancelDrag();if(window)ShowWindow(window,SW_HIDE);return;}if(dragging&&dragMatch!=commandMatch())cancelDrag();
 if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2DataRadar127";cls.hCursor=nullptr;RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"战术小地图",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);if(window)SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);}
 if(!window)return;RECT old{};GetWindowRect(window,&old);int x=origin.x+destination.x,y=origin.y+destination.y;
 if(!IsWindowVisible(window)||old.left!=x||old.top!=y||old.right-old.left!=destination.w||old.bottom-old.top!=destination.h)overlaystack::place(window,overlaystack::Layer::Minimap,x,y,destination.w,destination.h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);
}
}

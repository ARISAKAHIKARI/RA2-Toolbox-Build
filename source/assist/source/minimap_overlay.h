#include "radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
#include "construction_buffer.h"
#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../shared/source/monitor_refresh.h"
#include "../../shared/source/secondary_display.h"
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
  external::ReadSession reader;external::cameraassets::Context<external::ReadSession> assets(reader);external::radar::SceneRenderer<decltype(assets)> renderer;external::radar::DiagramRenderer diagram;external::radar::Sampler<external::SharedReadView> sampler(external::SharedReadView{&reader});uint64_t active=~uint64_t(0);int previousFrame=-1;unsigned previousRevision=0;Request activeRequest{};
  for(;;){Request r;uint64_t token;{std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return stop||(!paused&&request.pid);});if(stop)return;r=request;token=generation;}
   if(token!=active){if(r.pid!=activeRequest.pid||r.match!=activeRequest.match){reader.close();sampler.reset();renderer.reset();diagram.reset();}else if(r.mode!=activeRequest.mode)sampler.reset();activeRequest=r;active=token;previousFrame=-1;previousRevision=0;}
   std::shared_ptr<const external::radar::Frame> next;
   if(reader.open(r.pid)){reader.begin();bool ok=sampler.step(r.reveal,4096,r.mode==1);int frame=reader.get<int>(0xA8ED84,-1);
    if(ok&&(r.mode==1||frame!=previousFrame||sampler.revision()!=previousRevision)){
     auto data=sampler.snapshot(GetTickCount());data.map=diagram.render(data.terrain,r.width,r.height);auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
     auto sampled=external::radar::publicationContext(fresh);
     if(external::radar::sameWorld(data,sampled)){
      if(!external::radar::freshMarkers(data,sampled)){data.markers.clear();data.repairs.clear();}
      // Publish terrain immediately; retain the last completed scene while the
      // next one loads. Initial MIX scans must not keep the whole window hidden.
      {std::lock_guard<std::mutex> lock(mutex);if(token==generation){if(r.mode==1&&latest&&latest->scene&&latest->terrain->reveal==data.terrain->reveal&&latest->terrain->geo.same(data.terrain->geo)&&latest->scene->width==r.width&&latest->scene->height==r.height)data.scene=latest->scene;latest=std::make_shared<external::radar::Frame>(data);}}
      if(r.mode==1){assets.identity(r.pid,r.match);auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets);if(scene)data.scene=std::move(scene);else if(renderer.capacityExceeded())data.scene.reset();}
      next=std::make_shared<external::radar::Frame>(std::move(data));
     }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
     previousFrame=frame;previousRevision=sampler.revision();
    }
    if(!ok){std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
   }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
   auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto completed=external::radar::publicationContext(fresh);
   {std::unique_lock<std::mutex> lock(mutex);if(token==generation&&next){if(external::radar::sameWorld(*next,completed))latest=std::move(next);else latest.reset();}cv.wait_for(lock,std::chrono::microseconds(monitorrefresh::periodMicros(r.rate)),[&]{return stop||paused||generation!=token||request.rate!=r.rate;});if(stop)return;}
  }
 }
public:
 ~Worker(){shutdown();}
 void set(Request r){std::lock_guard<std::mutex> lock(mutex);if(r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode||r.width!=request.width||r.height!=request.height){++generation;latest.reset();}request=r;paused=false;if(r.pid&&!thread.joinable()){stop=false;thread=std::thread([this]{run();});}cv.notify_one();}
 void pause(){std::lock_guard<std::mutex> lock(mutex);paused=true;cv.notify_one();}
 std::shared_ptr<const external::radar::Frame> get(){std::lock_guard<std::mutex> lock(mutex);return latest;}
 void shutdown(){{std::lock_guard<std::mutex> lock(mutex);stop=true;request={};latest.reset();++generation;cv.notify_one();}if(thread.joinable())thread.join();}
};
static assist::construction::PanelBuffer paintBuffer,secondaryPaintBuffer;
static Worker worker;static HWND window,secondaryWindow;static std::shared_ptr<const external::radar::Frame> frame;static overlayui::Recovery recovery,secondaryRecovery;
static secondarydisplay::Monitor secondaryMonitor;
static external::radar::Geometry lastGeometry;static bool dragging=false;static uint32_t dragMatch=0;static HWND dragWindow;
static void cancelDrag(){dragging=false;dragMatch=0;dragWindow=nullptr;auto captured=GetCapture();if((window&&captured==window)||(secondaryWindow&&captured==secondaryWindow))ReleaseCapture();}
static void hidePrimary(){if(dragWindow==window)cancelDrag();recovery.suspend();if(window)ShowWindow(window,SW_HIDE);}
static void hideSecondary(){if(dragWindow==secondaryWindow)cancelDrag();secondaryRecovery.suspend();if(secondaryWindow)ShowWindow(secondaryWindow,SW_HIDE);}
static void hide(){hidePrimary();hideSecondary();worker.pause();frame.reset();}
static void invalidateDisplays(){secondaryMonitor.invalidate();}
static void navigate(HWND w,LPARAM lp){
 DWORD now=GetTickCount();bool requested=w==secondaryWindow?minimapSecondary:minimapEnabled;
 if(!shared||!enabled||!requested||!foregroundGame()||snap.chat||!commandFresh(now)||!frame||!frame->terrain||DWORD(now-frame->tick)>500||dragMatch!=commandMatch()){cancelDrag();return;}
 RECT r{};GetClientRect(w,&r);int x=0,y=0;if(!external::radar::navigate(frame->terrain->geo,short(LOWORD(lp)),short(HIWORD(lp)),r.right,r.bottom,std::max(1,snap.width-160),snap.height,x,y))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);
}
static void clear(){hide();worker.set({});lastGeometry={};paintBuffer.clear();secondaryPaintBuffer.clear();}
static void destroy(){clear();worker.shutdown();if(window)DestroyWindow(window);if(secondaryWindow)DestroyWindow(secondaryWindow);window=secondaryWindow=nullptr;}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_SETCURSOR){
  if(w==secondaryWindow){SetCursor(LoadCursorW(nullptr,IDC_ARROW));return TRUE;}
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){dragging=true;dragWindow=w;dragMatch=commandMatch();SetCapture(w);navigate(w,lp);return 0;}
 if(msg==WM_MOUSEMOVE&&dragging&&dragWindow==w){if(!(wp&MK_LBUTTON)){cancelDrag();return 0;}navigate(w,lp);return 0;}
 if(msg==WM_LBUTTONUP){if(dragging&&dragWindow==w)navigate(w,lp);cancelDrag();return 0;}
 if(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE){cancelDrag();return 0;}
 if(msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK)return 0;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);int width=r.right,height=r.bottom;
  if(frame&&frame->terrain&&width>2&&height>2){int mw=width-2,mh=height-2;
   bool real=minimapMode==1&&bool(frame->scene);auto image=real?frame->scene:frame->map;
   auto&buffer=w==secondaryWindow?secondaryPaintBuffer:paintBuffer;HDC mem=buffer.acquire(dc,width,height);
   if(mem&&image&&image->width>0&&image->height>0&&!image->pixels.empty()){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image->width;info.bmiHeader.biHeight=-image->height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    if(image->width==mw&&image->height==mh)SetDIBitsToDevice(mem,1,1,mw,mh,0,0,0,mh,image->pixels.data(),&info,DIB_RGB_COLORS);
    else {SetStretchBltMode(mem,HALFTONE);SetBrushOrgEx(mem,0,0,nullptr);StretchDIBits(mem,1,1,mw,mh,0,0,image->width,image->height,image->pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);}
    if(!real)for(const auto&m:frame->markers){int x=1+int(m.u*mw),y=1+int(m.v*mh),radius=m.building?2:1;RECT mark{std::max(1,x-radius),std::max(1,y-radius),std::min(width-1,x+radius+1),std::min(height-1,y+radius+1)};HBRUSH b=CreateSolidBrush(RGB(m.color>>16,(m.color>>8)&255,m.color&255));FillRect(mem,&mark,b);DeleteObject(b);}
    const auto&g=frame->terrain->geo;double left=(snap.cameraX/30.0+g.offsetX)*g.factor/g.rw,top=(snap.cameraY/15.0-g.offsetY)*g.factor/g.rh;
    double right=left+std::max(0,snap.width-160)/30.0*g.factor/g.rw,bottom=top+snap.height/15.0*g.factor/g.rh;
    RECT view{1+int(std::clamp(left,0.,1.)*(mw-1)),1+int(std::clamp(top,0.,1.)*(mh-1)),1+int(std::clamp(right,0.,1.)*(mw-1)),1+int(std::clamp(bottom,0.,1.)*(mh-1))};
    HBRUSH white=CreateSolidBrush(RGB(230,234,205));if(view.right>view.left&&view.bottom>view.top)FrameRect(mem,&view,white);DeleteObject(white);
    HBRUSH border=CreateSolidBrush(RGB(157,161,115));FrameRect(mem,&r,border);DeleteObject(border);buffer.present(dc);
   }
  }EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void placePanel(bool second,int x,int y,int width,int height){
 auto&panel=second?secondaryWindow:window;auto&keep=second?secondaryRecovery:recovery;
 if(!panel){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2DataRadar127";cls.hCursor=nullptr;RegisterClassW(&cls);panel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,second?L"第二显示器小地图":L"战术小地图",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);if(panel)SetLayeredWindowAttributes(panel,0,255,LWA_ALPHA);}
 if(!panel)return;RECT old{};GetWindowRect(panel,&old);
 bool repair=keep.due(GetTickCount(),true,{uintptr_t(game),pid,displayMatch(),width,height},IsWindowVisible(panel)!=FALSE);
 if(repair||old.left!=x||old.top!=y||old.right-old.left!=width||old.bottom-old.top!=height)keep.submitted(overlaystack::place(panel,overlaystack::Layer::Minimap,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE);
 InvalidateRect(panel,nullptr,FALSE);UpdateWindow(panel);
}
static void update(POINT origin,const RECT&client){
 bool valid=pid&&snap.renderStatus==1&&snap.width>=320&&snap.height>=200&&external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}).visible();
 bool primary=valid&&enabled&&minimapEnabled&&foregroundGame()&&!snap.chat&&client.right>=320&&client.bottom>=200;
 auto target=minimapSecondary?secondaryMonitor.get(game,ui,GetTickCount()):secondarydisplay::Target{};
 bool secondary=valid&&minimapSecondary&&target.available();
 if(!primary)hidePrimary();if(!secondary)hideSecondary();if(!primary&&!secondary){worker.pause();frame.reset();return;}
 auto latest=worker.get();if(latest&&latest->terrain)lastGeometry=latest->terrain->geo;
 double rw=lastGeometry.valid()?lastGeometry.rw:1,rh=lastGeometry.valid()?lastGeometry.rh:1;if(minimapMode==1)rw*=2;double aspect=rw/rh;
 auto layout=primary?radarlayout::place(snap.width,snap.height,client.right,client.bottom,minimapScale,minimapPosition,minimapX,minimapY,aircraftButtonBox,aspect,minimapAlignX,minimapAlignY):radarlayout::Layout{};
 if(primary&&!layout.valid){primary=false;hidePrimary();}
 radarlayout::Box secondaryBox;
 if(secondary){int workW=target.work.right-target.work.left,workH=target.work.bottom-target.work.top;int width=std::max(3,std::min(600,workW*2/5-24)),height=std::max(3,std::min(workH/3-24,int((width-2)/aspect)+2));width=std::max(3,std::min(width,int((height-2)*aspect)+2));secondaryBox={int(target.work.right)-12-width,int(target.work.bottom)-12-height,width,height};}
 if(!primary&&!secondary){worker.pause();frame.reset();return;}
 // Keep the game panel at its exact native-area presentation size. The
 // additional desktop panel scales that same completed image; with the game
 // panel disabled, the shared worker renders directly to the desktop size.
 int width=primary?layout.destination.w:secondaryBox.w,height=primary?layout.destination.h:secondaryBox.h;
 worker.set({pid,displayMatch(),informationFPS(),uint32_t(extra.renderReveal||extra.mapStrategy==3),minimapMode,std::min(2048,width-2),std::min(2048,height-2)});
 frame=worker.get();if(!frame||!frame->terrain){cancelDrag();hidePrimary();hideSecondary();return;}if(dragging&&dragMatch!=commandMatch())cancelDrag();
 if(primary){auto d=layout.destination;placePanel(false,origin.x+d.x,origin.y+d.y,d.w,d.h);}
 if(secondary)placePanel(true,secondaryBox.x,secondaryBox.y,secondaryBox.w,secondaryBox.h);
}
}

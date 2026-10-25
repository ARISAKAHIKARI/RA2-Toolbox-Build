#include "radar_layout.h"
#include "secondary_radar_layout.h"
#include "../../external_reader/source/radar_navigation.h"
#include "construction_buffer.h"
#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../shared/source/monitor_refresh.h"
#include "../../shared/source/secondary_display.h"
#include "radar_paint_gate.h"
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
     const bool same=external::radar::sameWorld(data,sampled);
     // Sampling may span several game ticks. Keep the last complete image
     // until this world's markers are fresh instead of publishing a frame in
     // which every unit vanishes for one paint.
     if(same&&data.markerScanValid&&external::radar::freshMarkers(data,sampled)){
      // Publish terrain immediately; retain the last completed scene while the
      // next one loads. Initial MIX scans must not keep the whole window hidden.
      {std::lock_guard<std::mutex> lock(mutex);if(token==generation){if(r.mode==1&&latest&&latest->scene&&latest->terrain->reveal==data.terrain->reveal&&latest->terrain->geo.same(data.terrain->geo)&&latest->scene->width==r.width&&latest->scene->height==r.height)data.scene=latest->scene;latest=std::make_shared<external::radar::Frame>(data);}}
      if(r.mode==1){assets.identity(r.pid,r.match);auto scene=renderer.render(data,r.width,r.height,external::SharedReadView{&reader},assets);if(scene)data.scene=std::move(scene);}
      next=std::make_shared<external::radar::Frame>(std::move(data));
     }else if(!same&&sampled.readable){std::lock_guard<std::mutex> lock(mutex);if(token==generation&&latest&&!external::radar::sameWorld(*latest,sampled))latest.reset();}
     previousFrame=frame;previousRevision=sampler.revision();
    }
    if(!ok){auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto observed=external::radar::publicationContext(fresh);
     if(observed.readable){std::lock_guard<std::mutex> lock(mutex);if(token==generation&&latest&&!external::radar::sameWorld(*latest,observed))latest.reset();}}
   }else{std::lock_guard<std::mutex> lock(mutex);if(token==generation)latest.reset();}
   auto fresh=[&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};auto completed=external::radar::publicationContext(fresh);
   {std::unique_lock<std::mutex> lock(mutex);if(token==generation&&next){if(external::radar::sameWorld(*next,completed))latest=std::move(next);else if(completed.readable&&latest&&!external::radar::sameWorld(*latest,completed))latest.reset();}cv.wait_for(lock,std::chrono::microseconds(monitorrefresh::periodMicros(r.rate)),[&]{return stop||paused||generation!=token||request.rate!=r.rate;});if(stop)return;}
  }
 }
public:
 ~Worker(){shutdown();}
 void set(Request r){std::lock_guard<std::mutex> lock(mutex);bool changed=r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode||r.width!=request.width||r.height!=request.height;
  if(changed){++generation;
   // A size edit may present the last verified world while the one shared
   // renderer produces the requested dimensions. Identity/mode changes may not.
   if(r.pid!=request.pid||r.match!=request.match||r.reveal!=request.reveal||r.mode!=request.mode)latest.reset();
  }
  request=r;paused=false;if(r.pid&&!thread.joinable()){stop=false;thread=std::thread([this]{run();});}cv.notify_one();}
 void pause(){std::lock_guard<std::mutex> lock(mutex);paused=true;cv.notify_one();}
 std::shared_ptr<const external::radar::Frame> get(){std::lock_guard<std::mutex> lock(mutex);return latest;}
 void shutdown(){{std::lock_guard<std::mutex> lock(mutex);stop=true;request={};latest.reset();++generation;cv.notify_one();}if(thread.joinable())thread.join();}
};
static assist::construction::PanelBuffer paintBuffer,secondaryPaintBuffer;
static Worker worker;static HWND window,secondaryWindow;static std::shared_ptr<const external::radar::Frame> frame;static overlayui::Recovery recovery,secondaryRecovery;
static radargate::PaintGate<external::radar::Frame> primaryPaintGate,secondaryPaintGate;
static secondarydisplay::Monitor secondaryMonitor;
static external::radar::Geometry lastGeometry;static bool dragging=false;static uint32_t dragMatch=0;static HWND dragWindow;
enum class SecondaryEdit {None,Move,Resize,ResizeUpperLeft};
static SecondaryEdit secondaryEdit=SecondaryEdit::None;
static POINT secondaryStartCursor{};static RECT secondaryStartRect{};
static int secondaryRequestedWidth=320;
static double secondaryAspect(){
 double rw=lastGeometry.valid()?lastGeometry.rawWidth:1,rh=lastGeometry.valid()?lastGeometry.rawHeight:1;
 if(minimapMode==1)rw*=2;return rw/rh;
}
static void finishSecondaryEdit(){
 if(secondaryEdit==SecondaryEdit::None)return;
 secondaryEdit=SecondaryEdit::None;
 if(GetCapture()==secondaryWindow)ReleaseCapture();
 if(!secondaryWindow)return;
 RECT r{};if(!GetWindowRect(secondaryWindow,&r))return;
 auto monitor=secondaryMonitor.get(game,ui,GetTickCount());if(!monitor.available())return;
 auto saved=secondaryradar::capture(monitor.work.left,monitor.work.top,monitor.work.right,monitor.work.bottom,
                                    {r.left,r.top,r.right-r.left,r.bottom-r.top},secondaryRequestedWidth);
 if(saved.x==minimapSecondaryX&&saved.y==minimapSecondaryY&&saved.width==minimapSecondaryWidth)return;
 minimapSecondaryX=saved.x;minimapSecondaryY=saved.y;minimapSecondaryWidth=saved.width;
 secondaryPaintGate.invalidate();syncSecondaryLayoutControls();changedSetting();
}
static void moveSecondaryEdit(){
 if(secondaryEdit==SecondaryEdit::None||!secondaryWindow)return;
 POINT current{};RECT r{};
 auto monitor=secondaryMonitor.get(game,ui,GetTickCount());
 if(!monitor.available()||!GetCursorPos(&current)||!GetWindowRect(secondaryWindow,&r))return;
 auto work=monitor.work;
 const int64_t workW=int64_t(work.right)-work.left,workH=int64_t(work.bottom)-work.top;
 if(workW<3||workH<3)return;
 const int margin=int(std::min<int64_t>(12,std::min((workW-3)/2,(workH-3)/2)));
 if(secondaryEdit==SecondaryEdit::Move){
  const int width=secondaryStartRect.right-secondaryStartRect.left,height=secondaryStartRect.bottom-secondaryStartRect.top;
  const int64_t leftBound=int64_t(work.left)+margin,topBound=int64_t(work.top)+margin;
  int left=int(std::clamp(int64_t(secondaryStartRect.left)+current.x-secondaryStartCursor.x,
                          leftBound,std::max(leftBound,int64_t(work.right)-margin-width)));
  int top=int(std::clamp(int64_t(secondaryStartRect.top)+current.y-secondaryStartCursor.y,
                         topBound,std::max(topBound,int64_t(work.bottom)-margin-height)));
  SetWindowPos(secondaryWindow,nullptr,left,top,0,0,SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOSIZE);
 }else{
  bool upperLeft=secondaryEdit==SecondaryEdit::ResizeUpperLeft;
  int64_t delta=upperLeft?int64_t(secondaryStartCursor.x)-current.x:int64_t(current.x)-secondaryStartCursor.x;
  int requested=int(std::clamp(int64_t(secondaryStartRect.right-secondaryStartRect.left)+delta,
                                int64_t(160),int64_t(4096)));
  auto b=secondaryradar::place(work.left,work.top,work.right,work.bottom,0,0,requested,secondaryAspect());
  if(b.w<3||b.h<3)return;
  int64_t requestedX=upperLeft?int64_t(secondaryStartRect.right)-b.w:secondaryStartRect.left;
  int64_t requestedY=upperLeft?int64_t(secondaryStartRect.bottom)-b.h:secondaryStartRect.top;
  int x=int(std::clamp(requestedX,int64_t(work.left)+margin,int64_t(work.right)-margin-b.w));
  int y=int(std::clamp(requestedY,int64_t(work.top)+margin,int64_t(work.bottom)-margin-b.h));
  secondaryRequestedWidth=requested;
  SetWindowPos(secondaryWindow,nullptr,x,y,b.w,b.h,SWP_NOACTIVATE|SWP_NOZORDER);
 }
}
static void cancelDrag(){dragging=false;dragMatch=0;dragWindow=nullptr;auto captured=GetCapture();if((window&&captured==window)||(secondaryWindow&&captured==secondaryWindow))ReleaseCapture();}
static void hidePrimary(){if(dragWindow==window)cancelDrag();recovery.suspend();primaryPaintGate.invalidate();if(window)ShowWindow(window,SW_HIDE);}
static void hideSecondary(){if(secondaryEdit!=SecondaryEdit::None)finishSecondaryEdit();if(dragWindow==secondaryWindow)cancelDrag();secondaryRecovery.suspend();secondaryPaintGate.invalidate();if(secondaryWindow)ShowWindow(secondaryWindow,SW_HIDE);}
static void hide(){hidePrimary();hideSecondary();worker.pause();frame.reset();}
static void invalidateDisplays(){secondaryMonitor.invalidate();primaryPaintGate.invalidate();secondaryPaintGate.invalidate();}
static void navigate(HWND w,LPARAM lp){
 DWORD now=GetTickCount();bool requested=w==secondaryWindow?minimapSecondary:minimapEnabled;
 if(!shared||!enabled||!requested||!foregroundGame()||snap.chat||!commandFresh(now)||!frame||!frame->terrain||DWORD(now-frame->tick)>500||dragMatch!=commandMatch()){cancelDrag();return;}
 RECT r{};GetClientRect(w,&r);int x=0,y=0;if(!external::radar::navigate(frame->terrain->geo,short(LOWORD(lp)),short(HIWORD(lp)),r.right,r.bottom,std::max(1,snap.width-160),snap.height,x,y))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);wakeGame();
}
static SecondaryEdit secondaryHit(HWND w,int x,int y){
 if(w!=secondaryWindow)return SecondaryEdit::None;
 RECT r{};GetClientRect(w,&r);
 if(x>=r.right-18&&y>=r.bottom-18)return SecondaryEdit::Resize;
 if(x>=0&&x<18&&y>=0&&y<18)return SecondaryEdit::ResizeUpperLeft;
 if(y>=0&&y<18)return SecondaryEdit::Move;
 return SecondaryEdit::None;
}
static void clear(){hide();worker.set({});lastGeometry={};paintBuffer.clear();secondaryPaintBuffer.clear();}
static void destroy(){clear();worker.shutdown();if(window)DestroyWindow(window);if(secondaryWindow)DestroyWindow(secondaryWindow);window=secondaryWindow=nullptr;}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_SETCURSOR){
  if(w==secondaryWindow){POINT pointer{};GetCursorPos(&pointer);ScreenToClient(w,&pointer);
   auto hit=secondaryHit(w,pointer.x,pointer.y);
   SetCursor(LoadCursorW(nullptr,(hit==SecondaryEdit::Resize||hit==SecondaryEdit::ResizeUpperLeft)?IDC_SIZENWSE:hit==SecondaryEdit::Move?IDC_SIZEALL:IDC_ARROW));return TRUE;}
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){
  auto hit=secondaryHit(w,short(LOWORD(lp)),short(HIWORD(lp)));
  if(hit!=SecondaryEdit::None&&GetCursorPos(&secondaryStartCursor)&&GetWindowRect(w,&secondaryStartRect)){
   secondaryEdit=hit;secondaryRequestedWidth=minimapSecondaryWidth;SetCapture(w);return 0;
  }
  dragging=true;dragWindow=w;dragMatch=commandMatch();SetCapture(w);navigate(w,lp);return 0;
 }
 if(msg==WM_MOUSEMOVE){
  if(w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){if(wp&MK_LBUTTON)moveSecondaryEdit();else finishSecondaryEdit();return 0;}
  if(dragging&&dragWindow==w){if(!(wp&MK_LBUTTON)){cancelDrag();return 0;}navigate(w,lp);return 0;}
 }
 if(msg==WM_LBUTTONUP){if(w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){finishSecondaryEdit();return 0;}if(dragging&&dragWindow==w)navigate(w,lp);cancelDrag();return 0;}
 if(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE){if(w==secondaryWindow)finishSecondaryEdit();cancelDrag();return 0;}
 if(msg==WM_SIZE&&w==secondaryWindow&&secondaryEdit!=SecondaryEdit::None){secondaryPaintGate.invalidate();InvalidateRect(w,nullptr,FALSE);return 0;}
 if(msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK)return 0;if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);int width=r.right,height=r.bottom;
  if(frame&&frame->terrain&&width>2&&height>2){int mw=width-2,mh=height-2;
   bool real=minimapMode==1&&bool(frame->scene);auto image=real?frame->scene:frame->map;
   auto&buffer=w==secondaryWindow?secondaryPaintBuffer:paintBuffer;HDC mem=buffer.acquire(dc,width,height);
   if(mem&&image&&image->width>0&&image->height>0&&!image->pixels.empty()){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image->width;info.bmiHeader.biHeight=-image->height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    if(image->width==mw&&image->height==mh)SetDIBitsToDevice(mem,1,1,mw,mh,0,0,0,mh,image->pixels.data(),&info,DIB_RGB_COLORS);
    else {SetStretchBltMode(mem,HALFTONE);SetBrushOrgEx(mem,0,0,nullptr);StretchDIBits(mem,1,1,mw,mh,0,0,image->width,image->height,image->pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);}
    if(!real)for(const auto&m:frame->markers){int x=1+int(m.u*mw),y=1+int(m.v*mh),radius=m.building?2:1;RECT mark{std::max(1,x-radius),std::max(1,y-radius),std::min(width-1,x+radius+1),std::min(height-1,y+radius+1)};HBRUSH b=CreateSolidBrush(RGB(m.color>>16,(m.color>>8)&255,m.color&255));FillRect(mem,&mark,b);DeleteObject(b);}
    const auto&g=frame->terrain->geo;double left=(snap.cameraX/30.0+g.offsetX)/g.rawWidth,top=(snap.cameraY/15.0-g.offsetY)/g.rawHeight;
    double right=left+std::max(0,snap.width-160)/30.0/g.rawWidth,bottom=top+snap.height/15.0/g.rawHeight;
    RECT view{1+int(std::clamp(left,0.,1.)*(mw-1)),1+int(std::clamp(top,0.,1.)*(mh-1)),1+int(std::clamp(right,0.,1.)*(mw-1)),1+int(std::clamp(bottom,0.,1.)*(mh-1))};
    HBRUSH white=CreateSolidBrush(RGB(230,234,205));if(view.right>view.left&&view.bottom>view.top)FrameRect(mem,&view,white);DeleteObject(white);
    HBRUSH border=CreateSolidBrush(RGB(157,161,115));FrameRect(mem,&r,border);DeleteObject(border);buffer.present(dc);
   }
  }EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void placePanel(bool second,int x,int y,int width,int height){
 if(second&&secondaryEdit!=SecondaryEdit::None)return;
 auto&panel=second?secondaryWindow:window;auto&keep=second?secondaryRecovery:recovery;
 if(!panel){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2DataRadar127";cls.hCursor=nullptr;RegisterClassW(&cls);panel=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,second?L"第二显示器小地图":L"战术小地图",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);if(panel)SetLayeredWindowAttributes(panel,0,255,LWA_ALPHA);}
 if(!panel)return;RECT old{};GetWindowRect(panel,&old);
 bool visible=IsWindowVisible(panel)!=FALSE;
 bool repair=keep.due(GetTickCount(),true,{uintptr_t(game),pid,displayMatch(),width,height},visible);
 bool moved=old.left!=x||old.top!=y||old.right-old.left!=width||old.bottom-old.top!=height;
 if(repair||moved)keep.submitted(overlaystack::place(panel,overlaystack::Layer::Minimap,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE);
 auto&gate=second?secondaryPaintGate:primaryPaintGate;
 if(gate.needs(frame,snap.cameraX,snap.cameraY,snap.width,snap.height,minimapMode,repair||moved||!visible)){
  if(InvalidateRect(panel,nullptr,FALSE)){UpdateWindow(panel);gate.committed(frame,snap.cameraX,snap.cameraY,snap.width,snap.height,minimapMode);}
 }
}
static void update(POINT origin,const RECT&client,bool verifiedBattlefield){
 bool valid=verifiedBattlefield&&pid&&snap.renderStatus==1&&snap.width>=320&&snap.height>=200;
 bool primary=valid&&enabled&&minimapEnabled&&foregroundGame()&&!snap.chat&&client.right>=320&&client.bottom>=200;
 auto target=minimapSecondary?secondaryMonitor.get(game,ui,GetTickCount()):secondarydisplay::Target{};
 bool secondary=valid&&minimapSecondary&&target.available();
 if(!primary)hidePrimary();if(!secondary)hideSecondary();if(!primary&&!secondary){worker.pause();frame.reset();return;}
 auto latest=worker.get();if(latest&&latest->terrain)lastGeometry=latest->terrain->geo;
 double rw=lastGeometry.valid()?lastGeometry.rawWidth:1,rh=lastGeometry.valid()?lastGeometry.rawHeight:1;if(minimapMode==1)rw*=2;double aspect=rw/rh;
 auto native=minimapMode==1?lastGeometry.nativeViewport():external::mapbounds::NativeViewport{};
 auto layout=primary?radarlayout::place(snap.width,snap.height,client.right,client.bottom,minimapScale,minimapPosition,minimapX,minimapY,aircraftButtonBox,aspect,minimapAlignX,minimapAlignY,native.width,native.height):radarlayout::Layout{};
 if(primary&&!layout.valid){primary=false;hidePrimary();}
 radarlayout::Box secondaryBox;
 if(secondary){
  secondaryBox=secondaryradar::place(target.work.left,target.work.top,target.work.right,target.work.bottom,
                                     minimapSecondaryX,minimapSecondaryY,minimapSecondaryWidth,aspect);
  if(secondaryBox.w<3||secondaryBox.h<3){secondary=false;hideSecondary();}
 }
 if(!primary&&!secondary){worker.pause();frame.reset();return;}
 // Render once at the largest visible panel size, then present that completed
 // image to each window. Enlarging the desktop panel does not stretch the
 // smaller game-panel raster into a blurry preview.
 int width=std::max(primary?layout.destination.w:0,secondary?secondaryBox.w:0);
 int height=std::max(primary?layout.destination.h:0,secondary?secondaryBox.h:0);
 worker.set({pid,displayMatch(),informationFPS(),uint32_t(extra.renderReveal||extra.mapStrategy==3),minimapMode,std::min(2048,width-2),std::min(2048,height-2)});
 frame=worker.get();if(!frame||!frame->terrain){cancelDrag();hidePrimary();hideSecondary();return;}if(dragging&&dragMatch!=commandMatch())cancelDrag();
 if(primary){auto d=layout.destination;placePanel(false,origin.x+d.x,origin.y+d.y,d.w,d.h);}
 if(secondary)placePanel(true,secondaryBox.x,secondaryBox.y,secondaryBox.w,secondaryBox.h);
}
}

// The game and second-display panels consume the same immutable watch frames.
#include "watch_panel.h"
#include "../../shared/source/secondary_display.h"
namespace watchwindows {
static HWND windows[3]{},secondaryWindows[3]{};static watchpanel::Buffer buffers[3],secondaryBuffers[3];
static overlayui::Recovery recovery[3],secondaryRecovery[3];static bool paintPending[6]{};
static watchpresentation::Visual visual(const watchhost::View&view,int w,int h){
 watchpresentation::Visual v;v.sourceWidth=view.pixelWidth;v.sourceHeight=view.pixelHeight;v.width=w;v.height=h;v.gpu=bool(view.gpu);v.dangerous=view.frame.dangerous;
 if(view.gpu&&view.gpu->resource){v.image=view.gpu->resource;v.sequence=view.gpu->resource->sequence;}else v.image=view.pixels;return v;
}
// Each HWND owns its composition target; all presenters borrow the adapter's
// immediate context with a zero-wait lock. Published scenes stay immutable.
struct GpuPanel {
 gpupresent::Compositor compositor;watchpresentation::Gate gate;watchpresentation::Metric presents;bool active=false,pending=false;
 void cpu(HWND window){
  if(!active){pending=false;return;}compositor.reset();
  const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_NOREDIRECTIONBITMAP;
  SetWindowLongPtrW(window,GWL_EXSTYLE,style&~WS_EX_LAYERED);
  SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
  active=pending=false;gate.invalidate();
 }
 void reset(){compositor.reset();active=pending=false;gate.invalidate();}
 bool needs(const watchhost::View&view,int w,int h)const{return !active||pending||gate.needs(visual(view,w,h));}
 bool present(HWND window,const watchhost::View&view,int width,int height){
  const auto key=visual(view,width,height);if(active&&!pending&&!gate.needs(key))return true;
  const double started=watchpresentation::now();
  if(!active){SetWindowLongPtrW(window,GWL_EXSTYLE,(GetWindowLongPtrW(window,GWL_EXSTYLE)&~WS_EX_LAYERED)|WS_EX_NOREDIRECTIONBITMAP);active=true;}
  pending=!compositor.presentFrame(window,view.gpu,width,height,view.frame.dangerous?0xffffa73eu:0xff69c3e1u);
  presents.add(watchpresentation::now()-started,!pending);if(!pending)gate.committed(key);
  if(pending&&compositor.reason()!="GPU frame not ready")gpupresent::available()=false;
  return !pending;
 }
};
static GpuPanel gpuPanels[6];
static void reportTimings(){
 static DWORD reported=GetTickCount();const DWORD now=GetTickCount();if(DWORD(now-reported)<5000)return;reported=now;
 watchpresentation::Metric stage,copy,gpu;auto collect=[](watchpresentation::Metric&to,watchpresentation::Metric&from){to.count+=from.count;to.failed+=from.failed;to.total+=from.total;to.maximum=std::max(to.maximum,from.maximum);from={};};
 for(int i=0;i<3;++i){collect(stage,buffers[i].stages);collect(stage,secondaryBuffers[i].stages);collect(copy,buffers[i].copies);collect(copy,secondaryBuffers[i].copies);collect(gpu,gpuPanels[i].presents);collect(gpu,gpuPanels[i+3].presents);}
 if(!stage.count&&!copy.count&&!gpu.count)return;wchar_t line[512];swprintf(line,512,L"watch UI stage n=%llu fail=%llu ms=%.2f/max%.2f copy n=%llu fail=%llu ms=%.2f/max%.2f gpu-present n=%llu fail=%llu ms=%.2f/max%.2f",(unsigned long long)stage.count,(unsigned long long)stage.failed,stage.average(),stage.maximum,(unsigned long long)copy.count,(unsigned long long)copy.failed,copy.average(),copy.maximum,(unsigned long long)gpu.count,(unsigned long long)gpu.failed,gpu.average(),gpu.maximum);critical(line);
}
static secondarydisplay::Monitor secondaryMonitor;
static HWND dragWindow=nullptr;static HMONITOR dragMonitor=nullptr;static RECT dragWork{};
static POINT dragCursor{},dragGroup{},dragCurrent{},dragOffsets[3]{};
static int dragWidth=0,dragHeight=0,dragCount=0;
static bool dragging=false;
static bool paintingVerified=false;
static void cancelDrag(){
 if(!dragging)return;HWND held=dragWindow;dragging=false;dragWindow=nullptr;dragMonitor=nullptr;
 if(GetCapture()==held)ReleaseCapture();
}
static bool battlefield(){return pid&&snap.renderStatus==1&&external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}).visible();}
static bool usable(){return enabled&&extra.watchEnabled&&foregroundGame()&&!snap.chat&&battlefield();}
static bool secondaryUsable(){return extra.watchSecondary&&battlefield();}
static void hidePrimary(){for(int i=0;i<3;++i){watchhost::demand(i,false,0,0);recovery[i].suspend();if(windows[i])ShowWindow(windows[i],SW_HIDE);}}
static void hideSecondary(){cancelDrag();for(int i=0;i<3;++i){watchhost::demand(i,true,0,0);secondaryRecovery[i].suspend();if(secondaryWindows[i])ShowWindow(secondaryWindows[i],SW_HIDE);}}
static void hide(){hidePrimary();hideSecondary();}
static void invalidateDisplays(){cancelDrag();secondaryMonitor.invalidate();}
static void destroy(){hide();for(int i=0;i<3;++i){gpuPanels[i].reset();gpuPanels[i+3].reset();if(windows[i])DestroyWindow(windows[i]);if(secondaryWindows[i])DestroyWindow(secondaryWindows[i]);windows[i]=secondaryWindows[i]=nullptr;buffers[i].clear();secondaryBuffers[i].clear();}watchhost::reset();}
static void occlude(Graphics&g){
 for(int i=0;i<3;++i)if(windows[i]&&IsWindowVisible(windows[i])&&watchhost::views[i].shown){auto b=watchhost::views[i].box;g.SetClip(RectF(float(b.x-1),float(b.y-1),float(b.w+2),float(b.h+2)),CombineModeExclude);}
}
static void navigate(int slot){
 if(slot<0||slot>=3||!enabled||!foregroundGame()||snap.chat||(!extra.watchEnabled&&!extra.watchSecondary)||!battlefield()||!shared||!commandFresh(GetTickCount()))return;
 auto fresh=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};
 auto&track=watchhost::tracks[slot];external::watch::Target target;
 if(track){watchhost::followTracks(fresh,slot);target=track.target;}else target=watchhost::views[slot].frame.focus;
 auto center=track?external::watch::Anchor{track.spatial,track.x,track.y,track.z,track.located,track.spatialKind,track.discoveryFrame,track.searched,track.discoveryCursor}:watchhost::views[slot].frame.center;
 int x,y;if(!external::watch::camera(fresh,target,snap.width,snap.height,x,y,center))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);wakeGame();
}
static bool beginDrag(HWND w){
 if(!secondaryUsable())return false;
 auto target=secondaryMonitor.get(game,ui,GetTickCount());if(!target.available())return false;
 RECT group{target.work.right,target.work.bottom,target.work.left,target.work.top};int count=0,expected=0;
 for(const auto&view:watchhost::views)expected+=view.shown;
 for(int i=0;i<3;++i)if(watchhost::views[i].shown&&secondaryWindows[i]&&IsWindowVisible(secondaryWindows[i])){
  RECT r{};if(!GetWindowRect(secondaryWindows[i],&r))return false;
  group.left=std::min(group.left,r.left);group.top=std::min(group.top,r.top);
  group.right=std::max(group.right,r.right);group.bottom=std::max(group.bottom,r.bottom);++count;
 }
 if(count!=expected||!count||group.right-group.left>target.work.right-target.work.left||
    group.bottom-group.top>target.work.bottom-target.work.top||!GetCursorPos(&dragCursor))return false;
 for(int i=0;i<3;++i)if(watchhost::views[i].shown){RECT r{};GetWindowRect(secondaryWindows[i],&r);dragOffsets[i]={r.left-group.left,r.top-group.top};}
 dragMonitor=target.monitor;dragWork=target.work;dragWindow=w;dragGroup=dragCurrent={group.left,group.top};
 dragWidth=group.right-group.left;dragHeight=group.bottom-group.top;dragCount=count;dragging=true;SetCapture(w);
 if(GetCapture()!=w){cancelDrag();return false;}return true;
}
static void moveDrag(){
 if(!dragging)return;
 POINT cursor{};if(!GetCursorPos(&cursor))return;
 int workW=dragWork.right-dragWork.left,workH=dragWork.bottom-dragWork.top;
 int margin=std::min({12,(workW-1)/2,(workH-dragCount)/2});
 int minX=dragWork.left+margin,maxX=dragWork.right-margin-dragWidth;
 int minY=dragWork.top+margin,maxY=dragWork.bottom-margin-dragHeight;
 int x=int(std::clamp(int64_t(dragGroup.x)+int64_t(cursor.x)-dragCursor.x,int64_t(minX),int64_t(std::max(minX,maxX))));
 int y=int(std::clamp(int64_t(dragGroup.y)+int64_t(cursor.y)-dragCursor.y,int64_t(minY),int64_t(std::max(minY,maxY))));
 if(x==dragCurrent.x&&y==dragCurrent.y)return;
 bool placed=true;
 for(int i=0;i<3;++i)if(watchhost::views[i].shown&&secondaryWindows[i])
  placed=SetWindowPos(secondaryWindows[i],nullptr,x+dragOffsets[i].x,y+dragOffsets[i].y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE&&placed;
 if(!placed){cancelDrag();return;}
 dragCurrent={x,y};
}
static void finishDrag(){
 if(!dragging)return;moveDrag();if(!dragging)return;
 if(dragCurrent.x==dragGroup.x&&dragCurrent.y==dragGroup.y){cancelDrag();return;}
 int workW=dragWork.right-dragWork.left,workH=dragWork.bottom-dragWork.top;
 int margin=std::min({12,(workW-1)/2,(workH-dragCount)/2});
 int freeX=workW-2*margin-dragWidth,freeY=workH-2*margin-dragHeight;
 int x=freeX>0?int((int64_t(dragCurrent.x-dragWork.left-margin)*10000+freeX/2)/freeX):extra.watchSecondaryX;
 int y=freeY>0?int((int64_t(dragCurrent.y-dragWork.top-margin)*10000+freeY/2)/freeY):extra.watchSecondaryY;
 x=std::clamp(x,0,10000);y=std::clamp(y,0,10000);
 bool changed=x!=extra.watchSecondaryX||y!=extra.watchSecondaryY;
 extra.watchSecondaryX=x;extra.watchSecondaryY=y;cancelDrag();
 if(changed){changedSetting();syncSecondaryLayoutControls();}
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCCREATE){auto*create=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));return TRUE;}
 int panel=int(GetWindowLongPtrW(w,GWLP_USERDATA))-1,slot=panel%3;bool second=panel>=3;
 if(panel<0||panel>=6)return DefWindowProcW(w,msg,wp,lp);
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(msg==WM_SETCURSOR){
  if(second){
   POINT cursor{};RECT bounds{};GetClientRect(w,&bounds);
   bool inHandle=GetCursorPos(&cursor)&&ScreenToClient(w,&cursor)&&
       cursor.x>=0&&cursor.x<bounds.right&&cursor.y>=0&&cursor.y<22;
   SetCursor(LoadCursorW(nullptr,(inHandle||(dragging&&dragWindow==w))?IDC_SIZEALL:IDC_ARROW));return TRUE;
  }
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){
  if(second&&short(HIWORD(lp))>=0&&short(HIWORD(lp))<22&&beginDrag(w))return 0;
  navigate(slot);return 0;
 }
 if(second&&msg==WM_MOUSEMOVE&&dragging&&dragWindow==w){if(!(wp&MK_LBUTTON))cancelDrag();else moveDrag();return 0;}
 if(second&&msg==WM_LBUTTONUP&&dragging&&dragWindow==w){finishDrag();return 0;}
 if(second&&(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE)){cancelDrag();return 0;}
 if(msg==WM_RBUTTONDOWN){if(second&&dragging)cancelDrag();if((wp&MK_SHIFT)&&(second?secondaryUsable():usable())){watchhost::close(slot);if(windows[slot])ShowWindow(windows[slot],SW_HIDE);if(secondaryWindows[slot])ShowWindow(secondaryWindows[slot],SW_HIDE);recovery[slot].suspend();secondaryRecovery[slot].suspend();}return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK||msg==WM_RBUTTONDBLCLK||msg==WM_MOUSEMOVE)return 0;
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
  // The owner already read and verified the tactical display immediately
  // before a synchronous UpdateWindow. An unrelated WM_PAINT still checks
  // the live display gate before it can draw a stale game scene.
  bool eligible=paintingVerified?(second?extra.watchSecondary:enabled&&extra.watchEnabled&&foregroundGame()&&!snap.chat):(second?secondaryUsable():usable());
  if(eligible&&watchhost::views[slot].shown){
   auto&view=watchhost::views[slot];auto&gpu=gpuPanels[panel];auto&buffer=second?secondaryBuffers[slot]:buffers[slot];
   if(view.gpu){if(!gpu.active)buffer.clear();paintPending[panel]=!gpu.present(w,view,r.right,r.bottom);}
   else{gpu.cpu(w);const bool staged=buffer.stage(dc,view.pixels,visual(view,r.right,r.bottom));const bool copied=buffer.present(dc,r.right,r.bottom);paintPending[panel]=!staged||!copied;}
  }else ShowWindow(w,SW_HIDE);
  EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void placePanel(int slot,bool second,int x,int y,int w,int h){
 watchhost::demand(slot,second,w,h);
 auto&view=watchhost::views[slot];auto&window=second?secondaryWindows[slot]:windows[slot];auto&keep=second?secondaryRecovery[slot]:recovery[slot];const int panel=slot+(second?3:0);
 if(!view.shown||w<1||h<1){keep.suspend();if(window)ShowWindow(window,SW_HIDE);return;}
 if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2TrackingWindow137";RegisterClassW(&cls);
  window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,second?L"第二显示器单位跟踪":L"单位跟踪",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,reinterpret_cast<void*>(intptr_t(slot+1+(second?3:0))));if(window)SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
 }if(!window)return;
 RECT old{};GetWindowRect(window,&old);bool resized=old.right-old.left!=w||old.bottom-old.top!=h;bool changed=old.left!=x||old.top!=y||resized;
 bool repair=keep.due(GetTickCount(),true,{uintptr_t(game),pid,displayMatch(),w,h},IsWindowVisible(window)!=FALSE);
 if(changed||repair)keep.submitted(overlaystack::place(window,overlaystack::Layer::Watch,x,y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE);
 auto&gpu=gpuPanels[panel];auto&buffer=second?secondaryBuffers[slot]:buffers[slot];if(repair)gpu.gate.invalidate();
 const bool newImage=view.gpu?gpu.needs(view,w,h):buffer.needs(visual(view,w,h));
 if(resized||repair||newImage||paintPending[panel]){InvalidateRect(window,nullptr,FALSE);paintingVerified=true;UpdateWindow(window);paintingVerified=false;}
}
static void update(POINT origin,const RECT&client,bool verifiedBattlefield){
 reportTimings();
 if(verifiedBattlefield&&enabled&&extra.watchEnabled&&foregroundGame()&&!snap.chat&&client.right>0&&client.bottom>0&&snap.width>0&&snap.height>0){
  for(int i=0;i<3;++i){auto b=watchhost::views[i].box;placePanel(i,false,origin.x+int(int64_t(b.x)*client.right/snap.width),origin.y+int(int64_t(b.y)*client.bottom/snap.height),int(int64_t(b.w)*client.right/snap.width),int(int64_t(b.h)*client.bottom/snap.height));}
 }else hidePrimary();
 auto target=extra.watchSecondary?secondaryMonitor.get(game,ui,GetTickCount()):secondarydisplay::Target{};
 if(!verifiedBattlefield||!extra.watchSecondary||!target.available()){hideSecondary();return;}
 if(dragging){
  int count=0;for(int i=0;i<3;++i)if(watchhost::views[i].shown){++count;if(!secondaryWindows[i]||!IsWindowVisible(secondaryWindows[i])){count=-10;break;}}
  if(count==dragCount&&target.monitor==dragMonitor&&target.work.left==dragWork.left&&target.work.top==dragWork.top&&
     target.work.right==dragWork.right&&target.work.bottom==dragWork.bottom){
   // Keep drawing new frames while the group is being dragged.
   for(int i=0;i<3;++i)if(watchhost::views[i].shown){RECT r{};if(GetWindowRect(secondaryWindows[i],&r))placePanel(i,true,r.left,r.top,r.right-r.left,r.bottom-r.top);}
   return;
  }
  cancelDrag();
 }
 int count=0;for(const auto&view:watchhost::views)count+=view.shown;
 if(!count){hideSecondary();return;}
 const int workW=target.work.right-target.work.left,workH=target.work.bottom-target.work.top;
 if(workW<1||workH<count){hideSecondary();return;}
 const int margin=std::min({12,(workW-1)/2,(workH-count)/2});
 const int usableW=workW-2*margin,usableH=workH-2*margin;
 const int availableW=std::max(1,std::min(workW*2/5-2*margin,usableW));
 const int availableH=std::max(count,std::min(workH*2/3-2*margin,usableH));
 const int gap=count>1?std::min(8,(availableH-count)/(count-1)):0;
 const int contentH=availableH-(count-1)*gap;
 const int width=std::max(1,std::min({640,availableW,int(int64_t(contentH)*watchhost::Width/(count*watchhost::Height))}));
 const int height=std::max(1,std::min(int(int64_t(width)*watchhost::Height/watchhost::Width),contentH/count));
 const int groupH=count*height+(count-1)*gap;
 // Ten-thousandths travel across the free work area, so 10000/0 preserves the
 // existing upper-right placement and all three panels remain on-screen.
 const int x=target.work.left+margin+int(int64_t(usableW-width)*std::clamp(extra.watchSecondaryX,0,10000)/10000);
 const int y=target.work.top+margin+int(int64_t(usableH-groupH)*std::clamp(extra.watchSecondaryY,0,10000)/10000);
 int row=0;
 for(int i=0;i<3;++i){if(watchhost::views[i].shown){placePanel(i,true,x,y+row*(height+gap),width,height);++row;}else placePanel(i,true,0,0,0,0);}
}
}

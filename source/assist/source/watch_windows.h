// The game and second-display panels consume the same immutable watch frames.
#include "construction_buffer.h"
#include "../../shared/source/secondary_display.h"
namespace watchwindows {
static HWND windows[3]{},secondaryWindows[3]{};static construction::PanelBuffer buffers[3],secondaryBuffers[3];
static overlayui::Recovery recovery[3],secondaryRecovery[3];static uint32_t revisions[3]{},secondaryRevisions[3]{};
static secondarydisplay::Monitor secondaryMonitor;
static bool battlefield(){return pid&&snap.renderStatus==1&&external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}).visible();}
static bool usable(){return enabled&&extra.watchEnabled&&foregroundGame()&&!snap.chat&&battlefield();}
static bool secondaryUsable(){return extra.watchSecondary&&battlefield();}
static void hidePrimary(){for(int i=0;i<3;++i){recovery[i].suspend();if(windows[i])ShowWindow(windows[i],SW_HIDE);}}
static void hideSecondary(){for(int i=0;i<3;++i){secondaryRecovery[i].suspend();if(secondaryWindows[i])ShowWindow(secondaryWindows[i],SW_HIDE);}}
static void hide(){hidePrimary();hideSecondary();}
static void invalidateDisplays(){secondaryMonitor.invalidate();}
static void destroy(){hide();for(int i=0;i<3;++i){if(windows[i])DestroyWindow(windows[i]);if(secondaryWindows[i])DestroyWindow(secondaryWindows[i]);windows[i]=secondaryWindows[i]=nullptr;buffers[i].clear();secondaryBuffers[i].clear();}watchhost::reset();}
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
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCCREATE){auto*create=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));return TRUE;}
 int panel=int(GetWindowLongPtrW(w,GWLP_USERDATA))-1,slot=panel%3;bool second=panel>=3;
 if(panel<0||panel>=6)return DefWindowProcW(w,msg,wp,lp);
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(msg==WM_SETCURSOR){
  if(second){SetCursor(LoadCursorW(nullptr,IDC_ARROW));return TRUE;}
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){navigate(slot);return 0;}
 if(msg==WM_RBUTTONDOWN){if((wp&MK_SHIFT)&&(second?secondaryUsable():usable())){watchhost::close(slot);if(windows[slot])ShowWindow(windows[slot],SW_HIDE);if(secondaryWindows[slot])ShowWindow(secondaryWindows[slot],SW_HIDE);recovery[slot].suspend();secondaryRecovery[slot].suspend();}return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK||msg==WM_RBUTTONDBLCLK||msg==WM_MOUSEMOVE)return 0;
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
  if((second?secondaryUsable():usable())&&watchhost::views[slot].shown){auto&buffer=second?secondaryBuffers[slot]:buffers[slot];HDC mem=buffer.acquire(dc,r.right,r.bottom);if(mem){
    {Graphics g(mem);g.ScaleTransform(float(r.right)/watchhost::Width,float(r.bottom)/watchhost::Height);watchhost::drawView(g,watchhost::views[slot]);}buffer.present(dc);
  }}EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void placePanel(int slot,bool second,int x,int y,int w,int h){
 auto&view=watchhost::views[slot];auto&window=second?secondaryWindows[slot]:windows[slot];auto&keep=second?secondaryRecovery[slot]:recovery[slot];auto&revision=second?secondaryRevisions[slot]:revisions[slot];
 if(!view.shown||w<1||h<1){keep.suspend();if(window)ShowWindow(window,SW_HIDE);return;}
 if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2TrackingWindow137";RegisterClassW(&cls);
  window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,second?L"第二显示器单位跟踪":L"单位跟踪",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,reinterpret_cast<void*>(intptr_t(slot+1+(second?3:0))));if(window)SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
 }if(!window)return;
 RECT old{};GetWindowRect(window,&old);bool changed=old.left!=x||old.top!=y||old.right-old.left!=w||old.bottom-old.top!=h;
 bool repair=keep.due(GetTickCount(),true,{uintptr_t(game),pid,displayMatch(),w,h},IsWindowVisible(window)!=FALSE);
 if(changed||repair)keep.submitted(overlaystack::place(window,overlaystack::Layer::Watch,x,y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE);
 if(changed||repair||revision!=view.revision){revision=view.revision;InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);}
}
static void update(POINT origin,const RECT&client){
 if(usable()&&client.right>0&&client.bottom>0&&snap.width>0&&snap.height>0){
  for(int i=0;i<3;++i){auto b=watchhost::views[i].box;placePanel(i,false,origin.x+int(int64_t(b.x)*client.right/snap.width),origin.y+int(int64_t(b.y)*client.bottom/snap.height),int(int64_t(b.w)*client.right/snap.width),int(int64_t(b.h)*client.bottom/snap.height));}
 }else hidePrimary();
 auto target=extra.watchSecondary?secondaryMonitor.get(game,ui,GetTickCount()):secondarydisplay::Target{};
 if(!secondaryUsable()||!target.available()){hideSecondary();return;}
 int count=0;for(const auto&view:watchhost::views)count+=view.shown;
 if(!count){hideSecondary();return;}
 const int margin=12,gap=8,workW=target.work.right-target.work.left,workH=target.work.bottom-target.work.top;
 const int availableW=std::max(1,workW*2/5-margin*2),availableH=std::max(1,workH*2/3-margin*2);
 int width=std::max(1,std::min({640,availableW,int(int64_t(std::max(1,availableH-(count-1)*gap))*watchhost::Width/(count*watchhost::Height))}));
 int height=std::max(1,int(int64_t(width)*watchhost::Height/watchhost::Width)),row=0;
 for(int i=0;i<3;++i){if(watchhost::views[i].shown){placePanel(i,true,target.work.right-margin-width,target.work.top+margin+row*(height+gap),width,height);++row;}else placePanel(i,true,0,0,0,0);}
}
}

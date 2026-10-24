// Rectangular, non-activating windows own their input surface. They reuse the
// same assets as the formation/overlay renderer, without a transparent hitbox
// or a second cursor layered above the rendered scene.
#include "construction_buffer.h"
namespace watchwindows {
static HWND windows[3]{};static construction::PanelBuffer buffers[3];
static overlayui::Recovery recovery[3];static uint32_t revisions[3]{};
static bool usable(){return enabled&&extra.watchEnabled&&foregroundGame()&&snap.renderStatus==1&&!snap.chat&&external::display::inspect([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}).visible();}
static void hide(){for(int i=0;i<3;++i){recovery[i].suspend();if(windows[i])ShowWindow(windows[i],SW_HIDE);}}
static void destroy(){hide();for(int i=0;i<3;++i){if(windows[i])DestroyWindow(windows[i]);windows[i]=nullptr;buffers[i].clear();}watchhost::reset();}
static void occlude(Graphics&g){
 // The tactical overlay is itself topmost. Exclude the opaque camera panels
 // before drawing any badges/bubbles/previews, regardless of Windows' last
 // topmost reassertion order. Use the same game-pixel geometry as update().
 for(int i=0;i<3;++i)if(windows[i]&&IsWindowVisible(windows[i])&&watchhost::views[i].shown){auto b=watchhost::views[i].box;g.SetClip(RectF(float(b.x-1),float(b.y-1),float(b.w+2),float(b.h+2)),CombineModeExclude);}
}
static void navigate(int slot){
 if(slot<0||slot>=3||!usable()||!shared||!commandFresh(GetTickCount()))return;
 auto fresh=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};
 auto&track=watchhost::tracks[slot];external::watch::Target target;
 if(track){external::watch::follow(fresh,track);target=track.target;}else target=watchhost::views[slot].frame.focus;
 int x,y;if(!external::watch::camera(fresh,target,snap.width,snap.height,x,y))return;
 InterlockedIncrement(&shared->cameraSeq);shared->cameraMatch=commandMatch();shared->cameraTick=sampleClock(shared);shared->cameraTargetX=x;shared->cameraTargetY=y;MemoryBarrier();InterlockedIncrement(&shared->cameraSeq);wakeGame();
}
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_NCCREATE){auto*create=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));return TRUE;}
 int slot=int(GetWindowLongPtrW(w,GWLP_USERDATA))-1;if(slot<0||slot>=3)return DefWindowProcW(w,msg,wp,lp);
 if(msg==WM_NCHITTEST)return HTCLIENT;
 if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(msg==WM_SETCURSOR){
#ifdef RA2_UNIFIED
  if(!suite::hardwareCursorActive())SetCursor(nullptr);
#else
  SetCursor(nullptr);
#endif
  return TRUE;
 }
 if(msg==WM_LBUTTONDOWN){navigate(slot);return 0;}
 if(msg==WM_RBUTTONDOWN){if((wp&MK_SHIFT)&&usable()){watchhost::close(slot);ShowWindow(w,SW_HIDE);recovery[slot].suspend();}return 0;}
 if(msg==WM_LBUTTONUP||msg==WM_RBUTTONUP||msg==WM_LBUTTONDBLCLK||msg==WM_RBUTTONDBLCLK||msg==WM_MOUSEMOVE)return 0;
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
  if(usable()&&watchhost::views[slot].shown){HDC mem=buffers[slot].acquire(dc,r.right,r.bottom);if(mem){
    {Graphics g(mem);g.ScaleTransform(float(r.right)/watchhost::Width,float(r.bottom)/watchhost::Height);watchhost::drawView(g,watchhost::views[slot]);}
    buffers[slot].present(dc);
  }}EndPaint(w,&ps);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
static void update(POINT origin,const RECT&client){
 if(!usable()||client.right<1||client.bottom<1){hide();return;}
 for(int i=0;i<3;++i){auto&view=watchhost::views[i];auto&window=windows[i];auto box=view.box;
  if(!view.shown||box.w<1||box.h<1){recovery[i].suspend();if(window)ShowWindow(window,SW_HIDE);continue;}
  if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2TrackingWindow137";RegisterClassW(&cls);
   window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST|WS_EX_LAYERED,cls.lpszClassName,L"单位跟踪",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,reinterpret_cast<void*>(intptr_t(i+1)));if(window)SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);
  }if(!window)continue;
  int x=origin.x+int(int64_t(box.x)*client.right/snap.width),y=origin.y+int(int64_t(box.y)*client.bottom/snap.height),w=std::max(1,int(int64_t(box.w)*client.right/snap.width)),h=std::max(1,int(int64_t(box.h)*client.bottom/snap.height));
  RECT old{};GetWindowRect(window,&old);bool changed=old.left!=x||old.top!=y||old.right-old.left!=w||old.bottom-old.top!=h;
  bool repair=recovery[i].due(GetTickCount(),true,{uintptr_t(game),pid,commandMatch(),w,h},IsWindowVisible(window)!=FALSE);
  if(changed||repair)recovery[i].submitted(SetWindowPos(window,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE);
  if(changed||repair||revisions[i]!=view.revision){revisions[i]=view.revision;InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);}
 }
}
}

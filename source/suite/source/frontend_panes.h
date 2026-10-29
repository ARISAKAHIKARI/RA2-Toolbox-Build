#pragma once
// Settings windows have their own message loops. Never parent/own them across
// threads: Win32 would implicitly join the input queues and stall the shell
// whenever a reader/render thread is busy. Only asynchronous messages cross
// this boundary; the shell paints immutable snapshots while it is being moved.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

namespace frontendpanes {
constexpr UINT LayoutMessage=WM_APP+80,ActivityMessage=WM_APP+87,SnapshotMessage=WM_APP+88,DestroyedMessage=WM_APP+89;
constexpr UINT_PTR PaneSubclass=0x6328;
struct Composition {
 using Set=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);using Get=HRESULT(WINAPI*)(HWND,DWORD,PVOID,DWORD);
 Set set=nullptr;Get get=nullptr;
 Composition(){auto library=LoadLibraryW(L"dwmapi.dll");if(library){set=reinterpret_cast<Set>(GetProcAddress(library,"DwmSetWindowAttribute"));get=reinterpret_cast<Get>(GetProcAddress(library,"DwmGetWindowAttribute"));}}
};
inline const Composition& composition(){static const Composition api;return api;}
struct Layout {
 RECT rect{};bool visible=false;unsigned dpi=96;uint64_t order=0;
 bool operator==(const Layout& b)const{return rect.left==b.rect.left&&rect.top==b.rect.top&&rect.right==b.rect.right&&rect.bottom==b.rect.bottom&&visible==b.visible&&dpi==b.dpi&&order==b.order;}
};
struct Snapshot {
 int width=0,height=0;std::vector<uint32_t> pixels;
 void paint(HDC dc,const RECT& r)const{
  if(width<=0||height<=0||r.right<=r.left||r.bottom<=r.top)return;
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
  info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  SetStretchBltMode(dc,COLORONCOLOR);
  StretchDIBits(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,0,0,width,height,pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);
 }
};
class Host {
 struct Slot {
  std::atomic<HWND> window{nullptr};std::mutex mutex;Layout latest{},applied{};
  bool pending=false,processing=false,appliedValid=false;Layout executing{};std::shared_ptr<const Snapshot> snapshot;
  Host* host=nullptr;unsigned index=0;bool snapshotPending=false,capturing=false;unsigned dpi=0;
  // After adopt publishes the HWND, only the shell thread changes composition
  // visibility. A worker can never uncloak an obsolete frame during a new drag.
  bool cloakKnown=false,cloaked=false,cloakSupported=false;
 };
 std::array<Slot,3> slots_{};HWND shell_=nullptr;std::atomic<bool> interactive_{false},closing_{false};
 const Composition* compositionOverride_=nullptr;
 const Composition& compositor()const{return compositionOverride_?*compositionOverride_:composition();}
 Slot* slot(HWND w){for(auto& s:slots_)if(s.window.load()==w)return &s;return nullptr;}
 bool cloak(Slot& s,bool value){
  // DWM/User32 may synchronously coordinate with the owning UI thread. That
  // thread also consumes this slot's mailbox. Never hold its mutex across an
  // external window API; only the shell changes cloak state after publication.
  if(s.cloakKnown&&s.cloaked==value)return s.cloakSupported;
  auto set=compositor().set;BOOL flag=value?TRUE:FALSE;auto w=s.window.load();
  bool ok=w&&set&&SUCCEEDED(set(w,13 /* DWMWA_CLOAK */,&flag,sizeof flag));
  s.cloakSupported=ok;s.cloakKnown=ok;if(ok)s.cloaked=value;return ok;
 }
 static LRESULT CALLBACK paneProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR data){
  auto& s=*reinterpret_cast<Slot*>(data);auto& host=*s.host;
  if(m==SnapshotMessage){s.snapshotPending=false;host.capture(s);return 0;}
  if(m==WM_NCDESTROY){
   {std::lock_guard<std::mutex> lock(s.mutex);s.window.store(nullptr);s.pending=false;s.appliedValid=false;s.snapshot.reset();}
   RemovePropW(w,L"RA2.Suite.Managed");RemoveWindowSubclass(w,paneProc,id);
   PostMessageW(host.shell_,DestroyedMessage,s.index,(LPARAM)w);
  }
  auto result=DefSubclassProc(w,m,a,b);
  if(m==WM_ACTIVATE||m==WM_SHOWWINDOW)PostMessageW(host.shell_,ActivityMessage,s.index,0);
  // A frozen settings image is refreshed after a user edit/layout, not at the
  // game/render cadence. The capture always runs in this window's own thread.
  if(m==WM_COMMAND||m==WM_VSCROLL||m==WM_HSCROLL||m==WM_MOUSEWHEEL||m==WM_APP+86)host.requestSnapshot(s);
  return result;
 }
 void requestSnapshot(Slot& s){
  if(s.snapshotPending||s.capturing)return;s.snapshotPending=true;
  if(!PostMessageW(s.window.load(),SnapshotMessage,0,0))s.snapshotPending=false;
 }
 void capture(Slot& s){
  HWND w=s.window.load();if(!w||s.capturing||!IsWindowVisible(w))return;
  RECT r{};if(!GetWindowRect(w,&r))return;int width=r.right-r.left,height=r.bottom-r.top;
  // Retain the last complete image on allocation failure or pathological sizes.
  if(width<=0||height<=0||uint64_t(width)*uint64_t(height)>16u*1024u*1024u)return;
  s.capturing=true;
  HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  void* bits=nullptr;HBITMAP bitmap=dc?CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0):nullptr;
  if(bitmap&&bits){auto old=SelectObject(dc,bitmap);RECT all{0,0,width,height};HBRUSH background=CreateSolidBrush(RGB(18,24,34));FillRect(dc,&all,background);DeleteObject(background);
   // This is an in-thread call. The shell never calls PrintWindow/SendMessage
   // on a reader window and never waits for a fresh capture before moving.
   SendMessageW(w,WM_PRINT,(WPARAM)dc,PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
   GdiFlush();
   try{auto image=std::make_shared<Snapshot>();image->width=width;image->height=height;
    image->pixels.resize(size_t(width)*size_t(height));std::memcpy(image->pixels.data(),bits,image->pixels.size()*sizeof(uint32_t));
    {std::lock_guard<std::mutex> lock(s.mutex);s.snapshot=std::move(image);}
    PostMessageW(shell_,ActivityMessage,s.index,0);
   }catch(...){/* Keep the prior complete image. */}
   SelectObject(dc,old);
  }
  if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);s.capturing=false;
 }
public:
 explicit Host(const Composition* api=nullptr):compositionOverride_(api){}
 void shell(HWND window){shell_=window;}
 HWND shell()const{return shell_;}
 HWND window(unsigned index)const{return index<slots_.size()?slots_[index].window.load():nullptr;}
 // Serialize publication with WM_NCDESTROY retirement. A cached HWND may have
 // been reused by another window by the time a later shell message is handled.
 bool post(unsigned index,UINT message,WPARAM a=0,LPARAM b=0){
  if(index>=slots_.size())return false;auto& s=slots_[index];std::lock_guard<std::mutex> lock(s.mutex);
  auto w=s.window.load();return w&&PostMessageW(w,message,a,b);
 }
 bool managed(HWND w)const{return GetPropW(w,L"RA2.Suite.Managed")!=nullptr;}
 bool contains(HWND w)const{
  if(!w)return false;auto root=GetAncestor(w,GA_ROOTOWNER);if(root==shell_||w==shell_)return true;
  for(const auto& s:slots_)if(auto pane=s.window.load())if(root==pane||w==pane)return true;
  return false;
 }
 bool canPresent()const{return !interactive_.load()&&!closing_.load()&&shell_&&IsWindowVisible(shell_)&&!IsIconic(shell_)&&contains(GetForegroundWindow());}
 bool interactive()const{return interactive_.load();}
 void beginInteraction(){interactive_.store(true);for(auto& s:slots_)if(s.window.load())cloak(s,true);}
 void endInteraction(){interactive_.store(false);}
 void closing(){closing_.store(true);for(auto& s:slots_)if(s.window.load())cloak(s,true);}
 void adopt(unsigned index,HWND w){
  if(index>=slots_.size()||!w)return;auto& s=slots_[index];s.host=this;s.index=index;
  // All calls here execute on the module thread, before its message loop starts.
  // No cross-thread parent OR owner relationship may be introduced.
  auto style=GetWindowLongPtrW(w,GWL_STYLE);
  style&=~LONG_PTR(WS_CHILD|WS_VISIBLE|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME);
  SetWindowLongPtrW(w,GWL_STYLE,style|WS_POPUP|WS_CLIPCHILDREN);
  auto ex=GetWindowLongPtrW(w,GWL_EXSTYLE);SetWindowLongPtrW(w,GWL_EXSTYLE,(ex&~LONG_PTR(WS_EX_APPWINDOW))|WS_EX_TOOLWINDOW);
  SetWindowLongPtrW(w,GWLP_HWNDPARENT,0);
  SetPropW(w,L"RA2.Suite.Managed",reinterpret_cast<HANDLE>(1));SetWindowSubclass(w,paneProc,PaneSubclass,reinterpret_cast<DWORD_PTR>(&s));
  SetWindowPos(w,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
  // Before publication the module owns initial concealment; afterwards the
  // shell is the single cloak/uncloak owner. No GUI input queues are attached.
  if(auto set=compositor().set){BOOL value=TRUE;s.cloakSupported=SUCCEEDED(set(w,13,&value,sizeof value));s.cloakKnown=s.cloakSupported;s.cloaked=s.cloakSupported;}
  s.window.store(w);
 }
 bool request(unsigned index,const Layout& wanted){
  if(index>=slots_.size())return false;auto& s=slots_[index];
  if(!wanted.visible)cloak(s,true);
  std::lock_guard<std::mutex> lock(s.mutex);
  auto w=s.window.load();if(!w)return false;
  s.latest=wanted;
  if(s.pending)return true;
  if(s.processing&&s.executing==wanted)return true;
  // Hidden worker panels need no intermediate resize/DPI work while the shell
  // is moving. The next visible request carries the final complete geometry.
  if(!s.processing&&!wanted.visible&&s.appliedValid&&!s.applied.visible)return true;
  if(!s.processing&&s.appliedValid&&s.applied==wanted)return true;
  if(!PostMessageW(w,LayoutMessage,0,0))return false;s.pending=true;return true;
 }
 bool apply(HWND w,UINT message,UINT dpiMessage){
  if(message!=LayoutMessage)return false;auto* s=slot(w);if(!s)return true;Layout next;
  {std::lock_guard<std::mutex> lock(s->mutex);if(!s->pending)return true;next=s->latest;s->pending=false;s->processing=true;s->executing=next;}
  bool show=next.visible&&canPresent();
  if(!show){
   // DWM already concealed this surface from the shell thread. Capture while
   // WS_VISIBLE still permits native controls to print, then hide it normally.
   if(IsWindowVisible(w)){capture(*s);ShowWindow(w,SW_HIDE);}
   next.visible=false;
  }else{
   if(s->dpi!=next.dpi){s->dpi=next.dpi;SendMessageW(w,dpiMessage,next.dpi,0);}
   RECT current{};GetWindowRect(w,&current);UINT flags=SWP_NOACTIVATE|SWP_NOOWNERZORDER;
   if(current.left==next.rect.left&&current.top==next.rect.top&&current.right==next.rect.right&&current.bottom==next.rect.bottom)flags|=SWP_NOMOVE|SWP_NOSIZE;
   // The check is repeated immediately before show: a queued old request must
   // never bring a settings panel in front of the game/another foreground app.
   if(canPresent()){
    SetWindowPos(w,HWND_TOP,next.rect.left,next.rect.top,next.rect.right-next.rect.left,next.rect.bottom-next.rect.top,flags|SWP_SHOWWINDOW);
    // Finish the real surface and a full frozen frame in this worker thread.
    // Completion is acknowledged only afterwards; the shell alone uncloaks.
    RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);capture(*s);
   }else{ShowWindow(w,SW_HIDE);next.visible=false;}
  }
  {std::lock_guard<std::mutex> lock(s->mutex);s->applied=next;s->appliedValid=true;s->processing=false;}
  PostMessageW(shell_,ActivityMessage,s->index,0);
  return true;
 }
 // Shell-thread only. A worker completion may arrive after many newer moves:
 // compare the whole requested/applied state, including activation order, and
 // refuse stale completions. Worker code never calls this function.
 void present(){
  bool allowed=canPresent();
  for(auto& s:slots_)if(s.window.load()){
   bool show=false;{std::lock_guard<std::mutex> lock(s.mutex);show=allowed&&!s.pending&&!s.processing&&s.appliedValid&&s.latest.visible&&s.applied==s.latest;}
   cloak(s,!show);
  }
 }
 bool synchronousConcealment(unsigned index)const{return index<slots_.size()&&slots_[index].cloakSupported;}
 bool queryCloaked(unsigned index,bool& value)const{
  if(index>=slots_.size())return false;auto get=compositor().get;DWORD flags=0;auto w=slots_[index].window.load();
  if(!w||!get||FAILED(get(w,14 /* DWMWA_CLOAKED */,&flags,sizeof flags)))return false;
  value=(flags&1u)!=0;return true;
 }
 std::shared_ptr<const Snapshot> snapshot(unsigned index){
  if(index>=slots_.size())return {};auto& s=slots_[index];std::lock_guard<std::mutex> lock(s.mutex);return s.snapshot;
 }
 void paint(HDC dc,unsigned index,const RECT& target){if(auto image=snapshot(index))image->paint(dc,target);}
};
}

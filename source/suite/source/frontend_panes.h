#pragma once
// Settings HWNDs stay on independent UI threads. They are real input surfaces,
// never DWM-cloaked controls behind a picture in the shell. Only asynchronous
// layout mailboxes cross threads; no parent/owner input-queue attachment.
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
#include <mutex>
namespace frontendpanes {
constexpr UINT LayoutMessage=WM_APP+80,ActivityMessage=WM_APP+87,DestroyedMessage=WM_APP+89;
constexpr UINT CategoryMessage=WM_APP+86;
constexpr UINT_PTR PaneSubclass=0x6328;
struct Layout {
 RECT rect{};bool visible=false;unsigned dpi=96;uint64_t order=0;int category=-1;
 bool operator==(const Layout& b)const{return rect.left==b.rect.left&&rect.top==b.rect.top&&rect.right==b.rect.right&&rect.bottom==b.rect.bottom&&visible==b.visible&&dpi==b.dpi&&order==b.order&&category==b.category;}
};
class Host {
 struct Slot {
  std::atomic<HWND> window{nullptr};std::mutex mutex;Layout latest{},applied{},executing{};
  bool pending=false,processing=false,appliedValid=false;Host*host=nullptr;unsigned index=0,dpi=0;
 };
 std::array<Slot,3>slots_{};HWND shell_=nullptr;std::atomic<bool>interactive_{false},closing_{false};
 Slot*slot(HWND w){for(auto&s:slots_)if(s.window.load()==w)return &s;return nullptr;}
 static LRESULT CALLBACK paneProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR data){
  auto&s=*reinterpret_cast<Slot*>(data);auto&host=*s.host;
  if(m==WM_NCDESTROY){
   {std::lock_guard<std::mutex>lock(s.mutex);s.window=nullptr;s.pending=false;s.appliedValid=false;}
   RemovePropW(w,L"RA2.Suite.Managed");RemoveWindowSubclass(w,paneProc,id);
   PostMessageW(host.shell_,DestroyedMessage,s.index,(LPARAM)w);
  }
  auto result=DefSubclassProc(w,m,a,b);
  if(m==WM_ACTIVATE||m==WM_SHOWWINDOW)PostMessageW(host.shell_,ActivityMessage,s.index,0);
  return result;
 }
public:
 void shell(HWND w){shell_=w;}HWND shell()const{return shell_;}
 HWND window(unsigned i)const{return i<slots_.size()?slots_[i].window.load():nullptr;}
 bool post(unsigned i,UINT m,WPARAM a=0,LPARAM b=0){
  if(i>=slots_.size())return false;auto&s=slots_[i];std::lock_guard<std::mutex>lock(s.mutex);
  auto w=s.window.load();return w&&PostMessageW(w,m,a,b);
 }
 bool managed(HWND w)const{return GetPropW(w,L"RA2.Suite.Managed")!=nullptr;}
 bool contains(HWND w)const{
  if(!w)return false;auto root=GetAncestor(w,GA_ROOTOWNER);if(root==shell_||w==shell_)return true;
  for(const auto&s:slots_)if(auto pane=s.window.load())if(root==pane||w==pane)return true;return false;
 }
 // Focus only controls z-order. Alt-Tab must never turn a live page into a
 // hidden input surface and leave its shell looking like an operable page.
 bool canPresent()const{return !interactive_.load()&&!closing_.load()&&shell_&&IsWindowVisible(shell_)&&!IsIconic(shell_);}
 bool canRaise()const{return canPresent()&&contains(GetForegroundWindow());}
 static bool above(HWND pane,HWND shell){
  // Restoring/activating the shell can raise it after an earlier pane layout
  // completed. WS_VISIBLE alone says nothing about which HWND wins hit tests.
  HWND at=GetWindow(shell,GW_HWNDPREV);
  for(unsigned depth=0;at&&depth<4096;++depth,at=GetWindow(at,GW_HWNDPREV))if(at==pane)return true;
  return false;
 }
 bool interactive()const{return interactive_.load();}
 void beginInteraction(){interactive_=true;}
 void endInteraction(){interactive_=false;}
 void reconcileInteraction(){
  if(!interactive_)return;GUITHREADINFO info{sizeof info};
  if(GetGUIThreadInfo(GetCurrentThreadId(),&info)&&!(info.flags&GUI_INMOVESIZE))interactive_=false;
 }
 void closing(){closing_=true;}
 void adopt(unsigned i,HWND w,unsigned dpi=0){
  if(i>=slots_.size()||!w)return;auto&s=slots_[i];s.host=this;s.index=i;
  auto style=GetWindowLongPtrW(w,GWL_STYLE);
  style&=~LONG_PTR(WS_CHILD|WS_VISIBLE|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME);
  SetWindowLongPtrW(w,GWL_STYLE,style|WS_POPUP|WS_CLIPCHILDREN);
  auto ex=GetWindowLongPtrW(w,GWL_EXSTYLE);
  SetWindowLongPtrW(w,GWL_EXSTYLE,(ex&~LONG_PTR(WS_EX_APPWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_TOPMOST))|WS_EX_TOOLWINDOW);
  SetWindowLongPtrW(w,GWLP_HWNDPARENT,0);
  SetPropW(w,L"RA2.Suite.Managed",reinterpret_cast<HANDLE>(1));SetWindowSubclass(w,paneProc,PaneSubclass,reinterpret_cast<DWORD_PTR>(&s));
  SetWindowPos(w,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
  s.dpi=dpi;s.window=w;
 }
 bool request(unsigned i,const Layout&wanted){
  if(i>=slots_.size())return false;auto&s=slots_[i];
  auto handle=s.window.load();
  const bool shown=handle&&IsWindowVisible(handle);
  const bool ready=shown&&!IsIconic(handle)&&above(handle,shell_);
  std::lock_guard<std::mutex>lock(s.mutex);auto w=s.window.load();if(!w||w!=handle)return false;
  s.latest=wanted;
  if(s.pending)return true;
  if(s.processing&&s.executing==wanted)return true;
  if(!s.processing&&!wanted.visible&&s.appliedValid&&!s.applied.visible&&!shown)return true;
  if(!s.processing&&s.appliedValid&&s.applied==wanted&&(wanted.visible?ready:!shown))return true;
  if(!PostMessageW(w,LayoutMessage,0,0))return false;s.pending=true;return true;
 }
 bool apply(HWND w,UINT message,UINT dpiMessage){
  if(message!=LayoutMessage)return false;auto*s=slot(w);if(!s)return true;Layout next;
  {std::lock_guard<std::mutex>lock(s->mutex);if(!s->pending)return true;next=s->latest;s->pending=false;s->processing=true;s->executing=next;}
  bool show=next.visible&&canPresent();
  if(show&&s->dpi!=next.dpi){s->dpi=next.dpi;SendMessageW(w,dpiMessage,next.dpi,0);}
  // DPI/layout processing can take time. A later switch-away must win before
  // an obsolete page is shown, without any shell-thread synchronous call.
  {std::lock_guard<std::mutex>lock(s->mutex);show=show&&s->latest==next;}
  show=show&&canPresent();
  if(show){
   if(IsIconic(w))ShowWindow(w,SW_SHOWNOACTIVATE);
   RECT current{};GetWindowRect(w,&current);UINT flags=SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW;
   // Keep this independent page immediately above its shell, BELOW any
   // unrelated window already above the shell. Showing the selected page
   // must not depend on a transient foreground/activation observation.
   HWND anchor=GetWindow(shell_,GW_HWNDPREV);
   if(anchor==w)flags|=SWP_NOZORDER;
   if(current.left==next.rect.left&&current.top==next.rect.top&&current.right==next.rect.right&&current.bottom==next.rect.bottom)flags|=SWP_NOMOVE|SWP_NOSIZE;
   show=SetWindowPos(w,anchor?anchor:HWND_TOP,next.rect.left,next.rect.top,next.rect.right-next.rect.left,next.rect.bottom-next.rect.top,flags)!=FALSE;
   show=show&&IsWindowVisible(w)&&!IsIconic(w);
   if(show){
    // A navigation click advances Layout::order even on the selected page.
    // Rebuild child visibility/clipping when Windows kept the same geometry;
    // no WM_SIZE is otherwise generated for that recovery request.
    if(next.category>=0)SendMessageW(w,CategoryMessage,next.category,0);
    else if(flags&SWP_NOSIZE){RECT client{};if(GetClientRect(w,&client))SendMessageW(w,WM_SIZE,SIZE_RESTORED,MAKELPARAM(client.right,client.bottom));}
    RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
   }
  }else ShowWindow(w,SW_HIDE);
  next.visible=show;
  {std::lock_guard<std::mutex>lock(s->mutex);s->applied=next;s->appliedValid=true;s->processing=false;}
  PostMessageW(shell_,ActivityMessage,s->index,0);return true;
 }
};
}

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
namespace overlaystack {
// The opaque replacement scene is a background, including its shared host
// HWND. Every information/interaction window must remain above that backdrop.
enum class Layer:int {RenderUpgrade=5,World=10,Statistics=20,Minimap=30,Watch=40,Aircraft=50,Construction=60,Diagnostics=80,Cursor=100};
struct Entry {HWND window=nullptr;Layer layer=Layer::World;};
inline std::mutex mutex;
inline std::vector<Entry> entries;
inline bool own(HWND w){DWORD process=0;return w&&IsWindow(w)&&GetWindowThreadProcessId(w,&process)&&process==GetCurrentProcessId();}
inline bool opaque(Layer l){return l==Layer::Minimap||l==Layer::Watch||l==Layer::Construction;}
inline HWND above(HWND window,Layer layer){
 std::lock_guard<std::mutex> lock(mutex);
 entries.erase(std::remove_if(entries.begin(),entries.end(),[](const Entry&e){return !own(e.window);}),entries.end());
 auto found=std::find_if(entries.begin(),entries.end(),[&](const Entry&e){return e.window==window;});
 if(found==entries.end())entries.push_back({window,layer});else found->layer=layer;
 HWND anchor=HWND_TOPMOST;int priority=1000000;
 for(const auto&e:entries)if(e.window!=window&&IsWindowVisible(e.window)){
  int p=int(e.layer);bool higher=p>int(layer)||(p==int(layer)&&uintptr_t(e.window)>uintptr_t(window));
  if(higher&&(p<priority||(p==priority&&uintptr_t(e.window)<uintptr_t(anchor)))){priority=p;anchor=e.window;}
 }
 return anchor;
}
// A fullscreen game can promote itself above the entire group while every
// overlay retains WS_VISIBLE. Inserting behind a stale higher-layer anchor
// cannot recover that group, even when each owner repeats its usual repair.
// Check the actual desktop order and restore only the already-visible group.
inline std::atomic_flag repairing=ATOMIC_FLAG_INIT;
inline DWORD checkedAt=0;inline HWND checkedForeground=nullptr;inline bool checked=false;
inline BOOL keepAbove(HWND foreground,bool force=false){
 if(!foreground||!IsWindow(foreground)||repairing.test_and_set())return TRUE;
 struct Release{~Release(){repairing.clear();}} release;
 HWND root=GetAncestor(foreground,GA_ROOT);if(root)foreground=root;
 std::vector<Entry> visible;
 {std::lock_guard<std::mutex> lock(mutex);for(const auto&e:entries)if(own(e.window)&&IsWindowVisible(e.window))visible.push_back(e);}
 if(visible.empty()||std::any_of(visible.begin(),visible.end(),[&](const Entry&e){return e.window==foreground;}))return TRUE;
 // Hidden/system topmost HWNDs may remain above a game which has just covered
 // our entire group. Absolute desktop-top identity cannot validate the cache.
 // Instead prove that EVERY visible member is still above this foreground.
 // A healthy contiguous group takes one hop per member; only the periodic
 // audit or an obscured/changed group needs the full desktop scan and repair.
 DWORD now=GetTickCount();
 if(!force&&checked&&foreground==checkedForeground&&DWORD(now-checkedAt)<250&&visible.size()<=64){
  const uint64_t complete=visible.size()==64?~uint64_t(0):(uint64_t(1)<<visible.size())-1;uint64_t seen=0;int checkedWindows=0;
  for(HWND w=GetWindow(foreground,GW_HWNDPREV);w&&checkedWindows++<4096;w=GetWindow(w,GW_HWNDPREV)){
   for(size_t n=0;n<visible.size();++n)if(visible[n].window==w)seen|=uint64_t(1)<<n;
   if(seen==complete)return TRUE;
  }
 }
 checked=true;checkedAt=now;checkedForeground=foreground;
 bool behind=false,obscured=false;int scanned=0;
 for(HWND w=GetTopWindow(nullptr);w&&scanned++<4096;w=GetWindow(w,GW_HWNDNEXT)){
  if(w==foreground){behind=true;continue;}
  if(behind&&std::any_of(visible.begin(),visible.end(),[&](const Entry&e){return e.window==w;})){obscured=true;break;}
 }
 if(!obscured)return TRUE;
 std::sort(visible.begin(),visible.end(),[](const Entry&a,const Entry&b){return a.layer!=b.layer?int(a.layer)>int(b.layer):uintptr_t(a.window)>uintptr_t(b.window);});
 // Keep all User32 callbacks outside the registry mutex. Real Windows rejected
 // deferred repairs even with one UI owner. Individual calls preserve the
 // same highest-to-lowest anchor chain without relying on deferred support.
 // No SHOWWINDOW flag can revive a hidden panel.
 constexpr UINT flags=SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOSENDCHANGING;
 HWND anchor=HWND_TOPMOST;
 for(const auto&e:visible)if(own(e.window)&&IsWindowVisible(e.window)){
  if(!SetWindowPos(e.window,anchor,0,0,0,0,flags))return FALSE;
  anchor=e.window;
 }
 return TRUE;
}
// Normal placement changes only the requesting owner's geometry. Visibility
// and foreground eligibility remain the caller's responsibility.
inline BOOL place(HWND window,Layer layer,int x,int y,int w,int h,UINT flags){
 if(!own(window))return FALSE;HWND anchor=above(window,layer);
 // Static panels and keepalives should not repeatedly trigger User32 position
 // callbacks or discard their painted content. Only skip a request whose real
 // geometry, visibility, topmost state and immediate layer anchor agree.
 const bool ordered=(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST)&&
  (anchor==HWND_TOPMOST?GetTopWindow(nullptr)==window:GetWindow(window,GW_HWNDPREV)==anchor);
 RECT current{};const bool geometry=GetWindowRect(window,&current)&&
  ((flags&SWP_NOMOVE)||(current.left==x&&current.top==y))&&
  ((flags&SWP_NOSIZE)||(current.right-current.left==w&&current.bottom-current.top==h));
 constexpr UINT ordinary=SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOOWNERZORDER|SWP_NOSENDCHANGING|SWP_NOCOPYBITS;
 if(!(flags&~ordinary)&&ordered&&geometry&&IsWindowVisible(window))return keepAbove(GetForegroundWindow());
 if(!SetWindowPos(window,anchor,x,y,w,h,(flags&~SWP_NOZORDER)|SWP_NOACTIVATE|SWP_NOOWNERZORDER))return FALSE;
 return keepAbove(GetForegroundWindow(),!ordered);
}
inline void forget(HWND window){
 std::lock_guard<std::mutex> lock(mutex);
 entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const Entry&e){return e.window==window;}),entries.end());
}
inline std::vector<RECT> opaqueBoxes(HWND game,int logicalW,int logicalH){
 std::vector<Entry> snapshot;{std::lock_guard<std::mutex> lock(mutex);snapshot=entries;}
 std::vector<RECT> boxes;RECT client{};POINT origin{};if(!GetClientRect(game,&client)||client.right<=0||client.bottom<=0||logicalW<=0||logicalH<=0||!ClientToScreen(game,&origin))return boxes;
 for(const auto&e:snapshot){RECT r{};if(!opaque(e.layer)||!own(e.window)||!IsWindowVisible(e.window)||!GetWindowRect(e.window,&r))continue;
  RECT b{LONG((int64_t(r.left)-origin.x)*logicalW/client.right)-1,LONG((int64_t(r.top)-origin.y)*logicalH/client.bottom)-1,
   LONG((int64_t(r.right)-origin.x)*logicalW/client.right)+1,LONG((int64_t(r.bottom)-origin.y)*logicalH/client.bottom)+1};
  b.left=std::max<LONG>(0,b.left);b.top=std::max<LONG>(0,b.top);b.right=std::min<LONG>(logicalW,b.right);b.bottom=std::min<LONG>(logicalH,b.bottom);
  if(b.right>b.left&&b.bottom>b.top)boxes.push_back(b);
 }
 return boxes;
}
}

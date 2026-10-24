#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <algorithm>
#include <mutex>
#include <vector>
namespace overlaystack {
enum class Layer:int {World=10,Statistics=20,Minimap=30,Watch=40,Aircraft=50,Construction=60,Diagnostics=80,Cursor=100};
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
// Each owner thread moves only its own window. The nearest higher registered
// surface is the insertion anchor; no cross-thread callbacks under a lock,
// activation, timer, or competing HWND_TOPMOST repair loop is required.
inline BOOL place(HWND window,Layer layer,int x,int y,int w,int h,UINT flags){
 if(!own(window))return FALSE;HWND anchor=above(window,layer);
 return SetWindowPos(window,anchor,x,y,w,h,(flags&~SWP_NOZORDER)|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
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

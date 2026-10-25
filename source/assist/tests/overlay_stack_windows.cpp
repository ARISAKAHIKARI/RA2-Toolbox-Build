#include "../../shared/source/overlay_stack.h"
#include <cassert>
#include <iostream>
#include <atomic>
#include <thread>
static bool higher(HWND a,HWND b){for(HWND w=GetTopWindow(nullptr);w;w=GetWindow(w,GW_HWNDNEXT)){if(w==a)return true;if(w==b)return false;}return false;}
static std::atomic<unsigned> positionCallbacks{0};
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_WINDOWPOSCHANGED){std::lock_guard<std::mutex> lock(overlaystack::mutex);++positionCallbacks;}
 if(m==WM_CLOSE){overlaystack::forget(w);DestroyWindow(w);PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
int main(){
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"RA2OverlayStackFixture";assert(RegisterClassW(&cls));
 auto create=[&](){auto w=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_POPUP,10,20,800,600,nullptr,nullptr,cls.hInstance,nullptr);assert(w);return w;};
 HWND game=create(),world=create(),map=create(),watch=create(),cursor=create();std::atomic<HWND> statsHandle{nullptr};
 HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(ready);
 std::thread statisticsOwner([&]{statsHandle=create();SetEvent(ready);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}});
 assert(WaitForSingleObject(ready,5000)==WAIT_OBJECT_0);CloseHandle(ready);HWND stats=statsHandle;assert(GetWindowThreadProcessId(stats,nullptr)!=GetCurrentThreadId());
 ShowWindow(game,SW_SHOWNOACTIVATE);
 using namespace overlaystack;auto put=[&](HWND w,Layer l){assert(place(w,l,110,140,200,100,SWP_SHOWWINDOW));};
 put(world,Layer::World);put(map,Layer::Minimap);put(cursor,Layer::Cursor);put(stats,Layer::Statistics);put(watch,Layer::Watch);
 for(int i=0;i<100;++i){put(world,Layer::World);put(stats,Layer::Statistics);put(map,Layer::Minimap);assert(higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world));}
 // Promote a real topmost game HWND while all surfaces remain WS_VISIBLE.
 // Pass that controlled foreground candidate to the production group check;
 // the fixture does not seize the runner's actual desktop foreground.
 HWND foreground=GetForegroundWindow();unsigned callbacks=positionCallbacks;
 for(int i=0;i<4;++i){assert(SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE));assert(higher(game,cursor));Sleep(260);assert(keepAbove(game));
  for(auto w:{world,stats,map,watch,cursor})assert(IsWindowVisible(w)&&higher(w,game));
  assert(higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world));
 }
 assert(positionCallbacks>callbacks&&GetForegroundWindow()==foreground);
 auto boxes=opaqueBoxes(game,800,600);assert(boxes.size()==2);for(auto r:boxes)assert(r.left==99&&r.top==119&&r.right==301&&r.bottom==221);
 ShowWindow(map,SW_HIDE);assert(SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE));Sleep(260);assert(keepAbove(game));
 assert(!IsWindowVisible(map)&&higher(world,game)&&opaqueBoxes(game,800,600).size()==1);put(map,Layer::Minimap);assert(higher(watch,map)&&higher(map,world));
 DestroyWindow(watch);put(world,Layer::World);assert(opaqueBoxes(game,800,600).size()==1);
 PostMessageW(stats,WM_CLOSE,0,0);statisticsOwner.join();
 for(auto w:{world,map,cursor,game}){forget(w);DestroyWindow(w);}
 std::cout<<"PASS real HWND stacking, four promoted-game recoveries, cross-owner-thread position callbacks outside the registry lock, no activation, hidden-panel preservation, stale handles and opaque clipping; no actual game/Alt-Tab/DWM visual claim\n";
}

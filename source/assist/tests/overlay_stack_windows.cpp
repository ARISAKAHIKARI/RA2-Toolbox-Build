#include "../../shared/source/overlay_stack.h"
#include <cassert>
#include <iostream>
static bool higher(HWND a,HWND b){for(HWND w=GetTopWindow(nullptr);w;w=GetWindow(w,GW_HWNDNEXT)){if(w==a)return true;if(w==b)return false;}return false;}
int main(){
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"RA2OverlayStackFixture";assert(RegisterClassW(&cls));
 auto create=[&](){auto w=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_POPUP,10,20,800,600,nullptr,nullptr,cls.hInstance,nullptr);assert(w);return w;};
 HWND game=create(),world=create(),stats=create(),map=create(),watch=create(),cursor=create();ShowWindow(game,SW_SHOWNOACTIVATE);
 using namespace overlaystack;auto put=[&](HWND w,Layer l){assert(place(w,l,110,140,200,100,SWP_SHOWWINDOW));};
 put(world,Layer::World);put(map,Layer::Minimap);put(cursor,Layer::Cursor);put(stats,Layer::Statistics);put(watch,Layer::Watch);
 for(int i=0;i<100;++i){put(world,Layer::World);put(stats,Layer::Statistics);put(map,Layer::Minimap);assert(higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world));}
 auto boxes=opaqueBoxes(game,800,600);assert(boxes.size()==2);for(auto r:boxes)assert(r.left==99&&r.top==119&&r.right==301&&r.bottom==221);
 ShowWindow(map,SW_HIDE);put(world,Layer::World);assert(opaqueBoxes(game,800,600).size()==1);put(map,Layer::Minimap);assert(higher(watch,map)&&higher(map,world));
 DestroyWindow(watch);put(world,Layer::World);assert(opaqueBoxes(game,800,600).size()==1);
 for(auto w:{world,stats,map,cursor,game}){forget(w);DestroyWindow(w);}
 std::cout<<"PASS real HWND stacking, repeated repaints, show/hide, stale handles and opaque panel clipping; no game visual claim\n";
}

#include "../../shared/source/physical_window.h"
#include <cstdio>
#include <cstdlib>
int main(){
 // Use a small fake window: no display mode changes, no exclusive fullscreen,
 // no game process, no layered window. This must NEVER claim physical 4K.
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);
 cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"RA2PhysicalProbeTest";
 RegisterClassW(&cls);
 HWND w=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"",
   WS_POPUP,35,35,420,280,nullptr,nullptr,cls.hInstance,nullptr);
 if(!w){std::puts("FAIL: create test HWND");return 2;}
 RECT rect{};POINT origin{};
 bool ok=GetClientRect(w,&rect)!=FALSE&&ClientToScreen(w,&origin)!=FALSE;
 if(!ok){DestroyWindow(w);return 3;}
 auto result=physicalpresentation::inspect(w,2560,1440,origin,rect);
 if(result.permitted()||
    physicalpresentation::renderPath(result,true)!=physicalpresentation::RenderPath::LegacyClientPixels||
    !physicalpresentation::keepExistingOverlay(true,true)){
  DestroyWindow(w);std::puts("FAIL: non-4K window unexpectedly lost native overlay fallback");return 4;
 }
 DestroyWindow(w);
 std::printf("PASS: monitor/DPI-safe Win32 read-only probe, no 4K claim for small window (status=%d)\n",
  int(result.status));
}

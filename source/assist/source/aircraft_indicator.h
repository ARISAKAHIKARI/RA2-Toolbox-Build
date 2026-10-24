static HWND aircraftButton;
#include "radar_layout.h"
#include "idle_aircraft_selection.h"
static radarlayout::Box aircraftButtonBox;
static void hideAircraftButton(){aircraftButtonBox={};if(aircraftButton)ShowWindow(aircraftButton,SW_HIDE);}
static void destroyAircraftButton(){if(aircraftButton)DestroyWindow(aircraftButton);aircraftButton=nullptr;}
static LRESULT CALLBACK aircraftButtonProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(message==WM_ERASEBKGND)return 1;
 if(message==WM_LBUTTONUP){if(extra.idleAircraft)selectNextIdleAircraft();return 0;}
 if(message==WM_PAINT){
  PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);RECT r{};GetClientRect(window,&r);
  HBRUSH bg=CreateSolidBrush(RGB(27,34,30));FillRect(dc,&r,bg);DeleteObject(bg);
  HPEN light=CreatePen(PS_SOLID,1,RGB(151,162,116)),dark=CreatePen(PS_SOLID,1,RGB(7,12,9));auto oldPen=SelectObject(dc,light);
  MoveToEx(dc,0,r.bottom-1,nullptr);LineTo(dc,0,0);LineTo(dc,r.right-1,0);SelectObject(dc,dark);LineTo(dc,r.right-1,r.bottom-1);LineTo(dc,0,r.bottom-1);
  int cx=17,cy=r.bottom/2;POINT plane[]={{cx,cy-9},{cx+3,cy-2},{cx+12,cy+3},{cx+12,cy+5},{cx+3,cy+3},{cx+3,cy+8},{cx+6,cy+10},{cx-6,cy+10},{cx-3,cy+8},{cx-3,cy+3},{cx-12,cy+5},{cx-12,cy+3},{cx-3,cy-2}};
  HBRUSH ink=CreateSolidBrush(externalSampler.idleAircraftCount?RGB(235,214,128):RGB(116,130,112));auto oldBrush=SelectObject(dc,ink);SelectObject(dc,light);Polygon(dc,plane,int(sizeof plane/sizeof plane[0]));SelectObject(dc,oldBrush);DeleteObject(ink);
  SelectObject(dc,oldPen);DeleteObject(light);DeleteObject(dark);auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(239,231,178));
  std::wstring text=std::to_wstring(externalSampler.idleAircraftCount)+L"/"+std::to_wstring(externalSampler.aircraftTotal);r.left=36;r.right-=5;DrawTextW(dc,text.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,oldFont);EndPaint(window,&paint);return 0;
 }
 return DefWindowProcW(window,message,wp,lp);
}
static void updateAircraftButton(POINT origin,const RECT&client){
 if(!extra.idleAircraft||!enabled||!externalData||snap.renderStatus!=1||!foregroundGame()||snap.chat||
    !externalSampler.idleAircraftTick||uint32_t(sampleRenderClock(nullptr)-externalSampler.idleAircraftTick)>500||snap.width<=0||snap.height<=0){hideAircraftButton();return;}
 if(!aircraftButton){WNDCLASSW cls{};cls.lpfnWndProc=aircraftButtonProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RA2IdleAircraft103";cls.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&cls);
  aircraftButton=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,cls.lpszClassName,L"空闲飞机 / 总飞机",WS_POPUP,0,0,104,28,nullptr,nullptr,cls.hInstance,nullptr);
 }
 if(!aircraftButton)return;
 aircraftButtonBox=radarlayout::aircraft(snap.width,snap.height,client.right,client.bottom);
 const auto&box=aircraftButtonBox;
 overlaystack::place(aircraftButton,overlaystack::Layer::Aircraft,origin.x+box.x,origin.y+box.y,box.w,box.h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 static int idle=-1,total=-1;if(idle!=externalSampler.idleAircraftCount||total!=externalSampler.aircraftTotal){idle=externalSampler.idleAircraftCount;total=externalSampler.aircraftTotal;InvalidateRect(aircraftButton,nullptr,FALSE);}
}

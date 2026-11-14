#if defined(RA2_CONSOLE_WEBVIEW2)
#include "modern_console_main.h"
#else
#include "../../shared/source/release_version.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include "app_icon.h"
#include "../../shared/source/host_crash.h"
#include "../../shared/source/bounded_shutdown.h"
#include <commctrl.h>
#include <string>
#include <atomic>
#include <mutex>
#include <algorithm>
#include "suite_host.h"
#include "../../shared/source/initial_profiles.h"
#include "../../shared/source/modern_ui.h"
extern "C" int WINAPI CursorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI MonitorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI TacticalEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
static HWND frame,nav[suite::categories::Count],videoNav[2],footer,heading,subtitle;
static HANDLE workers[3];static std::atomic<bool> active[3];static HFONT font;
static unsigned consoleDpi=96;
static std::wstring config;static int selected=0,videoSection=0,closePhase=0;static bool closeAsked=false;
static constexpr UINT PaneReady=WM_APP+60,EnabledChanged=WM_APP+61,CloseDecision=WM_APP+62;
static HINSTANCE instance;static std::wstring args;
static std::atomic<bool> statsShown{false},hardwarePointer{false};
static std::atomic<unsigned> commonRefreshRate{0};
static std::mutex displayMutex;static suite::DisplayState displayState;static bool displayReady=false;
static RECT paneRect{};static bool layoutValid=false;static SIZE layoutSize{};static unsigned layoutDpi=0;static uint64_t paneOrder=0;
namespace suite {
 HWND parent(){return frame;}
 bool enabled(int module){return active[module].load();}
 void registerPane(int module,HWND pane,WNDPROC){
  PostMessageW(pane,DisplaySettingsChanged,0,0);
  if(module==2)PostMessageW(pane,RefreshRateChanged,0,0);
  modernui::styleChildren(pane);frontend.adopt(unsigned(module),pane,modernui::windowDpi(pane));
  PostMessageW(frame,PaneReady,module,(LPARAM)pane);
 }
 void reportEnabled(int module,bool value){if(active[module].exchange(value)!=value)PostMessageW(frame,EnabledChanged,module,value);}
 void requestClose(){PostMessageW(frame,WM_CLOSE,0,0);}
 void closeDecision(bool allow){PostMessageW(frame,CloseDecision,allow,0);}
 void setStatsVisible(bool value){statsShown.store(value);}
 bool statsVisible(){return statsShown.load();}
 void publishHardwareCursor(bool active){hardwarePointer.store(active);}
 bool hardwareCursorActive(){return hardwarePointer.load();}
 void publishDisplaySettings(const DisplayState& state){
  {std::lock_guard<std::mutex> lock(displayMutex);if(displayReady&&displayState==state)return;displayState=state;displayReady=true;}
  for(unsigned i=0;i<2;++i)frontend.post(i,DisplaySettingsChanged);
 }
 bool readDisplaySettings(DisplayState& state){std::lock_guard<std::mutex> lock(displayMutex);if(!displayReady)return false;state=displayState;return true;}
 bool requestDisplaySetting(int option,int value){if(!validDisplayEdit(option,value))return false;return frontend.post(2,ApplyDisplaySetting,option,value);}
 void publishRefreshRate(unsigned hz){
  if(hz<30||hz>500||commonRefreshRate.exchange(hz)==hz)return;
  frontend.post(2,RefreshRateChanged);
 }
 unsigned refreshRate(){return commonRefreshRate.load();}
}
static void save(){WritePrivateProfileStringW(L"Video",L"CursorEnabled",active[0]?L"1":L"0",config.c_str());WritePrivateProfileStringW(L"Video",L"MonitorEnabled",active[1]?L"1":L"0",config.c_str());}
static unsigned selectedPane(){return selected==suite::categories::Video?unsigned(videoSection):2u;}
static void layoutPanes(){
 if(!frame)return;RECT r{};GetClientRect(frame,&r);auto px=[](int n){return modernui::scale(n,consoleDpi);};
 paneRect={px(196),px(selected==suite::categories::Video?66:12),std::max(px(197),int(r.right)-px(16)),std::max(px(103),int(r.bottom)-px(38))};
 RECT screen=paneRect;MapWindowPoints(frame,nullptr,reinterpret_cast<POINT*>(&screen),2);
 bool visible=suite::frontend.canPresent();
 // Category and visibility share ONE replaceable request. A delayed old
 // category message cannot relayout the page after a newer selection.
 for(unsigned i=0;i<3;++i)if(suite::frontend.window(unsigned(i)))suite::frontend.request(i,{screen,visible&&i==selectedPane(),consoleDpi,paneOrder,i==2?selected:-1});

}
static void layout(){
 RECT r{};GetClientRect(frame,&r);auto px=[](int n){return modernui::scale(n,consoleDpi);};
 if(layoutValid&&layoutSize.cx==r.right&&layoutSize.cy==r.bottom&&layoutDpi==consoleDpi){
  for(int i=0;i<suite::categories::Count;++i)modernui::select(nav[i],selected==i);for(int i=0;i<2;++i){ShowWindow(videoNav[i],selected==suite::categories::Video?SW_SHOWNA:SW_HIDE);modernui::select(videoNav[i],videoSection==i);}layoutPanes();InvalidateRect(frame,&paneRect,FALSE);return;
 }
 layoutValid=true;layoutSize={r.right,r.bottom};layoutDpi=consoleDpi;
 MoveWindow(heading,px(22),px(22),px(152),px(32),FALSE);MoveWindow(subtitle,px(24),px(59),px(145),px(24),FALSE);
 for(int i=0;i<suite::categories::Count;++i){MoveWindow(nav[i],px(16),px(116+i*52),px(160),px(42),FALSE);modernui::select(nav[i],selected==i);}
 for(int i=0;i<2;++i){MoveWindow(videoNav[i],px(210+i*246),px(14),px(234),px(38),FALSE);ShowWindow(videoNav[i],selected==suite::categories::Video?SW_SHOWNA:SW_HIDE);modernui::select(videoNav[i],videoSection==i);}
 MoveWindow(footer,px(210),std::max(px(105),int(r.bottom)-px(29)),std::max(1,int(r.right)-px(230)),px(22),FALSE);
 layoutPanes();
 // Only the shell controls belong to this thread. Never invalidate a worker
 // settings subtree on every resize, and never synchronously paint it here.
 RedrawWindow(frame,nullptr,nullptr,RDW_INVALIDATE|RDW_NOCHILDREN);
 for(HWND c:nav)InvalidateRect(c,nullptr,FALSE);
 for(HWND c:{heading,subtitle,footer,videoNav[0],videoNav[1]})InvalidateRect(c,nullptr,FALSE);
}
static void applyDpi(unsigned dpi){
 consoleDpi=modernui::panelDpi(dpi,false);HFONT next=modernui::makeFont(modernui::BodyPixels,consoleDpi);
 if(next){for(HWND c:nav)if(c)SendMessageW(c,WM_SETFONT,(WPARAM)next,FALSE);for(HWND c:{heading,subtitle,footer,videoNav[0],videoNav[1]})if(c)SendMessageW(c,WM_SETFONT,(WPARAM)next,FALSE);auto old=font;font=next;if(old)DeleteObject(old);}
 modernui::heading(heading,consoleDpi);
}

static bool done(int i){return !workers[i]||WaitForSingleObject(workers[i],0)==WAIT_OBJECT_0;}
static void shutdown(){boundedshutdown::begin();closePhase=2;suite::frontend.closing();layoutPanes();SetWindowTextW(frame,L"红警工具箱 · 正在退出（恢复最多等待 2 秒）…");
 for(unsigned i=0;i<3;++i)suite::frontend.post(i,WM_CLOSE,suite::CloseToken);
}
static DWORD WINAPI run(LPVOID arg){int i=int((uintptr_t)arg);LPWSTR cmd=const_cast<LPWSTR>(args.c_str());
 int code=i==0?CursorEntry(instance,nullptr,cmd,SW_HIDE):i==1?MonitorEntry(instance,nullptr,cmd,SW_HIDE):TacticalEntry(instance,nullptr,cmd,SW_HIDE);
 return DWORD(code);
}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){switch(m){
 case WM_CREATE:{frame=w;suite::frontend.shell(w);consoleDpi=modernui::windowDpi(w);auto ctl=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){auto c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,w,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);modernui::style(c);return c;};
  heading=ctl(L"STATIC",L"红警工具箱",0,0);modernui::heading(heading);
  subtitle=ctl(L"STATIC",L"v" RA2_RELEASE_VERSION_W,0,0);
  for(int i=0;i<suite::categories::Count;++i)nav[i]=ctl(L"BUTTON",suite::categories::name(i),BS_PUSHBUTTON|WS_TABSTOP,200+i);
  videoNav[0]=ctl(L"BUTTON",L"光标与渲染",BS_PUSHBUTTON|WS_TABSTOP,210);videoNav[1]=ctl(L"BUTTON",L"监控视图",BS_PUSHBUTTON|WS_TABSTOP,211);
  footer=ctl(L"STATIC",L"每项功能独立启用 · 设置自动保存",0,0);SetTimer(w,1,100,nullptr);applyDpi(consoleDpi);layout();return 0;}
 case PaneReady:{int i=int(wp);if(i>=0&&i<3&&suite::frontend.window(unsigned(i))==(HWND)lp){layout();suite::frontend.post(unsigned(i),suite::ModuleEnable,active[i]);if(closePhase==2)suite::frontend.post(unsigned(i),WM_CLOSE,suite::CloseToken);}return 0;}
 case frontendpanes::DestroyedMessage:case frontendpanes::ActivityMessage:layoutPanes();InvalidateRect(w,&paneRect,FALSE);return 0;
 case EnabledChanged:save();return 0;
 case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);if(id>=200&&id<200+suite::categories::Count){selected=id-200;suite::frontend.endInteraction();++paneOrder;WritePrivateProfileStringW(L"UI",L"Category5",std::to_wstring(selected).c_str(),config.c_str());layout();return 0;}
  if(id==210||id==211){videoSection=id-210;++paneOrder;WritePrivateProfileStringW(L"UI",L"VideoSection",std::to_wstring(videoSection).c_str(),config.c_str());layout();return 0;}}break;

 case WM_DISPLAYCHANGE:for(unsigned i=0;i<3;++i)suite::frontend.post(i,WM_DISPLAYCHANGE,wp,lp);return 0;
 case WM_SIZE:layout();return 0;
 case WM_MOVE:layoutPanes();return 0;
 case WM_ENTERSIZEMOVE:suite::frontend.beginInteraction();layoutPanes();InvalidateRect(w,&paneRect,FALSE);return 0;
 case WM_CANCELMODE:suite::frontend.endInteraction();layoutPanes();return 0;
 case WM_ACTIVATEAPP:if(!wp){suite::frontend.endInteraction();layoutPanes();}break;
 case WM_EXITSIZEMOVE:suite::frontend.endInteraction();layoutPanes();InvalidateRect(w,&paneRect,FALSE);return 0;
 case WM_ACTIVATE:if(LOWORD(wp)!=WA_INACTIVE)++paneOrder;layoutPanes();break;
 case WM_SHOWWINDOW:PostMessageW(w,frontendpanes::ActivityMessage,0,0);break;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);SetDCBrushColor(dc,modernui::Background);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));EndPaint(w,&ps);return 0;}
 case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={modernui::scale(760,consoleDpi),modernui::scale(620,consoleDpi)};return 0;
 case WM_DPICHANGED:{applyDpi(HIWORD(wp));auto r=(RECT*)lp;SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout();return 0;}
 case WM_CLOSE:if(!closePhase){closePhase=1;closeAsked=false;SetWindowTextW(frame,L"红警工具箱 · 正在准备退出…");}return 0;
 case CloseDecision:if(closePhase==1){if(wp)shutdown();else{closePhase=0;SetWindowTextW(frame,L"红警工具箱 v" RA2_RELEASE_VERSION_W);}}return 0;
 case WM_TIMER:{
  suite::frontend.reconcileInteraction();layoutPanes();
  if(closePhase==1){if(done(0))shutdown();else if(!closeAsked)closeAsked=suite::frontend.post(0,suite::CloseCheck);}
  if(closePhase==2&&done(0)&&done(1)&&done(2)){DestroyWindow(w);return 0;}
  if(!closePhase){for(int i=0;i<3;++i)if(done(i)&&!IsWindow(suite::frontend.window(unsigned(i)))){active[i]=false;}}
  return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{auto dc=(HDC)wp;SetBkMode(dc,OPAQUE);SetTextColor(dc,(HWND)lp==heading?modernui::Text:modernui::Muted);SetBkColor(dc,modernui::Background);SetDCBrushColor(dc,modernui::Background);return (LRESULT)GetStockObject(DC_BRUSH);}
 case WM_DESTROY:KillTimer(w,1);save();PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,m,wp,lp);}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR cmd,int show){
 HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\RA2Toolbox_v020");if(GetLastError()==ERROR_ALREADY_EXISTS){HWND old=FindWindowW(L"RA2Toolbox020",nullptr);if(old){ShowWindow(old,SW_RESTORE);SetForegroundWindow(old);}CloseHandle(single);return 0;}
 hostcrash::install();
 instance=h;args=cmd;wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);std::wstring root=exe;root.resize(root.find_last_of(L"\\/"));config=root+L"\\Toolbox.ini";
 if(!initialprofiles::initialize(root)){MessageBoxW(nullptr,L"無法建立預設配置，請將工具箱移到可寫入的資料夾後重試。",L"紅警工具箱",MB_OK|MB_ICONERROR);CloseHandle(single);return 1;}
 const int cursorDefault=GetPrivateProfileIntW(L"Startup",L"AutoStart",0,(root+L"\\settings.ini").c_str());
 active[0]=GetPrivateProfileIntW(L"Video",L"CursorEnabled",GetPrivateProfileIntW(L"Modules",L"Cursor",cursorDefault,config.c_str()),config.c_str())!=0;
 active[1]=GetPrivateProfileIntW(L"Video",L"MonitorEnabled",GetPrivateProfileIntW(L"Modules",L"Units",0,config.c_str()),config.c_str())!=0;
 active[2]=true; // Individual control/automation/rule settings are the gates.
 selected=std::clamp(int(GetPrivateProfileIntW(L"UI",L"Category5",0,config.c_str())),0,int(suite::categories::Count)-1);
 videoSection=std::clamp(int(GetPrivateProfileIntW(L"UI",L"VideoSection",0,config.c_str())),0,1);
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 font=modernui::makeFont();
 WNDCLASSEXW wc{};wc.cbSize=sizeof wc;wc.hInstance=h;wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2Toolbox020";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=suite::applicationIcon(h,false);wc.hIconSm=suite::applicationIcon(h,true);HBRUSH brush=CreateSolidBrush(modernui::Background);wc.hbrBackground=brush;RegisterClassExW(&wc);
 RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);frame=CreateWindowW(wc.lpszClassName,L"红警工具箱 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,std::max(550,std::min(900,int(work.bottom-work.top)-30)),nullptr,nullptr,h,nullptr);
 for(int i=0;i<3;++i){workers[i]=CreateThread(nullptr,0,run,(LPVOID)(uintptr_t)i,0,nullptr);if(!workers[i])active[i]=false;}
 consoleDpi=modernui::windowDpi(frame);applyDpi(consoleDpi);SetWindowPos(frame,nullptr,0,0,std::min(modernui::scale(1120,consoleDpi),int(work.right-work.left)-20),std::min(modernui::scale(900,consoleDpi),int(work.bottom-work.top)-30),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);layout();ShowWindow(frame,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 for(auto worker:workers)if(worker)CloseHandle(worker);DeleteObject(font);DeleteObject(brush);CloseHandle(single);return 0;
}
#endif

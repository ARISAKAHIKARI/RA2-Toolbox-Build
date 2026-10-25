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
static HWND frame,nav[5],checks[3],panes[3],footer,heading,subtitle,switchLabel;
static HANDLE workers[3];static std::atomic<bool> active[3];static HFONT font;
static unsigned consoleDpi=96;
static std::wstring config;static int selected=2,closePhase=0;static bool closeAsked=false;
static constexpr UINT PaneReady=WM_APP+60,EnabledChanged=WM_APP+61,CloseDecision=WM_APP+62;
static HINSTANCE instance;static std::wstring args;
static std::atomic<bool> statsShown{false},hardwarePointer{false};
static std::atomic<HWND> displayPanes[3]{};
static std::atomic<unsigned> commonRefreshRate{0};
static std::mutex displayMutex;static suite::DisplayState displayState;static bool displayReady=false;
static const wchar_t* names[]={L"画面设置",L"单位统计",L"战术控制",L"战场信息",L"实验功能"};
static const wchar_t* moduleNames[]={L"画面刷新",L"游戏内单位统计",L"游戏内战术与信息"};
static const wchar_t* configKeys[]={L"Cursor",L"Units",L"Assist"};
namespace suite {
 HWND parent(){return frame;}
 bool enabled(int module){return active[module].load();}
 void registerPane(int module,HWND pane){
  displayPanes[module].store(pane);PostMessageW(pane,DisplaySettingsChanged,0,0);
  if(module==2)PostMessageW(pane,RefreshRateChanged,0,0);
  auto style=GetWindowLongPtrW(pane,GWL_STYLE);
  style&=~LONG_PTR(WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME);
  SetWindowLongPtrW(pane,GWL_STYLE,style|WS_CHILD|WS_CLIPCHILDREN);
  SetParent(pane,frame);SetWindowPos(pane,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
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
  for(int i=0;i<2;++i)if(auto pane=displayPanes[i].load())PostMessageW(pane,DisplaySettingsChanged,0,0);
 }
 bool readDisplaySettings(DisplayState& state){std::lock_guard<std::mutex> lock(displayMutex);if(!displayReady)return false;state=displayState;return true;}
 bool requestDisplaySetting(int option,int value){if(!validDisplayEdit(option,value))return false;auto pane=displayPanes[2].load();return pane&&PostMessageW(pane,ApplyDisplaySetting,option,value);}
 void publishRefreshRate(unsigned hz){
  if(hz<30||hz>500||commonRefreshRate.exchange(hz)==hz)return;
  if(auto pane=displayPanes[2].load())PostMessageW(pane,RefreshRateChanged,0,0);
 }
 unsigned refreshRate(){return commonRefreshRate.load();}
}
static void save(){for(int i=0;i<3;++i)WritePrivateProfileStringW(L"Modules",configKeys[i],active[i]?L"1":L"0",config.c_str());}
static void layout(){
 RECT r{};GetClientRect(frame,&r);auto px=[](int n){return modernui::scale(n,consoleDpi);};
 MoveWindow(heading,px(22),px(22),px(152),px(32),FALSE);MoveWindow(subtitle,px(24),px(59),px(145),px(24),FALSE);
 for(int i=0;i<5;++i){MoveWindow(nav[i],px(16),px(116+i*52),px(160),px(42),FALSE);modernui::select(nav[i],selected==i);}
 for(int i=0;i<3;++i)MoveWindow(checks[i],px(22),px(448+i*36),px(150),px(28),FALSE);
 MoveWindow(switchLabel,px(24),px(412),px(148),px(24),FALSE);
 MoveWindow(footer,px(210),std::max(px(105),int(r.bottom)-px(29)),std::max(1,int(r.right)-px(230)),px(22),FALSE);
 if(panes[2])PostMessageW(panes[2],suite::SelectCategory,selected>=2?selected-2:0,0);
 for(int i=0;i<3;++i)if(panes[i]){auto p=new suite::Layout{{px(196),px(12),std::max(px(197),int(r.right)-px(16)),std::max(px(103),int(r.bottom)-px(38))},(i==2?selected>=2:selected==i),consoleDpi};
  if(!PostMessageW(panes[i],suite::PaneLayout,0,(LPARAM)p))delete p;}
 RedrawWindow(frame,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
static void applyDpi(unsigned dpi){
 consoleDpi=modernui::panelDpi(dpi,false);HFONT next=modernui::makeFont(modernui::BodyPixels,consoleDpi);
 if(next){for(HWND c:{heading,subtitle,switchLabel,footer,nav[0],nav[1],nav[2],nav[3],nav[4],checks[0],checks[1],checks[2]})if(c)SendMessageW(c,WM_SETFONT,(WPARAM)next,FALSE);auto old=font;font=next;if(old)DeleteObject(old);}
 modernui::heading(heading,consoleDpi);
}

static bool done(int i){return !workers[i]||WaitForSingleObject(workers[i],0)==WAIT_OBJECT_0;}
static void shutdown(){closePhase=2;SetWindowTextW(frame,L"红警工具箱 · 正在恢复并退出…");
 for(int i=0;i<3;++i)if(panes[i])PostMessageW(panes[i],WM_CLOSE,suite::CloseToken,0);
}
static DWORD WINAPI run(LPVOID arg){int i=int((uintptr_t)arg);LPWSTR cmd=const_cast<LPWSTR>(args.c_str());
 int code=i==0?CursorEntry(instance,nullptr,cmd,SW_HIDE):i==1?MonitorEntry(instance,nullptr,cmd,SW_HIDE):TacticalEntry(instance,nullptr,cmd,SW_HIDE);
 return DWORD(code);
}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){switch(m){
 case WM_CREATE:{frame=w;consoleDpi=modernui::windowDpi(w);auto ctl=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){auto c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,w,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);modernui::style(c);return c;};
  heading=ctl(L"STATIC",L"红警工具箱",0,0);modernui::heading(heading);
  subtitle=ctl(L"STATIC",L"v" RA2_RELEASE_VERSION_W,0,0);switchLabel=ctl(L"STATIC",L"功能开关",0,0);
  for(int i=0;i<5;++i)nav[i]=ctl(L"BUTTON",names[i],BS_PUSHBUTTON|WS_TABSTOP,200+i);
  for(int i=0;i<3;++i){checks[i]=ctl(L"BUTTON",moduleNames[i],BS_AUTOCHECKBOX|WS_TABSTOP,100+i);SendMessageW(checks[i],BM_SETCHECK,active[i]?BST_CHECKED:BST_UNCHECKED,0);}
  footer=ctl(L"STATIC",L"设置自动保存 · 切换界面不影响已开启的功能",0,0);SetTimer(w,1,100,nullptr);applyDpi(consoleDpi);layout();return 0;}
 case PaneReady:{int i=int(wp);if(i>=0&&i<3){panes[i]=(HWND)lp;layout();PostMessageW(panes[i],suite::ModuleEnable,active[i],0);if(closePhase==2)PostMessageW(panes[i],WM_CLOSE,suite::CloseToken,0);}return 0;}
 case EnabledChanged:{int i=int(wp);if(i>=0&&i<3){SendMessageW(checks[i],BM_SETCHECK,lp?BST_CHECKED:BST_UNCHECKED,0);save();}return 0;}
 case WM_COMMAND:if(LOWORD(wp)>=200&&LOWORD(wp)<=204&&HIWORD(wp)==BN_CLICKED){selected=LOWORD(wp)-200;WritePrivateProfileStringW(L"UI",L"Page",std::to_wstring(selected).c_str(),config.c_str());layout();return 0;}if(LOWORD(wp)>=100&&LOWORD(wp)<103&&HIWORD(wp)==BN_CLICKED){int i=LOWORD(wp)-100;active[i]=SendMessageW(checks[i],BM_GETCHECK,0,0)==BST_CHECKED;save();if(panes[i])PostMessageW(panes[i],suite::ModuleEnable,active[i],0);return 0;}break;

 case WM_DISPLAYCHANGE:for(auto& slot:displayPanes)if(auto pane=slot.load())PostMessageW(pane,WM_DISPLAYCHANGE,wp,lp);return 0;
 case WM_SIZE:layout();return 0;
 case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={modernui::scale(760,consoleDpi),modernui::scale(620,consoleDpi)};return 0;
 case WM_DPICHANGED:{applyDpi(HIWORD(wp));auto r=(RECT*)lp;SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout();return 0;}
 case WM_CLOSE:if(!closePhase){closePhase=1;closeAsked=false;SetWindowTextW(frame,L"红警工具箱 · 正在准备退出…");}return 0;
 case CloseDecision:if(closePhase==1){if(wp)shutdown();else{closePhase=0;SetWindowTextW(frame,L"红警工具箱 v" RA2_RELEASE_VERSION_W);}}return 0;
 case WM_TIMER:{
  if(closePhase==1&&!closeAsked){if(panes[0]){closeAsked=true;PostMessageW(panes[0],suite::CloseCheck,0,0);}else if(done(0))shutdown();}
  if(closePhase==2&&done(0)&&done(1)&&done(2)){DestroyWindow(w);return 0;}
  if(!closePhase){for(int i=0;i<3;++i)if(done(i)&&!IsWindow(panes[i])){active[i]=false;SendMessageW(checks[i],BM_SETCHECK,BST_UNCHECKED,0);EnableWindow(checks[i],FALSE);}}
  return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{auto dc=(HDC)wp;SetBkMode(dc,OPAQUE);SetTextColor(dc,(HWND)lp==heading?modernui::Text:modernui::Muted);SetBkColor(dc,modernui::Background);SetDCBrushColor(dc,modernui::Background);return (LRESULT)GetStockObject(DC_BRUSH);}
 case WM_DESTROY:KillTimer(w,1);save();PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,m,wp,lp);}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR cmd,int show){
 HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\RA2Toolbox_v020");if(GetLastError()==ERROR_ALREADY_EXISTS){HWND old=FindWindowW(L"RA2Toolbox020",nullptr);if(old){ShowWindow(old,SW_RESTORE);SetForegroundWindow(old);}CloseHandle(single);return 0;}
 instance=h;args=cmd;wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);std::wstring root=exe;root.resize(root.find_last_of(L"\\/"));config=root+L"\\Toolbox.ini";
 if(!initialprofiles::initialize(root)){MessageBoxW(nullptr,L"無法建立預設配置，請將工具箱移到可寫入的資料夾後重試。",L"紅警工具箱",MB_OK|MB_ICONERROR);CloseHandle(single);return 1;}
 int defaults[3]={int(GetPrivateProfileIntW(L"Startup",L"AutoStart",0,(root+L"\\settings.ini").c_str())),0,0};
 for(int i=0;i<3;++i)active[i]=GetPrivateProfileIntW(L"Modules",configKeys[i],defaults[i],config.c_str())!=0;
 selected=std::clamp(int(GetPrivateProfileIntW(L"UI",L"Page",2,config.c_str())),0,4);
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 font=modernui::makeFont();
 WNDCLASSW wc{};wc.hInstance=h;wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2Toolbox020";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);HBRUSH brush=CreateSolidBrush(modernui::Background);wc.hbrBackground=brush;RegisterClassW(&wc);
 RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);frame=CreateWindowW(wc.lpszClassName,L"红警工具箱 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,std::max(550,std::min(900,int(work.bottom-work.top)-30)),nullptr,nullptr,h,nullptr);
 for(int i=0;i<3;++i){workers[i]=CreateThread(nullptr,0,run,(LPVOID)(uintptr_t)i,0,nullptr);if(!workers[i])active[i]=false;}
 consoleDpi=modernui::windowDpi(frame);applyDpi(consoleDpi);SetWindowPos(frame,nullptr,0,0,std::min(modernui::scale(1120,consoleDpi),int(work.right-work.left)-20),std::min(modernui::scale(900,consoleDpi),int(work.bottom-work.top)-30),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);layout();ShowWindow(frame,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 for(auto worker:workers)if(worker)CloseHandle(worker);DeleteObject(font);DeleteObject(brush);CloseHandle(single);return 0;
}

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
#include <algorithm>
#include "suite_host.h"
#include "../../shared/source/modern_ui.h"
extern "C" int WINAPI CursorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI MonitorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI TacticalEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
static HWND frame,nav[3],checks[3],panes[3],footer,heading,subtitle,switchLabel;
static HANDLE workers[3];static std::atomic<bool> active[3];static HFONT font;
static std::wstring config;static int selected=2,closePhase=0;static bool closeAsked=false;
static constexpr UINT PaneReady=WM_APP+60,EnabledChanged=WM_APP+61,CloseDecision=WM_APP+62;
static HINSTANCE instance;static std::wstring args;
static std::atomic<bool> statsShown{false};
static const wchar_t* names[]={L"高刷光标",L"单位统计",L"战术助手"};
static const wchar_t* configKeys[]={L"Cursor",L"Units",L"Assist"};
namespace suite {
 HWND parent(){return frame;}
 bool enabled(int module){return active[module].load();}
 void registerPane(int module,HWND pane){
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
}
static void save(){for(int i=0;i<3;++i)WritePrivateProfileStringW(L"Modules",configKeys[i],active[i]?L"1":L"0",config.c_str());}
static void layout(){
 RECT r{};GetClientRect(frame,&r);
 MoveWindow(heading,22,22,152,32,TRUE);MoveWindow(subtitle,24,59,145,24,TRUE);
 for(int i=0;i<3;++i){MoveWindow(nav[i],16,116+i*52,160,42,TRUE);modernui::select(nav[i],selected==i);MoveWindow(checks[i],22,356+i*36,150,28,TRUE);}
 MoveWindow(switchLabel,24,318,148,24,TRUE);
 MoveWindow(footer,210,std::max(105,int(r.bottom)-29),std::max(1,int(r.right)-230),22,TRUE);
 for(int i=0;i<3;++i)if(panes[i]){auto p=new suite::Layout{{196,12,std::max(197,int(r.right)-16),std::max(103,int(r.bottom)-38)},selected==i};
  if(!PostMessageW(panes[i],suite::PaneLayout,0,(LPARAM)p))delete p;}
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
 case WM_CREATE:{frame=w;auto ctl=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){auto c=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,w,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);modernui::style(c);return c;};
  heading=ctl(L"STATIC",L"红警工具箱",0,0);modernui::heading(heading);
  subtitle=ctl(L"STATIC",L"v" RA2_RELEASE_VERSION_W,0,0);switchLabel=ctl(L"STATIC",L"功能开关",0,0);
  for(int i=0;i<3;++i){nav[i]=ctl(L"BUTTON",names[i],BS_PUSHBUTTON|WS_TABSTOP,200+i);checks[i]=ctl(L"BUTTON",names[i],BS_AUTOCHECKBOX|WS_TABSTOP,100+i);SendMessageW(checks[i],BM_SETCHECK,active[i]?BST_CHECKED:BST_UNCHECKED,0);}
  footer=ctl(L"STATIC",L"设置自动保存 · 切换界面不影响已开启的功能",0,0);SetTimer(w,1,100,nullptr);layout();return 0;}
 case PaneReady:{int i=int(wp);if(i>=0&&i<3){panes[i]=(HWND)lp;layout();PostMessageW(panes[i],suite::ModuleEnable,active[i],0);if(closePhase==2)PostMessageW(panes[i],WM_CLOSE,suite::CloseToken,0);}return 0;}
 case EnabledChanged:{int i=int(wp);if(i>=0&&i<3){SendMessageW(checks[i],BM_SETCHECK,lp?BST_CHECKED:BST_UNCHECKED,0);save();}return 0;}
 case WM_COMMAND:if(LOWORD(wp)>=200&&LOWORD(wp)<=202&&HIWORD(wp)==BN_CLICKED){selected=LOWORD(wp)-200;WritePrivateProfileStringW(L"UI",L"Page",std::to_wstring(selected).c_str(),config.c_str());layout();return 0;}if(LOWORD(wp)>=100&&LOWORD(wp)<103&&HIWORD(wp)==BN_CLICKED){int i=LOWORD(wp)-100;active[i]=SendMessageW(checks[i],BM_GETCHECK,0,0)==BST_CHECKED;save();if(panes[i])PostMessageW(panes[i],suite::ModuleEnable,active[i],0);return 0;}break;

 case WM_SIZE:layout();return 0;
 case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={960,600};return 0;
 case WM_DPICHANGED:{auto r=(RECT*)lp;SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout();return 0;}
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
 int defaults[3]={int(GetPrivateProfileIntW(L"Startup",L"AutoStart",1,(root+L"\\settings.ini").c_str())),1,1};
 for(int i=0;i<3;++i)active[i]=GetPrivateProfileIntW(L"Modules",configKeys[i],defaults[i],config.c_str())!=0;
 selected=std::clamp(int(GetPrivateProfileIntW(L"UI",L"Page",2,config.c_str())),0,2);
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 font=modernui::makeFont();
 WNDCLASSW wc{};wc.hInstance=h;wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2Toolbox020";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);HBRUSH brush=CreateSolidBrush(modernui::Background);wc.hbrBackground=brush;RegisterClassW(&wc);
 RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);frame=CreateWindowW(wc.lpszClassName,L"红警工具箱 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,std::max(550,std::min(900,int(work.bottom-work.top)-30)),nullptr,nullptr,h,nullptr);
 for(int i=0;i<3;++i){workers[i]=CreateThread(nullptr,0,run,(LPVOID)(uintptr_t)i,0,nullptr);if(!workers[i])active[i]=false;}
 ShowWindow(frame,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 for(auto worker:workers)if(worker)CloseHandle(worker);DeleteObject(font);DeleteObject(brush);CloseHandle(single);return 0;
}

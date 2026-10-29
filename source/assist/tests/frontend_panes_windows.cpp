// Actual native HWND, hit testing and SendInput after foreground transitions.
// Run on a Windows interactive desktop; cross-compilation alone proves no input.
#define UNICODE
#define _UNICODE
#include "../../suite/source/frontend_panes.h"
#include <cstdio>
#include <cstdlib>
static frontendpanes::Host host;
static std::atomic<HWND>pane{nullptr},button{nullptr};
static std::atomic<unsigned>commands{0},scrolls{0},layouts{0};
static HANDLE ready,busyStarted,busyRelease;
constexpr UINT Dpi=WM_APP+111,Busy=WM_APP+112;
static void check(bool value,const char*text){if(!value){std::fprintf(stderr,"FAIL %s error=%lu\n",text,GetLastError());std::abort();}}
static void pump(){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
template<class F>static void until(F f,const char*text){auto t=GetTickCount64();while(!f()){pump();check(GetTickCount64()-t<5000,text);Sleep(1);}pump();}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==frontendpanes::LayoutMessage){++layouts;host.apply(w,m,Dpi);return 0;}
 if(m==Dpi)return 0;
 if(m==WM_COMMAND&&LOWORD(a)==1&&HIWORD(a)==BN_CLICKED){++commands;return 0;}
 if(m==WM_VSCROLL){++scrolls;SCROLLINFO s{sizeof s,SIF_ALL};GetScrollInfo(w,SB_VERT,&s);s.fMask=SIF_POS;s.nPos=LOWORD(a)==SB_THUMBTRACK?s.nTrackPos:s.nPos+5;SetScrollInfo(w,SB_VERT,&s,TRUE);return 0;}
 if(m==Busy){SetEvent(busyStarted);check(WaitForSingleObject(busyRelease,5000)==WAIT_OBJECT_0,"busy release");return 0;}
 if(m==WM_CLOSE){DestroyWindow(w);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
static DWORD WINAPI module(void*){
 WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=proc;c.lpszClassName=L"RA2LivePaneFixture";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&c);
 auto w=CreateWindowW(c.lpszClassName,L"Real settings",WS_OVERLAPPEDWINDOW|WS_VSCROLL,0,0,360,300,nullptr,nullptr,c.hInstance,nullptr);check(w,"create pane");
 button=CreateWindowW(L"BUTTON",L"Actual button",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,20,20,200,30,w,(HMENU)1,c.hInstance,nullptr);
 SCROLLINFO s{sizeof s,SIF_RANGE|SIF_PAGE|SIF_POS};s.nMax=1000;s.nPage=100;SetScrollInfo(w,SB_VERT,&s,TRUE);
 host.adopt(0,w);pane=w;SetEvent(ready);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
}
static void mouse(DWORD flags){INPUT i{};i.type=INPUT_MOUSE;i.mi.dwFlags=flags;check(SendInput(1,&i,sizeof i)==1,"SendInput physical mouse");}
static void click(POINT p){check(SetCursorPos(p.x,p.y),"place actual pointer");mouse(MOUSEEVENTF_LEFTDOWN);mouse(MOUSEEVENTF_LEFTUP);}
int main(){
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);WNDCLASSW c{};c.hInstance=GetModuleHandleW(nullptr);c.lpfnWndProc=DefWindowProcW;c.lpszClassName=L"RA2ShellInputFixture";RegisterClassW(&c);
 auto shell=CreateWindowW(c.lpszClassName,L"Shell",WS_OVERLAPPEDWINDOW,40,40,600,480,nullptr,nullptr,c.hInstance,nullptr);host.shell(shell);ShowWindow(shell,SW_SHOW);SetForegroundWindow(shell);
 ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);busyStarted=CreateEventW(nullptr,TRUE,FALSE,nullptr);busyRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 auto worker=CreateThread(nullptr,0,module,nullptr,0,nullptr);until([]{return WaitForSingleObject(ready,0)==WAIT_OBJECT_0;},"pane startup");auto w=pane.load();
 check(!GetParent(w)&&!GetWindow(w,GW_OWNER),"independent UI queues, no cross-thread parenting/ownership");
 frontendpanes::Layout l{{90,100,450,400},true,96,1};host.request(0,l);until([&]{return IsWindowVisible(w);},"initial native page");
 auto foreign=CreateWindowW(c.lpszClassName,L"Game stand-in",WS_OVERLAPPEDWINDOW,40,40,650,500,nullptr,nullptr,c.hInstance,nullptr);ShowWindow(foreign,SW_SHOW);
 for(unsigned cycle=0;cycle<12;++cycle){
  SetForegroundWindow(foreign);until([&]{return GetForegroundWindow()==foreign;},"game foreground");check(host.canPresent(),"foreign foreground does not withdraw settings page");
  ShowWindow(shell,SW_MINIMIZE);auto hidden=l;hidden.visible=false;host.request(0,hidden);
  until([&]{return !IsWindowVisible(w);},"minimize hides real page");
  ShowWindow(shell,SW_RESTORE);SetForegroundWindow(shell);until([&]{return GetForegroundWindow()==shell;},"restore shell foreground");host.request(0,l);
  until([&]{return IsWindowVisible(w);},"restore without new layout geometry");
  // Simulate Windows finishing shell activation after the pane's show. The
  // mailbox must repair actual z-order even though every Layout field matches.
  SetWindowPos(shell,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);host.request(0,l);
  if(cycle%3==0){ShowWindow(w,SW_SHOWMINNOACTIVE);host.request(0,l);until([&]{return !IsIconic(w);},"restore an iconic content HWND without activation");}
  POINT p{60,35};ClientToScreen(w,&p);until([&]{return WindowFromPoint(p)==button.load();},"real button wins hit test after returning from game");
  auto before=commands.load();click(p);until([&]{return commands>before;},"physical mouse clicks actual button after Alt-Tab");
  RECT r{};GetWindowRect(w,&r);POINT bar{r.right-GetSystemMetrics(SM_CXVSCROLL)/2,r.bottom-9};
  check(GetAncestor(WindowFromPoint(bar),GA_ROOT)==w,"scrollbar wins actual hit test");auto old=scrolls.load();click(bar);until([&]{return scrolls>old;},"physical scrollbar click after Alt-Tab");
 }
 // A missed EXITSIZEMOVE must not leave the shell permanently in preview mode.
 host.beginInteraction();host.reconcileInteraction();check(!host.interactive(),"recover cancelled move with no native sizing loop");
 ResetEvent(busyStarted);ResetEvent(busyRelease);host.post(0,Busy);until([]{return WaitForSingleObject(busyStarted,0)==WAIT_OBJECT_0;},"busy pane");
 auto old=layouts.load();l.visible=false;for(unsigned i=0;i<10000;++i){l.rect.left=int(i);l.rect.right=int(i)+360;check(host.request(0,l),"latest-state mailbox");}
 SetEvent(busyRelease);until([&]{return layouts>old&&!IsWindowVisible(w);},"one coalesced hide consumed");check(layouts==old+1,"10000-to-1 layout coalescing");
 ShowWindow(shell,SW_MINIMIZE);check(!host.canPresent(),"minimize suppresses page");ShowWindow(shell,SW_RESTORE);SetForegroundWindow(shell);host.closing();check(!host.canPresent(),"closing suppresses page");
 host.post(0,WM_CLOSE);until([&]{return WaitForSingleObject(worker,0)==WAIT_OBJECT_0;},"owner-thread close");check(!host.window(0)&&!host.post(0,Busy),"retired handle rejects work");
 DestroyWindow(foreign);DestroyWindow(shell);for(auto h:{worker,ready,busyStarted,busyRelease})CloseHandle(h);
 std::puts("PASS actual native hit testing and physical button/scrollbar input after 12 foreground round trips; no DWM/snapshot input surface; busy-page mailbox and interrupted-drag recovery; not a real-game test");
}

// Controlled native-window boundary tests; this is not a visual/FPS or game test.
#include "../../suite/source/frontend_panes.h"
#include "../../shared/source/settings_dialog.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>

static frontendpanes::Composition compositionAPI;
static frontendpanes::Host host(&compositionAPI);
static std::atomic<HWND> pane{nullptr};
static std::atomic<unsigned> layouts{0},dpi{0},shellMessages{0},blockDpi{0};
static std::atomic<int> dialogResult{0};
static std::atomic<unsigned> prints{0},scrollInputs{0};
static HANDLE ready,busyStarted,busyRelease,idle;
static constexpr UINT Busy=WM_APP+110,Dpi=WM_APP+111,Ping=WM_APP+112,Idle=WM_APP+113,Dialog=WM_APP+114;
static constexpr UINT BeginScroll=WM_APP+115,EndScroll=WM_APP+116;
static constexpr auto DialogTitle=L"RA2 managed-pane modal fixture";
static std::atomic<bool> probeComposition{false};static std::atomic<unsigned> compositionProbes{0};
static void require(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s (error=%lu)\n",message,GetLastError());std::abort();}}
static DWORD WINAPI compositionReentry(void*){return host.post(0,Ping)?0:1;}
static HRESULT WINAPI checkedComposition(HWND w,DWORD attribute,LPCVOID data,DWORD bytes){
 if(probeComposition.exchange(false)){
  auto thread=CreateThread(nullptr,0,compositionReentry,nullptr,0,nullptr);require(thread!=nullptr,"composition reentry worker");
  require(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0,"composition call must not hold the pane mailbox mutex");
  DWORD code=1;GetExitCodeThread(thread,&code);CloseHandle(thread);require(code==0,"composition callback can publish a pane message");++compositionProbes;
 }
 auto set=frontendpanes::composition().set;return set?set(w,attribute,data,bytes):E_NOTIMPL;
}
static void pump(){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
template<class F> static void until(F f,const char* message){
 auto start=GetTickCount64();while(!f()){pump();if(GetTickCount64()-start>5000)require(false,message);Sleep(1);}pump();
}
static LRESULT CALLBACK shellProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==frontendpanes::ActivityMessage){host.present();return 0;}
 if(m==Ping){++shellMessages;return 0;}return DefWindowProcW(w,m,a,b);
}
static LRESULT CALLBACK paneProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==BeginScroll){SetCapture(w);require(GetCapture()==w,"capture on pane owner thread");return 0;}
 if(m==EndScroll){ReleaseCapture();return 0;}
 if(m==WM_VSCROLL||m==WM_MOUSEWHEEL||m==WM_COMMAND){++scrollInputs;return 0;}
 if(m==frontendpanes::LayoutMessage){++layouts;host.apply(w,m,Dpi);return 0;}
 if(m==Dpi){dpi=unsigned(a);if(blockDpi.load()==unsigned(a)){blockDpi=0;SetEvent(busyStarted);require(WaitForSingleObject(busyRelease,5000)==WAIT_OBJECT_0,"in-flight DPI fixture release");}return 0;}
 if(m==Busy){SetEvent(busyStarted);require(WaitForSingleObject(busyRelease,5000)==WAIT_OBJECT_0,"busy fixture release");return 0;}
 if(m==Idle){SetEvent(idle);return 0;}
 if(m==Dialog){dialogResult=settingsdialog::message(w,true,L"Hidden-pane owner must not conceal this prompt.",DialogTitle,MB_OK);return 0;}
 if(m==WM_PRINTCLIENT){++prints;RECT r{};GetClientRect(w,&r);auto brush=CreateSolidBrush(RGB(23,47,89));FillRect((HDC)a,&r,brush);DeleteObject(brush);return 0;}
 if(m==WM_CLOSE){DestroyWindow(w);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
static DWORD WINAPI module(void*){
 WNDCLASSW c{};c.lpfnWndProc=paneProc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"RA2FrontendPaneFixture";RegisterClassW(&c);
 auto w=CreateWindowW(c.lpszClassName,L"Busy settings",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,c.hInstance,nullptr);require(w!=nullptr,"create settings fixture");
 CreateWindowW(L"BUTTON",L"Visible setting",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,12,12,160,26,w,(HMENU)1,c.hInstance,nullptr);
 host.adopt(0,w);pane=w;SetEvent(ready);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
}
static bool rectIs(HWND w,const RECT& expected){RECT r{};return GetWindowRect(w,&r)&&EqualRect(&r,&expected);}
static void busy(){ResetEvent(busyStarted);ResetEvent(busyRelease);PostMessageW(pane.load(),Busy,0,0);until([]{return WaitForSingleObject(busyStarted,0)==WAIT_OBJECT_0;},"enter busy module");}
static void settle(){ResetEvent(idle);PostMessageW(pane.load(),Idle,0,0);until([]{return WaitForSingleObject(idle,0)==WAIT_OBJECT_0;},"module applies prior queued state");}
static void pauseDpi(unsigned next){ResetEvent(busyStarted);ResetEvent(busyRelease);blockDpi=next;}
int main(){
 compositionAPI=frontendpanes::composition();compositionAPI.set=checkedComposition;
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW c{};c.lpfnWndProc=shellProc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"RA2FrontendShellFixture";RegisterClassW(&c);
 HWND shell=CreateWindowW(c.lpszClassName,L"Settings shell",WS_OVERLAPPEDWINDOW,40,40,600,480,nullptr,nullptr,c.hInstance,nullptr);require(shell!=nullptr,"create shell");host.shell(shell);
 ShowWindow(shell,SW_SHOWNORMAL);SetForegroundWindow(shell);until([&]{return host.contains(GetForegroundWindow());},"foreground desktop needed for native activation tests");
 ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);busyStarted=CreateEventW(nullptr,TRUE,FALSE,nullptr);busyRelease=CreateEventW(nullptr,TRUE,FALSE,nullptr);idle=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 HANDLE worker=CreateThread(nullptr,0,module,nullptr,0,nullptr);require(worker!=nullptr,"start module");until([]{return WaitForSingleObject(ready,0)==WAIT_OBJECT_0;},"module ready");HWND w=pane.load();
 require(host.window(0)==w&&host.window(3)==nullptr,"only live registered handles are published");
 require(GetParent(w)==nullptr&&GetWindow(w,GW_OWNER)==nullptr,"no cross-thread parent or owner");
 require((GetWindowLongPtrW(w,GWL_STYLE)&WS_POPUP)!=0&&(GetWindowLongPtrW(w,GWL_STYLE)&WS_CHILD)==0,"independent popup");
 require((GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)!=0,"no extra taskbar/Alt-Tab entry");
 require(GetWindowThreadProcessId(w,nullptr)!=GetCurrentThreadId(),"actual independent module thread");
 frontendpanes::Layout first{{70,90,390,330},true,96,1};require(host.request(0,first),"initial layout post");
 until([&]{return IsWindowVisible(w)&&rectIs(w,first.rect)&&host.snapshot(0)!=nullptr;},"visible complete initial frame");
 auto image=host.snapshot(0);require(image->width==320&&image->height==240,"snapshot dimensions");
 require((image->pixels[size_t(150)*320+260]&0xffffffu)==0x172f59u,"last complete settings pixels, not an empty placeholder");
 unsigned widgetPixels=0;for(unsigned y=12;y<38;++y)for(unsigned x=12;x<172;++x)if((image->pixels[size_t(y)*320+x]&0xffffffu)!=0x172f59u)++widgetPixels;
 require(widgetPixels>100,"complete child-control drawing preserved in frozen image");
 require(dpi==96,"initial DPI delivery");

 host.post(0,BeginScroll);settle();auto printBefore=prints.load();
 for(unsigned i=0;i<200;++i)host.post(0,i%2?WM_VSCROLL:WM_MOUSEWHEEL);
 host.post(0,WM_COMMAND);settle();require(scrollInputs==201,"scroll and buttons still dispatch during capture");
 auto dragStarted=GetTickCount64();until([&]{return GetTickCount64()-dragStarted>300;},"pump captured scroll for idle timer");
 require(prints==printBefore,"no subtree print while scrollbar/input owns capture");
 host.post(0,EndScroll);until([&]{return prints>printBefore;},"idle snapshot resumes after capture release");
 image=host.snapshot(0);

 busy();unsigned before=layouts;auto start=GetTickCount64();probeComposition=true;host.beginInteraction();
 require(compositionProbes==1,"cloak callback reentry is exercised while module UI is busy");
 bool cloaked=false;
 if(host.synchronousConcealment(0)){
  until([&]{return host.queryCloaked(0,cloaked)&&cloaked;},"busy old-position panel immediately DWM-cloaked");
  std::puts("PASS: DWM cloak conceals the old-position popup while its module thread is busy");
 }else std::puts("SKIP: synchronous DWM cloak is unavailable; async-hide fallback is not a visual freeze guarantee");
 frontendpanes::Layout hidden=first;hidden.visible=false;
 for(unsigned i=0;i<10000;++i){hidden.rect.left=int(i);hidden.rect.right=int(i)+320;require(host.request(0,hidden),"coalesced layout post");}
 for(unsigned i=0;i<64;++i){SetWindowPos(shell,nullptr,40+int(i),40+int(i),0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);PostMessageW(shell,Ping,0,0);pump();}
 require(GetTickCount64()-start<1500,"shell must not wait for busy module");require(shellMessages==64,"shell messages continue during busy module");
 require(host.snapshot(0)==image,"last complete settings image retained during move");
 SetEvent(busyRelease);until([&]{return !IsWindowVisible(w)&&layouts>before;},"async hide consumed");
 require(layouts==before+1,"10000 requests become one latest-state notification");
 auto frozen=host.snapshot(0);require(frozen&&frozen->pixels[size_t(150)*320+260]==image->pixels[size_t(150)*320+260],"hidden panel retains complete prior pixels");
 // Repeated hidden resize requests must not enqueue or run module layouts.
 before=layouts;for(unsigned i=0;i<1000;++i){hidden.rect.bottom+=1;host.request(0,hidden);}pump();Sleep(20);pump();require(layouts==before,"no intermediate hidden resize work");
 host.endInteraction();SetForegroundWindow(shell);frontendpanes::Layout final{{110,130,510,430},true,144,2};host.request(0,final);
 until([&]{return IsWindowVisible(w)&&rectIs(w,final.rect)&&dpi==144;},"latest geometry and DPI restored after move");
 settle();
 host.present();if(host.synchronousConcealment(0)){require(host.queryCloaked(0,cloaked)&&!cloaked,"only completed latest layout is uncloaked");}
 // A->B->A while B is executing must enqueue the final A even though A was
 // the previously applied geometry. Applied-state dedup cannot ignore work
 // already in flight on a slow module thread.
 pauseDpi(192);auto overshoot=final;overshoot.dpi=192;overshoot.rect.right+=40;host.request(0,overshoot);host.present();
 until([]{return WaitForSingleObject(busyStarted,0)==WAIT_OBJECT_0;},"in-flight overshoot layout");host.request(0,final);host.present();SetEvent(busyRelease);settle();
 require(rectIs(w,final.rect)&&dpi==144,"latest A wins over in-flight B after A-B-A resize");host.present();
 // A hidden page can start showing while the user switches away. The newer
 // hide must not be deduplicated against the old already-hidden state.
 auto away=final;away.visible=false;host.request(0,away);settle();require(!IsWindowVisible(w),"page initially hidden");
 pauseDpi(192);auto showing=final;showing.dpi=192;showing.order=3;host.request(0,showing);host.present();
 until([]{return WaitForSingleObject(busyStarted,0)==WAIT_OBJECT_0;},"in-flight showing page");away=showing;away.visible=false;host.request(0,away);host.present();SetEvent(busyRelease);settle();
 require(!IsWindowVisible(w),"switch-away hide wins over late show completion");host.present();if(host.synchronousConcealment(0))require(host.queryCloaked(0,cloaked)&&cloaked,"late shown page stays cloaked");
 final.order=4;host.request(0,final);settle();host.present();
 // A completion/Activity from one move must not reveal the popup after a
 // second move has already started. Only the shell can change DWM visibility.
 busy();host.beginInteraction();final.visible=false;host.request(0,final);host.endInteraction();final.visible=true;final.order=5;host.request(0,final);
 host.beginInteraction();final.visible=false;host.request(0,final);PostMessageW(shell,frontendpanes::ActivityMessage,0,0);pump();
 if(host.synchronousConcealment(0))require(host.queryCloaked(0,cloaked)&&cloaked,"late completion cannot uncloak a new move");
 unsigned twiceBefore=layouts;SetEvent(busyRelease);until([&]{return layouts>twiceBefore&&!IsWindowVisible(w);},"second move remains hidden");
 host.endInteraction();final.visible=true;final.order=6;host.request(0,final);until([&]{return IsWindowVisible(w);},"controls resume after final move");settle();host.present();
 ShowWindow(shell,SW_MINIMIZE);final.visible=host.canPresent();require(!final.visible,"minimized shell rejects presentation");host.request(0,final);until([&]{return !IsWindowVisible(w);},"minimized shell hides settings");require(host.snapshot(0)!=nullptr,"minimize retains complete image");
 ShowWindow(shell,SW_RESTORE);SetForegroundWindow(shell);until([&]{return host.contains(GetForegroundWindow());},"restore shell foreground");

 // A close/error prompt can originate from a different, currently cloaked
 // settings page. It must not inherit that page's DWM cloak or cross-thread
 // ownership, and closing the prompt must return to the module message loop.
 auto forDialog=final;forDialog.visible=false;host.request(0,forDialog);settle();host.present();
 require(!IsWindowVisible(w),"dialog owner page is actually hidden");host.post(0,Dialog);
 HWND prompt=nullptr;until([&]{prompt=FindWindowW(L"#32770",DialogTitle);return prompt&&IsWindowVisible(prompt);},"hidden-page prompt is visible");
 require(GetWindow(prompt,GW_OWNER)==nullptr,"managed prompt has no cloaked or cross-thread owner");
 require(GetWindowThreadProcessId(prompt,nullptr)==GetWindowThreadProcessId(w,nullptr),"dialog stays on the module UI thread");
 require(IsWindowEnabled(shell)&&!IsWindowEnabled(w),"task modal prompt leaves the independent shell responsive");
 if(auto get=frontendpanes::composition().get){DWORD state=0;if(SUCCEEDED(get(prompt,14,&state,sizeof state)))require(state==0,"dialog cannot inherit the hidden pane's DWM cloak");}
 PostMessageW(prompt,WM_COMMAND,IDOK,0);until([]{return dialogResult.load()==IDOK;},"dismiss prompt and resume module UI");
 require(IsWindowEnabled(w),"module input reenabled after prompt");SetForegroundWindow(shell);settle();

 // An old queued show cannot float over a new foreground application.
 busy();final.visible=true;final.order=7;host.request(0,final);
 HWND foreign=CreateWindowW(c.lpszClassName,L"Other foreground window",WS_OVERLAPPEDWINDOW,700,40,220,180,nullptr,nullptr,c.hInstance,nullptr);ShowWindow(foreign,SW_SHOWNORMAL);SetForegroundWindow(foreign);
 until([&]{return GetForegroundWindow()==foreign;},"foreign foreground activation");unsigned blockedBefore=layouts;SetEvent(busyRelease);
 until([&]{return layouts>blockedBefore;},"stale show consumed");require(!IsWindowVisible(w),"stale show is rejected over foreign foreground");
 DestroyWindow(foreign);SetForegroundWindow(shell);host.closing();final.order=8;host.request(0,final);settle();pump();require(!IsWindowVisible(w),"closing shell cannot re-show module");
 PostMessageW(w,WM_CLOSE,0,0);until([&]{return WaitForSingleObject(worker,0)==WAIT_OBJECT_0;},"module close and cleanup");
 require(host.window(0)==nullptr,"destroyed pane handle is retired before its worker exits");
 require(host.snapshot(0)==nullptr,"destroyed pane cannot leave an interactive-looking frozen page");
 require(!host.request(0,final),"no layout message targets a retired pane handle");
 require(!host.post(0,Ping),"no setting or close message targets a retired pane handle");DestroyWindow(shell);
 for(HANDLE handle:{worker,ready,busyStarted,busyRelease,idle})CloseHandle(handle);
 std::puts("PASS: independent Win32 settings input queues; busy-reader responsiveness; 10000-to-1 layout coalescing; complete frozen pixels; final DPI/geometry; minimize/foreground/close guards");return 0;
}

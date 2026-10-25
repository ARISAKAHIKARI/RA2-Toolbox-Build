#include "../../shared/source/overlay_stack.h"
#include <cstdio>
#include <iostream>
#include <atomic>
#include <thread>
static std::atomic<const char*> phase{"startup"},statisticsPhase{"not-started"};
static std::atomic<int> iteration{-1};
static std::atomic<DWORD> statisticsThread{0};
[[noreturn]] static void fail(const char*expression,int line){
 std::fprintf(stderr,"FAIL overlay_stack_windows:%d %s error=%lu phase=%s statistics=%s iteration=%d\n",line,expression,GetLastError(),phase.load(),statisticsPhase.load(),iteration.load());std::fflush(stderr);ExitProcess(2);
}
#define CHECK(expression) do{if(!(expression))fail(#expression,__LINE__);}while(false)
static void checkpoint(const char*name){phase=name;std::cout<<"overlay-stack checkpoint: "<<name<<std::endl;}
// A window-owning thread must dispatch sent/posted messages while waiting for
// another owner. A raw wait/join can block its peer's Create/DestroyWindow.
static DWORD waitPumping(HANDLE object,DWORD timeout){
 DWORD start=GetTickCount();const DWORD count=object?1:0;
 for(;;){DWORD elapsed=GetTickCount()-start;if(elapsed>=timeout)return WAIT_TIMEOUT;
  DWORD result=MsgWaitForMultipleObjectsEx(count,object?&object:nullptr,timeout-elapsed,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
  if(object&&result==WAIT_OBJECT_0)return result;
  if(result!=WAIT_OBJECT_0+count)return result;
  MSG message;for(int n=0;n<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++n){CHECK(message.message!=WM_QUIT);TranslateMessage(&message);DispatchMessageW(&message);}
 }
}
static bool higher(HWND a,HWND b){for(HWND w=GetTopWindow(nullptr);w;w=GetWindow(w,GW_HWNDNEXT)){if(w==a)return true;if(w==b)return false;}return false;}
static std::atomic<unsigned> positionCallbacks{0};
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_WINDOWPOSCHANGED){bool statistics=GetCurrentThreadId()==statisticsThread;if(statistics)statisticsPhase="position-callback-lock";{std::lock_guard<std::mutex> lock(overlaystack::mutex);++positionCallbacks;}if(statistics)statisticsPhase="message-loop";}
 if(m==WM_CLOSE){statisticsPhase="close-forget";overlaystack::forget(w);statisticsPhase="close-destroy";CHECK(DestroyWindow(w));statisticsPhase="close-quit";PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 HANDLE finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(finished);
 // This thread owns no HWND and never needs a message pump. It also catches
 // a hang inside a synchronous User32 call, before waitPumping can run.
 std::thread watchdog([finished]{if(WaitForSingleObject(finished,20000)!=WAIT_OBJECT_0){char message[512];int n=std::snprintf(message,sizeof message,"FAIL overlay-stack watchdog (20s): phase=%s statistics=%s iteration=%d\n",phase.load(),statisticsPhase.load(),iteration.load());DWORD written=0;if(n>0)WriteFile(GetStdHandle(STD_ERROR_HANDLE),message,DWORD(std::min<int>(n,sizeof message-1)),&written,nullptr);TerminateProcess(GetCurrentProcess(),124);}});
 checkpoint("register-class");
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"RA2OverlayStackFixture";CHECK(RegisterClassW(&cls));
 auto create=[&](){auto w=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_POPUP,10,20,800,600,nullptr,nullptr,cls.hInstance,nullptr);CHECK(w);return w;};
 checkpoint("create-main-windows");
 HWND game=create(),world=create(),map=create(),watch=create(),cursor=create();std::atomic<HWND> statsHandle{nullptr};
 HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(ready);
 checkpoint("start-statistics-owner");
 std::thread statisticsOwner([&]{statisticsThread=GetCurrentThreadId();statisticsPhase="create-window";statsHandle=create();statisticsPhase="ready";CHECK(SetEvent(ready));statisticsPhase="message-loop";MSG msg;BOOL result;while((result=GetMessageW(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageW(&msg);}CHECK(result==0);statisticsPhase="thread-return";});
 checkpoint("wait-statistics-ready");
 CHECK(waitPumping(ready,5000)==WAIT_OBJECT_0);CloseHandle(ready);HWND stats=statsHandle;CHECK(GetWindowThreadProcessId(stats,nullptr)!=GetCurrentThreadId());
 checkpoint("show-and-place");
 ShowWindow(game,SW_SHOWNOACTIVATE);
 using namespace overlaystack;auto put=[&](HWND w,Layer l){phase=l==Layer::Statistics?"place-statistics":l==Layer::World?"place-world":l==Layer::Minimap?"place-minimap":l==Layer::Watch?"place-watch":"place-cursor";CHECK(place(w,l,110,140,200,100,SWP_SHOWWINDOW));};
 put(world,Layer::World);put(map,Layer::Minimap);put(cursor,Layer::Cursor);put(stats,Layer::Statistics);put(watch,Layer::Watch);
 checkpoint("repeat-stacking");
 for(int i=0;i<100;++i){iteration=i;put(world,Layer::World);put(stats,Layer::Statistics);put(map,Layer::Minimap);CHECK(higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world));}iteration=-1;
 // Promote a real topmost game HWND while all surfaces remain WS_VISIBLE.
 // Pass that controlled foreground candidate to the production group check;
 // the fixture does not seize the runner's actual desktop foreground.
 checkpoint("promoted-game-recovery");HWND foreground=GetForegroundWindow();unsigned callbacks=positionCallbacks;
 for(int i=0;i<4;++i){iteration=i;phase="promote-game";CHECK(SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE));CHECK(higher(game,cursor));phase="recovery-interval";CHECK(waitPumping(nullptr,260)==WAIT_TIMEOUT);phase="keep-above-batch";CHECK(keepAbove(game));
  for(auto w:{world,stats,map,watch,cursor})CHECK(IsWindowVisible(w)&&higher(w,game));
  CHECK(higher(cursor,watch)&&higher(watch,map)&&higher(map,stats)&&higher(stats,world));
 }
 iteration=-1;CHECK(positionCallbacks>callbacks&&GetForegroundWindow()==foreground);
 checkpoint("opaque-and-hidden-panels");
 auto boxes=opaqueBoxes(game,800,600);CHECK(boxes.size()==2);for(auto r:boxes)CHECK(r.left==99&&r.top==119&&r.right==301&&r.bottom==221);
 ShowWindow(map,SW_HIDE);CHECK(SetWindowPos(game,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE));CHECK(waitPumping(nullptr,260)==WAIT_TIMEOUT);phase="keep-hidden-above-batch";CHECK(keepAbove(game));
 CHECK(!IsWindowVisible(map)&&higher(world,game)&&opaqueBoxes(game,800,600).size()==1);put(map,Layer::Minimap);CHECK(higher(watch,map)&&higher(map,world));
 checkpoint("stale-handle");CHECK(DestroyWindow(watch));put(world,Layer::World);CHECK(opaqueBoxes(game,800,600).size()==1);
 checkpoint("close-statistics");CHECK(PostMessageW(stats,WM_CLOSE,0,0));
 // Wait for the actual owner thread to exit while servicing this thread's
 // windows. join() is safe only after its native handle is already signaled.
 checkpoint("wait-statistics-exit");CHECK(waitPumping(static_cast<HANDLE>(statisticsOwner.native_handle()),5000)==WAIT_OBJECT_0);statisticsOwner.join();
 checkpoint("destroy-main-windows");for(auto w:{world,map,cursor,game}){forget(w);CHECK(DestroyWindow(w));}
 checkpoint("complete");CHECK(SetEvent(finished));watchdog.join();CloseHandle(finished);
 std::cout<<"PASS real HWND stacking, four promoted-game recoveries, cross-owner-thread position callbacks outside the registry lock, no activation, hidden-panel preservation, stale handles and opaque clipping; no actual game/Alt-Tab/DWM visual claim\n";
}

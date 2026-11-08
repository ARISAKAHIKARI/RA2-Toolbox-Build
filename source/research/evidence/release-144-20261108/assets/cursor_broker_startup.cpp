
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
using DWORD=uint32_t;static constexpr DWORD ERROR_IO_PENDING=997;
static DWORD tick=1000,lastError=0,hookStartupAttemptAt=0,lastAutoCheck=0;
static bool hookStartupPending=false,autoStart=true,autoPaused=false,running=false,playbackReady=true,alive=true,window=true;
static int runtimeState=0,runtimeChecks=0,readyAdmissions=0,attaches=0,starts=0,pid=7,threadId=8,game=1;
static std::wstring lastStatus;static int stops=0,autoStartBox=2,autoBoxChecked=0;
static constexpr int BM_SETCHECK=1,BM_GETCHECK=2,BST_CHECKED=1,BST_UNCHECKED=0;
static DWORD GetTickCount(){return tick;}static DWORD GetLastError(){return lastError;}
static bool selectGameWindow(){return window;}static bool IsWindow(int){return window;}static bool live(){return alive;}
static void fail(const char*s){throw std::runtime_error(s);}static void status(const std::wstring&s){lastStatus=s;}
static std::wstring widen(const char*s){std::string value=s;return std::wstring(value.begin(),value.end());}
namespace gameinput {static bool nativeWindow(int,int){return window;}static bool runtimeAvailable(){++runtimeChecks;lastError=runtimeState==0?ERROR_IO_PENDING:193;return runtimeState==1;}}
struct Reader{bool valid(){return alive;}};static Reader cursorReader;static void syncTargetSelection(){}
static bool hookStart(bool hide=true){
 const DWORD now=GetTickCount();if(hookStartupPending&&uint32_t(now-hookStartupAttemptAt)<75)return false;
 hookStartupPending=false;hookStartupAttemptAt=now;
 if(!selectGameWindow()||!gameinput::nativeWindow(pid,threadId))fail("Game main window is still initializing; waiting to connect");
 if(!gameinput::runtimeAvailable()){
  const DWORD error=GetLastError();
  if(error==ERROR_IO_PENDING){hookStartupPending=true;status(L"正在连接光标运行组件…");return false;}
  fail("GameRuntime.dll or x86 hook broker missing/incompatible; replace the complete paired package");
 }
 ++readyAdmissions;return true;}

static void attach(bool){++attaches;alive=window=true;}static void start(){++starts;if(hookStart())running=true;}
static void stop(){++stops;hookStartupPending=false;running=false;}
static void updatePrimaryAction(){}static std::wstring ini(){return L"settings.ini";}
static int WritePrivateProfileStringW(const wchar_t*,const wchar_t*,const wchar_t*,const wchar_t*){return 1;}
static int SendMessageW(int,int message,int value,int){if(message==BM_SETCHECK)autoBoxChecked=value;return autoBoxChecked;}
static void togglePrimary(){
 if(autoStart&&!autoPaused){
  autoStart=false;autoPaused=true;
  if(running||hookStartupPending)stop();
  status(L"已关闭外部光标与自动检测。原游戏光标是否显示由当前隐藏模式决定。");
 }else{
  autoStart=true;autoPaused=false;lastAutoCheck=0;
  status(L"已开启自动检测：等待兼容游戏窗口（支持随机进程名）。");
 }
 WritePrivateProfileStringW(L"Startup",L"AutoStart",autoStart?L"1":L"0",ini().c_str());
 if(IsWindow(autoStartBox))SendMessageW(autoStartBox,BM_SETCHECK,autoStart?BST_CHECKED:BST_UNCHECKED,0);
 updatePrimaryAction();
}
static void checkboxChanged(){autoStart=SendMessageW(autoStartBox,BM_GETCHECK,0,0)==BST_CHECKED;autoPaused=!autoStart;if(!autoStart&&(running||hookStartupPending))stop();WritePrivateProfileStringW(L"Startup",L"AutoStart",autoStart?L"1":L"0",ini().c_str());updatePrimaryAction();}
static void autoTick(){syncTargetSelection();DWORD now=GetTickCount();if(uint32_t(now-lastAutoCheck)<(hookStartupPending?75u:1000u))return;lastAutoCheck=now;if(autoPaused&&cursorReader.valid()&&!live())autoPaused=false;if(!autoStart||autoPaused||running||!playbackReady)return;try{if(!hookStartupPending||!live()||!IsWindow(game))attach(true);start();}catch(const std::exception& e){hookStartupPending=false;status(L"自动等待："+widen(e.what()));}}
static int hook=0,unmaps=0,closes=0,wakes=0;static void*shared=nullptr,*mapping=nullptr;
static int UnmapViewOfFile(void*){++unmaps;return 1;}static int CloseHandle(void*){++closes;return 1;}
namespace gameinput {static void wake(int){++wakes;}}
static bool finishHookStart(){if(!hook){
  const DWORD error=GetLastError();
  if(error==ERROR_IO_PENDING){
   // Host has not admitted a lease. Release only this unsubmitted mapping;
   // there is no game subscription whose restore acknowledgement to await.
   if(shared){UnmapViewOfFile(shared);shared=nullptr;}
   if(mapping){CloseHandle(mapping);mapping=nullptr;}
   hookStartupPending=true;status(L"正在等待上一条光标连接退出…");return false;
  }
  fail("Unified runtime subscription failed");
 }gameinput::wake(hook);return true;}
int main(){unsigned checks=0;auto check=[&](bool v){assert(v);++checks;};
 check(!hookStart()&&hookStartupPending&&runtimeChecks==1&&!readyAdmissions);
 tick+=74;check(!hookStart()&&runtimeChecks==1);tick+=1;runtimeState=1;check(hookStart()&&!hookStartupPending&&runtimeChecks==2&&readyAdmissions==1);
 runtimeState=0;tick+=75;check(!hookStart()&&hookStartupPending);runtimeState=-1;tick+=75;bool failed=false;try{hookStart();}catch(const std::runtime_error&e){failed=std::string(e.what()).find("missing/incompatible")!=std::string::npos;}check(failed&&!hookStartupPending&&readyAdmissions==1);
 window=false;failed=false;try{hookStart();}catch(const std::runtime_error&e){failed=std::string(e.what()).find("window")!=std::string::npos;}check(failed);
 alive=window=false;runtimeState=0;tick=2000;lastAutoCheck=0;starts=attaches=0;running=false;autoTick();check(attaches==1&&starts==1&&hookStartupPending&&!running);
 tick+=74;autoTick();check(attaches==1&&starts==1);tick+=1;autoTick();check(attaches==1&&starts==2&&hookStartupPending&&!running);
 tick+=75;runtimeState=1;autoTick();check(attaches==1&&starts==3&&running&&!hookStartupPending);
 running=false;runtimeState=-1;tick+=1000;autoTick();check(!hookStartupPending&&lastStatus.find(L"自动等待")!=std::wstring::npos);int before=starts;tick+=75;autoTick();check(starts==before);
 hookStartupPending=true;hookStartupAttemptAt=UINT32_MAX-20;tick=40;runtimeState=1;before=runtimeChecks;check(!hookStart()&&runtimeChecks==before);tick=60;check(hookStart()&&!hookStartupPending);
 // Actual enable/disable handlers cancel pending attempts before any subscription exists.
 autoStart=true;autoPaused=false;running=false;hookStartupPending=true;togglePrimary();check(!autoStart&&autoPaused&&!hookStartupPending&&stops==1);
 before=starts;tick+=5000;autoTick();check(starts==before&&!hookStartupPending);
 autoStart=false;autoPaused=true;togglePrimary();check(autoStart&&!autoPaused&&lastAutoCheck==0);
 runtimeState=0;autoTick();check(hookStartupPending&&!running);
 autoBoxChecked=BST_UNCHECKED;checkboxChanged();check(!autoStart&&autoPaused&&!hookStartupPending&&stops==2);
 before=starts;tick+=5000;autoTick();check(starts==before&&!hookStartupPending);
 autoStart=true;autoPaused=true;togglePrimary();check(autoStart&&!autoPaused); // Paused explicit enable uses the same existing enabled state.
 // install() may become pending after the local Cursor mapping is allocated.
 hook=0;shared=reinterpret_cast<void*>(1);mapping=reinterpret_cast<void*>(2);lastError=ERROR_IO_PENDING;hookStartupPending=false;
 check(!finishHookStart()&&hookStartupPending&&!shared&&!mapping&&unmaps==1&&closes==1&&wakes==0);
 shared=reinterpret_cast<void*>(1);mapping=reinterpret_cast<void*>(2);lastError=193;hookStartupPending=false;failed=false;
 try{finishHookStart();}catch(const std::runtime_error&e){failed=std::string(e.what()).find("subscription failed")!=std::string::npos;}
 check(failed&&!hookStartupPending&&shared&&mapping&&unmaps==1&&closes==1&&wakes==0);
 hook=1;check(finishHookStart()&&wakes==1); // A valid subscription is retained even while the Host's hook ACK is pending.
 std::printf("Cursor broker startup production admission/tick: %u checks passed; pending retries at 75 ms, reader retained, genuine errors preserved\n",checks);
}

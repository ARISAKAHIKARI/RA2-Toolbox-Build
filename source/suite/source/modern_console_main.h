#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include "../../shared/source/release_version.h"
#include "../../shared/source/initial_profiles.h"
#include "../../shared/source/bounded_shutdown.h"
#include "../../shared/source/async_ini_store.h"
#include "suite_host.h"
#include "web_console.h"

extern "C" int WINAPI CursorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI MonitorEntry(HINSTANCE,HINSTANCE,LPWSTR,int);
extern "C" int WINAPI TacticalEntry(HINSTANCE,HINSTANCE,LPWSTR,int);

namespace consolemain {
constexpr UINT PaneReady=WM_APP+60,EnabledChanged=WM_APP+61,CloseDecision=WM_APP+62;
constexpr UINT BootReady=WM_APP+121,BackendStopped=WM_APP+122,ConfigChanged=WM_APP+123;
constexpr UINT_PTR CloseTimer=0x4191;
static HWND frame=nullptr,errorLabel=nullptr,retryButton=nullptr,installButton=nullptr;
static std::atomic<HWND> notificationWindow{nullptr};
static std::atomic<bool> configNoticePending{false};
static HINSTANCE instance=nullptr;
static HANDLE bootstrap=nullptr,workers[3]{};
static std::atomic<bool> active[3]{};
static std::atomic<bool> statsShown{false},hardwarePointer{false};
static std::atomic<unsigned> commonRefreshRate{0};
static std::mutex displayMutex;
static suite::DisplayState displayState;
static bool displayReady=false;
static consoleweb::Host browser;
static std::wstring root,config,args;
static std::string error,lastShell;
static bool appReady=false,bootReady=false,servicesStarted=false,userNavigated=false,closeAsked=false,storeClosing=false;
static int selectedCategory=0,videoSection=0,closePhase=0;
static HFONT errorFont=nullptr;
static void onError(const std::wstring& text);
struct BootSettings {bool ok=false,cursor=false,monitor=false;int category=0,video=0;};
static BootSettings bootSettings;
static std::mutex bootMutex;

static std::string utf8(const std::wstring& text){
 if(text.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
 if(n<=0)return {};std::string out(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),out.data(),n,nullptr,nullptr);return out;
}
static std::string quote(const std::string& value){return consolemodel::detail::quote(value);}
static bool done(unsigned module){return !workers[module]||WaitForSingleObject(workers[module],0)==WAIT_OBJECT_0;}
static void configNotify(void*){if(auto w=notificationWindow.load())if(!configNoticePending.exchange(true)&&!PostMessageW(w,ConfigChanged,0,0))configNoticePending=false;}
static bool send(const std::string& json){
 if(browser.post(json))return true;
 if(appReady)onError(json.size()>2*1024*1024?L"设置数据超出控制台处理范围，请重新打开工具箱。":L"控制台连接中断，请点击重试以恢复界面。");
 return false;
}
static void saveModuleState(){
 if(!bootReady||!servicesStarted)return;
 asyncini::write(L"Video",L"CursorEnabled",active[0]?L"1":L"0",config.c_str());
 asyncini::write(L"Video",L"MonitorEnabled",active[1]?L"1":L"0",config.c_str());
}
static void publishShell(){
 if(!appReady||!browser.ready())return;
 auto storage=asyncini::status();
 std::string json="{\"kind\":\"shell\",\"protocol\":1,\"version\":"+quote(utf8(RA2_RELEASE_VERSION_W))+",\"servicesReady\":"+(servicesStarted?"true":"false")+
  ",\"selectedCategory\":"+std::to_string(selectedCategory)+",\"videoSection\":"+std::to_string(videoSection)+",\"closing\":"+(closePhase?"true":"false")+",\"error\":"+quote(error)+",\"modules\":[";
 for(unsigned i=0;i<3;++i){if(i)json+=',';json+="{\"module\":"+std::to_string(i)+",\"ready\":"+(suite::frontend.window(i)?"true":"false")+",\"enabled\":"+(active[i]?"true":"false")+"}";}
 json+="],\"config\":{\"pending\":"+std::to_string(storage.pending)+",\"busy\":"+(storage.busy?"true":"false")+",\"failed\":"+(storage.failed?"true":"false")+",\"error\":"+
  quote(storage.failed?utf8(L"设置保存失败，请检查文件夹写入权限。 "+storage.errorPath):std::string{})+"}}";
 if(json!=lastShell&&send(json))lastShell=std::move(json);
}
static void publishModels(){
 if(!appReady||!browser.ready())return;
 consolemodel::Ack ack;
 unsigned count=0;
 while(count<32&&suite::frontend.takeAck(ack)){if(!send(consolemodel::encodeAck(ack)))return;++count;}
 if(count==32)PostMessageW(frame,consoleadapter::ChangedMessage,0,0);
 if(auto snapshot=suite::frontend.takeSnapshot())if(!send(consolemodel::encodeSnapshot(*snapshot)))return;
 publishShell();
}
static void selectPage(bool persist){
 const unsigned module=selectedCategory==0?unsigned(videoSection):2u;
 suite::frontend.navigate(module,selectedCategory==0?0:selectedCategory,selectedCategory==0?(videoSection?"monitor":"cursor"):"tactical/"+std::to_string(selectedCategory));
 if(persist&&bootReady){asyncini::write(L"UI",L"Category5",std::to_wstring(selectedCategory).c_str(),config.c_str());asyncini::write(L"UI",L"VideoSection",std::to_wstring(videoSection).c_str(),config.c_str());}
}
static DWORD WINAPI runModule(void* context){
 const unsigned i=unsigned(reinterpret_cast<uintptr_t>(context));auto command=const_cast<LPWSTR>(args.c_str());
 const int code=i==0?CursorEntry(instance,nullptr,command,SW_HIDE):i==1?MonitorEntry(instance,nullptr,command,SW_HIDE):TacticalEntry(instance,nullptr,command,SW_HIDE);
 if(auto w=notificationWindow.load())PostMessageW(w,BackendStopped,i,code);return DWORD(code);
}
static void startServices(){
 if(!appReady||!bootReady||servicesStarted||closePhase)return;
 BootSettings settings;{std::lock_guard<std::mutex> lock(bootMutex);settings=bootSettings;}
 if(!settings.ok){error=utf8(L"无法建立默认配置，请将工具箱放到可写入的文件夹后重试。");publishShell();return;}
 active[0]=settings.cursor;active[1]=settings.monitor;active[2]=true;
 if(!userNavigated){selectedCategory=settings.category;videoSection=settings.video;}
 servicesStarted=true;selectPage(false);
 for(unsigned i=0;i<3;++i){workers[i]=CreateThread(nullptr,0,runModule,reinterpret_cast<void*>(uintptr_t(i)),0,nullptr);if(!workers[i]){active[i]=false;error=utf8(L"部分功能服务未能启动，请重新打开工具箱。");}}
 publishShell();
}
static DWORD WINAPI prepareSettings(void*){
 BootSettings result;result.ok=initialprofiles::initialize(root);
 if(result.ok){
  auto read=[&](const wchar_t* section,const wchar_t* key,int fallback){return int(GetPrivateProfileIntW(section,key,fallback,config.c_str()));};
  const int cursorDefault=int(GetPrivateProfileIntW(L"Startup",L"AutoStart",0,(root+L"\\settings.ini").c_str()));
  result.cursor=read(L"Video",L"CursorEnabled",read(L"Modules",L"Cursor",cursorDefault))!=0;
  result.monitor=read(L"Video",L"MonitorEnabled",read(L"Modules",L"Units",0))!=0;
  result.category=std::clamp(read(L"UI",L"Category5",0),0,4);result.video=std::clamp(read(L"UI",L"VideoSection",0),0,1);
 }
 {std::lock_guard<std::mutex> lock(bootMutex);bootSettings=result;}
 if(auto w=notificationWindow.load())PostMessageW(w,BootReady,0,0);return 0;
}
static void beginShutdown(){
 if(closePhase==2)return;closePhase=2;suite::frontend.closing();boundedshutdown::begin();saveModuleState();publishShell();
 SetWindowTextW(frame,L"红警工具箱 · 正在退出…");
 for(unsigned i=0;i<3;++i)suite::frontend.post(i,WM_CLOSE,suite::CloseToken);
}
static void requestClose(){
 if(closePhase)return;closePhase=1;closeAsked=false;publishShell();
 SetTimer(frame,CloseTimer,50,nullptr);
 if(!servicesStarted||done(0))beginShutdown();
 else closeAsked=suite::frontend.post(0,suite::CloseCheck);
}
static void onMessage(const std::string& json){
 consolemodel::Message message;std::string reason;
 if(!consolemodel::parseMessage(json,message,&reason)){error=utf8(L"操作请求无效：")+reason;publishShell();return;}
 if(message.kind==consolemodel::MessageKind::Ready){
  const bool newConnection=!appReady;appReady=true;lastShell.clear();
  if(newConnection&&servicesStarted&&!closePhase)suite::frontend.renewSession();
  startServices();publishShell();if(auto state=suite::frontend.latestSnapshot())send(consolemodel::encodeSnapshot(*state));publishModels();return;
 }
 if(message.kind==consolemodel::MessageKind::Close){requestClose();return;}
 if(closePhase)return;
 if(message.kind==consolemodel::MessageKind::Navigate){
  userNavigated=true;selectedCategory=message.shell.module==2?message.shell.category:0;
  if(message.shell.module<2)videoSection=int(message.shell.module);error.clear();selectPage(servicesStarted);publishShell();return;
 }
 if(message.kind==consolemodel::MessageKind::Module){
  const unsigned i=message.shell.module;
  if(i>1||!servicesStarted||!suite::frontend.post(i,suite::ModuleEnable,message.shell.enabled)){error=utf8(L"功能服务尚未就绪，请稍后重试。");}
  else error.clear();publishShell();return;
 }
 if(message.kind==consolemodel::MessageKind::Action){
  if(!servicesStarted||!suite::frontend.post(message.action))send(consolemodel::encodeAck(message.action,false,utf8(L"服务繁忙或正在启动，请稍后重试。")));
 }
}
static void resizeError(){RECT r{};GetClientRect(frame,&r);MoveWindow(errorLabel,36,42,std::max(1,int(r.right)-72),160,FALSE);MoveWindow(retryButton,36,218,128,34,FALSE);MoveWindow(installButton,180,218,212,34,FALSE);}
static void onError(const std::wstring& text){
 appReady=false;lastShell.clear();browser.close();SetWindowTextW(errorLabel,text.c_str());
 ShowWindow(errorLabel,SW_SHOWNA);ShowWindow(retryButton,SW_SHOWNA);ShowWindow(installButton,SW_SHOWNA);resizeError();InvalidateRect(frame,nullptr,FALSE);
}
static void createBrowser(){
 ShowWindow(errorLabel,SW_HIDE);ShowWindow(retryButton,SW_HIDE);ShowWindow(installButton,SW_HIDE);
 appReady=false;lastShell.clear();browser.close();
 if(!browser.create(frame,onMessage,onError)&&!IsWindowVisible(errorLabel)){
  // Host has queued a specific error. Keep its dispatcher alive so that the
  // detailed installation/cache failure can replace this temporary message.
  SetWindowTextW(errorLabel,L"控制台启动失败，正在读取原因…");
  ShowWindow(errorLabel,SW_SHOWNA);ShowWindow(retryButton,SW_SHOWNA);ShowWindow(installButton,SW_SHOWNA);
 }
 RECT r{};GetClientRect(frame,&r);browser.resize(r);
}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
 switch(m){
 case WM_CREATE:{
  frame=w;notificationWindow=w;suite::frontend.shell(w);
  errorFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
  errorLabel=CreateWindowW(L"STATIC",L"正在启动控制台…",WS_CHILD|SS_LEFT,0,0,1,1,w,nullptr,instance,nullptr);
  retryButton=CreateWindowW(L"BUTTON",L"重试",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,w,reinterpret_cast<HMENU>(1),instance,nullptr);
  installButton=CreateWindowW(L"BUTTON",L"WebView2 安装说明",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,w,reinterpret_cast<HMENU>(2),instance,nullptr);
  for(auto control:{errorLabel,retryButton,installButton})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(errorFont),FALSE);
  asyncini::setNotify([]{configNotify(nullptr);});createBrowser();return 0;
 }
 case BootReady:bootReady=true;startServices();if(closePhase&&!servicesStarted)beginShutdown();return 0;
 case PaneReady:{const unsigned i=unsigned(a);if(i<3){suite::frontend.post(i,suite::ModuleEnable,active[i]);if(closePhase==2)suite::frontend.post(i,WM_CLOSE,suite::CloseToken);}publishModels();return 0;}
 case EnabledChanged:saveModuleState();publishShell();return 0;
 case consoleadapter::ChangedMessage:publishModels();return 0;
 case ConfigChanged:configNoticePending=false;publishShell();return 0;
 case BackendStopped:if(unsigned(a)<3){active[unsigned(a)]=false;if(!closePhase){error=utf8(L"部分功能服务已停止，请重新打开工具箱。");publishShell();}}return 0;
 case WM_COMMAND:if(HIWORD(a)==BN_CLICKED){if(LOWORD(a)==1){createBrowser();return 0;}if(LOWORD(a)==2){ShellExecuteW(w,L"open",L"https://developer.microsoft.com/microsoft-edge/webview2/#download-section",nullptr,nullptr,SW_SHOWNORMAL);return 0;}}break;
 case WM_DISPLAYCHANGE:for(unsigned i=0;i<3;++i)suite::frontend.post(i,m,a,b);return 0;
 case WM_SIZE:{RECT r{};GetClientRect(w,&r);browser.resize(r);resizeError();return 0;}
 case WM_DPICHANGED:{auto r=reinterpret_cast<RECT*>(b);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(b)->ptMinTrackSize={760,560};return 0;
 case WM_CLOSE:requestClose();return 0;
 case CloseDecision:if(closePhase==1){if(a)beginShutdown();else{closePhase=0;closeAsked=false;KillTimer(w,CloseTimer);SetWindowTextW(w,L"红警工具箱 v" RA2_RELEASE_VERSION_W);publishShell();}}return 0;
 case WM_TIMER:if(a==CloseTimer){
  if(closePhase==1){if(done(0))beginShutdown();else if(!closeAsked)closeAsked=suite::frontend.post(0,suite::CloseCheck);}
  if(closePhase==2&&done(0)&&done(1)&&done(2)&&(!bootstrap||WaitForSingleObject(bootstrap,0)==WAIT_OBJECT_0)){
   if(!storeClosing){asyncini::requestFlush();asyncini::beginShutdown();storeClosing=true;}
   if(asyncini::idle())DestroyWindow(w);
  }return 0;
 }break;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);SetDCBrushColor(dc,RGB(17,22,28));FillRect(dc,&r,reinterpret_cast<HBRUSH>(GetStockObject(DC_BRUSH)));EndPaint(w,&ps);return 0;}
 case WM_CTLCOLORSTATIC:{auto dc=reinterpret_cast<HDC>(a);SetTextColor(dc,RGB(220,230,238));SetBkColor(dc,RGB(17,22,28));SetDCBrushColor(dc,RGB(17,22,28));return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));}
 case WM_DESTROY:notificationWindow=nullptr;asyncini::setNotify({});KillTimer(w,CloseTimer);browser.close();PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(w,m,a,b);
}
}

namespace suite {
 HWND parent(){return consolemain::frame;}
 bool enabled(int module){return module>=0&&module<3&&consolemain::active[module].load();}
 void registerPane(int module,HWND pane,WNDPROC ownerProc){
  if(module<0||module>=3)return;
  if(!frontend.adopt(unsigned(module),pane,ownerProc)){PostMessageW(pane,WM_CLOSE,CloseToken,0);PostMessageW(consolemain::frame,consolemain::BackendStopped,module,0);return;}
  frontend.post(unsigned(module),DisplaySettingsChanged);
  if(module==2)frontend.post(2,RefreshRateChanged);
  PostMessageW(consolemain::frame,consolemain::PaneReady,module,reinterpret_cast<LPARAM>(pane));
 }
 void reportEnabled(int module,bool value){if(module>=0&&module<3&&consolemain::active[module].exchange(value)!=value)PostMessageW(consolemain::frame,consolemain::EnabledChanged,module,value);}
 void requestClose(){PostMessageW(consolemain::frame,WM_CLOSE,0,0);}
 void closeDecision(bool allow){PostMessageW(consolemain::frame,consolemain::CloseDecision,allow,0);}
 void setStatsVisible(bool value){consolemain::statsShown=value;}
 bool statsVisible(){return consolemain::statsShown.load();}
 void publishHardwareCursor(bool value){consolemain::hardwarePointer=value;}
 bool hardwareCursorActive(){return consolemain::hardwarePointer.load();}
 void publishDisplaySettings(const DisplayState& value){
  {std::lock_guard<std::mutex> lock(consolemain::displayMutex);if(consolemain::displayReady&&consolemain::displayState==value)return;consolemain::displayState=value;consolemain::displayReady=true;}
  frontend.post(0,DisplaySettingsChanged);frontend.post(1,DisplaySettingsChanged);
 }
 bool readDisplaySettings(DisplayState& out){std::lock_guard<std::mutex> lock(consolemain::displayMutex);if(!consolemain::displayReady)return false;out=consolemain::displayState;return true;}
 bool requestDisplaySetting(int option,int value){return validDisplayEdit(option,value)&&frontend.post(2,ApplyDisplaySetting,option,value);}
 void publishRefreshRate(unsigned hz){if(hz<30||hz>500||consolemain::commonRefreshRate.exchange(hz)==hz)return;frontend.post(2,RefreshRateChanged);}
 unsigned refreshRate(){return consolemain::commonRefreshRate.load();}
}

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR command,int show){
 using namespace consolemain;
 HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\RA2Toolbox_v020");
 if(GetLastError()==ERROR_ALREADY_EXISTS){if(auto old=FindWindowW(L"RA2Toolbox020",nullptr)){ShowWindow(old,SW_RESTORE);SetForegroundWindow(old);}if(single)CloseHandle(single);return 0;}
 instance=h;args=command?command:L"";wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);root=path;auto slash=root.find_last_of(L"\\/");if(slash!=std::wstring::npos)root.resize(slash);config=root+L"\\Toolbox.ini";
 const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 if(FAILED(com)){MessageBoxW(nullptr,L"无法初始化控制台界面，请重新启动工具箱。",L"红警工具箱",MB_OK|MB_ICONERROR);if(single)CloseHandle(single);return 1;}
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW wc{};wc.hInstance=h;wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2Toolbox020";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
 RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
 frame=CreateWindowW(wc.lpszClassName,L"红警工具箱 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,std::min(1220,int(work.right-work.left)-20),std::min(900,int(work.bottom-work.top)-30),nullptr,nullptr,h,nullptr);
 if(!frame){CoUninitialize();if(single)CloseHandle(single);return 1;}
 ShowWindow(frame,show);bootstrap=CreateThread(nullptr,0,prepareSettings,nullptr,0,nullptr);
 if(!bootstrap){bootReady=true;error=utf8(L"配置服务未能启动，请重新打开工具箱。");publishShell();}
 MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
 notificationWindow=nullptr;asyncini::setNotify({});browser.close();
 // GetMessage failure or an external WM_QUIT also follows bounded service
 // teardown. Never release global adapter storage while its threads use it.
 if(!done(0)||!done(1)||!done(2)||(bootstrap&&WaitForSingleObject(bootstrap,0)!=WAIT_OBJECT_0)){
  suite::frontend.closing();boundedshutdown::begin();
  for(unsigned i=0;i<3;++i)suite::frontend.post(i,WM_CLOSE,suite::CloseToken);
  const auto deadline=GetTickCount64()+1500;
  auto remaining=[&](){const auto now=GetTickCount64();return now<deadline?DWORD(deadline-now):0u;};
  for(auto thread:workers)if(thread)WaitForSingleObject(thread,remaining());
  if(bootstrap)WaitForSingleObject(bootstrap,remaining());
  if(!done(0)||!done(1)||!done(2)||(bootstrap&&WaitForSingleObject(bootstrap,0)!=WAIT_OBJECT_0))TerminateProcess(GetCurrentProcess(),1);
 }
 if(!storeClosing){
  asyncini::requestFlush();asyncini::beginShutdown();
  const auto flushDeadline=GetTickCount64()+400;
  while(!asyncini::idle()&&GetTickCount64()<flushDeadline)Sleep(5);
 }
 for(auto thread:workers)if(thread)CloseHandle(thread);if(bootstrap)CloseHandle(bootstrap);if(errorFont)DeleteObject(errorFont);if(single)CloseHandle(single);CoUninitialize();return 0;
}

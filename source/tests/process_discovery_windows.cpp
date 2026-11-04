// Controlled child processes exercise real Win32 discovery without a game binary.
#include "../external_reader/source/process_discovery.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

static void require(bool ok,const char* message){
 if(!ok){std::fprintf(stderr,"FAIL %s (Win32=%lu)\n",message,GetLastError());throw std::runtime_error(message);}
}
static int child(int argc,wchar_t** argv){
 if(argc!=5)return 2;
 HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,argv[3]);
 HANDLE stop=OpenEventW(SYNCHRONIZE,FALSE,argv[4]);
 if(!ready||!stop)return 3;
 const int mode=_wtoi(argv[2]);
 HWND window=nullptr;
 if(mode!=3){
  WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);
  wc.lpszClassName=L"RA2DiscoveryFixture";RegisterClassW(&wc);
  window=CreateWindowExW(0,wc.lpszClassName,L"Controlled discovery fixture",
    WS_POPUP,40,40,800,600,nullptr,nullptr,wc.hInstance,nullptr);
  if(!window)return 4;
  if(mode==0)ShowWindow(window,SW_SHOWNOACTIVATE);
  if(mode==1)ShowWindow(window,SW_SHOWMINNOACTIVE);
 }
 SetEvent(ready);
 for(;;){
  DWORD result=MsgWaitForMultipleObjects(1,&stop,FALSE,10000,QS_ALLINPUT);
  if(result==WAIT_OBJECT_0)break;
  if(result==WAIT_FAILED)return 5;
  MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
 }
 if(window)DestroyWindow(window);
 CloseHandle(ready);CloseHandle(stop);return 0;
}
static DWORD waitForChildWithMessages(HANDLE process,DWORD timeout){
 const DWORD began=GetTickCount();
 for(;;){
  const DWORD elapsed=DWORD(GetTickCount()-began);
  const DWORD waited=MsgWaitForMultipleObjects(1,&process,FALSE,
   elapsed<timeout?timeout-elapsed:0,QS_ALLINPUT);
  if(waited!=WAIT_OBJECT_0+1)return waited;
  // A reparented renderer sends synchronous parent notifications while its
  // window is destroyed. The owning controller must keep dispatching those
  // messages while waiting for this controlled child to exit.
  MSG msg{};
  while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
   TranslateMessage(&msg);DispatchMessageW(&msg);
   if(DWORD(GetTickCount()-began)>=timeout)break;
  }
  if(DWORD(GetTickCount()-began)>=timeout)return WaitForSingleObject(process,0);
 }
}
struct Child {
 PROCESS_INFORMATION pi{};HANDLE ready=nullptr,stop=nullptr;
 Child(const Child&)=delete;Child& operator=(const Child&)=delete;
 Child()=default;
 ~Child(){
  if(stop)SetEvent(stop);
  if(pi.hProcess){
   if(waitForChildWithMessages(pi.hProcess,5000)==WAIT_TIMEOUT)TerminateProcess(pi.hProcess,99);
   CloseHandle(pi.hProcess);
  }
  if(pi.hThread)CloseHandle(pi.hThread);
  if(ready)CloseHandle(ready);if(stop)CloseHandle(stop);
 }
 void start(const std::wstring& path,int mode,unsigned serial){
  std::wstring prefix=L"Local\\RA2Discovery-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial);
  const auto readyName=prefix+L"-ready",stopName=prefix+L"-stop";
  ready=CreateEventW(nullptr,TRUE,FALSE,readyName.c_str());
  stop=CreateEventW(nullptr,TRUE,FALSE,stopName.c_str());
  require(ready&&stop,"create fixture events");
  std::wstring command=L"\""+path+L"\" --child "+std::to_wstring(mode)+L" \""+readyName+L"\" \""+stopName+L"\"";
  std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(0);
  STARTUPINFOW si{};si.cb=sizeof si;
  require(CreateProcessW(path.c_str(),mutableCommand.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=FALSE,"start fixture child");
  HANDLE handles[]={pi.hProcess,ready};
  require(WaitForMultipleObjects(2,handles,FALSE,15000)==WAIT_OBJECT_0+1,"child ready");
 }
 void finish(){
  require(SetEvent(stop)!=FALSE,"stop fixture");
  const DWORD waited=waitForChildWithMessages(pi.hProcess,5000);
  if(waited!=WAIT_OBJECT_0)std::fprintf(stderr,"child exit wait pid=%lu result=0x%08lx error=%lu\n",
   pi.dwProcessId,waited,waited==WAIT_FAILED?GetLastError():0);
  require(waited==WAIT_OBJECT_0,"child exited");
  DWORD code=99;require(GetExitCodeProcess(pi.hProcess,&code)&&code==0,"child successful exit");
 }
};
struct Files {
 std::wstring root;std::vector<std::wstring> paths;
 ~Files(){for(const auto&p:paths)DeleteFileW(p.c_str());if(!root.empty())RemoveDirectoryW(root.c_str());}
 std::wstring copy(const wchar_t* self,const wchar_t* name){
  const auto path=root+L"\\"+name;
  require(CopyFileW(self,path.c_str(),FALSE)!=FALSE,"copy fixture");
  paths.push_back(path);return path;
 }
};
struct WindowReader {
 HWND window;unsigned fresh=0;
 bool readFresh(uint32_t at,void*out,size_t size){
  require(at==external::discovery::YrppGameHwndAddress&&size==4,"authoritative HWND read layout");
  ++fresh;const uint32_t value=uint32_t(reinterpret_cast<uintptr_t>(window));
  std::memcpy(out,&value,4);return true;
 }
 bool read(uint32_t,void*,size_t){require(false,"must bypass stale reader cache");return false;}
};
struct OwnedWindow {
 HWND value=nullptr;
 ~OwnedWindow(){if(value)DestroyWindow(value);}
};
int wmain(int argc,wchar_t**argv){
 if(argc>1&&std::wstring(argv[1])==L"--child")return child(argc,argv);
 try{
  using namespace external::discovery;
  require(isGameName(L"gamemd-ares.exe")&&isGameName(L"GaMeMd-ArEs.ExE")&&
   isGameName(L"gamemd-spawn.exe")&&isGameName(L"GAMEMD.EXE")&&
   isGameName(L"gamemd.dat")&&isGameName(L"gamemd-spawn.dat"),"supported executable names");
  require(!isGameName(nullptr)&&!isGameName(L"")&&!isGameName(L"unrelated.exe")&&
   !isGameName(L"gamemd-ares.exe.bak"),"reject unrelated executable names");
  wchar_t self[32768]{},temp[MAX_PATH]{};
  require(GetModuleFileNameW(nullptr,self,32768)>0&&GetTempPathW(MAX_PATH,temp)>0,"fixture paths");
  Files files;files.root=std::wstring(temp)+L"ra2-discovery-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount());
  require(CreateDirectoryW(files.root.c_str(),nullptr)!=FALSE,"fixture directory");
  const auto ares=files.copy(self,L"gamemd-ares.exe");
  const auto spawn=files.copy(self,L"gamemd-spawn.exe");
  const auto unrelated=files.copy(self,L"unrelated.exe");
  unsigned serial=0;
  auto check=[&](const std::wstring& path,int mode){
   Child process;process.start(path,mode,++serial);
   const auto found=scan();
   require(found.processCount==1&&found.best.pid==process.pi.dwProcessId,"discover unique fixture PID");
   if(mode==3){
    require(!found.best.window&&found.uniqueWindowless&&found.best.thread==0,"windowless process selection");
   }else{
    require(found.best.window&&found.best.thread&&found.windowCount>=1,"discover child window");
    if(mode==0)require(IsWindowVisible(found.best.window)&&!IsIconic(found.best.window),"visible window");
    if(mode==1)require(IsIconic(found.best.window)!=FALSE,"minimized window");
    if(mode==2)require(!IsWindowVisible(found.best.window),"hidden window");
    WindowReader reader{found.best.window};DWORD thread=0;bool authoritative=false;
    require(resolveYrppWindow(reader,process.pi.dwProcessId,nullptr,&thread,&authoritative)==found.best.window&&
      thread==found.best.thread&&authoritative&&reader.fresh==1,"authoritative child HWND");
    HWND foreign=CreateWindowExW(0,L"STATIC",L"Parent ownership fixture",WS_POPUP,0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(foreign!=nullptr,"foreign HWND fixture");
    WindowReader wrong{foreign};
    const HWND rejected=resolveYrppWindow(wrong,process.pi.dwProcessId,nullptr,&thread,&authoritative);
    const bool rejectedState=!rejected&&!thread&&!authoritative;
    const HWND fallback=resolveYrppWindow(wrong,process.pi.dwProcessId,found.best.window,&thread,&authoritative);
    const HWND wrongFallback=resolveYrppWindow(wrong,process.pi.dwProcessId,foreign);
    DestroyWindow(foreign);
    require(rejectedState&&fallback==found.best.window&&thread==found.best.thread&&!authoritative&&!wrongFallback,"reject wrong owner and validate fallback");
   }
   require(scan(process.pi.dwProcessId).best.pid==process.pi.dwProcessId,"preferred PID selection");
   require(!scan(GetCurrentProcessId()).best.pid,"never substitute an unrecognized preferred PID");
   process.finish();
  };
  for(int mode=0;mode<4;++mode){check(ares,mode);check(spawn,mode);}
  {
   // Reparent a controlled spawn process's only HWND into a hidden platform
   // host belonging to this controller. The foreground is deliberately not
   // this host, and the renderer is nested two levels below it.
   OwnedWindow host{CreateWindowExW(0,L"STATIC",L"Hidden platform fixture",WS_POPUP,
    0,0,900,700,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
   require(host.value!=nullptr,"background platform host");
   OwnedWindow container{CreateWindowExW(0,L"STATIC",L"Nested platform fixture",WS_CHILD,
    0,0,900,700,host.value,nullptr,GetModuleHandleW(nullptr),nullptr)};
   require(container.value!=nullptr,"nested platform container");
   Child process;process.start(spawn,0,++serial);
   const HWND renderer=scan(process.pi.dwProcessId).best.window;
   require(renderer!=nullptr,"initial controlled renderer HWND");
   SetWindowLongPtrW(renderer,GWL_STYLE,WS_CHILD|WS_VISIBLE);
   SetParent(renderer,container.value);
   require(GetParent(renderer)==container.value&&GetForegroundWindow()!=host.value,
    "renderer embedded in nonforeground host");
   const auto found=scan(process.pi.dwProcessId);
   require(found.best.pid==process.pi.dwProcessId&&found.best.window==renderer&&
    found.best.thread&&found.windowCount==1,"discover background nested renderer exactly once");
   WindowReader reader{renderer};
   require(resolveYrppWindow(reader,process.pi.dwProcessId)==renderer,
    "authoritative nested renderer HWND remains PID checked");
   std::fprintf(stderr,"PASS background nested renderer discovery; stopping child pid=%lu\n",process.pi.dwProcessId);
   process.finish();
   std::fputs("PASS background nested renderer cleanup\n",stderr);
  }
  // Different casing is tested using the actual executable basename in a separate directory.
  const auto mixedRoot=files.root+L"\\mixed";require(CreateDirectoryW(mixedRoot.c_str(),nullptr)!=FALSE,"mixed-case directory");
  const auto mixed=mixedRoot+L"\\GaMeMd-ArEs.ExE";
  require(CopyFileW(self,mixed.c_str(),FALSE)!=FALSE,"mixed-case fixture");
  try{check(mixed,0);}catch(...){DeleteFileW(mixed.c_str());RemoveDirectoryW(mixedRoot.c_str());throw;}
  require(DeleteFileW(mixed.c_str())&&RemoveDirectoryW(mixedRoot.c_str()),"mixed-case cleanup");
  {
   Child a,b;a.start(ares,3,++serial);b.start(spawn,3,++serial);
   const auto ambiguous=scan();
   require(ambiguous.processCount==2&&!ambiguous.best.pid&&!ambiguous.uniqueWindowless,"multiple windowless processes remain ambiguous");
   require(scan(a.pi.dwProcessId).best.pid==a.pi.dwProcessId&&
    scan(b.pi.dwProcessId).best.pid==b.pi.dwProcessId,"explicit PID resolves ambiguity");
   a.finish();b.finish();
  }
  {
   Child process;process.start(unrelated,0,++serial);
   require(!scan().best.pid&&scan().processCount==0,"unrelated executable rejected despite visible window");
   process.finish();
  }
  require(scan().processCount==0,"all fixture processes exited");
  std::puts("PASS real Windows Ares/spawn discovery: visible, minimized, hidden, windowless, background nested renderer, mixed case, PID ownership, fresh HWND, ambiguity, unrelated executable");
  return 0;
 }catch(const std::exception&){return 1;}
}


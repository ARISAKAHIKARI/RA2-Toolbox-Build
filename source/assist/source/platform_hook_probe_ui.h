#pragma once
#include <commdlg.h>
#include <new>
#include "../../shared/source/platform_hook_probe_windows.h"

// Host-only, explicitly requested read-only diagnostics. Workers own their
// options, progress and result; they never touch HWNDs, Host IPC or settings.
static constexpr int HookProbePid=12300,HookProbeBaseline=12301,HookProbeDisk=12302,
 HookProbeStart=12303,HookProbeCancel=12304,HookProbeOpen=12305,HookProbeFolder=12306;
struct PlatformHookProbeJob {
 hookprobe::Options options;
 hookprobe::Progress progress;
 hookprobe::Result result;
 std::atomic<bool> done{false};
 std::atomic<uint32_t> resolvedPid{0};
 uint64_t generation=0;
};
static HWND hookProbePidEdit,hookProbeBaselineText,hookProbeStatusText,
 hookProbeBaselineButton,hookProbeDiskButton,hookProbeStartButton,
 hookProbeCancelButton,hookProbeOpenButton,hookProbeFolderButton;
static std::shared_ptr<PlatformHookProbeJob> hookProbeJob;
static std::vector<std::shared_ptr<PlatformHookProbeJob>> hookProbeRetired;
static std::wstring hookProbeBaseline,hookProbeReport,hookProbeReportDirectory,hookProbeStatus;
static uint64_t hookProbeGeneration=0,hookProbePublished=0;
static bool hookProbeClosing=false;

static void platformHookProbeStatus(const std::wstring& text){
 if(hookProbeClosing||text==hookProbeStatus)return;
 hookProbeStatus=text;if(hookProbeStatusText)SetWindowTextW(hookProbeStatusText,text.c_str());
}
static bool platformHookProbeParsePid(const wchar_t* text,uint32_t& value){
 value=0;if(!text)return true;
 for(const wchar_t* p=text;*p;++p){
  if(*p<L'0'||*p>L'9')return false;
  const uint32_t digit=uint32_t(*p-L'0');
  if(value>(UINT32_MAX-digit)/10)return false;
  value=value*10+digit;
 }
 return !*text||value!=0;
}
static bool platformHookProbeGameName(const wchar_t* name){
 for(const wchar_t* known:{L"gamemd.exe",L"gamemd-spawn.exe",L"gamemd-ares.exe",L"gamemdAres.exe",L"gamemd.dat",L"gamemd-spawn.dat"})
  if(!_wcsicmp(name,known))return true;
 return false;
}
static DWORD WINAPI platformHookProbeWorker(void* parameter){
 std::unique_ptr<std::shared_ptr<PlatformHookProbeJob>> owner(static_cast<std::shared_ptr<PlatformHookProbeJob>*>(parameter));
 const auto job=*owner;owner.reset();
 try{
  auto options=job->options;
  if(!options.pid&&!job->progress.cancel.load(std::memory_order_relaxed)){
   // Process-name discovery is also performed off the UI thread. It needs no
   // successful Runtime injection, visible game HWND or shared Host handle.
   HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
   uint32_t found=0;unsigned matches=0;
   if(snapshot!=INVALID_HANDLE_VALUE){
    PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
    if(Process32FirstW(snapshot,&entry))do{
     if(job->progress.cancel.load(std::memory_order_relaxed))break;
     if(entry.th32ProcessID!=GetCurrentProcessId()&&platformHookProbeGameName(entry.szExeFile)){found=entry.th32ProcessID;++matches;}
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
   }
   if(matches==1)options.pid=found;
   else job->result.summary=matches>1?L"发现多个游戏进程，请填写要扫描的 PID。":L"未找到游戏进程，请先启动游戏或填写 PID。";
  }
  job->resolvedPid.store(options.pid,std::memory_order_relaxed);
  if(options.pid)job->result=hookprobe::run(options,job->progress);
  else if(job->progress.cancel.load(std::memory_order_relaxed)){job->result.cancelled=true;job->result.summary=L"已取消扫描。";}
 }catch(...){job->result.success=false;job->result.partial=true;job->result.summary=L"扫描未能完成，请检查进程及诊断目录后重试。";}
 // Only this release publishes the non-atomic result. The UI never joins or
 // reads it early, and a superseded job cannot publish into the current job.
 job->done.store(true,std::memory_order_release);return 0;
}
static void updatePlatformHookProbeUI(){
 if(hookProbeClosing||!hookProbeStartButton)return;
 hookProbeRetired.erase(std::remove_if(hookProbeRetired.begin(),hookProbeRetired.end(),[](const auto& job){return job->done.load(std::memory_order_acquire);}),hookProbeRetired.end());
 const auto job=hookProbeJob;
 const bool running=job&&!job->done.load(std::memory_order_acquire),cancelling=running&&job->progress.cancel.load(std::memory_order_relaxed);
 // Permit one replacement while cancellation is pending, with a hard bound
 // of two workers even if a slow filesystem call is still returning.
 const bool canStart=(!running||cancelling)&&hookProbeRetired.size()+size_t(running)<2;
 EnableWindow(hookProbeStartButton,canStart);
 EnableWindow(hookProbeCancelButton,running&&!cancelling);
 EnableWindow(hookProbePidEdit,!running||cancelling);
 EnableWindow(hookProbeBaselineButton,!running||cancelling);
 EnableWindow(hookProbeDiskButton,(!running||cancelling)&&!hookProbeBaseline.empty());
 if(running){
  const uint32_t target=job->resolvedPid.load(std::memory_order_relaxed),done=job->progress.modulesDone.load(std::memory_order_relaxed),total=job->progress.modulesTotal.load(std::memory_order_relaxed),findings=job->progress.findings.load(std::memory_order_relaxed);
  platformHookProbeStatus((cancelling?L"正在取消":L"正在只读扫描")+std::wstring(target?L" · PID "+std::to_wstring(target):L" · 识别游戏进程中")+
   L"\r\n模块进度："+std::to_wstring(done)+L" / "+std::to_wstring(total)+L" · 改写候选："+std::to_wstring(findings));
 }else if(job&&job->generation==hookProbeGeneration&&hookProbePublished!=job->generation){
  hookProbePublished=job->generation;
  const auto& result=job->result;hookProbeReport=result.htmlPath;
  hookProbeReportDirectory=(!result.htmlPath.empty()||!result.jsonPath.empty())?job->options.outputDirectory:L"";
  const std::wstring outcome=result.cancelled?L"已取消":result.partial?L"部分完成":result.success?L"扫描完成":L"扫描未完成";
  const uint32_t target=job->resolvedPid.load(std::memory_order_relaxed);
  platformHookProbeStatus(outcome+(target?L" · PID "+std::to_wstring(target):L"")+L"\r\n"+result.summary+
   L"\r\n已扫描模块："+std::to_wstring(result.modulesScanned)+L" · 改写候选："+std::to_wstring(result.findings)+L"。覆盖范围与未完成项见报告。");
 }
 EnableWindow(hookProbeOpenButton,!hookProbeReport.empty());
 EnableWindow(hookProbeFolderButton,!hookProbeReportDirectory.empty());
}
static void platformHookProbeStart(){
 if(hookProbeClosing)return;
 updatePlatformHookProbeUI();
 if(!IsWindowEnabled(hookProbeStartButton))return;
 wchar_t text[32]{};GetWindowTextW(hookProbePidEdit,text,32);uint32_t target=0;
 if(!platformHookProbeParsePid(text,target)){platformHookProbeStatus(L"PID 需要填写 1 到 4294967295 的整数；留空自动识别游戏。");return;}
 auto job=std::make_shared<PlatformHookProbeJob>();
 // Capture Host's current target, but discovery and process opening belong
 // to the worker. No dependency on ready/shared/runtime bridge state.
 job->options.pid=target?target:pid;job->resolvedPid.store(job->options.pid,std::memory_order_relaxed);
 job->options.baselineExe=hookProbeBaseline;job->generation=hookProbeGeneration+1;
 SYSTEMTIME now{};GetSystemTime(&now);wchar_t suffix[128]{};
 swprintf(suffix,128,L"\\Diagnostics\\HookProbe-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%llu",unsigned(now.wYear),unsigned(now.wMonth),unsigned(now.wDay),unsigned(now.wHour),unsigned(now.wMinute),unsigned(now.wSecond),unsigned(now.wMilliseconds),static_cast<unsigned long>(GetCurrentProcessId()),static_cast<unsigned long long>(job->generation));
 job->options.outputDirectory=root+suffix;
 auto* context=new(std::nothrow) std::shared_ptr<PlatformHookProbeJob>(job);
 if(!context){platformHookProbeStatus(L"无法创建扫描任务，请稍后重试。");return;}
 HANDLE worker=CreateThread(nullptr,0,platformHookProbeWorker,context,0,nullptr);
 if(!worker){delete context;platformHookProbeStatus(L"无法启动扫描线程，错误码 "+std::to_wstring(GetLastError())+L"。");return;}
 CloseHandle(worker);
 if(hookProbeJob&&!hookProbeJob->done.load(std::memory_order_acquire))hookProbeRetired.push_back(hookProbeJob);
 hookProbeGeneration=job->generation;hookProbeJob=std::move(job);hookProbeReport.clear();hookProbeReportDirectory.clear();
 updatePlatformHookProbeUI();
}
static void platformHookProbeChooseBaseline(){
 if(hookProbeClosing||!IsWindowEnabled(hookProbeBaselineButton))return;
 HMODULE dialog=LoadLibraryExW(L"comdlg32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!dialog){platformHookProbeStatus(L"无法打开文件选择器，请稍后重试。");return;}
 using SelectFile=BOOL(WINAPI*)(LPOPENFILENAMEW);
 const auto select=reinterpret_cast<SelectFile>(GetProcAddress(dialog,"GetOpenFileNameW"));
 wchar_t filename[32768]{};OPENFILENAMEW open{};open.lStructSize=sizeof open;open.hwndOwner=ui;
 open.lpstrFilter=L"游戏原始程序 (*.exe;*.dat)\0*.exe;*.dat\0所有文件\0*.*\0\0";
 open.lpstrFile=filename;open.nMaxFile=32768;open.lpstrTitle=L"选择同一构建的原始游戏程序";
 open.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 const bool selected=select&&select(&open);FreeLibrary(dialog);
 // A modal file dialog pumps messages; WM_CLOSE may have destroyed the pane.
 if(hookProbeClosing||!IsWindow(ui)||!selected)return;
 hookProbeBaseline=filename;SetWindowTextW(hookProbeBaselineText,(L"外部基准："+hookProbeBaseline).c_str());
 platformHookProbeStatus(L"已选择原始程序。请使用与当前游戏同一构建的文件；是否匹配将在报告中说明。");updatePlatformHookProbeUI();
}
static bool platformHookProbeCommand(int id,unsigned notification){
 if(id<HookProbePid||id>HookProbeFolder)return false;
 if(id==HookProbePid||notification!=BN_CLICKED||hookProbeClosing)return true;
 if(id==HookProbeStart)platformHookProbeStart();
 else if(id==HookProbeCancel){if(hookProbeJob&&!hookProbeJob->done.load(std::memory_order_acquire))hookProbeJob->progress.cancel.store(true,std::memory_order_relaxed);updatePlatformHookProbeUI();}
 else if(id==HookProbeBaseline)platformHookProbeChooseBaseline();
 else if(id==HookProbeDisk){if(IsWindowEnabled(hookProbeDiskButton)){hookProbeBaseline.clear();SetWindowTextW(hookProbeBaselineText,L"基准：已加载模块对应的磁盘文件");platformHookProbeStatus(L"已恢复磁盘文件基准，无需移除平台 Hook。");updatePlatformHookProbeUI();}}
 else if(id==HookProbeOpen||id==HookProbeFolder){
  const auto target=id==HookProbeOpen?hookProbeReport:hookProbeReportDirectory;
  if(!target.empty()&&INT_PTR(ShellExecuteW(ui,L"open",target.c_str(),nullptr,root.c_str(),SW_SHOWNORMAL))<=32)
   platformHookProbeStatus(L"无法打开报告或目录，请检查 Diagnostics 目录及文件关联。");
 }
 return true;
}
static void shutdownPlatformHookProbe(){
 hookProbeClosing=true;++hookProbeGeneration;
 if(hookProbeJob)hookProbeJob->progress.cancel.store(true,std::memory_order_relaxed);
 for(const auto& job:hookProbeRetired)job->progress.cancel.store(true,std::memory_order_relaxed);
 hookProbeJob.reset();hookProbeRetired.clear();
 // Detached workers own their state and read-only process handles. Never
 // join, suspend, terminate a thread or wait for a filesystem read on close.
}
static int createPlatformHookProbeControls(int page,int y){
 auto add=[&](const wchar_t* cls,const wchar_t* text,int x,int row,int width,int height,int id,DWORD style=0,const wchar_t* help=L""){
  HWND control=pageControl(page,cls,text,x,row,width,height,id,style);
  pageControls.back().help=help;if(*help)modernui::tip(control,help);
  SetPropW(control,L"RA2.Console.Group",const_cast<wchar_t*>(L"平台 Hook 探测"));
  SetPropW(control,L"RA2.Console.Row",reinterpret_cast<HANDLE>(INT_PTR(row)+1));
  SetPropW(control,L"RA2.Console.Column",reinterpret_cast<HANDLE>(INT_PTR(x)+1));
  if(!wcscmp(cls,L"BUTTON"))SetPropW(control,L"RA2.Console.Immediate",reinterpret_cast<HANDLE>(1));
  return control;
 };
 modernui::heading(add(L"STATIC",L"平台 Hook 探测",20,y,610,26,0));y+=34;
 add(L"STATIC",L"只读检查正在运行的游戏，无需移除平台 Hook。",20,y,610,26,0);y+=34;
 add(L"STATIC",L"进程 PID（留空自动识别）",20,y,250,26,0);
 hookProbePidEdit=add(L"EDIT",L"",280,y,200,28,HookProbePid,WS_TABSTOP|ES_AUTOHSCROLL,
  L"留空优先使用当前游戏进程；未连接时独立查找 gamemd、gamemd-spawn、gamemd-ares / gamemdAres。也可填写任务管理器中的 PID。无需成功注入运行组件。");
 SendMessageW(hookProbePidEdit,EM_SETLIMITTEXT,10,0);y+=38;
 hookProbeBaselineText=add(L"STATIC",L"基准：已加载模块对应的磁盘文件",20,y,610,42,0,0,
  L"没有未注入平台的运行状态也能扫描。磁盘映像是代码基准；可另选同一构建的原始 EXE。报告会说明基准匹配情况和覆盖限制。");y+=50;
 hookProbeBaselineButton=add(L"BUTTON",L"选择原始 EXE…",20,y,190,30,HookProbeBaseline,WS_TABSTOP);
 hookProbeDiskButton=add(L"BUTTON",L"恢复磁盘基准",222,y,168,30,HookProbeDisk,WS_TABSTOP);y+=42;
 hookProbeStartButton=add(L"BUTTON",L"开始只读扫描",20,y,154,32,HookProbeStart,WS_TABSTOP);
 hookProbeCancelButton=add(L"BUTTON",L"取消扫描",186,y,116,32,HookProbeCancel,WS_TABSTOP);
 hookProbeOpenButton=add(L"BUTTON",L"打开 HTML 报告",314,y,162,32,HookProbeOpen,WS_TABSTOP);
 hookProbeFolderButton=add(L"BUTTON",L"报告目录",488,y,134,32,HookProbeFolder,WS_TABSTOP);y+=44;
 hookProbeStatusText=add(L"STATIC",L"尚未扫描。启动游戏后点击“开始只读扫描”。",20,y,610,82,0);y+=90;
 add(L"STATIC",L"改写候选是分析线索，不能单独证明功能用途或写入者。",20,y,610,42,0,0,
  L"仅由你点击开始扫描。报告保存在工具目录的 Diagnostics 子目录中；不自动上传，不修改游戏代码或签名校验设置。窗口关闭时只请求取消，不等待扫描线程。");y+=54;
 updatePlatformHookProbeUI();return y;
}

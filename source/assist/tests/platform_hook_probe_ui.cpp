// Execute the production UI commands and worker lifetime using controlled
// Win32 boundaries. This fixture does not run Windows, the scanner or a game.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <cstdlib>
#include "../../external_reader/source/game_process_names.h"

#define WINAPI
using DWORD=uint32_t;using BOOL=int;using HANDLE=void*;using HMODULE=void*;using INT_PTR=intptr_t;
using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
struct Window {std::wstring text;bool enabled=true,alive=true;int limit=0,id=0;std::map<std::wstring,HANDLE> properties;};
using HWND=Window*;
static constexpr DWORD TH32CS_SNAPPROCESS=2,WS_TABSTOP=0x10000,ES_AUTOHSCROLL=128,LOAD_LIBRARY_SEARCH_SYSTEM32=0x800,
 OFN_FILEMUSTEXIST=0x1000,OFN_PATHMUSTEXIST=0x800,OFN_NOCHANGEDIR=8;
static constexpr unsigned EM_SETLIMITTEXT=0xc5,BN_CLICKED=0,SW_SHOWNORMAL=1;
static HANDLE INVALID_HANDLE_VALUE=reinterpret_cast<HANDLE>(intptr_t(-1));
struct PROCESSENTRY32W {DWORD dwSize=0,th32ProcessID=0;wchar_t szExeFile[260]{};};
struct SYSTEMTIME {unsigned short wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;};
struct OPENFILENAMEW {DWORD lStructSize=0;HWND hwndOwner=nullptr;const wchar_t* lpstrFilter=nullptr;wchar_t* lpstrFile=nullptr;DWORD nMaxFile=0;const wchar_t* lpstrTitle=nullptr;DWORD Flags=0;};
using LPOPENFILENAMEW=OPENFILENAMEW*;
struct PageControl {HWND hwnd;int page;std::wstring help;};
static std::vector<PageControl> pageControls;
static std::vector<std::unique_ptr<Window>> windows;
static HWND ui;static DWORD pid=0,lastError=8;
static std::wstring root=L"C:\\Toolbox";
static unsigned writes=0,startedThreads=0,closedThreads=0,closedSnapshots=0,dialogCalls=0,freeCalls=0;
static bool threadFailure=false,snapshotFailure=false;
static std::wstring pickerPath;
static std::function<void()> dialogHook;
static std::vector<std::wstring> opened;
static INT_PTR shellResult=42;
struct PendingThread {DWORD(*fn)(void*);void* value;bool completed=false;};
static std::vector<PendingThread> threads;
static std::vector<std::pair<DWORD,std::wstring>> processes;
static size_t processAt=0;
static HWND pageControl(int page,const wchar_t*,const wchar_t* text,int,int,int,int,int id,DWORD=0){
 windows.push_back(std::make_unique<Window>());auto w=windows.back().get();w->text=text;w->id=id;pageControls.push_back({w,page,{}});return w;
}
namespace modernui {static void tip(HWND,const std::wstring&){}static void heading(HWND){} }
static BOOL SetPropW(HWND w,const wchar_t* key,HANDLE value){w->properties[key]=value;return 1;}
static BOOL SetWindowTextW(HWND w,const wchar_t* text){if(!w||!w->alive)std::abort();w->text=text;++writes;return 1;}
static BOOL EnableWindow(HWND w,BOOL on){if(!w||!w->alive)std::abort();w->enabled=on;return 1;}
static BOOL IsWindow(HWND w){return w&&w->alive;}
static BOOL IsWindowEnabled(HWND w){return IsWindow(w)&&w->enabled;}
static int GetWindowTextW(HWND w,wchar_t* text,int count){const auto n=std::min<size_t>(w->text.size(),size_t(count-1));w->text.copy(text,n);text[n]=0;return int(n);}
static LRESULT SendMessageW(HWND w,unsigned message,WPARAM value,LPARAM){if(message==EM_SETLIMITTEXT)w->limit=int(value);return 0;}
static DWORD GetCurrentProcessId(){return 99999;}
static DWORD GetLastError(){return lastError;}
static void GetSystemTime(SYSTEMTIME* value){*value={2026,11,0,7,10,11,12,123};}
static HANDLE CreateThread(void*,size_t,DWORD(*fn)(void*),void* value,DWORD,DWORD*){
 if(threadFailure)return nullptr;
 ++startedThreads;threads.push_back({fn,value});return reinterpret_cast<HANDLE>(uintptr_t(100+threads.size()));
}
static BOOL CloseHandle(HANDLE value){if(value==reinterpret_cast<HANDLE>(1))++closedSnapshots;else ++closedThreads;return 1;}
static HANDLE CreateToolhelp32Snapshot(DWORD,DWORD){return snapshotFailure?INVALID_HANDLE_VALUE:reinterpret_cast<HANDLE>(1);}
static BOOL processEntry(PROCESSENTRY32W* entry){if(processAt>=processes.size())return 0;entry->th32ProcessID=processes[processAt].first;std::wcsncpy(entry->szExeFile,processes[processAt].second.c_str(),259);return 1;}
static BOOL Process32FirstW(HANDLE,PROCESSENTRY32W* entry){processAt=0;return processEntry(entry);}
static BOOL Process32NextW(HANDLE,PROCESSENTRY32W* entry){++processAt;return processEntry(entry);}
static int _wcsicmp(const wchar_t* a,const wchar_t* b){return wcscasecmp(a,b);}
static BOOL selectFile(LPOPENFILENAMEW open){++dialogCalls;if(dialogHook)dialogHook();if(pickerPath.empty())return 0;std::wcsncpy(open->lpstrFile,pickerPath.c_str(),open->nMaxFile-1);return 1;}
static HMODULE LoadLibraryExW(const wchar_t*,void*,DWORD flags){return flags==LOAD_LIBRARY_SEARCH_SYSTEM32?reinterpret_cast<HMODULE>(2):nullptr;}
static void* GetProcAddress(HMODULE,const char*){return reinterpret_cast<void*>(selectFile);}
static BOOL FreeLibrary(HMODULE){++freeCalls;return 1;}
static HANDLE ShellExecuteW(HWND,const wchar_t*,const wchar_t* target,const wchar_t*,const wchar_t*,int){opened.emplace_back(target);return reinterpret_cast<HANDLE>(shellResult);}

namespace hookprobe {
struct Options {uint32_t pid=0;std::wstring baselineExe,outputDirectory;};
struct Progress {std::atomic<bool> cancel{false};std::atomic<uint32_t> modulesDone{0},modulesTotal{0},findings{0};};
struct Result {bool success=false,cancelled=false,partial=false;std::wstring summary,jsonPath,htmlPath;uint32_t modulesScanned=0,findings=0;};
static std::vector<Options> calls;
static bool partial=false,report=true;
static Result run(const Options& options,Progress& progress){
 calls.push_back(options);Result result;result.success=!partial&&!progress.cancel;result.cancelled=progress.cancel;result.partial=partial||result.cancelled;
 result.summary=L"受控报告 PID "+std::to_wstring(options.pid);result.modulesScanned=3;result.findings=4;
 if(report){result.htmlPath=options.outputDirectory+L"\\report.html";result.jsonPath=options.outputDirectory+L"\\report.json";}return result;
}
}
namespace external { namespace targetselection {static void enumerateProcesses(){}}
namespace discovery {
struct ProbeCandidate {DWORD pid=0;};
struct ProbeScan {ProbeCandidate best;DWORD processCount=0;bool explicitSelection=false;};
static ProbeScan scan(){
 ProbeScan result;if(snapshotFailure)return result;
 for(const auto& process:processes)if(process.first!=GetCurrentProcessId()&&isGameName(process.second.c_str())){++result.processCount;result.best.pid=process.first;}
 return result;
}
} }
#include "platform-hook-probe-ui-subject.inc"

static unsigned checks=0,scenarios=0;
static void check(bool okay,const char* label){++checks;if(!okay){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
static void finish(size_t index){check(index<threads.size()&&!threads[index].completed,"pending worker exists");auto fn=threads[index].fn;auto value=threads[index].value;threads[index].completed=true;fn(value);}
static bool contains(const std::wstring& text,const wchar_t* needle){return text.find(needle)!=std::wstring::npos;}
static void reset(){
 for(size_t i=0;i<threads.size();++i)if(!threads[i].completed)finish(i);
 shutdownPlatformHookProbe();hookProbeClosing=false;hookProbeGeneration=hookProbePublished=0;
 hookProbeBaseline.clear();hookProbeReport.clear();hookProbeReportDirectory.clear();hookProbeStatus.clear();
 windows.clear();pageControls.clear();threads.clear();processes.clear();hookprobe::calls.clear();opened.clear();
 threadFailure=snapshotFailure=hookprobe::partial=false;hookprobe::report=true;pickerPath.clear();dialogHook={};shellResult=42;pid=0;
 ui=pageControl(5,L"STATIC",L"Host",0,0,1,1,0);createPlatformHookProbeControls(5,100);
}
static void press(int id){check(platformHookProbeCommand(id,BN_CLICKED),"native command handled");}
int main(){
 reset();++scenarios;
 check(IsWindowEnabled(hookProbeStartButton)&&!IsWindowEnabled(hookProbeCancelButton),"idle scan and cancel availability");
 check(!IsWindowEnabled(hookProbeOpenButton)&&!IsWindowEnabled(hookProbeFolderButton),"no report actions before output exists");
 check(hookProbePidEdit->limit==10,"PID edit length bound");
 check(!platformHookProbeCommand(11000,BN_CLICKED),"unrelated commands remain unrelated");
 for(const auto& control:pageControls)if(control.hwnd!=ui)check(control.hwnd->properties.count(L"RA2.Console.Group")&&control.hwnd->properties.count(L"RA2.Console.Row"),"ordinary console controls retain group and row metadata");
 check(platformHookProbeGameName(L"gameMDAres.EXE")&&platformHookProbeGameName(L"gamemd-spawn.exe"),"known forced-platform executable names");
 check(platformHookProbeGameName(L"GAMEARES.EXE")&&platformHookProbeGameName(L"gameares.dat"),"probe shares actual gameares aliases with automatic discovery");

 ++scenarios;uint32_t parsed=0;
 check(platformHookProbeParsePid(L"",parsed)&&parsed==0,"blank chooses current/discovered process");
 check(platformHookProbeParsePid(L"4294967295",parsed)&&parsed==UINT32_MAX,"maximum uint32 PID is represented without narrowing");
 for(const wchar_t* invalid:{L"0",L"4294967296",L"-1",L"7x",L" 7",L"1e3"})check(!platformHookProbeParsePid(invalid,parsed),"invalid PID rejected");
 hookProbePidEdit->text=L"0";press(HookProbeStart);check(threads.empty()&&contains(hookProbeStatus,L"PID 需要"),"invalid command cannot launch scan");

 reset();++scenarios;pid=234;press(HookProbeStart);
 check(threads.size()==1&&hookProbeJob->options.pid==234,"default uses captured Host PID without Runtime readiness");
 check(!IsWindowEnabled(hookProbeStartButton)&&!IsWindowEnabled(hookProbePidEdit)&&IsWindowEnabled(hookProbeCancelButton),"running controls cannot alter current target");
 press(HookProbeStart);check(threads.size()==1,"duplicate start cannot launch concurrent scan");
 hookProbeJob->progress.modulesDone=2;hookProbeJob->progress.modulesTotal=5;hookProbeJob->progress.findings=9;updatePlatformHookProbeUI();
 check(contains(hookProbeStatus,L"2 / 5")&&contains(hookProbeStatus,L"9"),"atomic scan progress displayed");
 finish(0);updatePlatformHookProbeUI();check(hookprobe::calls.size()==1&&hookprobe::calls[0].pid==234,"worker invokes exact collector API");
 check(contains(hookProbeStatus,L"扫描完成")&&contains(hookProbeStatus,L"PID 234")&&contains(hookProbeStatus,L"已扫描模块：3"),"completed target and coverage displayed");
 check(IsWindowEnabled(hookProbeOpenButton)&&IsWindowEnabled(hookProbeFolderButton),"successful paths enable report actions");
 press(HookProbeOpen);press(HookProbeFolder);check(opened.size()==2&&opened[0]==hookProbeReport&&opened[1]==hookProbeJob->options.outputDirectory,"open actions use produced local report paths");
 shellResult=31;press(HookProbeOpen);updatePlatformHookProbeUI();check(contains(hookProbeStatus,L"无法打开"),"open failure remains visible after finished job poll");

 reset();++scenarios;pid=222;hookProbePidEdit->text=L"333";pickerPath=L"C:\\Original\\gamemdAres.exe";press(HookProbeBaseline);
 check(hookProbeBaseline==pickerPath&&IsWindowEnabled(hookProbeDiskButton),"file selection updates external baseline");
 press(HookProbeStart);check(hookProbeJob->options.pid==333&&hookProbeJob->options.baselineExe==pickerPath,"manual PID and selected original are immutable job inputs");
 const auto baseline=hookProbeBaseline;press(HookProbeDisk);check(hookProbeBaseline==baseline,"running baseline cannot change");
 hookprobe::partial=true;finish(0);updatePlatformHookProbeUI();check(contains(hookProbeStatus,L"部分完成")&&IsWindowEnabled(hookProbeOpenButton),"partial results retain report access");
 press(HookProbeDisk);check(hookProbeBaseline.empty()&&!IsWindowEnabled(hookProbeDiskButton),"explicit reset uses module disk baseline");

 reset();++scenarios;hookProbePidEdit->text=L"501";press(HookProbeStart);auto old=hookProbeJob;const auto oldDirectory=old->options.outputDirectory;
 press(HookProbeCancel);check(old->progress.cancel&&IsWindowEnabled(hookProbeStartButton)&&IsWindowEnabled(hookProbePidEdit),"cancel permits replacement without joining old worker");
 hookProbePidEdit->text=L"502";press(HookProbeStart);auto current=hookProbeJob;
 check(current!=old&&current->options.pid==502&&hookProbeRetired.size()==1,"new PID owns independent job");
 check(current->options.outputDirectory!=oldDirectory,"each scan gets independent timestamp directory");
 finish(0);updatePlatformHookProbeUI();check(hookProbeJob==current&&hookProbeReport.empty()&&contains(hookProbeStatus,L"502"),"old result cannot overwrite current PID or report");
 finish(1);updatePlatformHookProbeUI();check(contains(hookProbeStatus,L"受控报告 PID 502")&&hookProbeReport==current->result.htmlPath,"only current job publishes result");

 reset();++scenarios;pid=600;press(HookProbeStart);press(HookProbeCancel);press(HookProbeStart);press(HookProbeCancel);press(HookProbeStart);
 check(threads.size()==2&&!IsWindowEnabled(hookProbeStartButton),"in-flight cancelled work has a hard two-worker bound");
 finish(0);updatePlatformHookProbeUI();check(IsWindowEnabled(hookProbeStartButton),"completed retired job releases replacement admission");
 finish(1);updatePlatformHookProbeUI();check(contains(hookProbeStatus,L"已取消")&&IsWindowEnabled(hookProbeOpenButton),"cancelled current job can publish partial report");

 reset();++scenarios;pid=701;press(HookProbeStart);std::weak_ptr<PlatformHookProbeJob> lifetime=hookProbeJob;
 shutdownPlatformHookProbe();check(!lifetime.expired()&&!hookProbeJob,"worker owns lifetime after Host shutdown");
 const auto writesBeforeClose=writes;ui->alive=false;finish(0);updatePlatformHookProbeUI();
 check(lifetime.expired()&&writes==writesBeforeClose,"late worker completion never touches destroyed UI or leaves job alive");

 reset();++scenarios;processes={{801,L"GAMEMDARES.EXE"},{802,L"other.exe"}};press(HookProbeStart);check(hookProbeJob->options.pid==0,"unattached start defers discovery to worker");
 finish(0);updatePlatformHookProbeUI();check(hookprobe::calls.size()==1&&hookprobe::calls[0].pid==801,"independent discovery works without injected Runtime or window");
 reset();processes={{803,L"gameares.exe"},{804,L"other.exe"}};press(HookProbeStart);finish(0);updatePlatformHookProbeUI();
 check(hookprobe::calls.size()==1&&hookprobe::calls[0].pid==803,"shared discovery admits the reported gameares executable");
 reset();processes={{811,L"gamemd-spawn.exe"},{812,L"gamemd-ares.exe"}};press(HookProbeStart);finish(0);updatePlatformHookProbeUI();
 check(hookprobe::calls.empty()&&contains(hookProbeStatus,L"多个游戏进程"),"ambiguous discovery asks for PID rather than guessing");
 reset();press(HookProbeStart);finish(0);updatePlatformHookProbeUI();check(hookprobe::calls.empty()&&contains(hookProbeStatus,L"未找到游戏进程"),"discovery absence is visible and does not scan arbitrary process");

 reset();++scenarios;pid=900;press(HookProbeStart);finish(0);updatePlatformHookProbeUI();auto previous=hookProbeJob;auto generation=hookProbeGeneration;
 threadFailure=true;press(HookProbeStart);check(hookProbeJob==previous&&hookProbeGeneration==generation&&contains(hookProbeStatus,L"无法启动扫描线程"),"thread launch failure preserves current job and generation");

 reset();++scenarios;pickerPath=L"C:\\Original\\gamemd-spawn.exe";dialogHook=[](){shutdownPlatformHookProbe();ui->alive=false;};const auto beforeDialog=writes;
 press(HookProbeBaseline);check(hookProbeClosing&&hookProbeBaseline.empty()&&writes==beforeDialog,"modal WM_CLOSE cannot update a dead pane");
 check(dialogCalls==freeCalls,"file picker module handles released");

 reset();++scenarios;pid=950;hookprobe::partial=true;hookprobe::report=false;press(HookProbeStart);finish(0);updatePlatformHookProbeUI();
 check(!IsWindowEnabled(hookProbeOpenButton)&&!IsWindowEnabled(hookProbeFolderButton),"unsaved reports do not advertise open actions");
 check(closedThreads==startedThreads,"every launched thread handle closed immediately");
 shutdownPlatformHookProbe();
 std::cout<<"{\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"production_ui_commands_executed\":true,\"native_windows_executed\":false,\"game_process_accessed\":false}"<<'\n';
}

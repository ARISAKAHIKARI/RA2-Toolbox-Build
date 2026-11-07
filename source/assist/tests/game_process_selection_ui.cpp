// Exercise the complete production UI header. No native Windows UI, game
// process, memory writing, hook installation or INI filesystem is exercised.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

#define WINAPI
using DWORD=uint32_t;using BOOL=int;using HANDLE=void*;using INT_PTR=intptr_t;
using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;
constexpr DWORD WS_TABSTOP=0x10000,ES_AUTOHSCROLL=128,CBS_DROPDOWNLIST=3,WS_VSCROLL=0x200000;
constexpr DWORD ERROR_NOT_ENOUGH_MEMORY=8,ERROR_ACCESS_DENIED=5,ERROR_INVALID_PARAMETER=87;
constexpr BOOL FALSE=0;
constexpr unsigned BN_CLICKED=0,EN_CHANGE=0x300,CBN_SELCHANGE=1,EM_SETLIMITTEXT=0xc5,
 CB_GETCURSEL=0x147,CB_SETCURSEL=0x14e,CB_RESETCONTENT=0x14b,CB_ADDSTRING=0x143,CB_SETITEMDATA=0x151;
struct Window {
 std::wstring text,cls;bool enabled=true,alive=true;int limit=0,id=0,selected=-1;
 std::vector<std::wstring> options;std::vector<LPARAM> items;
 std::map<std::wstring,HANDLE> properties;
};
using HWND=Window*;
struct PageControl {HWND hwnd;int page;std::wstring help;};
static std::vector<PageControl> pageControls;
static std::vector<std::unique_ptr<Window>> windows;
static DWORD pid=0,lastData=0,clockNow=5000,lastError=8;
static bool enabled=true;
static void* shared=nullptr;
static struct {int status=0,commandStatus=0;}incoming;
static std::wstring statusText;
static unsigned uiWrites=0,startedThreads=0,closedThreads=0;
static bool threadFailure=false;
static std::wstring rejectOption;
static std::vector<std::wstring> logs;
struct PendingThread {DWORD(*fn)(void*);void* value;bool completed=false;};
static std::vector<PendingThread> threads;

static HWND pageControl(int page,const wchar_t* cls,const wchar_t* text,int,int,int,int,int id,DWORD=0){
 windows.push_back(std::make_unique<Window>());auto w=windows.back().get();
 w->text=text;w->cls=cls;w->id=id;pageControls.push_back({w,page,{}});return w;
}
namespace modernui {static void tip(HWND,const std::wstring&){}static void heading(HWND){} }
static BOOL SetPropW(HWND w,const wchar_t* key,HANDLE value){w->properties[key]=value;return 1;}
static BOOL IsWindow(HWND w){return w&&w->alive;}
static BOOL SetWindowTextW(HWND w,const wchar_t* text){if(!IsWindow(w))std::abort();w->text=text;++uiWrites;return 1;}
static BOOL EnableWindow(HWND w,BOOL on){if(!IsWindow(w))std::abort();w->enabled=on;++uiWrites;return 1;}
static int GetWindowTextLengthW(HWND w){return int(w->text.size());}
static int GetWindowTextW(HWND w,wchar_t* text,int count){
 const size_t size=std::min(w->text.size(),size_t(count-1));w->text.copy(text,size);text[size]=0;return int(size);
}
static LRESULT SendMessageW(HWND w,unsigned message,WPARAM value,LPARAM argument){
 if(!IsWindow(w))std::abort();
 if(message==EM_SETLIMITTEXT){w->limit=int(value);return 0;}
 if(message==CB_GETCURSEL)return w->selected;
 if(message==CB_SETCURSEL){const int at=int(value);w->selected=at>=0&&size_t(at)<w->options.size()?at:-1;return w->selected;}
 if(message==CB_RESETCONTENT){w->options.clear();w->items.clear();w->selected=-1;return 0;}
 if(message==CB_ADDSTRING){
  const std::wstring text=reinterpret_cast<const wchar_t*>(argument);if(!rejectOption.empty()&&text==rejectOption)return -2;
  w->options.push_back(text);w->items.push_back(0);return LRESULT(w->options.size()-1);
 }
 if(message==CB_SETITEMDATA){if(value>=w->items.size())return -1;w->items[size_t(value)]=argument;return 0;}
 return 0;
}
static DWORD GetTickCount(){return clockNow;}
static DWORD GetLastError(){return lastError;}
static HANDLE CreateThread(void*,size_t,DWORD(*fn)(void*),void* value,DWORD,DWORD*){
 if(threadFailure)return nullptr;
 ++startedThreads;threads.push_back({fn,value});return reinterpret_cast<HANDLE>(uintptr_t(threads.size()));
}
static BOOL CloseHandle(HANDLE){++closedThreads;return 1;}
static void critical(const std::wstring& text){logs.push_back(text);}

namespace external {namespace targetselection {
struct Policy {
 std::wstring keywords=L"Yuri Revenge";std::vector<std::wstring> rememberedPrograms;
 DWORD selectedPid=0;uint64_t selectedCreation=0,revision=1;
};
struct ProcessInfo {
 DWORD pid=0;uint64_t creation=0;std::wstring name,path,description;
 DWORD error=0,identityError=0,descriptionError=0;
};
static Policy policy;
static std::vector<ProcessInfo> running;
static DWORD enumerationError=0,writeError=0;
static bool throwEnumeration=false,rejectIdentity=false;
static unsigned enumerations=0,writes=0,identityChecks=0,removes=0,clears=0,policyReads=0;
static ProcessInfo rememberedProcess;
static std::wstring submittedTarget;
static size_t removedIndex=~size_t(0);
static std::wstring fold(std::wstring value){for(auto& c:value)c=wchar_t(std::towlower(c));return value;}
static bool matchesKeywords(const std::wstring& text,const std::wstring& words){
 const auto content=fold(text);size_t at=0;bool any=false;
 while(at<words.size()){
  while(at<words.size()&&std::iswspace(words[at]))++at;
  const size_t begin=at;while(at<words.size()&&!std::iswspace(words[at]))++at;
  if(begin==at)break;
  any=true;if(content.find(fold(words.substr(begin,at-begin)))==std::wstring::npos)return false;
 }
 return any;
}
static Policy current(){++policyReads;return policy;}
static uint64_t revision(){return policy.revision;}
static std::vector<ProcessInfo> enumerateProcesses(DWORD* error){
 ++enumerations;if(throwEnumeration)throw 1;*error=enumerationError;return enumerationError?std::vector<ProcessInfo>{}:running;
}
static bool writable(DWORD* error){*error=writeError;return writeError==0;}
static bool saveKeywords(const std::wstring& text,DWORD* error){
 if(!writable(error))return false;
 ++writes;policy.keywords=text;++policy.revision;return true;
}
static bool addProgram(const std::wstring& text,DWORD* error){
 submittedTarget=text;if(!writable(error))return false;++writes;
 if(std::find(policy.rememberedPrograms.begin(),policy.rememberedPrograms.end(),text)==policy.rememberedPrograms.end())policy.rememberedPrograms.push_back(text);
 ++policy.revision;return true;
}
static bool rememberProcess(const ProcessInfo& value,DWORD* error){
 ++identityChecks;rememberedProcess=value;
 if(rejectIdentity){*error=ERROR_INVALID_PARAMETER;return false;}
 if(!addProgram(value.path,error))return false;
 policy.selectedPid=value.pid;policy.selectedCreation=value.creation;return true;
}
static bool removeProgram(size_t index,DWORD* error){
 ++removes;removedIndex=index;if(!writable(error))return false;
 if(index>=policy.rememberedPrograms.size()){*error=ERROR_INVALID_PARAMETER;return false;}
 ++writes;policy.rememberedPrograms.erase(policy.rememberedPrograms.begin()+index);++policy.revision;return true;
}
static bool clearPreferredProcess(DWORD* error){
 ++clears;*error=0;policy.selectedPid=0;policy.selectedCreation=0;++policy.revision;return true;
}
}}

#include "game-process-selection-ui-subject.inc"

static unsigned checks=0,scenarios=0;
static void check(bool yes,const char* label){++checks;if(!yes){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
static bool contains(const std::wstring& text,const wchar_t* part){return text.find(part)!=std::wstring::npos;}
static void finish(size_t index){
 check(index<threads.size()&&!threads[index].completed,"worker is pending");
 auto fn=threads[index].fn;auto value=threads[index].value;threads[index].completed=true;const unsigned before=uiWrites;fn(value);
 check(uiWrites==before,"metadata worker never touches a HWND");
}
static void finishCurrent(){finish(threads.size()-1);updateGameProcessSelectionUI(true);}
static void press(int id){check(gameProcessSelectionCommand(id,BN_CLICKED),"production button command handled");}
static void search(const std::wstring& text){gameProcessSearchEdit->text=text;check(gameProcessSelectionCommand(GameProcessSearch,EN_CHANGE),"search edit handled");}
static void selectRunning(int row){SendMessageW(gameProcessRunningList,CB_SETCURSEL,WPARAM(row),0);gameProcessSelectionCommand(GameProcessRunning,CBN_SELCHANGE);}
static void selectRemembered(int row){SendMessageW(gameProcessRememberedList,CB_SETCURSEL,WPARAM(row),0);gameProcessSelectionCommand(GameProcessRemembered,CBN_SELCHANGE);}
static void reset(){
 shutdownGameProcessSelectionUI();
 for(size_t i=0;i<threads.size();++i)if(!threads[i].completed)finish(i);
 windows.clear();pageControls.clear();threads.clear();logs.clear();
 gameProcessCandidates.clear();gameProcessVisible.clear();gameProcessRememberedPrograms.clear();
 gameProcessSelectionFeedback.clear();gameProcessSelectionConnection.clear();
 gameProcessSelectionGeneration=gameProcessSelectionPublished=0;
 gameProcessSelectionClosing=gameProcessSelectionUpdating=false;
 pid=lastData=0;clockNow=5000;enabled=true;shared=nullptr;incoming={};statusText=L"未发现兼容游戏进程";
 startedThreads=closedThreads=0;threadFailure=false;rejectOption.clear();
 namespace api=external::targetselection;
 api::policy={};api::running={{101,1001,L"gameares.exe",L"C:\\RA2\\gameares.exe",L"Yuri Revenge"},
  {102,1002,L"launcher.exe",L"C:\\Platform\\launcher.exe",L"Platform launcher"},
  {103,1003,L"gamemd-spawn.exe",L"D:\\Yuri\\gamemd-spawn.exe",L"Yuri Revenge game"}};
 api::enumerationError=api::writeError=0;api::throwEnumeration=api::rejectIdentity=false;
 api::enumerations=api::writes=api::identityChecks=api::removes=api::clears=api::policyReads=0;
 api::rememberedProcess={};api::submittedTarget.clear();api::removedIndex=~size_t(0);
 createGameProcessSelectionControls(5,100);
}
int main(){
 namespace api=external::targetselection;
 reset();++scenarios;
 check(threads.size()==1&&api::enumerations==0,"opening page defers process/file metadata work to worker");
 check(!gameProcessRefreshButton->enabled&&!gameProcessRememberButton->enabled,"loading controls cannot submit absent choice or duplicate enumeration");
 press(GameProcessRefresh);check(threads.size()==1,"refresh coalesces while metadata worker runs");
 check(gameProcessSearchEdit->limit==512&&gameProcessTargetEdit->limit==32760,"edits have API-compatible length limits");
 check(!gameProcessSelectionCommand(12300,BN_CLICKED),"probe and unrelated controls are untouched");
 for(const auto& entry:pageControls){
  check(entry.hwnd->properties.count(L"RA2.Console.Group")&&entry.hwnd->properties.count(L"RA2.Console.Row")&&entry.hwnd->properties.count(L"RA2.Console.Column"),"all controls have ordinary console group/row/column metadata");
  check(!entry.hwnd->properties.count(L"RA2.Console.Debug")&&!entry.hwnd->properties.count(L"RA2.Console.Immediate"),"discovery settings do not become live hook controls or expiring gameplay commands");
 }
 finishCurrent();check(api::enumerations==1&&gameProcessVisible.size()==2,"saved fuzzy words filter completed process list");
 check(gameProcessRefreshButton->enabled&&!gameProcessRememberButton->enabled,"ambiguous search result requires explicit selection");

 ++scenarios;const unsigned beforePoll=api::policyReads,beforePollWrites=uiWrites;
 for(unsigned tick=0;tick<100;++tick)updateGameProcessSelectionUI();
 clockNow+=249;updateGameProcessSelectionUI();
 check(api::policyReads==beforePoll&&uiWrites==beforePollWrites,"frequent Host ticks and 249ms boundary do not copy policy or send UI mutations");
 ++clockNow;updateGameProcessSelectionUI();check(api::policyReads==beforePoll+1,"250ms boundary refreshes current policy once");
 updateGameProcessSelectionUI(true);check(api::policyReads==beforePoll+2,"explicit force refresh bypasses cadence");
 clockNow=0xfffffff0u;updateGameProcessSelectionUI(true);const unsigned beforeWrap=api::policyReads;
 clockNow+=249;updateGameProcessSelectionUI();check(api::policyReads==beforeWrap,"UI cadence remains quiet across DWORD clock wrap");
 ++clockNow;updateGameProcessSelectionUI();check(api::policyReads==beforeWrap+1,"wrapped 250ms deadline still refreshes");
 clockNow=5000;updateGameProcessSelectionUI(true);

 ++scenarios;const auto originalPolicy=api::policy;
 search(L"  GAMEARES  ");check(gameProcessVisible.size()==1&&gameProcessSelectionSelected()->pid==101,"case-insensitive search filters by executable name");
 check(api::writes==0&&api::policy.keywords==originalPolicy.keywords,"editing search draft does not mutate discovery policy");
 api::policy.rememberedPrograms={L"prior.exe"};++api::policy.revision;press(GameProcessSaveSearch);
 check(api::policy.keywords==L"GAMEARES"&&api::policy.rememberedPrograms==std::vector<std::wstring>{L"prior.exe"},"explicit search save preserves remembered detection rules");
 search(L" ");check(gameProcessVisible.size()==3,"empty search shows all running processes without writing policy");
 search(L"platform launcher");check(gameProcessVisible.size()==1&&gameProcessSelectionSelected()->pid==102,"fuzzy words search description as well as path and filename");
 search(L"does-not-exist");check(gameProcessVisible.empty()&&!gameProcessRememberButton->enabled,"empty result cannot become an implicit PID");
 const unsigned checksBefore=api::identityChecks;press(GameProcessRemember);check(api::identityChecks==checksBefore,"remember without a selected row never calls identity API");

 ++scenarios;search(L"gameares");press(GameProcessRemember);
 check(api::identityChecks==1&&api::rememberedProcess.pid==101&&api::rememberedProcess.creation==1001,"connect passes the selected process identity for fresh API verification");
 check(api::policy.selectedPid==101&&api::policy.selectedCreation==1001,"only explicit verified selection establishes session preference");
 check(api::policy.rememberedPrograms==std::vector<std::wstring>({L"prior.exe",L"C:\\RA2\\gameares.exe"}),"selection appends persistent program path rather than replacing rules with PID");
 check(pid==0&&shared==nullptr,"selection never changes live Host binding or writes game memory");
 const auto saved=api::policy;api::rejectIdentity=true;press(GameProcessRemember);
 check(api::policy.revision==saved.revision&&api::policy.rememberedPrograms==saved.rememberedPrograms&&contains(gameProcessSelectionFeedback,L"Win32=87"),"exited or recycled process is reported without publishing success");
 api::rejectIdentity=false;

 ++scenarios;gameProcessTargetEdit->text=L"   ";const unsigned beforeEmpty=api::writes;press(GameProcessAdd);
 check(api::writes==beforeEmpty&&contains(gameProcessSelectionFeedback,L"请输入"),"empty manual target never reaches persistence");
 gameProcessTargetEdit->text=L"  custom-game.exe  ";press(GameProcessAdd);
 check(api::submittedTarget==L"custom-game.exe"&&api::policy.rememberedPrograms.back()==L"custom-game.exe","manual exact name is trimmed and appended");
 gameProcessTargetEdit->text=L"E:\\Games\\second-game.exe";press(GameProcessAdd);
 check(api::policy.rememberedPrograms.back()==L"E:\\Games\\second-game.exe"&&api::policy.rememberedPrograms.front()==L"prior.exe","complete path and earlier programs coexist");
 api::writeError=ERROR_ACCESS_DENIED;const auto beforeFailure=api::policy;gameProcessTargetEdit->text=L"blocked.exe";press(GameProcessAdd);
 check(api::policy.rememberedPrograms==beforeFailure.rememberedPrograms&&contains(gameProcessSelectionFeedback,L"未能")&&contains(gameProcessSelectionFeedback,L"Win32=5"),"save failure leaves existing rules intact and stays visible");
 search(L"new words");press(GameProcessSaveSearch);check(api::policy.keywords==beforeFailure.keywords&&contains(gameProcessSelectionFeedback,L"未能保存"),"keyword save failure cannot claim persisted state");
 api::writeError=0;

 ++scenarios;const auto programsBeforeClear=api::policy.rememberedPrograms;const auto wordsBeforeClear=api::policy.keywords;
 press(GameProcessClearPreferred);
 check(api::policy.selectedPid==0&&api::policy.selectedCreation==0&&api::policy.rememberedPrograms==programsBeforeClear&&api::policy.keywords==wordsBeforeClear,"clear current preference preserves every remembered rule and keyword");
 check(!gameProcessClearButton->enabled&&contains(gameProcessSelectionConnection,L"自动匹配全部检测规则"),"current preference and persistent rules are visibly distinct");

 ++scenarios;api::policy.rememberedPrograms={L"a.exe",L"b.exe"};++api::policy.revision;updateGameProcessSelectionUI(true);selectRemembered(0);
 api::policy.rememberedPrograms={L"b.exe",L"a.exe"};++api::policy.revision;
 press(GameProcessRemove);check(api::removedIndex==1&&api::policy.rememberedPrograms==std::vector<std::wstring>{L"b.exe"},"stale list index resolves displayed program again before removal");
 selectRemembered(0);api::policy.rememberedPrograms={L"different.exe"};++api::policy.revision;
 const unsigned beforeRemove=api::removes;press(GameProcessRemove);
 check(api::removes==beforeRemove&&api::policy.rememberedPrograms==std::vector<std::wstring>{L"different.exe"},"vanished saved item cannot remove a different replacement record");
 rejectOption=L"a.exe";api::policy.rememberedPrograms={L"a.exe",L"b.exe"};++api::policy.revision;updateGameProcessSelectionUI(true);press(GameProcessRemove);
 check(api::policy.rememberedPrograms==std::vector<std::wstring>{L"a.exe"},"failed combo insertion cannot misalign visible record and removal target");rejectOption.clear();

 ++scenarios;const std::wstring longPath=L"C:\\"+std::wstring(2400,L'x')+L"\\gameares.exe",longDescription(1800,L'd');
 api::running={{301,4001,L"gameares.exe",longPath,longDescription}};search(L"");press(GameProcessRefresh);finishCurrent();
 check(gameProcessRunningList->options.size()==1&&gameProcessRunningList->options[0].size()<900&&contains(gameProcessRunningList->options[0],L"…"),"long process metadata is bounded for console option serialization");
 check(contains(gameProcessRunningList->options[0],L"gameares.exe"),"middle truncation preserves executable suffix");
 press(GameProcessRemember);check(api::rememberedProcess.path==longPath&&api::rememberedProcess.description==longDescription,"display truncation never changes selected process identity or metadata");
 check(gameProcessRememberedList->options.back().size()<=360&&gameProcessRememberedPrograms.back()==longPath,"remembered option display is bounded while full record is retained");
 selectRemembered(int(gameProcessRememberedPrograms.size()-1));press(GameProcessRemove);
 check(std::find(api::policy.rememberedPrograms.begin(),api::policy.rememberedPrograms.end(),longPath)==api::policy.rememberedPrograms.end(),"remove resolves full remembered string behind truncated label");

 ++scenarios;search(L"");selectRunning(0);const auto oldCandidates=gameProcessCandidates.size();
 api::enumerationError=5;press(GameProcessRefresh);finishCurrent();
 check(gameProcessCandidates.size()==oldCandidates&&contains(gameProcessSelectionFeedback,L"保留上一次列表"),"snapshot failure retains existing list and reports staleness");
 api::enumerationError=0;threadFailure=true;const auto beforeJob=gameProcessSelectionJob;press(GameProcessRefresh);
 check(gameProcessSelectionJob==beforeJob&&contains(gameProcessSelectionFeedback,L"无法刷新"),"thread creation failure preserves last usable publication");threadFailure=false;
 api::throwEnumeration=true;press(GameProcessRefresh);finishCurrent();
 check(contains(gameProcessSelectionFeedback,L"Win32=8")&&gameProcessCandidates.size()==oldCandidates,"worker exception publishes bounded failure without replacing prior data");api::throwEnumeration=false;

 ++scenarios;pid=101;shared=reinterpret_cast<void*>(1);incoming={};lastData=0;updateGameProcessSelectionUI(true);
 check(contains(gameProcessSelectionConnection,L"等待游戏内回调")&&!contains(gameProcessSelectionConnection,L"操作连接就绪"),"mapping existence alone never reports ready");
 incoming.commandStatus=1;lastData=clockNow;updateGameProcessSelectionUI(true);check(contains(gameProcessSelectionConnection,L"操作连接就绪"),"fresh command snapshot reports operational state");
 clockNow+=1600;updateGameProcessSelectionUI();check(contains(gameProcessSelectionConnection,L"等待游戏数据更新"),"stale readiness is not presented as current");
 incoming.status=-2;updateGameProcessSelectionUI(true);check(contains(gameProcessSelectionConnection,L"操作连接异常"),"latched runtime failure remains visible");
 enabled=false;updateGameProcessSelectionUI(true);check(contains(gameProcessSelectionConnection,L"战术控制已暂停"),"host pause is shown separately from runtime state");

 ++scenarios;press(GameProcessRefresh);std::weak_ptr<GameProcessSelectionJob> lifetime=gameProcessSelectionJob;
 shutdownGameProcessSelectionUI();check(!lifetime.expired()&&!gameProcessSelectionJob,"worker owns its job after nonblocking UI shutdown");
 for(auto& window:windows)window->alive=false;
 const unsigned beforeClose=uiWrites;finish(threads.size()-1);updateGameProcessSelectionUI();
 check(lifetime.expired()&&uiWrites==beforeClose,"late result cannot publish into destroyed controls");
 check(startedThreads==closedThreads,"every worker thread handle closes immediately");
 std::cout<<"{\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"production_ui_executed\":true,\"native_windows_executed\":false,\"game_process_accessed\":false,\"ini_persistence_executed\":false}"<<'\n';
}

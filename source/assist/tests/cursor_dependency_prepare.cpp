// Executes extracted production preparation routes against controlled Win32 seams.
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include "../../cursor/source/ondemand.h"
using DWORD=uint32_t;using UINT=unsigned;using BOOL=int;using HANDLE=void*;using HWND=uintptr_t;
using Bytes=std::vector<uint8_t>;
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD GENERIC_WRITE=0x40000000,FILE_SHARE_READ=1,CREATE_ALWAYS=2,FILE_ATTRIBUTE_NORMAL=0x80;
constexpr DWORD CREATE_NO_WINDOW=0x08000000,BELOW_NORMAL_PRIORITY_CLASS=0x4000;
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,WAIT_FAILED=0xffffffff,STILL_ACTIVE=259;
constexpr DWORD ERROR_ALREADY_EXISTS=183,ERROR_ACCESS_DENIED=5,ERROR_PATH_NOT_FOUND=3;
constexpr DWORD MOVEFILE_WRITE_THROUGH=8;constexpr int SW_SHOWNORMAL=1;
const HANDLE INVALID_HANDLE_VALUE=reinterpret_cast<HANDLE>(intptr_t(-1));
struct STARTUPINFOW{DWORD cb=0;};struct PROCESS_INFORMATION{HANDLE hProcess=nullptr,hThread=nullptr;};
static HWND prepareInfo=11,prepareButton=12,mainw=13;
static HANDLE prepareProcess=nullptr;
static std::wstring prepareStatus,prepareCancel,preparedProfile,root,profilePath,workProfile,profileId,materializedProfile;
static bool prepareCancelling=false,prepareAutomatic=false,previewQueued=false;
static unsigned preparedEpoch=0,motionEpoch=1;
static cursorondemand::Mode preparedMode=cursorondemand::Mode::All;
static cursorondemand::Queue prepareQueue;
static std::vector<unsigned>preparedPairs;
static Bytes workOriginal,workColors,workGroups;
static bool running=true,smoothing=true,displayLoaded=true,gameResourceConfirmed=true,playbackPending=false,playbackFullPending=false;
static std::set<unsigned>playbackPendingPairs;
static std::map<unsigned,int>motionCache;
static std::set<unsigned>allowedPairs;
static struct Display {std::vector<int>frames;}displayAsset;
static struct Job {bool active=false;bool valid()const{return active;}}playbackJob;
static std::set<std::wstring> files,directories;
static std::map<std::wstring,Bytes>fileContents;
static std::vector<std::wstring>savedFiles,previewPages;
static unsigned materializations=0,sourceMoves=0,hookStops=0,preparedIndexReads=0;
static bool materializeDirectoryOk=true,saveOk=true,moveOk=true;
static int moveRace=0;
using Progress=std::map<std::wstring,std::wstring>;
static std::map<std::wstring,Progress> progressFiles;
static Progress& progress(){return progressFiles[prepareStatus];}
static std::map<HWND,std::wstring> labels;
static std::wstring systemDirectory=L"C:\\Windows\\System32",application,command,currentDirectory,lastLog;
static DWORD waitResult=WAIT_TIMEOUT,exitCode=0,clockNow=0,creationFlags=0,lastError=0;
static std::vector<std::wstring> deletedFiles,collidedDirectories;
static unsigned directoryCollisions=0;
static bool parentDirectoryOk=true,taskDirectoryOk=true;
static std::vector<DWORD> waitArguments;
static std::vector<HANDLE> closedHandles;
static bool createOk=true,cancelOk=true,systemOk=true,exitQueryOk=true,inheritHandles=true;
static unsigned launches=0,cancelWrites=0,pairsReads=0,playbackResets=0,checks=0,failures=0,scenarios=0;
static void check(bool condition,const char* name){++checks;if(!condition){++failures;std::cerr<<"FAIL "<<name<<"\n";}}
[[noreturn]] static void fail(const char* text){throw std::runtime_error(text);}
static bool exists(const std::wstring& value){return files.count(value)!=0;}
static Bytes file(const std::wstring&path){auto i=fileContents.find(path);if(i==fileContents.end())fail("fixture missing file");return i->second;}
static void save(const std::wstring&path,const Bytes&bytes){if(!saveOk)fail("fixture write denied");fileContents[path]=bytes;files.insert(path);savedFiles.push_back(path);}
static unsigned pairKey(int a,int b){return (unsigned(a)<<16)|unsigned(b);}
static std::istringstream fixturePreparedPairStream(){
 ++preparedIndexReads;const auto i=fileContents.find(workProfile+L"\\pairs.txt");
 if(i==fileContents.end()){std::istringstream stream;stream.setstate(std::ios::failbit);return stream;}
 return std::istringstream(std::string(i->second.begin(),i->second.end()));
}
static std::wstring widen(const std::string&value){return std::wstring(value.begin(),value.end());}
// Only the std::filesystem directory-creation call is adapted by the extractor.
// The production materialization/byte comparison/temporary publish logic runs.
static void fixtureCreateDirectories(const std::wstring&path){++materializations;if(!materializeDirectoryOk)fail("fixture directory denied");directories.insert(path);}
static BOOL MoveFileExW(const wchar_t*from,const wchar_t*to,DWORD flags){
 check(flags==MOVEFILE_WRITE_THROUGH,"materialization uses write-through without replace-existing");++sourceMoves;
 const std::wstring source=from,target=to;if(moveRace){fileContents[target]=fileContents[source];if(moveRace==2)fileContents[target].push_back(0xFF);files.insert(target);moveRace=0;return FALSE;}
 if(!moveOk||files.count(target))return FALSE;fileContents[target]=fileContents[source];files.insert(target);files.erase(source);fileContents.erase(source);return TRUE;
}
static HANDLE ShellExecuteW(HWND,const wchar_t*verb,const wchar_t*path,const wchar_t*,const wchar_t*,int show){check(std::wstring(verb)==L"open"&&show==SW_SHOWNORMAL,"preview uses explicit local open");previewPages.push_back(path);return reinterpret_cast<HANDLE>(33);}
static void textIfChanged(HWND control,const std::wstring& text){labels[control]=text;}
static UINT GetSystemDirectoryW(wchar_t* out,UINT capacity){
 if(!systemOk)return 0;const auto n=systemDirectory.size();
 if(n>=capacity)return UINT(n+1);std::copy(systemDirectory.begin(),systemDirectory.end(),out);out[n]=0;return UINT(n);
}
static HANDLE CreateFileW(const wchar_t* path,DWORD,DWORD,void*,DWORD,DWORD,HANDLE){
 ++cancelWrites;if(!cancelOk)return INVALID_HANDLE_VALUE;files.insert(path);return reinterpret_cast<HANDLE>(3);
}
static BOOL CloseHandle(HANDLE value){closedHandles.push_back(value);return TRUE;}
static BOOL DeleteFileW(const wchar_t* path){deletedFiles.push_back(path);fileContents.erase(path);return files.erase(path)?TRUE:FALSE;}
static DWORD GetLastError(){return lastError;}
static DWORD GetCurrentProcessId(){return 42;}
static uint64_t GetTickCount64(){return 1000;}
static BOOL CreateDirectoryW(const wchar_t* path,void*){
 const std::wstring p=path;const bool leaf=p.find(L"\\.preprocess\\")!=std::wstring::npos;
 if((leaf&&!taskDirectoryOk)||(!leaf&&!parentDirectoryOk)){lastError=ERROR_ACCESS_DENIED;return FALSE;}
 if(leaf&&directoryCollisions){
  --directoryCollisions;directories.insert(p);files.insert(p+L"\\cancel.flag");collidedDirectories.push_back(p);
 }
 if(directories.count(p)||files.count(p)){lastError=ERROR_ALREADY_EXISTS;return FALSE;}
 const auto parent=p.substr(0,p.find_last_of(L"\\"));
 if(!directories.count(parent)){lastError=ERROR_PATH_NOT_FOUND;return FALSE;}
 directories.insert(p);return TRUE;
}
static BOOL CreateProcessW(const wchar_t* app,wchar_t* args,void*,void*,BOOL inherit,DWORD flags,void*,const wchar_t* dir,STARTUPINFOW*,PROCESS_INFORMATION* pi){
 ++launches;application=app;command=args;currentDirectory=dir;inheritHandles=inherit;creationFlags=flags;
 if(!createOk)return FALSE;pi->hProcess=reinterpret_cast<HANDLE>(1);pi->hThread=reinterpret_cast<HANDLE>(2);return TRUE;
}
static BOOL SetPriorityClass(HANDLE,DWORD value){check(value==BELOW_NORMAL_PRIORITY_CLASS,"background process priority");return TRUE;}
static DWORD WaitForSingleObject(HANDLE,DWORD timeout){waitArguments.push_back(timeout);return waitResult;}
static DWORD GetTickCount(){clockNow+=500;return clockNow;}
static BOOL GetExitCodeProcess(HANDLE,DWORD* out){if(!exitQueryOk)return FALSE;*out=exitCode;return TRUE;}
static DWORD GetPrivateProfileStringW(const wchar_t*,const wchar_t* key,const wchar_t* fallback,wchar_t* out,DWORD size,const wchar_t* path){
 const auto& data=progressFiles[path];const auto found=data.find(key);const auto text=found==data.end()?std::wstring(fallback):found->second;
 const size_t n=std::min(text.size(),size_t(size?size-1:0));if(size){std::copy_n(text.data(),n,out);out[n]=0;}return DWORD(n);
}
static UINT GetPrivateProfileIntW(const wchar_t*,const wchar_t* key,int fallback,const wchar_t* path){
 const auto& data=progressFiles[path];const auto found=data.find(key);if(found==data.end())return UINT(fallback);return UINT(std::stoul(found->second));
}
static void readPairs(){++pairsReads;}static void resetPrepared(bool=true){++playbackResets;}
static void stop(){++hookStops;running=false;}
static void log(const std::wstring& text){lastLog=text;}
#include PREPARATION_PRODUCTION

// Independent consumer of a Windows command-line string. Test data covers the
// whitespace/quote/backslash rules used by native command-line consumers.
static std::vector<std::wstring> arguments(const std::wstring& text){
 std::vector<std::wstring> result;size_t i=0;
 while(i<text.size()){
  while(i<text.size()&&(text[i]==L' '||text[i]==L'\t'))++i;if(i==text.size())break;
  std::wstring value;bool quoted=false;
  while(i<text.size()&&(quoted||(text[i]!=L' '&&text[i]!=L'\t'))){
   size_t slashes=0;while(i<text.size()&&text[i]==L'\\'){++slashes;++i;}
   if(i<text.size()&&text[i]==L'"'){
    value.append(slashes/2,L'\\');if(slashes%2)value+=L'"';else quoted=!quoted;++i;
   }else{value.append(slashes,L'\\');if(i<text.size()&&(quoted||(text[i]!=L' '&&text[i]!=L'\t')))value+=text[i++];}
  }
  result.push_back(std::move(value));
 }
 return result;
}
static void reset(){
 ++scenarios;prepareProcess=nullptr;prepareCancelling=false;prepareStatus.clear();prepareCancel.clear();preparedProfile.clear();
 root=L"C:\\Games\\RA2 Toolbox & 100% 中文";profilePath=root+L"\\profiles\\老师's $(Start-Process notepad)";
 workProfile=root+L"\\cache\\老师's $(Start-Process notepad) & 100%";profileId.clear();materializedProfile.clear();
 prepareAutomatic=previewQueued=false;preparedEpoch=0;motionEpoch=1;preparedMode=cursorondemand::Mode::All;prepareQueue.reset();preparedPairs.clear();
 workOriginal={1,2,3,4};workColors.assign(1024,0x22);workGroups={'[',']'};displayAsset.frames.assign(520,0);allowedPairs={pairKey(0,1)};
 running=smoothing=displayLoaded=gameResourceConfirmed=true;playbackPending=playbackFullPending=false;playbackPendingPairs.clear();motionCache.clear();playbackJob.active=false;
 fileContents.clear();savedFiles.clear();previewPages.clear();materializations=sourceMoves=hookStops=preparedIndexReads=0;materializeDirectoryOk=saveOk=moveOk=true;moveRace=0;
 files={systemDirectory+L"\\WindowsPowerShell\\v1.0\\powershell.exe",root+L"\\engine\\prepare_runtime.ps1",root+L"\\engine\\precompute.py"};
 directories={root,profilePath};progressFiles.clear();labels.clear();application.clear();command.clear();currentDirectory.clear();lastLog.clear();
 deletedFiles.clear();collidedDirectories.clear();directoryCollisions=0;parentDirectoryOk=taskDirectoryOk=true;
 waitResult=WAIT_TIMEOUT;exitCode=0;createOk=cancelOk=systemOk=exitQueryOk=true;inheritHandles=true;
 waitArguments.clear();closedHandles.clear();launches=cancelWrites=pairsReads=playbackResets=0;
}
static bool threwBegin(){try{beginPrepare();return false;}catch(const std::exception&){return true;}}
static bool contains(const std::wstring& text,const wchar_t* piece){return text.find(piece)!=std::wstring::npos;}
static void finish(const wchar_t* state,DWORD code=0){progress()[L"State"]=state;exitCode=code;waitResult=WAIT_OBJECT_0;pollPrepare();}
static void queueEdges(unsigned count,unsigned first=0){for(unsigned i=0;i<count;++i)check(prepareQueue.request(pairKey(first+i,first+i+1),displayAsset.frames.size()),"fixture records a distinct observed animation edge");}
static std::string requestText(){const auto task=prepareStatus.substr(0,prepareStatus.find_last_of(L'\\'));const auto bytes=file(task+L"\\request.json");return std::string(bytes.begin(),bytes.end());}
static void generatedPairs(const std::string&text){save(workProfile+L"\\pairs.txt",Bytes(text.begin(),text.end()));}
static void automaticRoutes(){
 reset();for(unsigned i=0;i<20;++i)pollPrepare();
 check(launches==0&&materializations==0&&savedFiles.empty()&&waitArguments.empty(),"idle and static cursors perform no materialization, downloads, process launch or wait");
 check(!prepareQueue.request(pairKey(0,0),520)&&!prepareQueue.request(pairKey(519,520),520),"invalid animation edges cannot create demand");pollPrepare();check(launches==0,"invalid edges remain idle");
 for(unsigned guard=0;guard<7;++guard){reset();queueEdges(1);
  switch(guard){case 0:running=false;break;case 1:smoothing=false;break;case 2:displayLoaded=false;break;case 3:workProfile.clear();break;case 4:playbackPending=true;break;case 5:playbackJob.active=true;break;case 6:prepareQueue.block();break;}
  for(unsigned i=0;i<3;++i)pollPrepare();check(launches==0&&materializations==0,"ineligible automatic state performs no preparation");
 }
 reset();gameResourceConfirmed=false;queueEdges(1);pollPrepare();check(launches==0&&materializations==0,"unconfirmed original game source never starts automatic computation");
 profileId=L"explicit-imported-style";pollPrepare();check(launches==1&&prepareAutomatic,"an explicit imported style can supply its own confirmed source");

 reset();queueEdges(7);pollPrepare();
 check(launches==1&&prepareProcess&&prepareAutomatic&&preparedMode==cursorondemand::Mode::Pairs&&preparedPairs.size()==4&&prepareQueue.size()==3,"one automatic launch consumes at most four pending pairs");
 check(requestText()=="{\"schema\":1,\"mode\":\"pairs\",\"pairs\":[[0,1],[1,2],[2,3],[3,4]]}\n","bounded pair job is written as literal JSON data");
 check(preparedEpoch==motionEpoch&&preparedProfile==workProfile,"job captures the source path and epoch");
 const auto epoch=motionEpoch;serviceAutomaticPrepare();check(launches==1,"active worker cannot launch a duplicate automatic job");
 generatedPairs("0 1\n1 2\n2 3\n3 4\n4 5\n5 6\n6 7\n");finish(L"done");
 check(playbackPending&&!playbackFullPending&&playbackPendingPairs.size()==4&&preparedIndexReads==1&&motionEpoch==epoch&&running&&hookStops==0&&playbackResets==0,"pair completion requests only incremental playback loading and keeps the hook running");
 pollPrepare();check(launches==1,"pending playback merge gates the next automatic batch");
 playbackPending=false;for(unsigned i=0;i<4;++i)motionCache[pairKey(i,i+1)]=1;waitResult=WAIT_TIMEOUT;pollPrepare();
 check(launches==2&&preparedPairs.size()==3&&prepareQueue.empty(),"remaining observed edges form the next bounded batch");
 check(materializations==1&&sourceMoves==3,"successive automatic batches reuse the already materialized immutable inputs");
 finish(L"done");playbackPending=false;for(unsigned i=0;i<5;++i)pollPrepare();check(launches==2,"completed pairs do not trigger an automatic whole-profile retry");

 reset();queueEdges(2);motionCache[pairKey(0,1)]=motionCache[pairKey(1,2)]=1;pollPrepare();
 check(launches==0&&materializations==0&&prepareQueue.empty(),"cache-ready demand is discarded before dependency setup");
 for(const auto state:{L"cancelled",L"error"}){reset();queueEdges(6);pollPrepare();finish(state,1);const auto before=launches;
  for(unsigned i=0;i<10;++i)pollPrepare();check(prepareQueue.blocked()&&launches==before&&!playbackPending,"cancelled or failed automatic work stays quiet until explicit retry");
  waitResult=WAIT_TIMEOUT;beginPrepare();check(launches==before+1&&!prepareAutomatic&&preparedMode==cursorondemand::Mode::All&&!prepareQueue.blocked(),"explicit retry starts one full preparation attempt");
 }
 reset();queueEdges(6);pollPrepare();beginPrepare();check(prepareCancelling&&prepareQueue.blocked(),"user cancellation blocks remaining automatic demand immediately");finish(L"cancelled",2);
 for(unsigned i=0;i<5;++i)pollPrepare();check(launches==1&&!playbackPending,"cancel completion does not automatically resume queued pairs");
 reset();queueEdges(1);createOk=false;pollPrepare();check(!prepareProcess&&prepareQueue.blocked()&&running&&launches==1,"automatic launch failure preserves the original running cursor");
 for(unsigned i=0;i<5;++i)pollPrepare();check(launches==1,"automatic launch failures do not spin retries");
 reset();workGroups.clear();queueEdges(1);pollPrepare();check(!prepareProcess&&prepareQueue.blocked()&&launches==0&&materializations==0,"incomplete immutable source fails before filesystem or process work");
}
static void staleAndPreviewRoutes(){
 for(bool changePath:{false,true})for(const auto state:{L"done",L"error",L"cancelled"}){
  reset();queueEdges(1);pollPrepare();if(changePath)workProfile+=L"-new-source";else ++motionEpoch;
  prepareQueue.reset();queueEdges(1,20);previewQueued=true;materializedProfile=workProfile;
  finish(state,!wcscmp(state,L"done")?0:1);
  check(!playbackPending&&playbackPendingPairs.empty()&&preparedIndexReads==0&&previewPages.empty(),"late work from a different source or epoch cannot apply its results");
  check(!prepareQueue.blocked()&&prepareQueue.size()==1&&previewQueued&&materializedProfile==workProfile,"late completion cannot cancel the new source queue or explicit preview");
 }
 reset();files.insert(workProfile+L"\\preview.html");pollPrepare();check(previewPages.empty()&&launches==0,"an existing preview never opens from idle polling");
 beginPrepare();finish(L"done");check(previewPages.empty(),"full preparation completion does not open an unsolicited preview");
 previewCurrent();check(previewPages.size()==1&&previewPages[0]==workProfile+L"\\preview.html"&&launches==1,"explicit preview opens the existing literal local path without launching another worker");
 reset();previewCurrent();check(launches==1&&!prepareAutomatic&&preparedMode==cursorondemand::Mode::Preview,"explicit missing preview launches a preview-only job");
 check(requestText()=="{\"schema\":1,\"mode\":\"preview\"}\n","preview job has a distinct request mode");
 files.insert(workProfile+L"\\preview.html");finish(L"done");check(previewPages.size()==1&&playbackPending&&playbackFullPending&&running&&hookStops==0,"only successful current explicit preview completion opens the page");
 reset();previewCurrent();files.insert(workProfile+L"\\preview.html");++motionEpoch;finish(L"done");check(previewPages.empty()&&!playbackPending,"late preview completion never opens a different source");
 reset();queueEdges(1);pollPrepare();previewCurrent();check(previewQueued&&launches==1,"explicit preview queues behind an active pair worker");
 finish(L"done");check(previewPages.empty()&&previewQueued,"pair completion itself does not open the queued preview");
 waitResult=WAIT_TIMEOUT;pollPrepare();check(launches==2&&preparedMode==cursorondemand::Mode::Preview,"queued explicit preview is dispatched after the worker completes");
}
static void materializationRoutes(){
 reset();materializeWorkProfile();
 check(sourceMoves==3&&file(workProfile+L"\\original.sha")==workOriginal&&file(workProfile+L"\\palette.bgra")==workColors&&file(workProfile+L"\\groups.json")==workGroups,"actual materialization commits all immutable source bytes");
 check(!exists(workProfile+L"\\pairs.txt"),"derived pairs index is not an immutable materialization input");
 const auto saves=savedFiles.size();materializeWorkProfile();check(sourceMoves==3&&savedFiles.size()==saves&&materializations==1,"same immutable source is reused without repeated source I/O");
 fileContents[workProfile+L"\\original.sha"]={0xFF};check(threwBegin()&&launches==0&&file(workProfile+L"\\original.sha")==Bytes{0xFF},"explicit retry revalidates complete bytes and never overwrites a mismatched source");
 reset();materializeDirectoryOk=false;check(threwBegin()&&launches==0&&savedFiles.empty(),"directory failure precedes any source publication or launch");
 reset();saveOk=false;check(threwBegin()&&launches==0&&sourceMoves==0,"source write failure cannot launch partial inputs");
 reset();moveOk=false;check(threwBegin()&&launches==0&&!deletedFiles.empty()&&!exists(workProfile+L"\\original.sha"),"failed source publish removes only its temporary file");
 reset();moveRace=1;materializeWorkProfile();check(materializedProfile==workProfile&&deletedFiles.size()==1,"an equal competing source is accepted after full byte comparison");
 reset();moveRace=2;check(threwBegin()&&launches==0&&exists(workProfile+L"\\original.sha"),"a different competing source remains intact and is rejected");
 reset();readPreparedPairs();check(allowedPairs==std::set<unsigned>{pairKey(0,1)},"missing generated index preserves the installed fallback");
 generatedPairs("0 1\n516 517\n");readPreparedPairs();
 check(allowedPairs==std::set<unsigned>({pairKey(0,1),pairKey(516,517)}),"actual generated-pairs parser accepts current 517-frame edges");
 const auto accepted=allowedPairs;
 for(const auto malformed:{"0 1\n3 broken\n","0 1\n3","0 1\n520 1\n","0 1\n2 2\n","0 1\n-1 4\n","0 1\n1 -1\n","0 1\n999999999999999999999 2\n","0 1\n999999999999999999999"}){
  allowedPairs=accepted;generatedPairs(malformed);readPreparedPairs();check(allowedPairs==accepted,"missing endpoint, malformed, overflowing, out-of-range or static entry rejects the complete generated index");
 }
 allowedPairs=accepted;std::string oversized;for(unsigned i=0;i<513;++i)oversized+=std::to_string(i)+" "+std::to_string(i+1)+"\n";generatedPairs(oversized);readPreparedPairs();check(allowedPairs==accepted,"oversized generated index is rejected without truncation");
 generatedPairs("0 1\n516 517 \t\n");readPreparedPairs();check(allowedPairs==accepted,"clean EOF after whitespace accepts the complete generated index");
 generatedPairs("");readPreparedPairs();check(allowedPairs.empty(),"present empty generated index represents no animation edges");
}
int main(){
 reset();pollPrepare();check(launches==0&&waitArguments.empty(),"idle feature makes no process or wait call");
 check(!exists(root+L"\\runtime\\python.exe"),"fresh slim install fixture has no Python");
 beginPrepare();check(prepareProcess!=nullptr&&launches==1,"missing Python starts asynchronous bootstrap");
 check(application==systemDirectory+L"\\WindowsPowerShell\\v1.0\\powershell.exe","absolute trusted system launcher");
 check(!inheritHandles&&creationFlags==CREATE_NO_WINDOW,"no inherited handles or console window");
 check(currentDirectory==root,"working directory is app root");
 check(waitArguments.empty(),"begin returns without waiting");
 const auto parsed=arguments(command);
 const std::vector<std::wstring> expected={application,L"-NoLogo",L"-NoProfile",L"-NonInteractive",L"-ExecutionPolicy",L"Bypass",L"-File",root+L"\\engine\\prepare_runtime.ps1",L"-Profile",workProfile,L"-Status",prepareStatus,L"-Cancel",prepareCancel};
 check(parsed==expected,"spaces, Unicode, apostrophe, percent, ampersand and dollar stay literal arguments");
 check(prepareStatus.find(workProfile+L"\\.preprocess\\")==0&&prepareCancel.find(workProfile+L"\\.preprocess\\")==0,"status and cancel live in a dedicated profile task directory");
 check(contains(labels[prepareInfo],L"缺失时从公网下载"),"first setup explains demand download");
 const auto opening=labels[prepareInfo];pollPrepare();check(labels[prepareInfo]==opening,"no status yet retains initial checking state");

 ++scenarios;
 const std::vector<std::wstring> values={L"",L"plain",L"has space",L"\t",L"ends\\",L"ends\\\\",L"literal\"quote",L"\\\"",L"\\\\\"",L"C:\\老师\\a'b & %x% $(noop)\\",L"first\nline"};
 for(const auto& value:values){const auto got=arguments(prepareArgument(L"launcher.exe")+L" "+prepareArgument(value));check(got.size()==2&&got[1]==value,"quoted argument roundtrip");}
 check(prepareArgument(L"ends\\")==L"\"ends\\\\\"","trailing backslash escaped before closing quote");

 for(const auto state:{L"dependency_check",L"dependency_wait",L"downloading",L"installing"}){
  ++scenarios;progress()={{L"State",state},{L"Message",L"正在下载 NumPy · 4 / 15 MiB"},{L"Done",L"4194304"},{L"Total",L"15728640"}};
  pollPrepare();check(labels[prepareInfo]==L"正在下载 NumPy · 4 / 15 MiB","dependency message updates from external progress");
  check(!contains(labels[prepareInfo],L"1000")&&!contains(labels[prepareInfo],L" 段"),"download bytes are not interpreted as animation segments");
  check(labels[prepareButton]==L"取消准备","setup cancel label");
  progress()[L"Message"]=L"";pollPrepare();check(!labels[prepareInfo].empty()&&!contains(labels[prepareInfo],L"1000"),"empty setup message has phase fallback");
 }
 ++scenarios;progress()={{L"State",L"running"},{L"Done",L"14"},{L"Total",L"388"}};pollPrepare();
 check(contains(labels[prepareInfo],L"14 / 388 段")&&contains(labels[prepareInfo],L"1000"),"preprocessing uses segment counts");
 check(labels[prepareButton]==L"取消预处理","running cancel label");
 progress()[L"Total"]=L"0";pollPrepare();check(contains(labels[prepareInfo],L"本机预处理"),"running before segment totals has fallback");

 ++scenarios;progress()={{L"State",L"downloading"},{L"Message",L"正在下载"}};beginPrepare();
 check(prepareCancelling&&exists(prepareCancel)&&launches==1,"cancel writes flag without duplicate process");
 pollPrepare();check(contains(labels[prepareInfo],L"正在取消"),"later progress does not overwrite requested cancellation");
 finish(L"cancelled",2);check(!prepareProcess&&!prepareCancelling&&pairsReads==0&&playbackResets==0,"cancel finalizes without refreshing playback");
 check(contains(labels[prepareInfo],L"已取消"),"cancel completion shown");
 const auto cancelledTaskStatus=prepareStatus,cancelledTaskFlag=prepareCancel;
 files.insert(prepareStatus);files.insert(prepareStatus+L".error.txt");beginPrepare();
 check(launches==2&&prepareStatus!=cancelledTaskStatus&&prepareCancel!=cancelledTaskFlag,"retry gets new status/cancel identities");
 check(exists(cancelledTaskFlag)&&exists(cancelledTaskStatus)&&exists(cancelledTaskStatus+L".error.txt"),"retry preserves all old task evidence and cancel flag");
 check(!prepareCancelling&&prepareProcess&&progress().empty(),"retry starts clean active task without inheriting old progress");
 check(deletedFiles.empty(),"new task never deletes another generation's files");

 reset();beginPrepare();cancelOk=false;beginPrepare();
 check(!prepareCancelling&&prepareProcess&&launches==1,"unwritable cancellation flag does not claim success");
 check(contains(labels[prepareInfo],L"取消失败"),"cancel write failure explained");

 reset();beginPrepare();progress()={{L"State",L"error"},{L"ErrorKind",L"dependency"},{L"Message",L"无法连接 Python 官方下载站"}};exitCode=1;waitResult=WAIT_OBJECT_0;pollPrepare();
 check(!prepareProcess&&labels[prepareButton]==L"重试预处理","setup failure returns retry action");
 check(contains(labels[prepareInfo],L"无法连接")&&contains(labels[prepareInfo],L"依赖准备失败"),"setup failure preserves useful reason");
 check(!lastLog.empty()&&pairsReads==0&&playbackResets==0,"failure recorded without success side effects");

 reset();beginPrepare();progress()={{L"State",L"installing"}};exitCode=1;waitResult=WAIT_OBJECT_0;pollPrepare();
 check(contains(labels[prepareInfo],L"依赖准备失败")&&labels[prepareButton]==L"重试预处理","interrupted setup remains retryable");

 reset();beginPrepare();exitCode=1;waitResult=WAIT_OBJECT_0;pollPrepare();
 check(contains(labels[prepareInfo],L"PowerShell")&&contains(labels[prepareInfo],L"系统策略"),"policy failure before first status is actionable");
 check(!prepareProcess&&labels[prepareButton]==L"重试预处理","early helper exit cannot remain pending");

 reset();beginPrepare();finish(L"error",0);
 check(!prepareProcess&&pairsReads==0&&labels[prepareButton]==L"重试预处理","zero exit requires done state");
 reset();beginPrepare();finish(L"done",1);
 check(!prepareProcess&&pairsReads==0&&labels[prepareButton]==L"重试预处理","done state requires zero exit");
 reset();beginPrepare();progress()[L"Total"]=L"388";finish(L"done",0);
 check(!prepareProcess&&pairsReads==0&&playbackResets==0&&playbackPending&&playbackFullPending&&running&&hookStops==0,"same source success only schedules background playback without stopping the hook");
 check(contains(labels[prepareInfo],L"388 段")&&labels[prepareButton]==L"预处理补帧","success reports complete segments");
 reset();beginPrepare();const auto activeProfile=preparedProfile;workProfile=root+L"\\cache\\different";finish(L"done",0);
 check(preparedProfile==activeProfile&&pairsReads==0&&playbackResets==0,"switching profile does not retarget active task or refresh wrong profile");

 reset();beginPrepare();progress()[L"State"]=L"done";waitResult=WAIT_OBJECT_0;exitQueryOk=false;pollPrepare();
 check(!prepareProcess&&pairsReads==0&&labels[prepareButton]==L"重试预处理","exit query failure is not success");
 reset();beginPrepare();progress()[L"State"]=L"done";waitResult=WAIT_FAILED;pollPrepare();
 check(!prepareProcess&&pairsReads==0&&labels[prepareButton]==L"重试预处理","invalid process wait is not success");
 reset();beginPrepare();finish(L"done",STILL_ACTIVE);
 check(!prepareProcess&&pairsReads==0,"terminated process exit 259 cannot appear permanently running");

 reset();files.erase(root+L"\\engine\\prepare_runtime.ps1");check(threwBegin()&&launches==0,"missing bootstrap rejects before process");
 reset();files.erase(root+L"\\engine\\precompute.py");check(threwBegin()&&launches==0,"missing preprocessor rejects before downloads");
 reset();files.erase(systemDirectory+L"\\WindowsPowerShell\\v1.0\\powershell.exe");check(threwBegin()&&launches==0,"missing trusted launcher never searches PATH");
 reset();systemOk=false;check(threwBegin()&&launches==0,"system directory lookup failure never searches PATH");
 reset();workProfile+=std::wstring(12000,L'x');directories.insert(workProfile);check(threwBegin()&&launches==0,"oversized command rejected before CreateProcess");
 reset();parentDirectoryOk=false;check(threwBegin()&&launches==0,"unwritable task parent blocks launch");
 reset();taskDirectoryOk=false;check(threwBegin()&&launches==0,"unwritable task directory blocks launch");
 reset();files.insert(workProfile+L"\\.preprocess");check(threwBegin()&&launches==0,"task parent path occupied by file blocks launch");
 reset();directoryCollisions=1;beginPrepare();
 check(launches==1&&collidedDirectories.size()==1,"existing task id collision retries with a new id");
 check(prepareCancel!=collidedDirectories[0]+L"\\cancel.flag"&&exists(collidedDirectories[0]+L"\\cancel.flag"),"colliding old task cancellation flag is preserved");
 reset();directoryCollisions=20;check(threwBegin()&&launches==0&&collidedDirectories.size()==16,"task directory collision retries are bounded");
 reset();createOk=false;check(threwBegin()&&!prepareProcess&&launches==1,"process launch failure leaves no active task");

 reset();beginPrepare();closePrepare();
 check(!prepareProcess&&!prepareCancelling&&exists(prepareCancel),"close signals cancellation and releases process handle");
 check(waitArguments.empty(),"close never blocks on downloads");
 check(std::find(closedHandles.begin(),closedHandles.end(),reinterpret_cast<HANDLE>(1))!=closedHandles.end(),"close owns and closes child process handle");
 closePrepare();check(cancelWrites==1,"repeated close is harmless");
 ++scenarios;
 const auto closedTaskFlag=prepareCancel,closedTaskStatus=prepareStatus;
 progress()[L"State"]=L"cancelled";files.insert(closedTaskStatus);beginPrepare();
 check(prepareCancel!=closedTaskFlag&&prepareStatus!=closedTaskStatus,"quick reopen cannot reuse old task control files");
 check(exists(closedTaskFlag)&&exists(closedTaskStatus),"quick reopen leaves cancellation visible to old worker");
 check(progressFiles[closedTaskStatus][L"State"]==L"cancelled"&&progress().empty(),"old progress cannot masquerade as new task progress");
 check(deletedFiles.empty(),"quick reopen never clears the old cancel flag");
 reset();beginPrepare();cancelOk=false;closePrepare();check(!prepareProcess,"close remains nonblocking when flag is unwritable");

 reset();beginPrepare();for(int i=0;i<5;++i){progress()[L"State"]=L"downloading";pollPrepare();}
 check(waitArguments.size()==5&&std::all_of(waitArguments.begin(),waitArguments.end(),[](DWORD value){return value==0;}),"all active polls have zero wait timeout");
 automaticRoutes();staleAndPreviewRoutes();materializationRoutes();
 std::cout<<"{\"scenarios\":"<<scenarios<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
 return failures?1:0;
}

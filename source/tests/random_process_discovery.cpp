#define WINMOCK_IMAGE_FILES
#define WINMOCK_VERSION_FILES
#define WINMOCK_TARGET_PROFILE
#define WINMOCK_DESCRIPTION_WORKERS
#ifndef DISCOVERY_HEADER
#define DISCOVERY_HEADER "../external_reader/source/process_discovery.h"
#endif
#include DISCOVERY_HEADER
#include <cassert>
#include <fstream>
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>
struct Proc {std::wstring name,path;DWORD creation;bool accessible=true,alive=true;};
static std::map<DWORD,Proc> procs;
static std::vector<DWORD> listed,windows;static std::map<DWORD,std::vector<DWORD>> children;static DWORD foreground=999;static size_t nextProc,pos;
static std::map<DWORD,DWORD> owners;static std::map<DWORD,unsigned> identityCalls;
static std::vector<unsigned char> image,other;static const std::vector<unsigned char>* opened;
static DWORD ticks=10;static int fileOpens=0;static bool failSnapshot=false;
static DWORD lastError=0;static bool profileWriteFailure=false;
static std::map<std::wstring,std::wstring> settings;
static std::map<std::wstring,std::map<uint32_t,std::wstring>> versions;
static std::wstring versionPath;static unsigned versionReads=0,workerStarts=0;
struct Language {uint16_t language,codepage;};static std::vector<Language> versionLanguages;
static std::vector<std::pair<DWORD(*)(void*),void*>> workers;
static DWORD versionSize(const wchar_t* path,DWORD*){if(!versions.count(path)){lastError=ERROR_RESOURCE_TYPE_NOT_FOUND;return 0;}return 32;}
static BOOL versionRead(const wchar_t* path,DWORD,DWORD,void*){++versionReads;versionPath=path;return TRUE;}
static BOOL versionQuery(const void*,const wchar_t* key,void** value,unsigned* bytes){
 auto& strings=versions[versionPath];
 if(std::wstring(key)==L"\\VarFileInfo\\Translation"){
  versionLanguages.clear();for(const auto& item:strings)versionLanguages.push_back({uint16_t(item.first>>16),uint16_t(item.first)});
  *value=versionLanguages.data();*bytes=unsigned(versionLanguages.size()*sizeof(Language));return !versionLanguages.empty();
 }
 unsigned language=0,codepage=0;
 if(std::swscanf(key,L"\\StringFileInfo\\%4x%4x\\FileDescription",&language,&codepage)!=2)return FALSE;
 auto found=strings.find(uint32_t(language)<<16|codepage);if(found==strings.end())return FALSE;
 *value=const_cast<wchar_t*>(found->second.c_str());*bytes=unsigned(found->second.size()+1);return TRUE;
}
static void finishDescriptions(){while(!workers.empty()){auto work=workers.front();workers.erase(workers.begin());work.first(work.second);}}
extern "C" {
DWORD GetCurrentProcessId(){return 999;}DWORD GetLastError(){return lastError;}DWORD GetTickCount(){return ticks;}
HANDLE CreateToolhelp32Snapshot(DWORD,DWORD){nextProc=0;if(failSnapshot)lastError=ERROR_ACCESS_DENIED;return failSnapshot?INVALID_HANDLE_VALUE:(HANDLE)1;}
BOOL Process32FirstW(HANDLE,PROCESSENTRY32W*e){if(listed.empty()){lastError=ERROR_NO_MORE_FILES;return FALSE;}e->th32ProcessID=listed[0];std::wcscpy(e->szExeFile,procs[listed[0]].name.c_str());return TRUE;}
BOOL Process32NextW(HANDLE,PROCESSENTRY32W*e){if(++nextProc>=listed.size())return FALSE;e->th32ProcessID=listed[nextProc];std::wcscpy(e->szExeFile,procs[listed[nextProc]].name.c_str());return TRUE;}
BOOL CloseHandle(HANDLE){return TRUE;}
HANDLE OpenProcess(DWORD mask,BOOL,DWORD p){assert(mask==PROCESS_QUERY_LIMITED_INFORMATION||mask==(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE));const auto found=procs.find(p);if(found==procs.end()){lastError=ERROR_INVALID_PARAMETER;return nullptr;}if(!found->second.accessible)lastError=ERROR_ACCESS_DENIED;return found->second.accessible?(HANDLE)uintptr_t(p):nullptr;}
BOOL QueryFullProcessImageNameW(HANDLE h,DWORD,wchar_t*out,DWORD*n){auto&s=procs[DWORD(uintptr_t(h))].path;if(s.size()>=*n)return FALSE;std::wcscpy(out,s.c_str());*n=s.size();return TRUE;}
BOOL GetProcessTimes(HANDLE h,FILETIME*c,FILETIME*,FILETIME*,FILETIME*){const DWORD pid=DWORD(uintptr_t(h));++identityCalls[pid];*c={procs[pid].creation,0};return TRUE;}
HANDLE CreateFileW(const wchar_t* path,DWORD access,DWORD,void*,DWORD,DWORD,HANDLE){assert(access==GENERIC_READ);++fileOpens;pos=0;opened=std::wstring(path)==L"C:\\YR\\good.exe"?&image:&other;return (HANDLE)1000;}
BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*n){n->QuadPart=opened->size();return TRUE;}
BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER at,LARGE_INTEGER*,DWORD){pos=at.QuadPart;return TRUE;}
BOOL ReadFile(HANDLE,void*out,DWORD n,DWORD*got,void*){*got=DWORD(std::min<size_t>(n,pos<=opened->size()?opened->size()-pos:0));if(*got)std::memcpy(out,opened->data()+pos,*got);pos+=*got;return TRUE;}
BOOL EnumWindows(BOOL(*fn)(HWND,LPARAM),LPARAM p){for(auto id:windows)fn((HWND)uintptr_t(id),p);return TRUE;}
HWND GetForegroundWindow(){return (HWND)uintptr_t(foreground);}
BOOL EnumChildWindows(HWND parent,BOOL(*fn)(HWND,LPARAM),LPARAM p){for(auto id:children[DWORD(uintptr_t(parent))]){fn((HWND)uintptr_t(id),p);EnumChildWindows((HWND)uintptr_t(id),fn,p);}return TRUE;}
DWORD GetWindowThreadProcessId(HWND w,DWORD*p){const DWORD id=DWORD(uintptr_t(w));*p=owners.count(id)?owners[id]:id;return *p+1000;}
BOOL GetClientRect(HWND,RECT*r){*r={0,0,800,600};return TRUE;}BOOL IsWindowVisible(HWND){return TRUE;}
BOOL IsIconic(HWND){return FALSE;}LONG GetWindowLongW(HWND,int){return 0;}HWND GetWindow(HWND,unsigned){return nullptr;}
HMODULE LoadLibraryExW(const wchar_t*,HANDLE,DWORD flags){assert(flags==LOAD_LIBRARY_SEARCH_SYSTEM32);return (HMODULE)1;}
void* GetProcAddress(HMODULE,const char* name){if(!std::strcmp(name,"GetFileVersionInfoSizeW"))return reinterpret_cast<void*>(versionSize);if(!std::strcmp(name,"GetFileVersionInfoW"))return reinterpret_cast<void*>(versionRead);if(!std::strcmp(name,"VerQueryValueW"))return reinterpret_cast<void*>(versionQuery);return nullptr;}
DWORD GetModuleFileNameW(HMODULE,wchar_t*out,DWORD){std::wcscpy(out,L"C:\\Toolbox\\RA2Toolbox.exe");return DWORD(std::wcslen(out));}
DWORD GetPrivateProfileStringW(const wchar_t*,const wchar_t* key,const wchar_t* fallback,wchar_t*out,DWORD count,const wchar_t*){const auto found=settings.find(key);const auto text=found==settings.end()?std::wstring(fallback):found->second;std::wcsncpy(out,text.c_str(),count);out[count-1]=0;return DWORD(std::wcslen(out));}
BOOL WritePrivateProfileSectionW(const wchar_t* section,const wchar_t* body,const wchar_t* path){
 assert(std::wstring(section)==L"GameProcess"&&std::wstring(path)==L"C:\\Toolbox\\Toolbox.ini");
 if(profileWriteFailure){lastError=ERROR_ACCESS_DENIED;return FALSE;}settings.clear();
 for(const wchar_t* at=body;*at;at+=std::wcslen(at)+1){const std::wstring entry=at;const auto split=entry.find(L'=');assert(split!=std::wstring::npos);settings[entry.substr(0,split)]=entry.substr(split+1);}return TRUE;
}
HANDLE CreateThread(void*,size_t,DWORD(*fn)(void*),void* arg,DWORD,DWORD*){++workerStarts;workers.push_back({fn,arg});return (HANDLE)9999;}
DWORD WaitForSingleObject(HANDLE process,DWORD timeout){assert(!timeout);return procs[DWORD(uintptr_t(process))].alive?WAIT_TIMEOUT:WAIT_OBJECT_0;}
}
static unsigned discoveryChecks=0;
static void discoveryAssert(bool passed,const char* expression,int line){++discoveryChecks;if(!passed){std::cerr<<"FAIL line "<<line<<": "<<expression<<'\n';std::abort();}}
#undef assert
#define assert(expression) discoveryAssert(bool(expression),#expression,__LINE__)
int main(int argc,char**argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);image.assign(std::istreambuf_iterator<char>(f),{});assert(image.size()==external::imageidentity::FileBytes);
 using namespace external::discovery;
 unsigned nameFailures=0;
 for(const wchar_t* name:{L"gameares.exe",L"GAMEARES.DAT",L"gamemdAres.exe",L"GameMDAres.dat",L"gamemd-ares.dat"}){
  procs[71]={name,L"C:\\Other\\not-the-fingerprint.exe",71};listed={71};windows={71};
  const bool known=isGameName(name);const auto found=scan();++discoveryChecks;
  if(!known||found.best.pid!=71){++nameFailures;std::cout<<"FAIL known alias discovery: "<<std::string(name,name+std::wcslen(name))<<"\n";}
 }
#ifdef DISCOVERY_NAME_REGRESSION_ONLY
 std::cout<<"alias_failures="<<nameFailures<<"\n";return nameFailures?1:0;
#else
 assert(!nameFailures);finishDescriptions();
 auto matches=[](const std::vector<unsigned char>&b){return external::imageidentity::matches([&](uint32_t at,void*out,size_t n){if(at+n>b.size())return false;std::memcpy(out,b.data()+at,n);return true;},b.size());};
 assert(matches(image));other=image;other[0x2339b0]^=1;assert(!matches(other));
 for(size_t at:{size_t(0),size_t(0x237e37),size_t(0x3b8a00),size_t(0x42d028)}){auto b=image;b[at]^=1;assert(!matches(b));}
 auto shortFile=image;shortFile.pop_back();assert(!matches(shortFile));
 assert(external::imageidentity::randomName(L"8ed53a0454bfd.exe")&&external::imageidentity::randomName(L"5adb86d563c40d"));
 assert(!external::imageidentity::randomName(L"explorer.exe")&&!external::imageidentity::randomName(L"123"));
 procs[42]={L"8ed53a0454bfd.exe",L"C:\\YR\\good.exe",1};listed={42};windows={42};
 assert(scan().best.pid==42);int reads=fileOpens;assert(scan().best.pid==42&&fileOpens==reads);
 // Next platform launch has another name and PID; the previous HWND is gone.
 procs[43]={L"5adb86d563c40d.exe",L"C:\\YR\\good.exe",2};listed={43};windows={43};assert(scan().best.pid==43);
 // Window-backed recognition also supports names not matching the hex hint.
 procs[43].name=L"renamed-game.dat";assert(scan().best.pid==43);
 // Identical random name on an unrelated executable is insufficient.
 procs[44]={L"8ed53a0454bfd.exe",L"C:\\Other\\bad.exe",3};listed={44};windows={44};assert(!scan().best.pid);
 // A recycled PID must invalidate even a previously positive cache entry.
 procs[43]={L"5adb86d563c40d.exe",L"C:\\Other\\bad.exe",4};listed={43};windows={43};assert(!scan().best.pid);
 procs[45]={L"abcdef123456.exe",L"C:\\YR\\good.exe",5};listed={45};windows.clear();assert(scan().best.pid==45&&scan().uniqueWindowless);
 // Snapshot failure and a renderer inside a platform host still resolve.
 failSnapshot=true;windows={888};children[888]={45};foreground=888;assert(scan().best.pid==45&&scan().best.thread==1045);failSnapshot=false;children.clear();
 // Clicking the toolbox moves focus away from a platform's renderer host.
 // Arbitrary renamed child processes must still be found under background
 // top-level hosts; their file identity, not the name or foreground, admits them.
 procs[45].name=L"renamed-renderer.exe";listed={45};windows={888};children[888]={45};foreground=999;
 const auto background=scan();assert(background.best.pid==45&&background.best.window==(HWND)45&&background.best.thread==1045);
 assert(scan(45).best.pid==45&&scan(45).best.window==(HWND)45);
 failSnapshot=true;assert(scan().best.pid==45&&scan().best.window==(HWND)45);failSnapshot=false;
 // The actual API recursively visits grandchildren; production must not
 // re-enumerate the same subtree for each callback.
 children[888]={889};children[889]={45};assert(scan().best.window==(HWND)45);
 // A hex-named process found in the snapshot also needs its child HWND, or
 // the command bridge would remain windowless despite a visible renderer.
 procs[45].name=L"abcdef123456.exe";assert(scan().best.window==(HWND)45);
 // Traversing another host's children must never admit an unrelated image.
 procs[44].name=L"unrelated-child.exe";listed={44};children[888]={44};assert(!scan().best.pid);
 // Cross-process descendants may expose several HWNDs for the same renderer.
 // One scan performs one precise identity check/file read for that PID.
 procs[50]={L"another-renderer.exe",L"C:\\YR\\good.exe",50};listed={50};
 children.clear();children[888]={889};children[889]={50,501};owners[501]=50;
 reads=fileOpens;const auto duplicate=scan();
 assert(duplicate.best.pid==50&&duplicate.windowCount==2&&identityCalls[50]==2&&fileOpens==reads+1);
 // A negative hex-name identity tried during the snapshot is not repeated
 // for each of its windows in the same scan.
 procs[44].name=L"8ed53a0454bfd.exe";listed={44};children.clear();children[888]={44,441};owners[441]=44;
 const unsigned rejectedChecks=identityCalls[44];assert(!scan().best.pid&&identityCalls[44]==rejectedChecks+2);
 children.clear();foreground=999;
 procs[46]={L"gamemd.exe",L"C:\\YR\\gamemd.exe",6};listed={45,46};windows={45,46};assert(scan().processCount==2);assert(scan(45).best.pid==45);assert(!scan(47).best.pid);
 procs[45].accessible=false;listed={45};windows={45};assert(!scan().best.pid);
 finishDescriptions();
 // A renamed executable is admitted by its Task Manager FileDescription,
 // without requiring the one historical disk fingerprint or a visible HWND.
 procs[81]={L"platform-renderer.exe",L"C:\\Platform\\random.exe",81};listed={81};windows.clear();children.clear();
 versions[procs[81].path]={{0x080404b0,L"其他语言说明"},{0x041104e4,L"MAIN EXECUTABLE FOR YURI'S REVENGE"}};
 unsigned beforeReads=versionReads,beforeWorkers=workerStarts;
 auto pending=scan();assert(!pending.best.pid&&pending.descriptionPending==1&&versionReads==beforeReads);
 scan();scan();assert(workerStarts==beforeWorkers+1&&versionReads==beforeReads);
 finishDescriptions();auto described=scan();
 assert(described.best.pid==81&&described.descriptionMatches==1&&described.uniqueWindowless);
 assert(!described.fingerprintCandidates&&versionReads==beforeReads+1);
 // Cached metadata stays available while its bounded background refresh runs.
 ticks+=3001;auto refreshing=scan();assert(refreshing.best.pid==81&&refreshing.descriptionPending==1);finishDescriptions();
 // Reusing the PID for another program cannot reuse the old positive metadata.
 procs[81]={L"other.exe",L"C:\\Other\\unknown.exe",82};auto reused=scan();assert(!reused.best.pid);finishDescriptions();assert(!scan().best.pid);
 using namespace external::targetselection;
 assert(matchesKeywords(L"Main executable for Yuri's Revenge",L"  yUrI\tREVENGE  "));
 assert(!matchesKeywords(L"Yuri editor",L"Yuri Revenge")&&!matchesKeywords(L"any",L" \t"));
 // A translated description alone also works when English is not the first table.
 procs[82]={L"totally-renamed.exe",L"C:\\Platform\\other-build.exe",83};listed={82};windows.clear();
 versions[procs[82].path]={{0x041904b0,L"Main executable for Yuri's Revenge"}};
 auto all=enumerateProcesses();assert(all.size()==1&&all[0].description==L"Main executable for Yuri's Revenge");assert(scan().best.pid==82);
 assert(saveKeywords(L"totally renamed"));assert(scan().best.pid==82);assert(saveKeywords(L"Yuri Revenge"));
 // Remembered programs extend the detection list; they never lock out an
 // otherwise compatible game. Paths and exact names are distinct bounded keys.
 DWORD error=0;assert(!addProgram(L"",&error)&&error==ERROR_INVALID_PARAMETER);
 assert(!addProgram(L"relative\\game.exe",&error)&&error==ERROR_INVALID_PARAMETER);
 const uint64_t initialRevision=revision();assert(addProgram(L" custom.exe ")&&revision()>initialRevision);
 assert(addProgram(L"CUSTOM.EXE")&&current().rememberedPrograms.size()==1);
 procs[83]={L"custom.exe",L"C:\\Mod\\custom.exe",84};procs[84]={L"gamemd.exe",L"C:\\YR\\gamemd.exe",85};listed={83,84};windows={83,84};
 auto both=scan();assert(both.processCount==2&&both.rememberedMatches==1);
 auto selected=inspectIdentity(83);assert(rememberProcess(selected));auto chosen=scan();assert(chosen.best.pid==83&&chosen.explicitSelection);
 assert(current().rememberedPrograms.size()==2&&preferredAlive(current()));
 assert(scan(84).best.pid==84&&!scan(84).explicitSelection);
 // Clearing only the current preference retains every remembered program.
 assert(clearPreferredProcess()&&current().rememberedPrograms.size()==2&&!current().selectedPid);
 assert(removeProgram(1)&&removeProgram(0)&&current().rememberedPrograms.empty());
 listed={83};windows={83};assert(!scan().best.pid);assert(scan(83).best.pid==83); // old bound lease can still resolve its window
 // Persisted keys contain no PID. A selected stale/recycled PID is ignored.
 assert(rememberProcess(selected));assert(!settings.count(L"SelectedPid")&&!settings.count(L"SelectedCreation"));
 procs[83].creation=86;assert(!preferredAlive(current()));assert(!scan().explicitSelection);
 assert(!rememberProcess(selected,&error)&&error==ERROR_NOT_FOUND);
 procs[83].alive=false;assert(!preferredAlive(current()));procs[83].alive=true;
 auto beforeFailure=current();profileWriteFailure=true;assert(!addProgram(L"not-saved.exe",&error)&&error==ERROR_ACCESS_DENIED);profileWriteFailure=false;
 assert(current().rememberedPrograms==beforeFailure.rememberedPrograms&&revision()==beforeFailure.revision);
 finishDescriptions();
 // Persist/reload multiple records without restoring a recyclable PID or
 // converting the additive list into an exclusive startup target.
 for(size_t i=current().rememberedPrograms.size();i<64;++i)assert(addProgram(L"remembered-"+std::to_wstring(i)+L".exe"));
 assert(!addProgram(L"over-capacity.exe",&error)&&error==ERROR_TOO_MANY_NAMES);
 const auto persisted=current();
 {auto& shared=external::targetselection::state();std::lock_guard<std::mutex> guard(shared.mutex);shared.loaded=false;shared.value=Policy{};}
 const auto reloaded=current();assert(reloaded.rememberedPrograms==persisted.rememberedPrograms&&reloaded.keywords==persisted.keywords&&!reloaded.selectedPid&&!reloaded.selectedCreation);
 // A large system process list cannot create a worker per PID or an unbounded
 // pending queue; misses left by the first bounded batch are handled later.
 listed.clear();windows.clear();const unsigned startsBeforeBatch=workerStarts;
 for(DWORD i=2000;i<2600;++i){procs[i]={L"background-"+std::to_wstring(i)+L".exe",L"C:\\Other\\background-"+std::to_wstring(i)+L".exe",i};listed.push_back(i);}
 const auto large=scan();assert(large.descriptionPending==600&&workerStarts==startsBeforeBatch+1);
 {auto& cache=descriptionCache();std::lock_guard<std::mutex> guard(cache.mutex);assert(cache.queue.size()==512);}
 finishDescriptions();const auto remaining=scan();assert(remaining.descriptionPending==88&&workerStarts==startsBeforeBatch+2);
 finishDescriptions();assert(scan().descriptionPending==0);
 std::cout<<"PASS production discovery: gameares aliases, fingerprint boundary, all-window trees, descriptions/translations, bounded asynchronous metadata, cache/birth expiry, keyword AND search, additive remembered programs, current-process preference, stale PID rejection and persistent-write failures\n";
 std::cout<<"{\"passed\":true,\"checks\":"<<discoveryChecks<<",\"known_alias_cases\":5,\"metadata_queue_processes\":600,\"remembered_program_capacity\":64,\"real_windows_tested\":false,\"real_game_tested\":false}\n";
#endif
}


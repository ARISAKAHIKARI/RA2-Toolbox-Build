#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "process_file_description.h"

namespace external { namespace targetselection {
struct Policy {
 std::wstring keywords=L"Yuri Revenge";
 std::vector<std::wstring> rememberedPrograms;
 DWORD selectedPid=0;uint64_t selectedCreation=0,revision=1;
};
struct ProcessInfo {
 DWORD pid=0;uint64_t creation=0;
 std::wstring name,path,description;
 DWORD error=0,identityError=0,descriptionError=0;
 bool descriptionPending=false;
};
inline std::wstring trim(const std::wstring& value){
 size_t first=0,last=value.size();
 while(first<last&&std::iswspace(value[first]))++first;
 while(last>first&&std::iswspace(value[last-1]))--last;
 return value.substr(first,last-first);
}
inline std::wstring folded(std::wstring value){
 for(auto& c:value){if(c==L'/')c=L'\\';else c=static_cast<wchar_t>(std::towlower(c));}
 return value;
}
inline bool matchesKeywords(const std::wstring& text,const std::wstring& words){
 const auto haystack=folded(text);bool any=false;size_t at=0;
 while(at<words.size()){
  while(at<words.size()&&std::iswspace(words[at]))++at;
  const size_t begin=at;while(at<words.size()&&!std::iswspace(words[at]))++at;
  if(at==begin)break;
  any=true;if(haystack.find(folded(words.substr(begin,at-begin)))==std::wstring::npos)return false;
 }
 return any;
}
inline bool sameProgram(const std::wstring& a,const std::wstring& b){return folded(a)==folded(b);}
inline std::wstring basename(const std::wstring& path){const size_t at=path.find_last_of(L"\\/");return at==std::wstring::npos?path:path.substr(at+1);}
inline bool remembered(const ProcessInfo& info,const Policy& policy){
 for(const auto& item:policy.rememberedPrograms){
  const bool path=item.find_first_of(L"\\/")!=std::wstring::npos;
  if(path?!info.path.empty()&&sameProgram(item,info.path):!info.name.empty()&&sameProgram(item,info.name))return true;
 }
 return false;
}
inline DWORD actualError(DWORD fallback){const DWORD error=GetLastError();return error?error:fallback;}
inline ProcessInfo inspectIdentity(DWORD pid,const wchar_t* name=nullptr){
 ProcessInfo info;info.pid=pid;if(name)info.name=name;
 HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
 if(!process){info.error=info.identityError=actualError(ERROR_ACCESS_DENIED);return info;}
 wchar_t path[32768]{};DWORD count=32768;FILETIME created{},exited{},kernel{},user{};
 const bool pathOkay=QueryFullProcessImageNameW(process,0,path,&count)!=FALSE;
 if(!pathOkay)info.identityError=actualError(ERROR_INVALID_DATA);
 const bool timeOkay=GetProcessTimes(process,&created,&exited,&kernel,&user)!=FALSE;
 if(!timeOkay&&!info.identityError)info.identityError=actualError(ERROR_INVALID_DATA);
 CloseHandle(process);
 if(pathOkay){info.path=path;if(info.name.empty())info.name=basename(info.path);}
 if(timeOkay)info.creation=uint64_t(created.dwHighDateTime)<<32|created.dwLowDateTime;
 info.error=info.identityError;
 return info;
}
struct DescriptionRequest {DWORD pid=0;uint64_t creation=0,serial=0;std::wstring path;};
struct DescriptionEntry {
 uint64_t creation=0,serial=0;std::wstring path,description;DWORD tick=0,error=0;bool pending=false;
};
struct DescriptionCache {
 std::mutex mutex;std::unordered_map<DWORD,DescriptionEntry> values;
 std::deque<DescriptionRequest> queue;uint64_t serial=0;bool workerRunning=false;
};
inline DescriptionCache& descriptionCache(){static auto* cache=new DescriptionCache;return *cache;}
inline bool sameDescriptionIdentity(const DescriptionEntry& entry,const ProcessInfo& info){return entry.creation==info.creation&&entry.path==info.path;}
inline DescriptionRequest reserveDescription(DescriptionCache& cache,const ProcessInfo& info){
 if(cache.values.size()>=4096&&!cache.values.count(info.pid))cache.values.erase(cache.values.begin());
 const uint64_t serial=++cache.serial;
 auto& entry=cache.values[info.pid];
 if(!sameDescriptionIdentity(entry,info))entry={info.creation,serial,info.path,L"",0,0,true};
 entry.serial=serial;entry.pending=true;
 return {info.pid,info.creation,serial,info.path};
}
inline void publishDescription(DescriptionCache& cache,const DescriptionRequest& request,const std::wstring& description,DWORD error){
 const auto found=cache.values.find(request.pid);
 if(found==cache.values.end()||found->second.serial!=request.serial)return;
 auto& entry=found->second;entry.description=description;entry.error=error;entry.tick=GetTickCount();entry.pending=false;
}
inline DWORD WINAPI descriptionWorker(void*){
 auto& cache=descriptionCache();
 for(;;){
  DescriptionRequest request;
  {std::lock_guard<std::mutex> lock(cache.mutex);
   if(cache.queue.empty()){cache.workerRunning=false;return 0;}
   request=std::move(cache.queue.front());cache.queue.pop_front();
   const auto found=cache.values.find(request.pid);
   if(found==cache.values.end()||found->second.serial!=request.serial||!found->second.pending)continue;
  }
  DWORD error=0;std::wstring description;
  try{description=filedescription::read(request.path,error);}catch(...){error=ERROR_NOT_ENOUGH_MEMORY;}
  {std::lock_guard<std::mutex> lock(cache.mutex);publishDescription(cache,request,description,error);}
 }
}
inline void obtainDescription(ProcessInfo& info,bool background){
 if(info.identityError||info.path.empty())return;
 auto& cache=descriptionCache();DescriptionRequest request;bool startWorker=false;
 {std::lock_guard<std::mutex> lock(cache.mutex);const auto found=cache.values.find(info.pid);
  if(found!=cache.values.end()&&sameDescriptionIdentity(found->second,info)&&!found->second.pending&&DWORD(GetTickCount()-found->second.tick)<3000){
   info.description=found->second.description;info.error=info.descriptionError=found->second.error;return;
  }
  if(background){
   info.descriptionPending=true;
   // Preserve the last result for this exact creation/path while refreshing.
   // A three-second expiry must not temporarily hide a running known game.
   if(found!=cache.values.end()&&sameDescriptionIdentity(found->second,info)){
    info.description=found->second.description;info.error=info.descriptionError=found->second.error;
   }
   if((found==cache.values.end()||!sameDescriptionIdentity(found->second,info)||!found->second.pending)&&cache.queue.size()<512)
    cache.queue.push_back(reserveDescription(cache,info));
   if(!cache.workerRunning&&!cache.queue.empty()){cache.workerRunning=true;startWorker=true;}
  }else request=reserveDescription(cache,info);
 }
 if(background){
  if(startWorker){
   HANDLE worker=CreateThread(nullptr,0,descriptionWorker,nullptr,0,nullptr);
   if(worker)CloseHandle(worker);
   else{const DWORD error=actualError(ERROR_NOT_ENOUGH_MEMORY);std::lock_guard<std::mutex> lock(cache.mutex);cache.workerRunning=false;info.error=info.descriptionError=error;}
  }
  return;
 }
 // UI enumeration runs on its own worker. Automatic attachment never enters
 // this synchronous branch, and no cache lock is held across version/file I/O.
 info.description=filedescription::read(info.path,info.descriptionError);info.error=info.descriptionError;
 {std::lock_guard<std::mutex> lock(cache.mutex);publishDescription(cache,request,info.description,info.descriptionError);}
}
inline ProcessInfo inspectProcess(DWORD pid,const wchar_t* name=nullptr){auto info=inspectIdentity(pid,name);obtainDescription(info,false);return info;}
inline ProcessInfo inspectCachedProcess(DWORD pid,const wchar_t* name=nullptr){auto info=inspectIdentity(pid,name);obtainDescription(info,true);return info;}
inline std::vector<ProcessInfo> enumerateProcessList(bool background,DWORD* snapshotError){
 if(snapshotError)*snapshotError=0;std::vector<ProcessInfo> values;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snapshot==INVALID_HANDLE_VALUE){if(snapshotError)*snapshotError=actualError(ERROR_GEN_FAILURE);return values;}
 PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
 if(Process32FirstW(snapshot,&entry))do{
  if(entry.th32ProcessID&&entry.th32ProcessID!=GetCurrentProcessId()){
   auto info=inspectIdentity(entry.th32ProcessID,entry.szExeFile);obtainDescription(info,background);values.push_back(std::move(info));
  }
 }while(Process32NextW(snapshot,&entry));
 else if(snapshotError){const DWORD error=GetLastError();if(error!=ERROR_NO_MORE_FILES)*snapshotError=error;}
 CloseHandle(snapshot);return values;
}
inline std::vector<ProcessInfo> enumerateProcesses(DWORD* snapshotError=nullptr){return enumerateProcessList(false,snapshotError);}
inline std::vector<ProcessInfo> enumerateCandidates(DWORD* snapshotError=nullptr){return enumerateProcessList(true,snapshotError);}
struct State {std::mutex mutex;Policy value;std::wstring ini;bool loaded=false;};
inline State& state(){static State value;return value;}
inline std::wstring readSetting(const wchar_t* key,const wchar_t* fallback,const std::wstring& ini){
 wchar_t text[32768]{};GetPrivateProfileStringW(L"GameProcess",key,fallback,text,32768,ini.c_str());return text;
}
inline void load(State& value){
 if(value.loaded)return;
 wchar_t module[32768]{};const DWORD length=GetModuleFileNameW(nullptr,module,32768);
 if(length&&length<32768){value.ini=module;const size_t slash=value.ini.find_last_of(L"\\/");value.ini=slash==std::wstring::npos?L"":value.ini.substr(0,slash+1);}
 value.ini+=L"Toolbox.ini";
 value.value.keywords=readSetting(L"Keywords",L"Yuri Revenge",value.ini);
 const auto stored=readSetting(L"ProgramCount",L"0",value.ini);const unsigned long count=std::wcstoul(stored.c_str(),nullptr,10);
 for(unsigned i=0;i<std::min<unsigned long>(count,64);++i){
  const std::wstring key=L"Program"+std::to_wstring(i+1),program=trim(readSetting(key.c_str(),L"",value.ini));
  if(program.empty())continue;
  bool duplicate=false;for(const auto& existing:value.value.rememberedPrograms)if(sameProgram(existing,program))duplicate=true;
  if(!duplicate)value.value.rememberedPrograms.push_back(program);
 }
 value.loaded=true;
}
inline Policy current(){auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);return value.value;}
inline uint64_t revision(){auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);return value.value.revision;}
inline bool preferredAlive(const Policy& policy){
 if(!policy.selectedPid||!policy.selectedCreation)return false;
 HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,policy.selectedPid);
 if(!process)return false;
 FILETIME created{},exited{},kernel{},user{};
 const bool alive=WaitForSingleObject(process,0)==WAIT_TIMEOUT&&GetProcessTimes(process,&created,&exited,&kernel,&user);
 CloseHandle(process);
 return alive&&((uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime)==policy.selectedCreation;
}
inline bool publish(State& state,Policy next,DWORD* error,bool persist){
 if(error)*error=0;
 if(persist){
  std::wstring section=L"Keywords="+next.keywords;section.push_back(0);
  section+=L"ProgramCount="+std::to_wstring(next.rememberedPrograms.size());section.push_back(0);
  for(size_t i=0;i<next.rememberedPrograms.size();++i){section+=L"Program"+std::to_wstring(i+1)+L"="+next.rememberedPrograms[i];section.push_back(0);}
  section.push_back(0);
  if(section.size()>60000){if(error)*error=ERROR_BAD_LENGTH;return false;}
  if(!WritePrivateProfileSectionW(L"GameProcess",section.c_str(),state.ini.c_str())){if(error)*error=actualError(ERROR_WRITE_FAULT);return false;}
 }
 next.revision=state.value.revision+1;if(!next.revision)next.revision=1;state.value=std::move(next);return true;
}
inline bool saveKeywords(const std::wstring& text,DWORD* error=nullptr){
 if(error)*error=0;if(text.size()>512||text.find_first_of(L"\r\n")!=std::wstring::npos||text.find(wchar_t(0))!=std::wstring::npos){if(error)*error=ERROR_INVALID_PARAMETER;return false;}
 auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);auto next=value.value;next.keywords=trim(text);
 if(next.keywords==value.value.keywords)return true;return publish(value,std::move(next),error,true);
}
inline bool normalizedProgram(const std::wstring& text,std::wstring& result){
 result=trim(text);if(result.size()>=2&&result.front()==L'"'&&result.back()==L'"')result=trim(result.substr(1,result.size()-2));
 if(result.empty()||result.size()>32760||result.find_first_of(L"*?\r\n\"")!=std::wstring::npos||result.find(wchar_t(0))!=std::wstring::npos)return false;
 if(result.find_first_of(L"\\/")!=std::wstring::npos){
  const bool drive=result.size()>=3&&std::iswalpha(result[0])&&result[1]==L':'&&(result[2]==L'\\'||result[2]==L'/');
  const bool unc=result.size()>2&&(result[0]==L'\\'||result[0]==L'/')&&(result[1]==L'\\'||result[1]==L'/');
  if((!drive&&!unc)||basename(result).empty())return false;
 }else if(result==L"."||result==L".."||result.find(L':')!=std::wstring::npos)return false;
 return true;
}
inline bool appendProgram(Policy& policy,const std::wstring& program,DWORD* error){
 for(const auto& item:policy.rememberedPrograms)if(sameProgram(item,program))return true;
 if(policy.rememberedPrograms.size()>=64){if(error)*error=ERROR_TOO_MANY_NAMES;return false;}
 policy.rememberedPrograms.push_back(program);return true;
}
inline bool addProgram(const std::wstring& text,DWORD* error=nullptr){
 if(error)*error=0;std::wstring program;if(!normalizedProgram(text,program)){if(error)*error=ERROR_INVALID_PARAMETER;return false;}
 auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);auto next=value.value;
 if(!appendProgram(next,program,error))return false;
 if(next.rememberedPrograms==value.value.rememberedPrograms)return true;return publish(value,std::move(next),error,true);
}
inline bool removeProgram(size_t index,DWORD* error=nullptr){
 if(error)*error=0;auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);auto next=value.value;
 if(index>=next.rememberedPrograms.size()){if(error)*error=ERROR_INVALID_PARAMETER;return false;}
 next.rememberedPrograms.erase(next.rememberedPrograms.begin()+index);return publish(value,std::move(next),error,true);
}
inline bool rememberProcess(const ProcessInfo& candidate,DWORD* error=nullptr){
 if(error)*error=0;
 if(!candidate.pid||candidate.pid==GetCurrentProcessId()||!candidate.creation){if(error)*error=ERROR_INVALID_PARAMETER;return false;}
 const auto live=inspectIdentity(candidate.pid);
 if(live.identityError){if(error)*error=live.identityError;return false;}
 if(live.creation!=candidate.creation||live.path.empty()||(!candidate.path.empty()&&!sameProgram(live.path,candidate.path))){if(error)*error=ERROR_NOT_FOUND;return false;}
 auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);auto next=value.value;
 if(!appendProgram(next,live.path,error))return false;
 next.selectedPid=live.pid;next.selectedCreation=live.creation;
 return publish(value,std::move(next),error,true);
}
inline bool clearPreferredProcess(DWORD* error=nullptr){
 if(error)*error=0;auto& value=state();std::lock_guard<std::mutex> lock(value.mutex);load(value);auto next=value.value;
 if(!next.selectedPid&&!next.selectedCreation)return true;
 next.selectedPid=0;next.selectedCreation=0;return publish(value,std::move(next),error,false);
}
inline bool setManualTarget(const std::wstring& text,DWORD* error=nullptr){return addProgram(text,error);}
inline bool selectProcess(const ProcessInfo& value,DWORD* error=nullptr){return rememberProcess(value,error);}
inline bool useAutomatic(DWORD* error=nullptr){return clearPreferredProcess(error);}
} }

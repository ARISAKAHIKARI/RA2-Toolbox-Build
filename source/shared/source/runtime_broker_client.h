#pragma once
#include <windows.h>
#include <stdint.h>
#include <string>
#include <mutex>
#include "runtime_broker_protocol.h"
namespace runtimebroker {
// Only the x64 host includes this client. The helper owns the x86 HHOOKs; all
// game mailboxes, cadence and controller identity remain in the real host.
class Client {
 std::recursive_mutex mutex_;HANDLE process_=nullptr,owner_=nullptr,mapping_=nullptr,request_=nullptr,response_=nullptr;
 Packet*packet_=nullptr;uint32_t generation_=0;bool poisoned_=false,starting_=false,stopping_=false,shutdownRequested_=false;
 DWORD startupAt_=0,shutdownAt_=0;
 // Exactly one request owns the private mailbox until its ACK or retirement.
 // The host's intent identifies a retained Target, independently of PID reuse.
 struct Exchange {bool active=false,complete=false,cancelled=false;Operation operation=Describe;DWORD pid=0,thread=0;uint32_t window=0,token=0,generation=0;uint64_t intent=0;LONG sequence=0;DWORD at=0,error=0;uint32_t resultToken=0;} exchange_;
 static bool pending(){SetLastError(ERROR_IO_PENDING);return false;}
 bool owns(DWORD pid,uint64_t intent)const{return exchange_.active&&exchange_.pid==pid&&exchange_.intent==intent;}
 // Retain the exact CreateProcess handle and its mailbox until exit is proven.
 // No PID lookup, mailbox reuse, or replacement helper is allowed while retiring.
 static constexpr DWORD RetirementInterval=1000;
 DWORD retirementAt_=0,retirementError_=ERROR_BUSY,retirementCause_=ERROR_PROCESS_ABORTED;
 bool retirementAttempted_=false,retirementRunning_=false;
 static DWORD waitError(DWORD wait,DWORD fallback){if(wait!=WAIT_FAILED)return fallback;const DWORD error=GetLastError();return error?error:ERROR_GEN_FAILURE;}
 void close(){
  if(packet_)UnmapViewOfFile(packet_);packet_=nullptr;
  for(HANDLE h:{process_,owner_,mapping_,request_,response_})if(h)CloseHandle(h);
  process_=owner_=mapping_=request_=response_=nullptr;poisoned_=starting_=stopping_=shutdownRequested_=false;startupAt_=shutdownAt_=0;exchange_={};
  retirementAt_=0;retirementError_=ERROR_BUSY;retirementCause_=ERROR_PROCESS_ABORTED;
  retirementAttempted_=retirementRunning_=false;
 }
 bool retire(){
  // Reentrant close/ensure must not close a handle used by TerminateProcess.
  if(retirementRunning_){SetLastError(ERROR_BUSY);return false;}
  if(!process_){close();SetLastError(ERROR_SUCCESS);return true;}
  DWORD wait=WaitForSingleObject(process_,0);
  if(wait==WAIT_OBJECT_0){close();SetLastError(ERROR_SUCCESS);return true;}
  if(wait!=WAIT_TIMEOUT){retirementError_=waitError(wait,ERROR_GEN_FAILURE);SetLastError(retirementError_);return false;}
  const DWORD now=GetTickCount();
  if(retirementAttempted_&&DWORD(now-retirementAt_)<RetirementInterval){SetLastError(retirementError_);return false;}
  retirementRunning_=true;
  const BOOL requested=TerminateProcess(process_,retirementCause_);
  const DWORD error=requested?ERROR_SUCCESS:waitError(WAIT_FAILED,ERROR_GEN_FAILURE);
  // Start the quiet interval after the system call completes, including failures.
  retirementAt_=GetTickCount();retirementAttempted_=true;retirementRunning_=false;
  wait=WaitForSingleObject(process_,0);
  if(wait==WAIT_OBJECT_0){close();SetLastError(ERROR_SUCCESS);return true;}
  retirementError_=wait==WAIT_TIMEOUT?(error?error:ERROR_BUSY):waitError(wait,ERROR_GEN_FAILURE);
  SetLastError(retirementError_);return false;
 }
 bool abort(DWORD error){
  if(!poisoned_){poisoned_=true;retirementCause_=error?error:ERROR_PROCESS_ABORTED;retirementError_=ERROR_BUSY;retirementAttempted_=false;}
  retire();SetLastError(error);return false;
 }
 bool pollStartup(){
  HANDLE waits[]={process_,response_};const DWORD wait=WaitForMultipleObjects(2,waits,FALSE,0);
  // Consume an already-ready ACK first; the deadline expires only while
  // still waiting. A delayed host pulse must not discard an earlier ACK.
  if(wait==WAIT_TIMEOUT&&DWORD(GetTickCount()-startupAt_)<3000)return pending();
  if(wait!=WAIT_OBJECT_0+1||InterlockedCompareExchange((volatile LONG*)&packet_->state,0,0)!=1){
   const DWORD error=wait==WAIT_FAILED?waitError(wait,ERROR_GEN_FAILURE):packet_->error?packet_->error:wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_BAD_EXE_FORMAT;return abort(error);
  }
  starting_=false;if(!++generation_)++generation_;SetLastError(ERROR_SUCCESS);return true;
 }
 bool poll(){
  if(exchange_.complete)return true;
  HANDLE waits[]={process_,response_};const DWORD wait=WaitForMultipleObjects(2,waits,FALSE,0);
  if(wait==WAIT_TIMEOUT&&DWORD(GetTickCount()-exchange_.at)<2000)return pending();
  if(wait!=WAIT_OBJECT_0+1&&!(exchange_.operation==Shutdown&&wait==WAIT_OBJECT_0&&InterlockedCompareExchange((volatile LONG*)&packet_->response,0,0)==exchange_.sequence))
   return abort(waitError(wait,wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_BROKEN_PIPE));
  if(InterlockedCompareExchange((volatile LONG*)&packet_->response,0,0)!=exchange_.sequence)return abort(ERROR_INVALID_DATA);
  exchange_.error=packet_->error;exchange_.resultToken=packet_->token;exchange_.complete=true;return true;
 }
 bool begin(Operation operation,DWORD pid=0,DWORD thread=0,uint32_t window=0,uint32_t token=0,uint64_t intent=0){
  // Callers must finish/cancel the previous owner before writing any field.
  if(exchange_.active)return pending();
  exchange_={};exchange_.active=true;exchange_.operation=operation;exchange_.pid=pid;exchange_.thread=thread;exchange_.window=window;
  exchange_.token=token;exchange_.generation=generation_;exchange_.intent=intent;exchange_.at=GetTickCount();
  packet_->operation=operation;packet_->pid=pid;packet_->thread=thread;packet_->window=window;packet_->token=token;packet_->error=0;
  if(!ResetEvent(response_))return abort(GetLastError());
  exchange_.sequence=InterlockedIncrement((volatile LONG*)&packet_->request);
  if(!SetEvent(request_))return abort(GetLastError());return poll();
 }
 bool cancel(){
  // A cancelled Install still owns its mapping. Drain the late ACK, remove
  // its exact token, and only then let the host release/reuse that Target.
  if(exchange_.operation==Install){
   exchange_.cancelled=true;if(!poll())return false;
   if(exchange_.error)return abort(exchange_.error);
   if(!exchange_.resultToken)return abort(ERROR_INVALID_DATA);
   const DWORD pid=exchange_.pid;const uint32_t token=exchange_.resultToken;const uint64_t intent=exchange_.intent;exchange_={};
   if(!begin(Remove,pid,0,0,token,intent))return false;
  }
  if(!poll())return false;
  if(exchange_.error)return abort(exchange_.error);
  exchange_={};SetLastError(ERROR_SUCCESS);return true;
 }
 bool finishShutdown(){
  const DWORD wait=WaitForSingleObject(process_,0);
  if(wait==WAIT_OBJECT_0){close();SetLastError(ERROR_SUCCESS);return true;}
  if(wait==WAIT_TIMEOUT&&DWORD(GetTickCount()-shutdownAt_)<1000)return pending();
  return abort(waitError(wait,ERROR_TIMEOUT));
 }

public:
 bool ensure(){
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if(process_){
   if(poisoned_){if(!retire())return false;}
   else {
    const DWORD wait=WaitForSingleObject(process_,0);
    if(wait==WAIT_TIMEOUT){
     if(shutdownRequested_)return pending();
     if(starting_)return pollStartup();
     if(stopping_){if(!finishShutdown())return false;return pending();}
     SetLastError(ERROR_SUCCESS);return true;
    }
    if(wait!=WAIT_OBJECT_0){SetLastError(waitError(wait,ERROR_GEN_FAILURE));return false;}
    close();
   }
  }
  wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768){SetLastError(ERROR_BAD_PATHNAME);return false;}
  while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')--n;
  const wchar_t name[]=L"GameRuntimeHost32.exe";if(!n||n+sizeof(name)/sizeof(*name)>32768){SetLastError(ERROR_BAD_PATHNAME);return false;}
  for(unsigned i=0;i<sizeof(name)/sizeof(*name);++i)path[n+i]=name[i];
  SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};
  if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&owner_,SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,TRUE,0))return false;
  mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,sizeof(Packet),nullptr);
  if(mapping_)packet_=(Packet*)MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Packet));
  request_=CreateEventW(&security,FALSE,FALSE,nullptr);response_=CreateEventW(&security,FALSE,FALSE,nullptr);
  if(!packet_||!request_||!response_){DWORD error=GetLastError();close();SetLastError(error);return false;}
  *packet_=Packet{};packet_->owner=GetCurrentProcessId();
  HANDLE inherited[]={owner_,mapping_,request_,response_};std::wstring command=L"\""+std::wstring(path)+L"\"";
  for(HANDLE handle:inherited){uintptr_t value=uintptr_t(handle);if(value>0xffffffffu){close();SetLastError(ERROR_INVALID_HANDLE);return false;}command+=L" "+std::to_wstring(value);}
  SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
  auto*attributes=(LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(),0,bytes);
  if(!attributes){close();SetLastError(ERROR_NOT_ENOUGH_MEMORY);return false;}
  bool initialized=InitializeProcThreadAttributeList(attributes,1,0,&bytes)!=FALSE;
  bool selected=initialized&&UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr)!=FALSE;
  STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof startup;startup.lpAttributeList=attributes;PROCESS_INFORMATION child{};
  bool created=selected&&CreateProcessW(path,&command[0],nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,nullptr,&startup.StartupInfo,&child)!=FALSE;
  DWORD error=GetLastError();if(initialized)DeleteProcThreadAttributeList(attributes);HeapFree(GetProcessHeap(),0,attributes);
  for(HANDLE handle:inherited)SetHandleInformation(handle,HANDLE_FLAG_INHERIT,0);
  if(!created){close();SetLastError(error);return false;}
  process_=child.hProcess;CloseHandle(child.hThread);
  starting_=true;startupAt_=GetTickCount();return pollStartup();
 }
 bool alive(uint32_t generation){std::lock_guard<std::recursive_mutex> lock(mutex_);return process_&&!poisoned_&&!starting_&&!stopping_&&!shutdownRequested_&&generation==generation_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
 bool install(DWORD pid,DWORD thread,uint32_t window,uint32_t&token,uint32_t&generation,uint64_t intent=0){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!ensure())return false;
  if(exchange_.active){
   if(!owns(pid,intent)){if(!poll())return false;return pending();}
   if(exchange_.operation!=Install||exchange_.cancelled||exchange_.thread!=thread||exchange_.window!=window){if(!cancel())return false;}
  }
  if(!exchange_.active){if(!begin(Install,pid,thread,window,0,intent))return false;}
  else if(!poll())return false;
  // An error can include a partially installed hook pair. Retire the exact
  // helper before permitting its target mapping to disappear.
  if(exchange_.error)return abort(exchange_.error);
  if(!exchange_.resultToken)return abort(ERROR_INVALID_DATA);
  token=exchange_.resultToken;generation=exchange_.generation;exchange_={};SetLastError(ERROR_SUCCESS);return true;
 }
 bool remove(DWORD pid,uint32_t token,uint32_t generation,uint64_t intent=0){
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if(!process_)return true;
  if(poisoned_)return retire();
  const DWORD wait=WaitForSingleObject(process_,0);
  if(wait==WAIT_OBJECT_0){close();return true;}
  if(wait!=WAIT_TIMEOUT){SetLastError(waitError(wait,ERROR_GEN_FAILURE));return false;}
  if(owns(pid,intent)&&(intent||generation==generation_||(!token&&!generation)))return cancel();
  if(!token||generation!=generation_)return true;
  if(exchange_.active){if(!poll())return false;return pending();}
  if(starting_||stopping_)return pending();
  if(!begin(Remove,pid,0,0,token,intent))return false;
  return cancel();
 }

 bool identity(uint32_t&version,uint32_t&bytes,const wchar_t*prefix){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!ensure())return false;
  version=packet_->assistVersion;bytes=packet_->assistBytes;
  for(unsigned i=0;i<32;++i){if(packet_->mappingPrefix[i]!=uint16_t(prefix[i])){SetLastError(ERROR_REVISION_MISMATCH);return false;}if(!prefix[i])return true;}
  SetLastError(ERROR_REVISION_MISMATCH);return false;
 }
 bool transport(uint32_t version,uint32_t bytes,uint32_t entryVersion,uint32_t entryBytes){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!ensure())return false;
  bool valid=packet_->runtimeVersion==version&&packet_->runtimeBytes==bytes&&packet_->entryVersion==entryVersion&&packet_->entryBytes==entryBytes;
  if(!valid)SetLastError(ERROR_REVISION_MISMATCH);return valid;
 }
 uint32_t hookRva(){std::lock_guard<std::recursive_mutex> lock(mutex_);return !poisoned_&&packet_?packet_->hookRva:0;}
 uint32_t moduleBase(){std::lock_guard<std::recursive_mutex> lock(mutex_);return !poisoned_&&packet_?packet_->runtimeBase:0;}
 DWORD processId(){std::lock_guard<std::recursive_mutex> lock(mutex_);return process_?GetProcessId(process_):0;}
 bool shutdown(){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!process_)return true;
  if(poisoned_)return retire();
  const DWORD wait=WaitForSingleObject(process_,0);
  if(wait==WAIT_OBJECT_0){close();return true;}
  if(wait!=WAIT_TIMEOUT){SetLastError(waitError(wait,ERROR_GEN_FAILURE));return false;}
  // Shutdown supersedes all target intents. No late install completion can
  // escape to a caller once this request has claimed the mailbox.
  shutdownRequested_=true;
  if(starting_)return abort(ERROR_OPERATION_ABORTED);
  if(stopping_)return finishShutdown();
  if(exchange_.active){
   exchange_.cancelled=true;if(!poll())return false;
   if(exchange_.error)return abort(exchange_.error);
   if(exchange_.operation==Shutdown){exchange_={};stopping_=true;shutdownAt_=GetTickCount();return finishShutdown();}
   exchange_={};
  }
  if(!begin(Shutdown))return false;
  if(exchange_.error)return abort(exchange_.error);
  exchange_={};stopping_=true;shutdownAt_=GetTickCount();return finishShutdown();
 }

};
inline Client&client(){static Client*value=new Client;return *value;}
}

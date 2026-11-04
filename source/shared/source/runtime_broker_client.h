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
 Packet*packet_=nullptr;uint32_t generation_=0;bool poisoned_=false;
 void close(){
  if(packet_)UnmapViewOfFile(packet_);packet_=nullptr;
  for(HANDLE h:{process_,owner_,mapping_,request_,response_})if(h)CloseHandle(h);
  process_=owner_=mapping_=request_=response_=nullptr;poisoned_=false;
 }
 bool abort(DWORD error){
  poisoned_=true;
  if(process_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT){
   TerminateProcess(process_,error?error:ERROR_PROCESS_ABORTED);
   if(WaitForSingleObject(process_,1000)!=WAIT_OBJECT_0){SetLastError(error);return false;}
  }
  close();SetLastError(error);return false;
 }
 bool exchange(Operation operation,DWORD pid=0,DWORD thread=0,uint32_t window=0,uint32_t token=0){
  if(!process_||poisoned_||WaitForSingleObject(process_,0)!=WAIT_TIMEOUT){SetLastError(ERROR_BROKEN_PIPE);return false;}
  packet_->operation=operation;packet_->pid=pid;packet_->thread=thread;packet_->window=window;packet_->token=token;packet_->error=0;
  ResetEvent(response_);LONG sequence=InterlockedIncrement((volatile LONG*)&packet_->request);
  if(!SetEvent(request_))return abort(GetLastError());
  HANDLE waits[]={process_,response_};DWORD wait=WaitForMultipleObjects(2,waits,FALSE,2000);
  if(wait!=WAIT_OBJECT_0+1&&!(operation==Shutdown&&wait==WAIT_OBJECT_0&&InterlockedCompareExchange((volatile LONG*)&packet_->response,0,0)==sequence))
   return abort(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_BROKEN_PIPE);
  if(InterlockedCompareExchange((volatile LONG*)&packet_->response,0,0)!=sequence)return abort(ERROR_INVALID_DATA);
  if(packet_->error){SetLastError(packet_->error);return false;}return true;
 }
public:
 bool ensure(){
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if(process_){
   if(WaitForSingleObject(process_,0)==WAIT_TIMEOUT){if(!poisoned_)return true;SetLastError(ERROR_BUSY);return false;}
   close();
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
  HANDLE waits[]={process_,response_};DWORD wait=WaitForMultipleObjects(2,waits,FALSE,3000);
  if(wait!=WAIT_OBJECT_0+1||InterlockedCompareExchange((volatile LONG*)&packet_->state,0,0)!=1){
   error=packet_->error?packet_->error:wait==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_BAD_EXE_FORMAT;return abort(error);
  }
  if(!++generation_)++generation_;return true;
 }
 bool alive(uint32_t generation){std::lock_guard<std::recursive_mutex> lock(mutex_);return process_&&!poisoned_&&generation==generation_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT;}
 bool install(DWORD pid,DWORD thread,uint32_t window,uint32_t&token,uint32_t&generation){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!ensure()||!exchange(Install,pid,thread,window))return false;
  if(!packet_->token)return abort(ERROR_INVALID_DATA);token=packet_->token;generation=generation_;return true;
 }
 bool remove(DWORD pid,uint32_t token,uint32_t generation){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!alive(generation))return true;
  return exchange(Remove,pid,0,0,token);
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
 uint32_t hookRva(){std::lock_guard<std::recursive_mutex> lock(mutex_);return packet_?packet_->hookRva:0;}
 uint32_t moduleBase(){std::lock_guard<std::recursive_mutex> lock(mutex_);return packet_?packet_->runtimeBase:0;}
 DWORD processId(){std::lock_guard<std::recursive_mutex> lock(mutex_);return process_?GetProcessId(process_):0;}
 bool shutdown(){
  std::lock_guard<std::recursive_mutex> lock(mutex_);if(!process_)return true;
  if(!poisoned_&&WaitForSingleObject(process_,0)==WAIT_TIMEOUT&&!exchange(Shutdown))return false;
  if(process_&&WaitForSingleObject(process_,1000)!=WAIT_OBJECT_0)return abort(ERROR_TIMEOUT);
  close();return true;
 }
};
inline Client&client(){static Client*value=new Client;return *value;}
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <wchar.h>
#include <cstring>
#include "../../shared/source/runtime_broker_protocol.h"
#include "../../shared/source/runtime_protocol.h"
#include "../../shared/source/game_entry_api.h"
#include "../../assist/source/protocol.h"

static_assert(sizeof(void*)==4,"The game hook broker must be x86");
namespace {
struct Target {DWORD pid=0,thread=0;HWND window=nullptr;HANDLE process=nullptr;HHOOK message=nullptr,post=nullptr;uint32_t token=0;};
Target targets[8];uint32_t serial=0;HMODULE module=nullptr;HOOKPROC messageProc=nullptr,postProc=nullptr;
bool remove(Target&t){
 bool ok=true;if(t.post){if(UnhookWindowsHookEx(t.post)||GetLastError()==ERROR_INVALID_HOOK_HANDLE)t.post=nullptr;else ok=false;}
 if(t.message){if(UnhookWindowsHookEx(t.message)||GetLastError()==ERROR_INVALID_HOOK_HANDLE)t.message=nullptr;else ok=false;}
 if(!ok)return false;if(t.process)CloseHandle(t.process);t={};return true;
}
void cleanup(){for(auto&t:targets)remove(t);}
bool arguments(const wchar_t*text,HANDLE(&handles)[4]){
 for(unsigned n=0;n<4;++n){while(*text==L' ')++text;if(*text<L'0'||*text>L'9')return false;
  uint64_t value=0;while(*text>=L'0'&&*text<=L'9'){value=value*10+(*text++-L'0');if(value>0xffffffffu)return false;}
  if(!value||value==0xffffffffu||(*text&&*text!=L' '))return false;handles[n]=(HANDLE)(uintptr_t)value;
 }while(*text==L' ')++text;return !*text;
}
bool describe(runtimebroker::Packet&p){
 wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768){SetLastError(ERROR_BAD_PATHNAME);return false;}
 while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')--n;
 const wchar_t name[]=L"GameRuntime.dll";if(!n||n+sizeof(name)/sizeof(*name)>32768){SetLastError(ERROR_BAD_PATHNAME);return false;}
 memcpy(path+n,name,sizeof name);module=LoadLibraryW(path);if(!module)return false;
 const auto*rt=(const uint32_t*)GetProcAddress(module,"GameRuntimeInfo");
 const auto*entry=(const uint32_t*)GetProcAddress(module,"GameEntryManagerInfo");
 const auto*policy=(const uint32_t*)GetProcAddress(module,"GameCodeWritePolicyInfo");
 const auto*assistInfo=(const uint32_t*)GetProcAddress(module,"AssistProtocolInfo");
 const auto*prefix=(const wchar_t*)GetProcAddress(module,"AssistMappingPrefix");
 messageProc=(HOOKPROC)GetProcAddress(module,"GameRuntimeHook");postProc=(HOOKPROC)GetProcAddress(module,"GameRuntimePostHook");
 if(!gameentry::strictCodeWritePolicy(policy)||!rt||!entry||!assistInfo||!prefix||!messageProc||!postProc||rt[0]!=runtime::Version||rt[1]!=sizeof(runtime::Control)||
    entry[0]!=gameentry::Version||entry[1]!=sizeof(gameentry::Request)||assistInfo[0]!=assist::Version||assistInfo[1]!=sizeof(assist::Shared)||wcscmp(prefix,assist::MappingPrefix.text)){
  SetLastError(ERROR_REVISION_MISMATCH);return false;
 }
 p.runtimeVersion=rt[0];p.runtimeBytes=rt[1];p.entryVersion=entry[0];p.entryBytes=entry[1];
 p.assistVersion=assistInfo[0];p.assistBytes=assistInfo[1];p.runtimeBase=uint32_t(uintptr_t(module));p.hookRva=uint32_t(uintptr_t(messageProc))-p.runtimeBase;
 for(unsigned i=0;i<32;++i){p.mappingPrefix[i]=uint16_t(prefix[i]);if(!prefix[i])break;}
 return true;
}
bool x86Process(HANDLE process){
 using Check=BOOL(WINAPI*)(HANDLE,USHORT*,USHORT*);
 auto check=(Check)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"IsWow64Process2");
 if(check){USHORT machine=0,native=0;return check(process,&machine,&native)&&
   (machine==IMAGE_FILE_MACHINE_I386||(machine==IMAGE_FILE_MACHINE_UNKNOWN&&native==IMAGE_FILE_MACHINE_I386));}
 BOOL wow64=FALSE;if(!IsWow64Process(process,&wow64))return false;SYSTEM_INFO system{};GetNativeSystemInfo(&system);
 return wow64||system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL;
}
bool install(runtimebroker::Packet&p){
 if(!p.pid||!p.thread||!p.window||p.pid==p.owner){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 HWND window=(HWND)(uintptr_t)p.window;DWORD owner=0;
 if(GetWindowThreadProcessId(window,&owner)!=p.thread||owner!=p.pid){SetLastError(ERROR_INVALID_PARAMETER);return false;}
 HANDLE process=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.pid);if(!process)return false;
 if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT||!x86Process(process)){CloseHandle(process);SetLastError(ERROR_BAD_EXE_FORMAT);return false;}
 wchar_t name[96];runtime::mapName(name,p.pid,p.owner);HANDLE mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,name);
 auto*c=mapping?(const runtime::Control*)MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(runtime::Control)):nullptr;
 bool valid=c&&c->magic==runtime::Magic&&c->version==runtime::Version&&c->pid==p.pid&&c->owner==p.owner;
 if(c)UnmapViewOfFile(c);if(mapping)CloseHandle(mapping);if(!valid){CloseHandle(process);SetLastError(ERROR_REVISION_MISMATCH);return false;}
 Target*slot=nullptr;
 for(auto&t:targets)if(t.pid==p.pid){
  if(t.thread==p.thread&&t.window==window&&t.process&&WaitForSingleObject(t.process,0)==WAIT_TIMEOUT&&t.message&&t.post){CloseHandle(process);p.token=t.token;return true;}
  if(t.process&&WaitForSingleObject(t.process,0)==WAIT_TIMEOUT&&t.thread!=p.thread){
   HANDLE previous=OpenThread(SYNCHRONIZE,FALSE,t.thread);
   bool exited=previous?WaitForSingleObject(previous,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;
   if(previous)CloseHandle(previous);if(!exited){CloseHandle(process);SetLastError(ERROR_BUSY);return false;}
  }
  if(!remove(t)){DWORD error=GetLastError();CloseHandle(process);SetLastError(error);return false;}slot=&t;break;
 }
 if(!slot)for(auto&t:targets)if(!t.pid){slot=&t;break;}
 if(!slot){CloseHandle(process);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return false;}
 slot->pid=p.pid;slot->thread=p.thread;slot->window=window;slot->process=process;
 slot->message=SetWindowsHookExW(WH_GETMESSAGE,messageProc,module,p.thread);
 if(slot->message)slot->post=SetWindowsHookExW(WH_CALLWNDPROCRET,postProc,module,p.thread);
 if(!slot->message||!slot->post){DWORD error=GetLastError();remove(*slot);SetLastError(error);return false;}
 if(!++serial)++serial;slot->token=serial;p.token=serial;return true;
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR command,int){
 HANDLE handles[4]{};if(!arguments(command,handles))return 2;
 HANDLE owner=handles[0],mapping=handles[1],request=handles[2],response=handles[3];
 auto*p=(runtimebroker::Packet*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(runtimebroker::Packet));
 if(!p)return 3;
 if(p->magic!=runtimebroker::Magic||p->version!=runtimebroker::Version||p->bytes!=sizeof(*p)||!p->owner||
    GetProcessId(owner)!=p->owner||WaitForSingleObject(owner,0)!=WAIT_TIMEOUT){UnmapViewOfFile(p);return 4;}
 if(!describe(*p)){p->error=GetLastError();InterlockedExchange((volatile LONG*)&p->state,-1);SetEvent(response);UnmapViewOfFile(p);return 5;}
 InterlockedExchange((volatile LONG*)&p->state,1);SetEvent(response);
 HANDLE waits[]={owner,request};bool stop=false;
 while(!stop){
  DWORD wait=WaitForMultipleObjects(2,waits,FALSE,250);
  if(wait==WAIT_OBJECT_0||wait==WAIT_FAILED)break;
  for(auto&t:targets)if(t.process&&WaitForSingleObject(t.process,0)!=WAIT_TIMEOUT)remove(t);
  if(wait!=WAIT_OBJECT_0+1)continue;
  LONG sequence=InterlockedCompareExchange((volatile LONG*)&p->request,0,0);if(sequence==p->response)continue;
  p->error=0;SetLastError(ERROR_SUCCESS);bool ok=false;
  if(p->magic!=runtimebroker::Magic||p->version!=runtimebroker::Version||p->bytes!=sizeof(*p)||p->owner!=GetProcessId(owner)){SetLastError(ERROR_REVISION_MISMATCH);}
  else if(p->operation==runtimebroker::Describe)ok=true;
  else if(p->operation==runtimebroker::Install)ok=install(*p);
  else if(p->operation==runtimebroker::Remove){
   // Target-thread exit may have already removed this exact lease. Removal
   // is idempotent and never authorizes a different target's hook pair.
   if(!p->token||!p->pid)SetLastError(ERROR_INVALID_PARAMETER);
   else {ok=true;for(auto&t:targets)if(t.token==p->token&&t.pid==p->pid){ok=remove(t);break;}}
  }else if(p->operation==runtimebroker::Shutdown){ok=true;stop=true;}
  else SetLastError(ERROR_INVALID_PARAMETER);
  if(!ok)p->error=GetLastError()?GetLastError():ERROR_GEN_FAILURE;
  InterlockedExchange((volatile LONG*)&p->response,sequence);SetEvent(response);
 }
 cleanup();UnmapViewOfFile(p);for(HANDLE handle:handles)CloseHandle(handle);return 0;
}

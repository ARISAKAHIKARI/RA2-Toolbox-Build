#pragma once
// Deterministic Win32 boundary for the actual broker Client. It never opens,
// terminates, or communicates with an OS process. HANDLE values name fixture
// objects, so target/owner identity and unsafe closes can be audited precisely.
#include <cstdint>
#include <cstddef>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <functional>
#include "../../shared/source/runtime_broker_protocol.h"
using DWORD=uint32_t;using UINT=unsigned;using LONG=int32_t;using BOOL=int;using BOOLEAN=unsigned char;
using HANDLE=void*;using HWND=void*;using HHOOK=void*;using SIZE_T=size_t;
#define CALLBACK
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,WAIT_FAILED=0xffffffffu;
constexpr DWORD ERROR_SUCCESS=0,ERROR_INVALID_HANDLE=6,ERROR_NOT_ENOUGH_MEMORY=8,ERROR_INVALID_DATA=13,ERROR_GEN_FAILURE=31,ERROR_BAD_EXE_FORMAT=193,ERROR_BAD_PATHNAME=161,ERROR_PROCESS_ABORTED=1067,ERROR_BROKEN_PIPE=109,ERROR_BUSY=170,ERROR_TIMEOUT=1460,ERROR_ACCESS_DENIED=5,ERROR_REVISION_MISMATCH=1306;
constexpr DWORD SYNCHRONIZE=0x100000,PROCESS_QUERY_LIMITED_INFORMATION=0x1000,PAGE_READWRITE=4,FILE_MAP_ALL_ACCESS=31,HANDLE_FLAG_INHERIT=1,PROC_THREAD_ATTRIBUTE_HANDLE_LIST=0x20002,EXTENDED_STARTUPINFO_PRESENT=0x80000,CREATE_NO_WINDOW=0x8000000;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
struct SECURITY_ATTRIBUTES{DWORD nLength;void*lpSecurityDescriptor;BOOL bInheritHandle;};
struct STARTUPINFOW{DWORD cb;};struct Attr{std::array<HANDLE,4>inherited{};};using LPPROC_THREAD_ATTRIBUTE_LIST=Attr*;
struct STARTUPINFOEXW{STARTUPINFOW StartupInfo;Attr*lpAttributeList;};
struct PROCESS_INFORMATION{HANDLE hProcess=nullptr,hThread=nullptr;DWORD dwProcessId=0,dwThreadId=0;};
struct LARGE_INTEGER{int64_t QuadPart=0;};
namespace brokerfixture {
enum class Kind{Owner,Mapping,Event,Broker,Thread,Game};
enum class Reply{Ack,Timeout,Failed,Exited,BadSequence,RemoteError};
struct Object{
 Kind kind=Kind::Event;DWORD pid=0;bool closed=false,exited=false,waitFailed=false;DWORD waitError=ERROR_ACCESS_DENIED;
 bool scheduled=false;DWORD exitAt=0;uint32_t tokens=0;
 std::unique_ptr<runtimebroker::Packet>packet;std::array<HANDLE,4>inherited{};
};
struct Wait{HANDLE handle;DWORD milliseconds;};
struct Termination{HANDLE handle;DWORD pid,cause,started,completed;};
struct Exchange{HANDLE process;uint32_t operation,pid,thread,window,token,sequence,owner;};
inline DWORD now=1000,error=0,hostPID=700,nextPID=9000,nextHandle=0x100;
inline std::map<uintptr_t,Object>objects;
inline std::vector<Wait>singleWaits;inline std::vector<DWORD>multipleWaits;
inline std::vector<Termination>terminations;inline std::vector<Exchange>exchanges;
inline std::vector<HANDLE>brokers,closed;inline std::vector<std::array<HANDLE,4>>inheritance;
inline unsigned creates=0,overlappingCreates=0,liveBrokerCloses=0,wrongTerminates=0,ownerMismatches=0,unmaps=0;
inline bool terminateOK=true,exitOnTerminate=true,exitDespiteFailure=false,eventOK=true;
inline DWORD terminateDelay=0,terminateError=ERROR_ACCESS_DENIED,startupDelay=0,exchangeDelay=0,shutdownDelay=0;
inline bool shutdownNeverExits=false;
inline Reply startup=Reply::Ack,reply=Reply::Ack;
inline std::function<void()>onTerminate;
inline HANDLE activeBroker=nullptr;
inline uintptr_t id(HANDLE h){return reinterpret_cast<uintptr_t>(h);}
inline HANDLE make(Kind kind,DWORD pid=0){HANDLE h=reinterpret_cast<HANDLE>(uintptr_t(nextHandle++));auto&o=objects[id(h)];o.kind=kind;o.pid=pid;return h;}
inline Object&object(HANDLE h){return objects.at(id(h));}
inline bool due(DWORD target){return int32_t(now-target)>=0;}
inline runtimebroker::Packet*packet(HANDLE process){auto&m=object(object(process).inherited[1]);return m.packet.get();}
inline bool allOpen(HANDLE process){for(HANDLE h:object(process).inherited)if(object(h).closed)return false;return !object(process).closed;}
inline void describe(runtimebroker::Packet&p){p.state=1;p.runtimeVersion=4;p.runtimeBytes=176;p.entryVersion=5;p.entryBytes=64;p.assistVersion=100;p.assistBytes=23162400;p.runtimeBase=0x65000000;p.hookRva=0x1234;const wchar_t*prefix=L"Fixture.";for(unsigned i=0;i<9;++i)p.mappingPrefix[i]=uint16_t(prefix[i]);}
inline void reset(){objects.clear();singleWaits.clear();multipleWaits.clear();terminations.clear();exchanges.clear();brokers.clear();closed.clear();inheritance.clear();now=1000;error=0;hostPID=700;nextPID=9000;nextHandle=0x100;creates=overlappingCreates=liveBrokerCloses=wrongTerminates=ownerMismatches=unmaps=0;terminateOK=exitOnTerminate=eventOK=true;exitDespiteFailure=false;terminateDelay=startupDelay=exchangeDelay=shutdownDelay=0;terminateError=ERROR_ACCESS_DENIED;shutdownNeverExits=false;startup=reply=Reply::Ack;onTerminate={};activeBroker=nullptr;}
}
inline DWORD GetTickCount(){return brokerfixture::now;}inline DWORD GetLastError(){return brokerfixture::error;}inline void SetLastError(DWORD e){brokerfixture::error=e;}
inline HANDLE GetCurrentProcess(){return reinterpret_cast<HANDLE>(uintptr_t(0xfffffff1));}inline DWORD GetCurrentProcessId(){return brokerfixture::hostPID;}
inline DWORD GetProcessId(HANDLE h){auto it=brokerfixture::objects.find(brokerfixture::id(h));if(it==brokerfixture::objects.end()||it->second.closed){SetLastError(ERROR_INVALID_HANDLE);return 0;}return it->second.pid;}
inline DWORD WaitForSingleObject(HANDLE h,DWORD milliseconds){using namespace brokerfixture;singleWaits.push_back({h,milliseconds});auto it=objects.find(id(h));if(it==objects.end()||it->second.closed){SetLastError(ERROR_INVALID_HANDLE);return WAIT_FAILED;}auto&o=it->second;if(o.waitFailed){SetLastError(o.waitError);return WAIT_FAILED;}
 if(o.scheduled&&due(o.exitAt))o.exited=true;if(o.exited)return WAIT_OBJECT_0;
 if(milliseconds&&o.scheduled&&DWORD(o.exitAt-now)<=milliseconds){now=o.exitAt;o.exited=true;return WAIT_OBJECT_0;}
 now+=milliseconds;return WAIT_TIMEOUT;}
inline BOOL TerminateProcess(HANDLE h,UINT cause){using namespace brokerfixture;auto&o=object(h);if(o.kind!=Kind::Broker||o.closed)++wrongTerminates;const DWORD started=now;
 if(onTerminate){auto callback=std::move(onTerminate);callback();}
 now+=terminateDelay;if((terminateOK&&exitOnTerminate)||exitDespiteFailure)o.exited=true;
 terminations.push_back({h,o.pid,cause,started,now});if(!terminateOK)SetLastError(terminateError);return terminateOK;}
inline BOOL CloseHandle(HANDLE h){using namespace brokerfixture;auto it=objects.find(id(h));if(it==objects.end()||it->second.closed){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}auto&o=it->second;if(o.kind==Kind::Broker&&!o.exited)++liveBrokerCloses;o.closed=true;closed.push_back(h);return TRUE;}
inline BOOL UnmapViewOfFile(const void*){++brokerfixture::unmaps;return TRUE;}
inline BOOL DuplicateHandle(HANDLE from,HANDLE source,HANDLE into,HANDLE*out,DWORD,BOOL,DWORD){using namespace brokerfixture;if(from!=GetCurrentProcess()||source!=GetCurrentProcess()||into!=GetCurrentProcess())++ownerMismatches;*out=make(Kind::Owner,hostPID);return TRUE;}
inline HANDLE CreateFileMappingW(HANDLE,SECURITY_ATTRIBUTES*,DWORD,DWORD,DWORD,const wchar_t*){using namespace brokerfixture;auto h=make(Kind::Mapping);object(h).packet=std::make_unique<runtimebroker::Packet>();return h;}
inline void*MapViewOfFile(HANDLE h,DWORD,DWORD,DWORD,SIZE_T){return brokerfixture::object(h).packet.get();}
inline HANDLE CreateEventW(SECURITY_ATTRIBUTES*,BOOL,BOOL,const wchar_t*){return brokerfixture::make(brokerfixture::Kind::Event);}
inline BOOL ResetEvent(HANDLE){return TRUE;}
inline BOOL SetEvent(HANDLE h){using namespace brokerfixture;for(HANDLE p:brokers){auto&o=object(p);if(o.closed||o.inherited[2]!=h)continue;auto*data=packet(p);exchanges.push_back({p,data->operation,data->pid,data->thread,data->window,data->token,uint32_t(data->request),data->owner});}
 if(!eventOK){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return TRUE;}
inline LONG InterlockedIncrement(volatile LONG*p){return ++*p;}inline LONG InterlockedCompareExchange(volatile LONG*p,LONG next,LONG expected){LONG old=*p;if(old==expected)*p=next;return old;}inline LONG InterlockedExchange(volatile LONG*p,LONG next){LONG old=*p;*p=next;return old;}
inline DWORD GetModuleFileNameW(HANDLE,wchar_t*out,DWORD count){const wchar_t*path=L"C:\\Toolbox\\RA2Toolbox.exe";std::wcsncpy(out,path,count);return DWORD(std::wcslen(path));}
inline BOOL InitializeProcThreadAttributeList(Attr*p,DWORD,DWORD,SIZE_T*bytes){*bytes=sizeof(Attr);return p?TRUE:FALSE;}
inline HANDLE GetProcessHeap(){return reinterpret_cast<HANDLE>(uintptr_t(1));}inline void*HeapAlloc(HANDLE,DWORD,SIZE_T n){return std::calloc(1,n);}inline BOOL HeapFree(HANDLE,DWORD,void*p){std::free(p);return TRUE;}
inline BOOL UpdateProcThreadAttribute(Attr*p,DWORD,DWORD,const void*handles,SIZE_T,void*,void*){std::memcpy(p->inherited.data(),handles,sizeof(HANDLE)*4);return TRUE;}inline void DeleteProcThreadAttributeList(Attr*){}
inline BOOL SetHandleInformation(HANDLE,DWORD,DWORD){return TRUE;}
inline BOOL CreateProcessW(const wchar_t*path,wchar_t*,void*,void*,BOOL,DWORD,void*,void*,STARTUPINFOW*startup,PROCESS_INFORMATION*out){using namespace brokerfixture;
 if(std::wstring(path)!=L"C:\\Toolbox\\GameRuntimeHost32.exe")++ownerMismatches;
 for(HANDLE h:brokers)if(!object(h).exited)++overlappingCreates;
 auto*extended=reinterpret_cast<STARTUPINFOEXW*>(startup);HANDLE h=make(Kind::Broker,nextPID++);auto&o=object(h);o.inherited=extended->lpAttributeList->inherited;
 if(GetProcessId(o.inherited[0])!=hostPID||packet(h)->owner!=hostPID)++ownerMismatches;
 out->hProcess=h;out->hThread=make(Kind::Thread);out->dwProcessId=o.pid;out->dwThreadId=o.pid+1;activeBroker=h;brokers.push_back(h);inheritance.push_back(o.inherited);++creates;return TRUE;}
inline DWORD WaitForMultipleObjects(DWORD,const HANDLE*handles,BOOL,DWORD milliseconds){using namespace brokerfixture;multipleWaits.push_back(milliseconds);auto&o=object(handles[0]);auto*p=packet(handles[0]);bool starting=milliseconds==3000;Reply mode=starting?startup:reply;now+=starting?startupDelay:exchangeDelay;
 if(mode==Reply::Timeout){now+=milliseconds;return WAIT_TIMEOUT;}if(mode==Reply::Failed){SetLastError(ERROR_ACCESS_DENIED);return WAIT_FAILED;}if(mode==Reply::Exited){o.exited=true;return WAIT_OBJECT_0;}
 if(starting){describe(*p);if(mode==Reply::RemoteError){p->state=-1;p->error=ERROR_REVISION_MISMATCH;}return WAIT_OBJECT_0+1;}
 p->response=mode==Reply::BadSequence?p->request-1:p->request;p->error=mode==Reply::RemoteError?ERROR_ACCESS_DENIED:ERROR_SUCCESS;
 if(p->operation==runtimebroker::Install&&mode==Reply::Ack)p->token=++o.tokens;
 if(p->operation==runtimebroker::Shutdown&&mode==Reply::Ack&&!shutdownNeverExits){o.scheduled=true;o.exitAt=now+shutdownDelay;if(!shutdownDelay)o.exited=true;}
 return WAIT_OBJECT_0+1;}
inline bool QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=uint64_t(brokerfixture::now)*1000;return true;}

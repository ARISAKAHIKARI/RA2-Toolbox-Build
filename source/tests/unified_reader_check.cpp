// Tests production read sessions against a thread-safe Win32 fixture.
#include "../external_reader/source/shared_reader.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>
#include <type_traits>
static_assert(!std::is_copy_constructible<external::ReadSession>::value);
static_assert(!std::is_copy_assignable<external::ReadSession>::value);
struct Process {std::atomic<bool> alive{true};bool readable=true;};
struct Handle {std::shared_ptr<Process> process;bool readable;};
static std::mutex fixture;
static std::unordered_map<DWORD,std::shared_ptr<Process>> processes;
static std::atomic<int> opened{0},closed{0},calls{0};
static thread_local DWORD error=0;
static thread_local unsigned char byteValue=0x11;
static std::mutex gateMutex;
static std::condition_variable gate;
static bool blocking=false,released=false;
static int entered=0;
extern external::ConnectionHub* otherTranslationUnitHub();
extern "C" {
HANDLE OpenProcess(DWORD rights,BOOL,DWORD pid){
 std::lock_guard<std::mutex> lock(fixture);
 auto it=processes.find(pid);
 if(it==processes.end()||!it->second->alive){error=ERROR_INVALID_PARAMETER;return nullptr;}
 if((rights&PROCESS_VM_READ)&&!it->second->readable){error=ERROR_ACCESS_DENIED;return nullptr;}
 ++opened;return new Handle{it->second,bool(rights&PROCESS_VM_READ)};
}
BOOL CloseHandle(HANDLE h){++closed;delete static_cast<Handle*>(h);return TRUE;}
DWORD GetLastError(){return error;}
BOOL GetExitCodeProcess(HANDLE h,DWORD*code){*code=static_cast<Handle*>(h)->process->alive?STILL_ACTIVE:0;return TRUE;}
BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*out,DWORD*n){const wchar_t p[]=L"C:\\YR\\gamemd.exe";if(*n<sizeof(p)/sizeof(wchar_t))return FALSE;std::memcpy(out,p,sizeof(p));*n=sizeof(p)/sizeof(wchar_t)-1;return TRUE;}
BOOL ReadProcessMemory(HANDLE h,LPCVOID,void*out,SIZE_T n,SIZE_T*got){
 ++calls;auto* handle=static_cast<Handle*>(h);*got=0;
 if(!handle->process->alive||!handle->readable){error=ERROR_ACCESS_DENIED;return FALSE;}
 {
  std::unique_lock<std::mutex> lock(gateMutex);
  if(blocking){++entered;gate.notify_all();gate.wait(lock,[]{return released;});}
 }
 // If another session closes our shared handle while blocked, ASan catches it.
 if(!handle->process->alive){error=ERROR_PARTIAL_COPY;return FALSE;}
 std::memset(out,byteValue,n);*got=n;return TRUE;
}
SIZE_T VirtualQueryEx(HANDLE,LPCVOID,MEMORY_BASIC_INFORMATION*,SIZE_T){return 0;}
}
int main(){
 using external::ReadSession;
 assert(otherTranslationUnitHub()==&external::connectionHub());
 processes[42]=std::make_shared<Process>();
 ReadSession a,b,c;
 assert(a.open(42)&&b.open(42)&&c.open(42));assert(opened==1);
 assert(a.generation()==b.generation()&&a.generation()==c.generation());
 unsigned value=0;
 assert(a.read(0x11000,&value,4)&&value==0x11111111);
 byteValue=0x22;assert(b.read(0x11000,&value,4)&&value==0x22222222);
 b.begin();assert(a.read(0x11000,&value,4)&&value==0x11111111);
 assert(a.reads==1&&a.pageHits==1&&b.reads==0);
 assert(a.readFresh(0x11000,&value,4)&&value==0x22222222);
 assert(a.read(0x11000,&value,4)&&value==0x11111111);
 wchar_t path[100];DWORD size=100;assert(a.imagePath(path,&size));
 puts("PASS independent cache/diagnostics, cross-TU shared connection, immediate read bypass");
 // All three native read calls must overlap. The timeout diagnoses accidental
 // serialization; there are no sleep-based throughput assumptions.
 {std::lock_guard<std::mutex> lock(gateMutex);blocking=true;}
 auto read=[](ReadSession& r){unsigned v=0;assert(r.readFresh(0x11000,&v,4));};
 std::thread ta(read,std::ref(a)),tb(read,std::ref(b)),tc(read,std::ref(c));
 bool parallel=false;
 {std::unique_lock<std::mutex> lock(gateMutex);parallel=gate.wait_for(lock,std::chrono::seconds(3),[]{return entered==3;});}
 // Registry and other clients remain usable while native reads are pending.
 ReadSession transient;assert(transient.open(42));transient.close();assert(closed==0);
 {std::lock_guard<std::mutex> lock(gateMutex);released=true;blocking=false;gate.notify_all();}
 ta.join();tb.join();tc.join();assert(parallel);
 puts("PASS three simultaneous RPM calls; connect/detach while other sessions read");
 // Closing two clients must not close the remaining client's connection.
 a.close();b.close();assert(closed==0&&c.readFresh(0x11000,&value,4));
 const auto oldGeneration=c.generation();auto old=processes[42];old->alive=false;
 processes[42]=std::make_shared<Process>();
 assert(a.open(42));assert(a.generation()!=oldGeneration&&!c.alive());
 c.begin();assert(!c.readFresh(0x11000,&value,4));assert(a.readFresh(0x11000,&value,4));
 c.close();assert(closed==1);assert(a.alive());
 assert(a.open(42));assert(a.pageHits==0&&a.reads==0);
 a.close();assert(opened==closed);
 puts("PASS retained handle lifetime; dead process/PID reuse creates new generation");
 processes[43]=std::make_shared<Process>();processes[43]->readable=false;
 assert(!a.open(43)&&a.lastOpenError==ERROR_ACCESS_DENIED);
 assert(b.open(43,true)&&b.valid()&&b.alive()&&!b.canRead());
 int before=calls;assert(!b.readFresh(0x11000,&value,4));assert(calls==before&&b.lastReadError==ERROR_ACCESS_DENIED);
 auto restricted=b.generation();processes[43]->readable=true;
 assert(a.open(43)&&a.canRead()&&a.generation()!=restricted);
 assert(b.open(43,true)&&b.canRead()&&b.generation()==a.generation());
 assert(a.readFresh(0x11000,&value,4));
 before=calls;assert(!a.readFresh(0x11000,nullptr,4));assert(!a.readFresh(0x11000,&value,32*1024*1024+1));assert(calls==before);
 a.close();b.close();assert(opened==closed);
 puts("PASS query-only fallback isolated from readable sessions; upgrade, bounds, all handles released");
}

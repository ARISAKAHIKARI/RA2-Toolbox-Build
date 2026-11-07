// Linux host fake Win32 ABI smoke test: validates our API access mask and
// fallback control flow. This does not replace a Windows x86 compile/test.
#include "../external_reader/source/shared_reader.h"
#include "../external_reader/source/process_discovery.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <atomic>
static bool failSnapshot=false, queryDenied=false, omitWindows=false, iconicWindow=false;
static DWORD maskRead=0,maskQuery=0;
static int regionCalls=0,readHandles=0,closedReaders=0;
static bool pageMode=false,partialRead=false;
static thread_local unsigned char pageValue=0xCD;
static std::atomic<unsigned> rpmCalls{0};
extern "C" {
HANDLE OpenProcess(DWORD mask,BOOL,DWORD pid){
 if(pid!=42&&pid!=43)return nullptr;
 if(mask&PROCESS_VM_READ){++readHandles;maskRead=mask;return reinterpret_cast<HANDLE>(0x20);}
 if(mask&PROCESS_QUERY_INFORMATION){maskQuery=mask;return queryDenied?nullptr:reinterpret_cast<HANDLE>(0x30);}
 return reinterpret_cast<HANDLE>(0x40);
}
BOOL CloseHandle(HANDLE h){if(h==reinterpret_cast<HANDLE>(0x20))++closedReaders;return TRUE;}
DWORD GetLastError(){return ERROR_ACCESS_DENIED;}
BOOL GetExitCodeProcess(HANDLE,DWORD*code){*code=STILL_ACTIVE;return TRUE;}
BOOL ReadProcessMemory(HANDLE,LPCVOID a,void*out,SIZE_T n,SIZE_T*got){
 ++rpmCalls;
 auto addr=reinterpret_cast<uintptr_t>(a);
 *got=0;
 if(partialRead){*got=n/2;std::memset(out,pageValue,*got);return FALSE;}
 if((n==4096&&!pageMode)||addr==0x12000)return FALSE;
 std::memset(out,pageValue,n);*got=n;return TRUE;
}
SIZE_T VirtualQueryEx(HANDLE,LPCVOID,MEMORY_BASIC_INFORMATION*info,SIZE_T n){
 ++regionCalls;info->State=0x1000;info->Protect=4;return n;
}
DWORD GetCurrentProcessId(){return 1;}
BOOL EnumWindows(BOOL(*fn)(HWND,LPARAM),LPARAM p){if(!omitWindows)fn(reinterpret_cast<HWND>(0x100),p);return TRUE;}
BOOL GetClientRect(HWND,RECT*r){*r=iconicWindow?RECT{0,0,0,0}:RECT{0,0,640,480};return TRUE;}
DWORD GetWindowThreadProcessId(HWND w,DWORD*pid){*pid=w==reinterpret_cast<HWND>(0x999)?99:42;return 12;}
BOOL IsWindowVisible(HWND){return FALSE;}
BOOL IsIconic(HWND){return iconicWindow?TRUE:FALSE;}
LONG GetWindowLongW(HWND,int){return 0;}
HWND GetWindow(HWND,unsigned){return nullptr;}
HANDLE CreateToolhelp32Snapshot(DWORD,DWORD){return failSnapshot?INVALID_HANDLE_VALUE:reinterpret_cast<HANDLE>(0x50);}
BOOL Process32FirstW(HANDLE,PROCESSENTRY32W*p){p->th32ProcessID=42;std::wcscpy(p->szExeFile,L"gamemd-spawn.exe");return TRUE;}
BOOL Process32NextW(HANDLE,PROCESSENTRY32W*){return FALSE;}
BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*path,DWORD*n){
 constexpr wchar_t filename[]=L"C:\\YR\\gamemd-spawn.exe";
 std::wcscpy(path,filename);*n=sizeof(filename)/sizeof(wchar_t)-1;return TRUE;
}
}
int main(){
 using namespace external;
 assert(discovery::isGameName(L"GAMEMD.DAT")&&discovery::isGameName(L"gamemd-spawn.dat")&&!discovery::isGameName(L"launcher.exe"));
 auto normal=discovery::scan();assert(normal.best.pid==42&&normal.windowCount==1);
 assert(normal.best.window&&normal.processCount==1);
 auto absent=discovery::scan(99);assert(!absent.best.pid&&!absent.best.window);
 struct WindowReader {uint32_t hwnd=0;bool read(uint32_t address,void*out,size_t bytes){
  assert(address==discovery::YrppGameHwndAddress);assert(bytes==sizeof hwnd);std::memcpy(out,&hwnd,bytes);return true;
 }} windows;
 DWORD resolvedThread=999;
 const auto own=reinterpret_cast<HWND>(0x100),foreign=reinterpret_cast<HWND>(0x999);
 windows.hwnd=0x999;
 assert(!discovery::resolveYrppWindow(windows,42,foreign,&resolvedThread)&&resolvedThread==0);
 assert(discovery::resolveYrppWindow(windows,42,own,&resolvedThread)==own&&resolvedThread==12);
 windows.hwnd=0x100;
 assert(discovery::resolveYrppWindow(windows,42,foreign,&resolvedThread)==own&&resolvedThread==12);
 iconicWindow=true;
 auto minimized=discovery::scan(42);assert(minimized.best.pid==42&&minimized.best.window&&minimized.windowCount==1);
 iconicWindow=false;
 omitWindows=true;
 assert(!discovery::scan(99).best.pid);
 auto passive=discovery::scan();assert(passive.best.pid==42&&!passive.best.window&&passive.uniqueWindowless);
 omitWindows=false;
 failSnapshot=true;
 auto fallback=discovery::scan();assert(fallback.best.pid==42&&fallback.processCount==1);
 ProcessReader reader;assert(reader.open(42));
 assert(maskRead==(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ));
 assert(maskQuery==0); // No extra rights requested before reading
 reader.begin();unsigned value=0;
 assert(reader.read(0x11000,&value,4));assert(maskQuery==0);
 assert(!reader.read(0x12000,&value,4));
 assert(maskQuery==PROCESS_QUERY_INFORMATION&&regionCalls==1);
 assert(reader.lastRegionQueried);
 reader.close();queryDenied=true;maskQuery=0;regionCalls=0;
 assert(reader.open(42));reader.begin();
 assert(!reader.read(0x12000,&value,4));
 assert(!reader.lastRegionQueried&&reader.lastRegionQueryError==ERROR_ACCESS_DENIED);
 assert(reader.read(0x11000,&value,4)); // Query failure cannot block actual read.
 reader.close();
 int opened=readHandles,closed=closedReaders;
 SharedProcessReader units,assist;
 assert(units.open(42)&&assist.open(42));assert(readHandles==opened+1);
 {
  units.begin();
  assert(SharedReadView{&units}(0x11000,&value,4));
  assert(assist.read(0x11000,&value,4));
 }
 units.close();assert(closedReaders==closed); // disabling statistics keeps tactical reading alive
 assert(assist.valid()&&assist.alive()&&assist.read(0x11000,&value,4));
 assist.close();assert(closedReaders==closed+1);
 assert(units.open(42));assert(readHandles==opened+2); // fresh handle after both clients release
 assert(assist.open(43));assert(readHandles==opened+3&&units.pid()==42&&assist.pid()==43);
 assist.close();
 units.close();
 // Successful pages are cached only for one sampling batch. Reads across
 // page boundaries use the exact span; failed/partial reads are never cached.
 assert(reader.open(42));reader.begin();pageMode=true;pageValue=0x11;
 auto before=rpmCalls.load();assert(reader.read(0x11010,&value,4)&&value==0x11111111u);
 pageValue=0x22;assert(reader.read(0x11020,&value,4)&&value==0x11111111u);assert(rpmCalls==before+1);
 reader.begin();assert(reader.read(0x11010,&value,4)&&value==0x22222222u);
 assert(reader.read(0x13ffe,&value,4)&&value==0x22222222u);
 reader.begin();partialRead=true;assert(!reader.read(0x11010,&value,4));assert(reader.errors==1&&reader.lastBytesRead==2);
 partialRead=false;pageValue=0x33;assert(reader.read(0x11010,&value,4)&&value==0x33333333u);
 before=rpmCalls.load();assert(!reader.read(0xffff,&value,4));assert(!reader.read(0x7fffffff,&value,4));assert(!reader.read(0x11000,&value,0));assert(!reader.read(0x11000,&value,4*1024*1024+1));assert(rpmCalls==before);
 reader.close();
 // Real host threads exercise the shared reader implementation. The mock
 // replaces Windows RPM only: this is not a game throughput measurement.
 assert(units.open(42)&&assist.open(42));before=rpmCalls.load();
 auto worker=[](SharedProcessReader& client,unsigned char seed){for(int batch=0;batch<2000;++batch){
  pageValue=static_cast<unsigned char>(seed+batch);client.begin();
  for(int k=0;k<64;++k){unsigned v=0;assert(client.read(0x11000+k*4,&v,4));assert(v==unsigned(pageValue)*0x01010101u);if(k%8==0)std::this_thread::yield();}
  assert(client.pageHits==63&&client.reads==1);
 }};
 std::thread a(worker,std::ref(units),1),b(worker,std::ref(assist),128);a.join();b.join();
 assert(rpmCalls==before+4000);units.close();assist.close();
 puts("PASS read robustness: cache lifetime, cross-page reads, partial failure/recovery, bounds; 2 threads / 4000 batches / 256000 reads / 4000 mocked RPM calls");
}

#define WINMOCK_IMAGE_FILES
#include "../external_reader/source/process_image_identity.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>

static std::mutex gate;static std::condition_variable wake;static bool entered=false,released=false;
extern "C" {
HANDLE OpenProcess(DWORD,BOOL,DWORD p){return (HANDLE)uintptr_t(p);}
BOOL CloseHandle(HANDLE){return TRUE;}
BOOL QueryFullProcessImageNameW(HANDLE h,DWORD,wchar_t*out,DWORD*n){auto name=uintptr_t(h)==1?L"slow.exe":L"cached.exe";size_t count=wcslen(name);if(*n<=count)return FALSE;wcscpy(out,name);*n=DWORD(count);return TRUE;}
BOOL GetProcessTimes(HANDLE h,FILETIME*c,FILETIME*,FILETIME*,FILETIME*){*c={DWORD(uintptr_t(h)),0};return TRUE;}
DWORD GetTickCount(){return 100;}
HANDLE CreateFileW(const wchar_t*path,DWORD,DWORD,void*,DWORD,DWORD,HANDLE){
 if(std::wstring(path)==L"slow.exe"){std::unique_lock<std::mutex> lock(gate);entered=true;wake.notify_all();wake.wait(lock,[]{return released;});}
 return INVALID_HANDLE_VALUE;
}
BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER*){return FALSE;}
BOOL SetFilePointerEx(HANDLE,LARGE_INTEGER,LARGE_INTEGER*,DWORD){return FALSE;}
BOOL ReadFile(HANDLE,void*,DWORD,DWORD*,void*){return FALSE;}
}
int main(){
 using external::imageidentity::processMatches;
 assert(!processMatches(2)); // Populate a negative cached result.
 auto slow=std::async(std::launch::async,[]{return processMatches(1);});
 {std::unique_lock<std::mutex> lock(gate);assert(wake.wait_for(lock,std::chrono::seconds(2),[]{return entered;}));}
 auto cached=std::async(std::launch::async,[]{return processMatches(2);});
 bool independent=cached.wait_for(std::chrono::milliseconds(250))==std::future_status::ready;
 {std::lock_guard<std::mutex> lock(gate);released=true;wake.notify_all();}
 assert(!slow.get()&&!cached.get()&&independent);
 std::cout<<"PASS cached process identification is independent of another slow image read\n";
}

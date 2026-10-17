"""Execute the host logger with fault-injected Win32 file APIs (no Windows OS)."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=r'''
#pragma once
#include <cstdint>
#include <cstddef>
using DWORD=unsigned long;using LONGLONG=long long;using HANDLE=void*;
struct LARGE_INTEGER{LONGLONG QuadPart;};struct SYSTEMTIME{unsigned short wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;};
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
constexpr DWORD FILE_APPEND_DATA=4,FILE_READ_ATTRIBUTES=128,FILE_SHARE_READ=1,OPEN_ALWAYS=4,FILE_ATTRIBUTE_NORMAL=128,INVALID_FILE_ATTRIBUTES=0xffffffff,MOVEFILE_REPLACE_EXISTING=1,CP_UTF8=65001;
DWORD GetLastError();void SetLastError(DWORD);bool CreateDirectoryW(const wchar_t*,void*);
DWORD GetEnvironmentVariableW(const wchar_t*,wchar_t*,DWORD);HANDLE CreateFileW(const wchar_t*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
bool GetFileSizeEx(HANDLE,LARGE_INTEGER*);bool CloseHandle(HANDLE);DWORD GetFileAttributesW(const wchar_t*);bool MoveFileExW(const wchar_t*,const wchar_t*,DWORD);
bool WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);void GetLocalTime(SYSTEMTIME*);DWORD GetCurrentProcessId();DWORD GetCurrentThreadId();
int WideCharToMultiByte(unsigned,DWORD,const wchar_t*,int,char*,int,void*,void*);
'''
body=r'''
#include "automatic_log.h"
#include <cassert>
#include <map>
#include <cwchar>
#include <cstring>
#include <codecvt>
#include <locale>
#include <thread>
static thread_local DWORD error=77;static bool denyPreferred=false,denyAll=false,denyRotate=false,failWrite=false;
static std::map<std::wstring,std::string> files;
DWORD GetLastError(){return error;}void SetLastError(DWORD e){error=e;}
bool CreateDirectoryW(const wchar_t*,void*){error=183;return true;}
DWORD GetEnvironmentVariableW(const wchar_t*,wchar_t*p,DWORD){std::wcscpy(p,L"C:\\Local");return 8;}
HANDLE CreateFileW(const wchar_t*p,DWORD,DWORD share,void*,DWORD,DWORD,HANDLE){assert(share==FILE_SHARE_READ);error=5;if(denyAll||(denyPreferred&&std::wstring(p).find(L"C:\\App")==0))return INVALID_HANDLE_VALUE;files[p];return new std::wstring(p);}
bool GetFileSizeEx(HANDLE h,LARGE_INTEGER*n){n->QuadPart=files[*(std::wstring*)h].size();return true;}
bool CloseHandle(HANDLE h){delete (std::wstring*)h;error=999;return true;}
DWORD GetFileAttributesW(const wchar_t*p){return files.count(p)?128:INVALID_FILE_ATTRIBUTES;}
bool MoveFileExW(const wchar_t*a,const wchar_t*b,DWORD){if(denyRotate)return false;assert(files.count(a));files[b]=files[a];files.erase(a);return true;}
bool WriteFile(HANDLE h,const void*p,DWORD n,DWORD*w,void*){if(failWrite){*w=0;return false;}files[*(std::wstring*)h].append((const char*)p,n);*w=n;return true;}
void GetLocalTime(SYSTEMTIME*t){*t={2026,10,5,16,18,30,12,345};}DWORD GetCurrentProcessId(){return 456;}DWORD GetCurrentThreadId(){return 789;}
int WideCharToMultiByte(unsigned,DWORD,const wchar_t*p,int n,char*out,int cap,void*,void*){std::wstring_convert<std::codecvt_utf8<wchar_t>> c;auto s=c.to_bytes(p,p+n);if(out){assert(cap>=int(s.size()));memcpy(out,s.data(),s.size());}return s.size();}
int main(){
 const std::wstring path=L"C:\\App\\logs\\Runtime.log",fallback=L"C:\\Local\\RA2Toolbox\\logs\\Runtime.log";
 autolog::initialize(L"C:\\App");assert(error==77);assert(autolog::write(L"bridge 回调"));assert(error==77);
 assert(files[path].find("2026-10-16 18:30:12.345")!=std::string::npos&&files[path].find("host=456 tid=789")!=std::string::npos&&files[path].find(u8"回调")!=std::string::npos);
 for(int i=0;i<4;++i){files[path]=std::string(autolog::Limit,'x');assert(autolog::write(L"rotate"));}
 assert(files.size()==3&&files.count(path+L".1")&&files.count(path+L".2")&&files[path].size()<autolog::Limit);
 denyPreferred=true;assert(autolog::write(L"fallback"));assert(autolog::state().active==fallback&&files[fallback].find("fallback")!=std::string::npos&&error==77);
 denyPreferred=false;autolog::state().active=path;files[path]=std::string(autolog::Limit,'x');denyRotate=true;
 assert(autolog::write(L"rotation blocked uses fallback"));assert(files[path].size()==autolog::Limit&&autolog::state().active==fallback);
 denyRotate=false;denyAll=true;assert(!autolog::write(L"no writable path")&&error==77);denyAll=false;
 failWrite=true;assert(!autolog::write(L"write failure")&&error==77);failWrite=false;
 files[fallback].clear();assert(autolog::write(std::wstring(20000,L'A')));assert(files[fallback].size()<4500);
 files[fallback].clear();std::thread workers[4];for(auto&t:workers)t=std::thread([]{for(int i=0;i<50;++i)assert(autolog::write(L"health"));});for(auto&t:workers)t.join();
 int lines=0;for(char c:files[fallback])if(c=='\n')++lines;assert(lines==200&&error==77);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'windows.h').write_text(header);(p/'main.cpp').write_text(body)
 subprocess.run(['g++','-std=c++17','-pthread','-I'+tmp,'-I'+str(root/'shared/source'),str(p/'main.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# The actual critical sink must remain independent of the optional verbose sink.
s=(root/'assist/source/main.cpp').read_text();runtime=(root/'game_runtime/source/runtime.cpp').read_text()
assert 'static void critical(const std::wstring& s){autolog::write(s);diag(s);}' in s
assert 'autolog::initialize(root);autolog::write(L"host-start' in s
assert 'static void tick(){reapBridges();logBridgeHealth();' in s
assert 'automatic_log.h' not in runtime and 'CreateFileW' not in runtime and 'WriteFile' not in runtime
print('PASS automatic logger: UTF-8; 2MiB + 2 backups; unwritable path/rotation fallback; explicit total failure; Win32 error preservation; line limit; 200 concurrent records; critical sink independent of verbose setting')

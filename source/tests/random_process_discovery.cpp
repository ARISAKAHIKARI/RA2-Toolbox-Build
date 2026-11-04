#define WINMOCK_IMAGE_FILES
#include "../external_reader/source/process_discovery.h"
#include <cassert>
#include <fstream>
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>
struct Proc {std::wstring name,path;DWORD creation;bool accessible=true;};
static std::map<DWORD,Proc> procs;
static std::vector<DWORD> listed,windows;static std::map<DWORD,std::vector<DWORD>> children;static DWORD foreground=999;static size_t nextProc,pos;
static std::map<DWORD,DWORD> owners;static std::map<DWORD,unsigned> identityCalls;
static std::vector<unsigned char> image,other;static const std::vector<unsigned char>* opened;
static DWORD ticks=10;static int fileOpens=0;static bool failSnapshot=false;
extern "C" {
DWORD GetCurrentProcessId(){return 999;}DWORD GetLastError(){return 5;}DWORD GetTickCount(){return ticks;}
HANDLE CreateToolhelp32Snapshot(DWORD,DWORD){nextProc=0;return failSnapshot?INVALID_HANDLE_VALUE:(HANDLE)1;}
BOOL Process32FirstW(HANDLE,PROCESSENTRY32W*e){if(listed.empty())return FALSE;e->th32ProcessID=listed[0];std::wcscpy(e->szExeFile,procs[listed[0]].name.c_str());return TRUE;}
BOOL Process32NextW(HANDLE,PROCESSENTRY32W*e){if(++nextProc>=listed.size())return FALSE;e->th32ProcessID=listed[nextProc];std::wcscpy(e->szExeFile,procs[listed[nextProc]].name.c_str());return TRUE;}
BOOL CloseHandle(HANDLE){return TRUE;}
HANDLE OpenProcess(DWORD mask,BOOL,DWORD p){assert(mask==PROCESS_QUERY_LIMITED_INFORMATION);return procs[p].accessible?(HANDLE)uintptr_t(p):nullptr;}
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
}
int main(int argc,char**argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);image.assign(std::istreambuf_iterator<char>(f),{});assert(image.size()==external::imageidentity::FileBytes);
 auto matches=[](const std::vector<unsigned char>&b){return external::imageidentity::matches([&](uint32_t at,void*out,size_t n){if(at+n>b.size())return false;std::memcpy(out,b.data()+at,n);return true;},b.size());};
 assert(matches(image));other=image;other[0x2339b0]^=1;assert(!matches(other));
 for(size_t at:{size_t(0),size_t(0x237e37),size_t(0x3b8a00),size_t(0x42d028)}){auto b=image;b[at]^=1;assert(!matches(b));}
 auto shortFile=image;shortFile.pop_back();assert(!matches(shortFile));
 using namespace external::discovery;
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
 assert(duplicate.best.pid==50&&duplicate.windowCount==2&&identityCalls[50]==1&&fileOpens==reads+1);
 // A negative hex-name identity tried during the snapshot is not repeated
 // for each of its windows in the same scan.
 procs[44].name=L"8ed53a0454bfd.exe";listed={44};children.clear();children[888]={44,441};owners[441]=44;
 const unsigned rejectedChecks=identityCalls[44];assert(!scan().best.pid&&identityCalls[44]==rejectedChecks+1);
 children.clear();foreground=999;
 procs[46]={L"gamemd.exe",L"C:\\YR\\gamemd.exe",6};listed={45,46};windows={45,46};assert(scan().processCount==2);assert(scan(45).best.pid==45);assert(!scan(47).best.pid);
 procs[45].accessible=false;listed={45};windows={45};assert(!scan().best.pid);
 std::cout<<"PASS image fingerprint, corrupt/truncated rejection, changing names/PIDs, unrelated executable, cache/PID reuse, foreground/background/nested child renderers, windowless/fallback discovery, ambiguity and access denial\n";
}


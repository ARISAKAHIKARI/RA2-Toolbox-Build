// Compile with -Isource/tests/winmock. No real process is terminated.
#include <windows.h>
#include <cassert>
#include <cstdio>
#define WINAPI
constexpr DWORD WAIT_TIMEOUT=258;
using Entry=DWORD(*)(void*);
static Entry entry=nullptr;static unsigned spawned=0,closed=0,terminated=0,waited=0;static bool failCreate=false;
static HANDLE self=reinterpret_cast<HANDLE>(uintptr_t(0x1234));
static HANDLE GetCurrentProcess(){return self;}
static DWORD WaitForSingleObject(HANDLE handle,DWORD timeout){assert(handle==self&&timeout==2000);++waited;return WAIT_TIMEOUT;}
static BOOL TerminateProcess(HANDLE handle,unsigned code){assert(handle==self&&code==0);++terminated;return TRUE;}
static HANDLE CreateThread(void*,size_t,Entry call,void*,DWORD,DWORD*){++spawned;entry=call;return failCreate?nullptr:reinterpret_cast<HANDLE>(uintptr_t(0x5678));}
extern "C" BOOL CloseHandle(HANDLE handle){assert(handle!=self);++closed;return TRUE;}
#include "../../shared/source/bounded_shutdown.h"
int main(int argc,char**){
 failCreate=argc>1;boundedshutdown::begin();boundedshutdown::begin();assert(spawned==1);
 if(failCreate)assert(terminated==1&&!closed&&!waited);
 else{assert(closed==1&&!terminated);assert(entry);entry(nullptr);assert(waited==1&&terminated==1);}
 std::puts("PASS one close deadline, 2-second bound, only toolbox self handle, safe thread ownership and creation failure fallback (Win32 mock)");
}

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
// Arm only after the user has accepted closing. This watchdog owns no game
// handles, hooks or worker storage. A stuck restoration/driver/join must not
// keep the toolbox alive, and must not make us unmap code executing in game.
namespace boundedshutdown {
constexpr DWORD GraceMilliseconds=2000;
inline DWORD WINAPI deadline(void*){
 if(WaitForSingleObject(GetCurrentProcess(),GraceMilliseconds)==WAIT_TIMEOUT)
  TerminateProcess(GetCurrentProcess(),0);
 return 0;
}
inline void begin(){
 static std::atomic<bool> armed{false};if(armed.exchange(true))return;
 HANDLE thread=CreateThread(nullptr,0,deadline,nullptr,0,nullptr);
 if(thread)CloseHandle(thread);
 else TerminateProcess(GetCurrentProcess(),0);
}
}

#pragma once
#include <windows.h>
#include <stdint.h>
// Separate test-only transport. The production Assist83 wire ABI is unchanged.
namespace hookdiagnostic {
constexpr uint32_t Magic=0x48444731,Version=1;
struct Channel {uint32_t magic=Magic,version=Version,pid=0,owner=0;volatile DWORD heartbeat=0;volatile LONG camera=0;};
static_assert(sizeof(Channel)==24,"diagnostic x86/x64 value-only wire");
inline void name(wchar_t*out,DWORD pid,DWORD owner){wsprintfW(out,L"Local\\RA2HookDiagnostic_v1_%lu_%lu",pid,owner);}
inline bool permitsCamera(const Channel&c,DWORD pid,DWORD owner,DWORD now){return c.magic==Magic&&c.version==Version&&c.pid==pid&&c.owner==owner&&c.camera==1&&DWORD(now-c.heartbeat)<2000;}
#ifdef RA2_HOOK_DIAGNOSTICS
inline bool camera(DWORD owner){
 static HANDLE mapping=nullptr;static const Channel*channel=nullptr;static DWORD boundOwner=0;
 if(boundOwner!=owner){if(channel)UnmapViewOfFile(channel);if(mapping)CloseHandle(mapping);channel=nullptr;mapping=nullptr;boundOwner=owner;}
 if(!owner)return false;
 if(!channel){wchar_t path[96];name(path,GetCurrentProcessId(),owner);mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,path);if(mapping)channel=(const Channel*)MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(Channel));if(!channel&&mapping){CloseHandle(mapping);mapping=nullptr;}}
 return channel&&permitsCamera(*channel,GetCurrentProcessId(),owner,GetTickCount());
}
#endif
}

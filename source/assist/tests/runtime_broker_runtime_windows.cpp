#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "runtime_broker_fixture.h"
#include "../../shared/source/runtime_protocol.h"
#include "../../shared/source/game_entry_api.h"
#include "../source/protocol.h"
static_assert(sizeof(void*)==4,"Synthetic hook DLL must be x86");
// Metadata is the exact production ABI. This DLL contains no engine calls,
// engine addresses or command execution; it only records tagged fixture IPC.
extern "C" __declspec(dllexport) const uint32_t GameRuntimeInfo[2]={runtime::Version
#ifdef BROKER_FIXTURE_WRONG_VERSION
 +1
#endif
 ,sizeof(runtime::Control)};
extern "C" __declspec(dllexport) const uint32_t GameEntryManagerInfo[2]={gameentry::Version,sizeof(gameentry::Request)};
extern "C" __declspec(dllexport) const uint32_t AssistProtocolInfo[2]={assist::Version,sizeof(assist::Shared)};
extern "C" __declspec(dllexport) const assist::MappingPrefixValue AssistMappingPrefix{};
static void observed(DWORD owner,bool post){
 wchar_t name[96];brokerfixture::name(name,owner);HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
 auto*probe=mapping?(brokerfixture::Probe*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(brokerfixture::Probe)):nullptr;
 if(probe&&probe->owner==owner&&probe->pid==GetCurrentProcessId())InterlockedIncrement(post?&probe->postCalls:&probe->messageCalls);
 if(probe)UnmapViewOfFile(probe);if(mapping)CloseHandle(mapping);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimeHook(int code,WPARAM wp,LPARAM lp){
 if(code>=0&&wp==PM_REMOVE&&lp){auto*msg=(const MSG*)lp;if(msg->message==brokerfixture::Message)observed(DWORD(msg->lParam),false);}
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimePostHook(int code,WPARAM wp,LPARAM lp){
 if(code>=0&&lp){auto*message=(const CWPRETSTRUCT*)lp;if(message->message==brokerfixture::SentMessage)observed(DWORD(message->lParam),true);}
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE;}

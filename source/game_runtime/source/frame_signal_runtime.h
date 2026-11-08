#pragma once
#include "../../shared/source/frame_signal_protocol.h"
#include "../../shared/source/game_entry_client.h"
#include "../../shared/source/multiplayer_patch_gate.h"
#include "../../shared/source/debug_entry_policy.h"
namespace frameobserver {
static HANDLE mapping=nullptr,event=nullptr;static framesignal::Channel*channel=nullptr;static DWORD owner=0;
static bool installed=false,conflict=false;static uint8_t patchBytes[6]{};
// Discovery is optional IPC work. Cleanup below remains immediate on every
// maintenance call; a late diagnostic channel is found within 100 ms.
static DWORD probeOwner=0,probeTick=0;
static const uint8_t original[6]={0x89,0x15,0x84,0xED,0xA8,0};
static uint32_t continuation=framesignal::CommitSite+6;
extern "C" void RuntimeFrameReady();
extern "C" void FrameSignalCommitted(){
 // Never scan objects, allocate, block, dispatch commands or call the engine.
 // The instruction adapter has already committed the native EDX frame value.
 const DWORD error=GetLastError();
 auto*c=channel;if(!c||c->state!=framesignal::Native||!c->enabled||GetCurrentThreadId()!=c->thread||DWORD(GetTickCount()-c->heartbeat)>500){SetLastError(error);return;}
 LARGE_INTEGER q{};QueryPerformanceCounter(&q);
 InterlockedIncrement(&c->seq);c->frame=*(volatile LONG*)(uintptr_t)framesignal::Counter;c->qpc=q.QuadPart;++c->serial;InterlockedIncrement(&c->seq);
 if(event)SetEvent(event);if(c->enabled&framesignal::Compute)RuntimeFrameReady();SetLastError(error);
}
static void __declspec(naked) committedAdapter(){
 __asm {
  mov dword ptr ds:[0xA8ED84], edx
  pushfd
  pushad
  mov ebx, esp
  sub esp, 528
  and esp, -16
  fxsave [esp]
  cld
  call FrameSignalCommitted
  fxrstor [esp]
  mov esp, ebx
  popad
  popfd
  jmp dword ptr [continuation]
 }
}
static void releaseChannel(){if(channel)UnmapViewOfFile(channel);channel=nullptr;if(event)CloseHandle(event);if(mapping)CloseHandle(mapping);event=mapping=nullptr;owner=0;}
static void maintain(DWORD requestedOwner){
 gameentry::scoped(gameentry::Client::Assist,false,[&]{
 const DWORD error=GetLastError();
 static uint32_t checkedPolicy=0;
 const bool changed=checkedPolicy!=debugentries::policyRevision;checkedPolicy=debugentries::policyRevision;
 // A previous signature rejection is retryable after policy changes. This
 // does not enable diagnostic mode or discard an installed/conflicted site.
 if(changed&&channel&&!installed&&!conflict&&channel->state==framesignal::SignatureRejected)channel->state=framesignal::Waiting;
 // A live controller's temporary pause must not remove/reinstall the site.
 // Heartbeat freshness gates callback work and NEW installation separately.
 bool alive=channel&&requestedOwner&&owner==requestedOwner&&channel->enabled;
 // Removing explicit diagnostic selection retires the code patch. A live
 // selected diagnostic still retains ownership across focus/heartbeat pauses.
 const bool deselected=channel&&!(channel->enabled&framesignal::ExplicitDiagnostic);
 if((!alive||deselected)&&installed){
  const bool restored=gameentry::patch(gameentry::Client::Assist,framesignal::CommitSite,patchBytes,original,6);
  // The writer can return an OS failure after changing and recording bytes.
  // Ownership, not the return code, decides whether cleanup still owes work.
  const auto actual=gameentry::inspect(framesignal::CommitSite);
  installed=!restored&&actual.active;conflict=actual.conflict!=0;
  if(channel)channel->state=installed?framesignal::RestoreFailed:framesignal::Waiting;
  // Keep the old channel/owner while its patch remains outstanding. Never
  // silently adopt a replacement controller around a failed restoration.
  if(installed){SetLastError(error);return;}
 }
 if(!alive)releaseChannel();
 if(!requestedOwner)probeOwner=0;
 if(!channel&&requestedOwner){
  const DWORD now=GetTickCount();
  if(probeOwner==requestedOwner&&DWORD(now-probeTick)<100){SetLastError(error);return;}
  probeOwner=requestedOwner;probeTick=now;
  wchar_t path[96];framesignal::name(path,GetCurrentProcessId(),requestedOwner);
  mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,path);if(mapping)channel=(framesignal::Channel*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(framesignal::Channel));
  if(!channel||channel->magic!=framesignal::Magic||channel->version!=framesignal::Version||channel->pid!=GetCurrentProcessId()||channel->owner!=requestedOwner||!channel->enabled||DWORD(GetTickCount()-channel->heartbeat)>500)releaseChannel();
  else{owner=requestedOwner;framesignal::name(path,GetCurrentProcessId(),owner,true);event=OpenEventW(EVENT_MODIFY_STATE,FALSE,path);if(!event)releaseChannel();}
 }
 // World readiness matters only for a NEW, explicitly requested hook.
 // Ordinary messages, installed hooks, and non-diagnostic consumers do not
 // need to read game memory to maintain ownership or discover a channel.
 if(channel&&!installed&&(channel->state==framesignal::Waiting||channel->state==framesignal::MessageFallback)){
  if(!(channel->enabled&framesignal::ExplicitDiagnostic)){channel->state=framesignal::MessageFallback;SetLastError(error);return;}
  if(DWORD(GetTickCount()-channel->heartbeat)>500){SetLastError(error);return;}
  auto read=[](uint32_t at,void*out,size_t n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(const void*)(uintptr_t)at,out,n,&got)&&got==n;};
  const bool playable=multiplayerpatch::mayInstall(read);
  int session=-1;if(playable)read(0xA8B238,&session,4);
  const bool permitted=playable&&framesignal::nativeAllowed(session,true);
  if(playable&&!permitted){channel->state=framesignal::MessageFallback;SetLastError(error);return;}
  // Loading/menu state is not a signature failure. Keep the message-based
  // fallback alive until the local game becomes playable; no peer-sync gate.
  if(!permitted){
   SetLastError(error);return;
  }
  patchBytes[0]=0xE9;const uint32_t delta=uint32_t(uintptr_t(&committedAdapter))-framesignal::CommitSite-5;memcpy(patchBytes+1,&delta,4);patchBytes[5]=0x90;
  const bool ok=!conflict&&gameentry::patch(gameentry::Client::Assist,framesignal::CommitSite,original,patchBytes,6);
  const DWORD installError=ok?0:GetLastError();
  installed=ok||gameentry::inspect(framesignal::CommitSite).active;
  channel->error=installError;channel->thread=GetCurrentThreadId();LARGE_INTEGER f{};QueryPerformanceFrequency(&f);channel->frequency=f.QuadPart;
  MemoryBarrier();InterlockedExchange(&channel->state,ok?framesignal::Native:framesignal::SignatureRejected);
 }
 SetLastError(error);
 });
}
}

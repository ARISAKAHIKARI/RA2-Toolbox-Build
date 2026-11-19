// Real x86/x64 Win32 thread, event, DLL pin and CaptureGraph execution.
// Only the game-address RPM seam is mocked; all worker lifecycle calls are real.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"

static fixture::Memory memory;
static DWORD gameThread=0;
static std::atomic<unsigned>offThreadReads{0},onThreadReads{0};
static std::atomic<bool>advanceMidGraph{false};
static BOOL WINAPI countedRPM(HANDLE,LPCVOID from,LPVOID out,SIZE_T n,SIZE_T*got){
 const bool game=GetCurrentThreadId()==gameThread;
 if(game)++onThreadReads;else ++offThreadReads;
 const uint32_t at=uint32_t(uintptr_t(from));
 const bool ok=memory.read(at,out,n);
 // Model a 60Hz game advancing while the full graph is being copied.
 // The worker must keep the finished graph as a read-only candidate, not
 // restamp it into the next frame's native command authority.
 if(!game&&advanceMidGraph.load(std::memory_order_acquire)&&
    uint64_t(at)<=fixture::ActorA&&uint64_t(at)+n>fixture::ActorA&&
    advanceMidGraph.exchange(false))
  memory.put(0xA8ED84,int(51));
 if(got)*got=ok?n:0;
 return ok?TRUE:FALSE;
}
#define ReadProcessMemory countedRPM
#include "../../shared/source/world_frame_command_worker_win.h"
#undef ReadProcessMemory
static worldframe::CommandCaptureWorker worker;
static worldframe::Snapshot accepted;
static worldframe::CommandRoster acceptedRoster;
static worldframe::Key job;
extern "C" __declspec(dllexport) int AsyncEnabled(){return worker.enabled()?1:0;}
extern "C" __declspec(dllexport) int Start(){
 gameThread=GetCurrentThreadId();fixture::setup(memory);offThreadReads=0;onThreadReads=0;
 job=fixture::key(50);job.tick=GetTickCount();job.serial=0;
 return worker.request(job,worldframe::Base,17,{})?1:0;
}
extern "C" __declspec(dllexport) int StartGlobal(){
 gameThread=GetCurrentThreadId();fixture::setup(memory);offThreadReads=0;onThreadReads=0;
 job=fixture::key(50);job.tick=GetTickCount();job.serial=0;
 return worker.request(job,worldframe::Base,17,{},true)?1:0;
}
extern "C" __declspec(dllexport) int RejectedCount(){return int(worker.rejected());}
extern "C" __declspec(dllexport) int PollWrongGlobal(){
 // The worker may not have completed yet. An *accepted* wrong-mode
 // snapshot is always an error, even if it is otherwise same-frame.
 return worker.take(job,worldframe::Base,17,accepted,&acceptedRoster,false)?-1:0;
}
extern "C" __declspec(dllexport) int PollGlobal(){
 if(!worker.take(job,worldframe::Base,17,accepted,&acceptedRoster,true))return 0;
 if(!accepted.sealed()||!acceptedRoster.current(accepted.key)||
    !acceptedRoster.global||acceptedRoster.globalVehicles.size()!=3||
    onThreadReads.load()!=0||offThreadReads.load()==0)return -1;
 return 1;
}
extern "C" __declspec(dllexport) int StartCrossFrame(){
 gameThread=GetCurrentThreadId();fixture::setup(memory);offThreadReads=0;onThreadReads=0;
 job=fixture::key(50);job.tick=GetTickCount();job.serial=0;
 advanceMidGraph.store(true,std::memory_order_release);
 return worker.request(job,worldframe::Base,19,{})?1:0;
}
extern "C" __declspec(dllexport) int PollCrossFrame(){
 auto next=job;next.frame=51;next.tick=GetTickCount();
 // The candidate must NOT silently pass the native exact-frame gate.
 if(worker.take(next,worldframe::Base,19,accepted,&acceptedRoster))return -1;
 if(!worker.takeObservation(next,worldframe::Base,accepted,acceptedRoster))return 0;
 if(advanceMidGraph.load()||accepted.key.frame!=50||!accepted.sealed()||
    !acceptedRoster.current(accepted.key)||
    acceptedRoster.objects->byId(fixture::ActorA+5)!=fixture::ActorA||
    onThreadReads.load()!=0||offThreadReads.load()==0)return -2;
 return 1;
}
extern "C" __declspec(dllexport) int StartExpired(){
 // An obsolete request must be dropped before doing any CaptureGraph RPM.
 job.tick=GetTickCount()-600u;
 return worker.request(job,worldframe::Base,18,{})?1:0;
}
extern "C" __declspec(dllexport) int WorkerReadCount(){return int(offThreadReads.load());}
extern "C" __declspec(dllexport) int Poll(){
 if(!worker.take(job,worldframe::Base,17,accepted,&acceptedRoster))return 0;
 int hp=0;uint32_t vt=0;
 if(!accepted.sealed()||accepted.overflow()||accepted.pages.size()<10||
    !accepted.read(fixture::ActorA+0x6C,&hp,4)||hp!=100||
    !accepted.read(fixture::ActorA,&vt,4)||vt!=0x7F5C70u||
    !acceptedRoster.current(accepted.key)||
    acceptedRoster.objects->byId(fixture::ActorA+5)!=fixture::ActorA||
    acceptedRoster.objects->contains(fixture::Passenger)||
    onThreadReads.load()!=0||offThreadReads.load()==0)return -1;
 return 1;
}
extern "C" __declspec(dllexport) int NeverAdmitStale(){
 auto wrong=job;++wrong.frame;
 if(worker.take(wrong,worldframe::Base,17,accepted))return -1;
 if(worker.take(job,worldframe::Base,18,accepted))return -1;
 return 1;
}
extern "C" __declspec(dllexport) int RestartAfterStop(){
 // Reproduce a new match after a fully joined worker. The game dispatcher
 // probes deferBusy BEFORE assembling the next graph/request list; it must
 // now allow relaunch rather than remaining permanently "busy/stopping".
 gameThread=GetCurrentThreadId();fixture::setup(memory);
 offThreadReads=0;onThreadReads=0;
 job=fixture::key(50);job.tick=GetTickCount();job.serial=0;
 if(worker.deferBusy(job,worldframe::Base,17))return -1;
 if(!worker.request(job,worldframe::Base,17,{}))return -2;
 return 1;
}
extern "C" __declspec(dllexport) int Stop(){return worker.stop()?1:0;}
extern "C" __declspec(dllexport) int GameReadCount(){return int(onThreadReads.load());}

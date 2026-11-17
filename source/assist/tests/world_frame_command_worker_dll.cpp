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
static BOOL WINAPI countedRPM(HANDLE,LPCVOID from,LPVOID out,SIZE_T n,SIZE_T*got){
 const bool game=GetCurrentThreadId()==gameThread;
 if(game)++onThreadReads;else ++offThreadReads;
 const uint32_t at=uint32_t(uintptr_t(from));
 const bool ok=memory.read(at,out,n);
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
extern "C" __declspec(dllexport) int Stop(){return worker.stop()?1:0;}
extern "C" __declspec(dllexport) int GameReadCount(){return int(onThreadReads.load());}

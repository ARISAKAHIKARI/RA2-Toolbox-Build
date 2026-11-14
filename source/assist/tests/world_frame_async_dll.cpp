#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cassert>
#include <cstring>
#include <cwchar>
static HANDLE gate=nullptr;static DWORD gameThread=0;
static std::atomic<bool>blockNext{true},entered{false};static unsigned threadStarts=0;
static HANDLE WINAPI countedThread(LPSECURITY_ATTRIBUTES security,SIZE_T stack,LPTHREAD_START_ROUTINE start,LPVOID context,DWORD flags,LPDWORD id){
 ++threadStarts;return CreateThread(security,stack,start,context,flags,id);
}
// Delay only the worker's first real payload mapping, using native Win32 for
// every mapping/event/thread. The product Runtime and publisher are unchanged.
static HANDLE WINAPI delayedMapping(HANDLE file,LPSECURITY_ATTRIBUTES security,DWORD protect,DWORD high,DWORD low,LPCWSTR name){
 if(name&&wcsncmp(name,L"Local\\RA2WorldData_",19)==0&&GetCurrentThreadId()!=gameThread&&blockNext.exchange(false)){
  entered.store(true,std::memory_order_release);if(WaitForSingleObject(gate,INFINITE)!=WAIT_OBJECT_0)return nullptr;
 }
 return CreateFileMappingW(file,security,protect,high,low,name);
}
#define CreateFileMappingW delayedMapping
#define CreateThread countedThread
#include "../../shared/source/world_frame_async_win.h"
#undef CreateFileMappingW
#undef CreateThread
#define WORLD_FRAME_FIXTURE_ONLY
#define REAL_WINDOWS_RPM
#include "world_frame_core.cpp"
#include "../source/world_lifecycle.h"
namespace assist {
struct FixtureShared {volatile LONG worldSampleSeq=0;uint32_t owner=0,worldSampleFailures=0,worldSamplePages=0,worldSampleReads=0,worldSampleBytes=0,worldSampleRosters=0,worldSampleSerial=0;int worldSampleFrame=-1;};
static FixtureShared shared;static FixtureShared*sh=&shared;static uint32_t match=7;
static struct {int frame=50;}snap;
// No WORLD_FRAME_PLATFORM_FIXTURE: execute the actual async Runtime path.
#include "../source/world_frame_runtime.h"
}
static std::shared_ptr<worldframe::Client>client;
static fixture::Memory memory;
static bool capture(){
 using namespace assist;client->subscribe(1,worldframe::Base);
 assert(worldFrameServer.wanted(GetCurrentProcessId(),GetTickCount()));
 const uint32_t serial=worldFrameSerial+1;memory.put(0xA8ED84,int(serial));memory.put(fixture::ActorA+0x6C,int(serial));snap.frame=int(serial);
 fixture::Read live{&memory};return captureWorldFrame(live,worldlife::capture(live),fixture::House,GetTickCount(),worldframe::Base);
}
extern "C" __declspec(dllexport) int Idle(){
 using namespace assist;gameThread=GetCurrentThreadId();shared.owner=GetCurrentProcessId();fixture::setup(memory);
 memory.beforeRead=[](uint32_t,size_t){assert(GetCurrentThreadId()==gameThread);};
 assert(!worldFrameServer.wanted(shared.owner,GetTickCount()));assert(threadStarts==0);
 fixture::Read live{&memory};snap.frame=50;
 assert(captureWorldFrame(live,worldlife::capture(live),fixture::House,GetTickCount(),worldframe::Base));
 assert(threadStarts==0);
 // Isolate this no-subscriber scenario from the next fresh connection: the
 // normal failed-open backoff is 100 ms and is not a worker failure.
 worldFrameServer.disconnect();return 1;
}
extern "C" __declspec(dllexport) int Start(HANDLE delayGate){
 using namespace assist;gate=delayGate;gameThread=GetCurrentThreadId();blockNext=true;entered=false;ResetEvent(gate);
 retireWorldFrame();worldFrameSerial=0;shared.owner=GetCurrentProcessId();fixture::setup(memory);
 memory.beforeRead=[](uint32_t,size_t){assert(GetCurrentThreadId()==gameThread);};
 client=worldframe::acquireClient(GetCurrentProcessId(),721);return capture()?1:0;
}
extern "C" __declspec(dllexport) int Entered(){return entered.load(std::memory_order_acquire)?1:0;}
extern "C" __declspec(dllexport) int Advance(){for(int i=0;i<1000;++i)if(!capture())return 0;return int(assist::worldFrameSerial);}
extern "C" __declspec(dllexport) int ReadLatest(){
 auto token=client->acquire(7);if(!token)return 0;int frame=-1,hp=-1;
 if(!token->read(0xA8ED84,&frame,4)||!token->read(fixture::ActorA+0x6C,&hp,4)||frame!=int(token->key.serial)||hp!=frame)return -1;
 return int(token->key.serial);
}
extern "C" __declspec(dllexport) int Resume(){
 using namespace assist;worldFrameServer.retire();client->subscribe(1,worldframe::Base);
 fixture::Read live{&memory};const auto before=worldlife::capture(live);const auto calls=memory.calls.size();const auto key=worldFrame.key;
 assert(captureWorldFrame(live,before,fixture::House,GetTickCount(),worldframe::Base));
 assert(memory.calls.size()==calls&&!std::memcmp(&worldFrame.key,&key,sizeof key));return 1;
}
extern "C" __declspec(dllexport) int Stop(){assist::retireWorldFrame();return assist::worldFramePublication.stop()?1:0;}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cassert>
#include <cstring>
#include "../../shared/source/world_frame_async_win.h"
static HANDLE gate=nullptr;static std::atomic<bool>blockNext{true},entered{false};
struct SlowWriter:worldframe::Server {
 explicit SlowWriter(Role role):Server(role){}
 template<class Frame,class Current>bool publish(const Frame&frame,Current current){
  if(blockNext.exchange(false)){entered.store(true,std::memory_order_release);if(WaitForSingleObject(gate,INFINITE)!=WAIT_OBJECT_0)return false;}
  return Server::publish(frame,current);
 }
};
static worldframe::Snapshot frame;
static std::shared_ptr<worldframe::Client>client;
static worldframe::Server control;
static worldframe::AsyncPublisher<SlowWriter>publisher;
static uint32_t serial=0,generation=70;
static bool capture(){
 if(!publisher.begin(frame))return false;
 worldframe::Key key{};key.pid=key.owner=GetCurrentProcessId();key.match=7;key.serial=++serial;key.frame=int(serial);key.tick=GetTickCount();key.generationLow=generation;key.connectionLow=721;key.scenario=0x10000;key.house=0x20000;key.cells=0x30000;key.cellCount=1;
 frame.reset(key);const auto read=[&](uint32_t,void*out,size_t n){memset(out,int(serial&255),n);return true;};
 for(unsigned i=0;i<8;++i)if(!frame.prefetch(read,0x10000+i*4096,4096))return false;
 if(!frame.seal())return false;control.observe(frame.key);return publisher.publish(frame);
}
extern "C" __declspec(dllexport) int Start(HANDLE delayGate){
 gate=delayGate;blockNext=true;entered=false;serial=0;++generation;ResetEvent(gate);
 client=worldframe::acquireClient(GetCurrentProcessId(),721);client->subscribe(1,worldframe::Base);
 if(!control.wanted(GetCurrentProcessId(),GetTickCount()))return 0;
 return capture()?1:0;
}
extern "C" __declspec(dllexport) int Entered(){return entered.load(std::memory_order_acquire)?1:0;}
extern "C" __declspec(dllexport) int Advance(){for(int i=0;i<1000;++i)if(!capture())return 0;return int(serial);}
extern "C" __declspec(dllexport) int ReadLatest(){
 auto token=client->acquire(7);if(!token)return 0;
 for(size_t i=0;i<token->pages.size();++i)for(auto byte:token->pages[i].bytes)if(byte!=uint8_t(token->key.serial))return -1;
 return int(token->key.serial);
}
extern "C" __declspec(dllexport) int Stop(){publisher.invalidate();control.retire();return publisher.stop()?1:0;}

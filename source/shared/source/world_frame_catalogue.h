#pragma once
#include "world_frame_capture.h"
#include "../../assist/source/world_lifecycle.h"
#include <atomic>
#include <mutex>

namespace worldframe {
// Local mailbox for the type catalogue alone. The dispatcher exchanges an
// immutable table observation, without acquiring a tactical roster or IPC pages.
class CatalogueMailbox {
 std::mutex gate_;
 std::atomic<uint32_t> epoch_{1};
 Key requested_{};
 uint32_t requestEpoch_=0,resultEpoch_=0;
 bool queued_=false,ready_=false;
 Snapshot staging_,completed_;
 CaptureScratch scratch_;

 template<class Read>static bool observe(Read read,const Key&key,int&frame){
  const auto world=assist::worldlife::capture(read);uint32_t house=0;
  frame=world.frame;
  return world.valid()&&world.running&&world.scenario==key.scenario&&world.cells==key.cells&&
   uint32_t(world.count)==key.cellCount&&world.session==key.session&&frame>=key.frame&&
   read(0xA83D4C,&house,4)&&house==key.house;
 }
 template<class Read>static bool tablesStable(Read read,const Snapshot&source){
  for(uint32_t table:external::catalogue::TypeTables){
   std::array<uint32_t,4> captured{},current{};
   if(!source.read(table+4,captured.data(),sizeof captured)||!read(table+4,current.data(),sizeof current)||captured!=current)return false;
   const uint32_t items=captured[0],count=captured[3];
   for(uint32_t first=0;first<count;first+=64){
    std::array<uint32_t,64> earlier{},latest{};
    const auto bytes=std::min(64u,count-first)*sizeof(uint32_t);
    if(!source.read(items+first*4,earlier.data(),bytes)||!read(items+first*4,latest.data(),bytes)||earlier!=latest)return false;
   }
  }
  return true;
 }
public:
 void invalidate(){epoch_.fetch_add(1,std::memory_order_acq_rel);}
 bool request(const Key&key){
  std::unique_lock<std::mutex> lock(gate_,std::try_to_lock);if(!lock.owns_lock())return false;
  requested_=key;requestEpoch_=epoch_.load(std::memory_order_acquire);queued_=true;return true;
 }
 bool take(const Key&current,Snapshot&out){
  std::unique_lock<std::mutex> lock(gate_,std::try_to_lock);if(!lock.owns_lock()||!ready_)return false;
  ready_=false;
  if(resultEpoch_!=epoch_.load(std::memory_order_acquire)||!current.sameWorld(completed_.key)||
     current.frame<completed_.key.frame||uint32_t(current.tick-completed_.key.tick)>500)return false;
  completed_.swap(out);return true;
 }
 template<class Read,class Clock>void work(Read live,Clock now){
  Key key;uint32_t epoch;
  {
   std::lock_guard<std::mutex> lock(gate_);if(!queued_)return;
   queued_=false;key=requested_;epoch=requestEpoch_;
  }
  if(epoch!=epoch_.load(std::memory_order_acquire)||uint32_t(now()-key.tick)>500)return;
  uint32_t calls=0;const uint32_t started=now();
  auto read=[&](uint32_t at,void*out,size_t size){
   if(++calls>8192||uint32_t(now()-started)>80||epoch!=epoch_.load(std::memory_order_acquire))return false;
   return live(at,out,size);
  };
  int before=-1,after=-1;if(!observe(read,key,before))return;
  key.frame=before;key.tick=started;staging_.reset(key);
  CaptureGraph<decltype(read)> capture(staging_,read,scratch_);
  if(!capture.captureCatalogue()||!staging_.seal()||!tablesStable(read,staging_)||!observe(read,key,after))return;
  stampTypeCatalogue(staging_,true);if(!staging_.catalogue.complete)return;
  std::lock_guard<std::mutex> lock(gate_);
  if(epoch!=epoch_.load(std::memory_order_acquire))return;
  staging_.swap(completed_);resultEpoch_=epoch;ready_=true;
 }
};
}

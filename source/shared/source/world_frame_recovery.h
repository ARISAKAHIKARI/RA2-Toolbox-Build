#pragma once
#include "world_frame.h"
#include <atomic>
#include <mutex>

// Async pre-read is a *hint*, never an authoritative world observation.
// The worker may see a different moment from the sealed command snapshot;
// only a subsequent normal CaptureGraph read can publish these fields.
namespace worldframe {
constexpr size_t RecoverySpans=16;
constexpr size_t RecoveryBytes=2048;
struct RecoveryJob {
 Key key{};
 uint32_t epoch=0;
 uint32_t count=0;
 std::array<RequestedSpan,RecoverySpans> fields{};
};
struct RecoveryResult {
 Key key{};
 uint32_t epoch=0,count=0,attempted=0,readable=0;
 std::array<RequestedSpan,RecoverySpans> fields{};
 std::array<uint8_t,RecoverySpans> success{};
};
// A game-thread producer NEVER waits for the background reader: its mutex is
// try-lock-only on submit/poll. Worker reads outside this tiny mailbox lock.
// Invalidating changes an atomic epoch and never joins an in-flight read.
class RecoveryMailbox {
 mutable std::mutex gate_;
 std::atomic<uint32_t> epoch_{1};
 RecoveryJob waiting_{};
 RecoveryResult result_{};
 bool queued_=false,hasResult_=false;
public:
 RecoveryMailbox()=default;
 RecoveryMailbox(const RecoveryMailbox&)=delete;
 RecoveryMailbox&operator=(const RecoveryMailbox&)=delete;
 void invalidate() noexcept{epoch_.fetch_add(1,std::memory_order_acq_rel);}
 uint32_t epoch()const noexcept{return epoch_.load(std::memory_order_acquire);}
 bool submit(const Key&key,const RequestedSpan*fields,size_t count){
  if(!fields||!count)return false;
  RecoveryJob next{};next.key=key;next.epoch=epoch();
  size_t bytes=0;
  for(size_t i=0;i<count&&next.count<RecoverySpans;++i){
   const auto&f=fields[i];
   if(!f.bytes||f.bytes>256||!range(f.address,f.bytes)||
      uint32_t(key.tick-f.tick)>1000||bytes+f.bytes>RecoveryBytes)continue;
   bool duplicate=false;
   for(size_t j=0;j<next.count;++j)
    if(next.fields[j].address==f.address&&next.fields[j].bytes==f.bytes){duplicate=true;break;}
   if(duplicate)continue;
   next.fields[next.count++]=f;bytes+=f.bytes;
  }
  if(!next.count)return false;
  std::unique_lock<std::mutex>guard(gate_,std::try_to_lock);
  if(!guard.owns_lock()||next.epoch!=epoch())return false;
  // This is a latest-work mailbox. Drop the old unstarted batch, never an
  // in-flight batch. The last completed result stays available until polled.
  waiting_=next;queued_=true;
  return true;
 }
 bool take(RecoveryJob&out){
  std::lock_guard<std::mutex>guard(gate_);
  if(!queued_)return false;
  out=waiting_;queued_=false;
  return out.epoch==epoch();
 }
 template<class Reader>static RecoveryResult probe(const RecoveryJob&job,Reader read){
  RecoveryResult next{};next.key=job.key;next.epoch=job.epoch;
  alignas(uint32_t)uint8_t scratch[256];
  for(size_t i=0;i<job.count&&i<RecoverySpans;++i){
   const auto&f=job.fields[i];
   if(!f.bytes||f.bytes>sizeof scratch||!range(f.address,f.bytes))continue;
   const size_t slot=next.count++;
   next.fields[slot]=f;++next.attempted;
   const bool ok=read(f.address,scratch,f.bytes);
   next.success[slot]=uint8_t(ok);next.readable+=uint32_t(ok);
  }
  // The acquired bytes are intentionally discarded. Never merge async bytes
  // into an authoritative frame or reuse them to justify native EventClass.
  return next;
 }
 void finish(const RecoveryResult&result){
  if(result.epoch!=epoch())return;
  std::lock_guard<std::mutex>guard(gate_);
  if(result.epoch!=epoch())return;
  // An older worker result cannot replace a newer completed result in the
  // same producer generation. Preserve the original read's provenance.
  if(hasResult_&&result_.epoch==result.epoch&&result_.key.sameWorld(result.key)&&result.key.frame<result_.key.frame)return;
  result_=result;hasResult_=true;
 }
 size_t ready(const Key&current,RequestedSpan*out,size_t max){
  if(!out||!max)return 0;
  std::unique_lock<std::mutex>guard(gate_,std::try_to_lock);
  if(!guard.owns_lock()||!hasResult_)return 0;
  if(result_.epoch!=epoch()||!current.sameWorld(result_.key)||
     current.frame<result_.key.frame||uint32_t(current.tick-result_.key.tick)>1000)return 0;
  size_t count=0;
  for(size_t i=0;i<result_.count&&count<max;++i)
   if(result_.success[i])out[count++]=result_.fields[i];
  return count;
 }
};
} // namespace worldframe

#include "../../shared/source/world_frame_publication.h"
#include <cassert>
#include <cstring>
#include <set>
#include <thread>
#include <atomic>
#include <cstdio>
using namespace worldframe;
static void fill(Snapshot&frame,uint32_t serial,unsigned pages=8){
 Key key{};key.pid=key.owner=key.match=1;key.serial=serial;key.tick=serial+100;key.frame=int(serial);key.generationLow=7;
 frame.reset(key);auto read=[&](uint32_t,void*out,size_t n){memset(out,int(serial&255),n);return true;};
 for(unsigned i=0;i<pages;++i)assert(frame.prefetch(read,0x10000+i*4096,4096));assert(frame.seal());
}
static bool bytes(const Publication&job){for(size_t p=0;p<job.pages.size();++p)for(auto byte:job.pages.data()[p].bytes)if(byte!=uint8_t(job.key.serial))return false;return true;}
int main(){
 {PublicationPool pool;Snapshot frame;bool queued=true;Publication job;
  assert(!pool.submit(frame,&queued)&&!queued);
  assert(pool.begin(frame));fill(frame,1);
  assert(pool.submit(frame,&queued)&&queued);
  for(unsigned i=0;i<1000;++i)assert(pool.submit(frame,&queued)&&!queued);
  assert(pool.take(job)&&job.key.serial==1&&bytes(job));
  for(unsigned i=0;i<1000;++i)assert(pool.submit(frame,&queued)&&!queued);
  Publication extra;assert(!pool.take(extra));pool.finish(job);
  // An actual retry after the previous ownership ends must still signal.
  assert(pool.submit(frame,&queued)&&queued);assert(pool.take(job)&&bytes(job));pool.finish(job);
  assert(pool.submit(frame,&queued)&&queued);pool.invalidate();assert(!pool.take(job));
  assert(pool.submit(frame,&queued)&&queued);assert(pool.take(job)&&pool.current(job));pool.finish(job);
 }
 {PublicationPool pool;Snapshot frame;assert(pool.begin(frame));fill(frame,1);assert(pool.submit(frame));Publication held;assert(pool.take(held)&&pool.current(held));const auto*original=held.pages.data();
  std::set<const Page*>addresses{original};
  for(uint32_t i=2;i<=10000;++i){assert(pool.begin(frame));fill(frame,i);addresses.insert(frame.pages.data());assert(pool.submit(frame));assert(bytes(held)&&held.pages.data()==original&&held.key.tick==101);}
  assert(addresses.size()<=3);Publication latest;assert(pool.take(latest)&&latest.key.serial==10000&&bytes(latest));assert(!pool.take(latest));pool.invalidate();assert(!pool.current(held));pool.finish(held);pool.finish(latest);
  assert(pool.begin(frame));fill(frame,10001);pool.submit(frame);pool.invalidate();assert(!pool.take(latest));
 }
 {PublicationPool pool;Snapshot frame;std::atomic<bool>done{false},bad{false};std::atomic<unsigned>received{0};
  std::thread worker([&]{Publication job;for(;;){if(pool.take(job)){if(!bytes(job))bad=true;std::this_thread::yield();if(!bytes(job))bad=true;++received;pool.finish(job);}else if(done.load(std::memory_order_acquire))break;else std::this_thread::yield();}});
  for(uint32_t i=1;i<=10000;++i){assert(pool.begin(frame));fill(frame,i,4);assert(pool.submit(frame));if(i%137==0)pool.invalidate();}
  done.store(true,std::memory_order_release);worker.join();assert(!bad&&received>0);
 }
 std::puts("PASS bounded publication pool: no page copy on handoff, pinned bytes immutable across 10000 captures, at most three buffers, newest pending state, cancellation, independent source stamps and concurrent producer/consumer stress");
}

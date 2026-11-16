// Real RecoveryMailbox state machine under concurrent producer and background
// read delays. No game process, Host connection or native EventClass runs here.
#include "../../shared/source/world_frame_recovery.h"
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
using namespace worldframe;
static Key key(int frame=10,int match=71,uint32_t tick=1000){
 Key k{};k.pid=123;k.owner=456;k.match=match;k.scenario=0x510000;
 k.house=0x520000;k.cells=0x530000;k.cellCount=9;k.session=2;
 k.generationLow=7;k.connectionLow=9;k.frame=frame;k.tick=tick;
 return k;
}
int main(){
 RecoveryMailbox mailbox;
 std::array<RequestedSpan,32>hints{};
 for(size_t i=0;i<hints.size();++i)
  hints[i]={uint32_t(0x610000+i*256),128,980};
 hints[1]=hints[0]; // duplicates cannot cause extra background reads
 hints[2].tick=0; // stale
 hints[3].bytes=500; // invalid 256-byte limit
 assert(mailbox.submit(key(),hints.data(),hints.size()));
 RecoveryJob pending{};assert(mailbox.take(pending));
 assert(pending.count==RecoverySpans);
 assert(pending.fields[0].address==hints[0].address);
 assert(pending.fields[1].address==hints[4].address);
 assert(!mailbox.take(pending));
 int attempts=0;
 const auto done=RecoveryMailbox::probe(pending,[&](uint32_t at,void*out,size_t size){
  ++attempts;std::memset(out,0xAB,size);return at!=hints[4].address;
 });
 assert(done.attempted==RecoverySpans&&done.readable==RecoverySpans-1);
 assert(attempts==int(RecoverySpans));
 mailbox.finish(done);
 RequestedSpan ready[RecoverySpans]{};
 assert(mailbox.ready(key(11,71,1016),ready,RecoverySpans)==RecoverySpans-1);
 assert(ready[0].address==hints[0].address);
 assert(mailbox.ready(key(11,72,1016),ready,RecoverySpans)==0); // match
 assert(mailbox.ready(key(9),ready,RecoverySpans)==0); // rewind
 assert(mailbox.ready(key(11,71,3001),ready,RecoverySpans)==0); // expiry
 Key replaced=key(11,71,1016);replaced.generationLow=8;
 assert(mailbox.ready(replaced,ready,RecoverySpans)==0);
 mailbox.invalidate();
 assert(mailbox.ready(key(11,71,1016),ready,RecoverySpans)==0);

 // Delayed worker must never make the game-side producer wait. Replace a
 // pending batch while another is being read, then invalidate its whole epoch.
 const RequestedSpan one{0x640000,4,1016};
 assert(mailbox.submit(key(11,71,1016),&one,1));
 RecoveryJob work{};assert(mailbox.take(work));
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 std::thread worker([&]{
  auto outcome=RecoveryMailbox::probe(work,[&](uint32_t,void*out,size_t size){
   {std::lock_guard<std::mutex>lock(mutex);entered=true;}cv.notify_one();
   std::unique_lock<std::mutex>lock(mutex);
   cv.wait(lock,[&]{return release;});
   std::memset(out,0x3B,size);return true;
  });
  mailbox.finish(outcome);
 });
 {std::unique_lock<std::mutex>lock(mutex);cv.wait(lock,[&]{return entered;});}
 const RequestedSpan updated{0x640080,4,1020};
 assert(mailbox.submit(key(12,71,1020),&updated,1));
 mailbox.invalidate(); // a world/connection retirement
 {std::lock_guard<std::mutex>lock(mutex);release=true;}cv.notify_one();
 worker.join();
 assert(mailbox.ready(key(12,71,1020),ready,RecoverySpans)==0);
 assert(!mailbox.take(pending)); // queued old-world request rejected
 assert(mailbox.submit(key(13,71,1040),&updated,1));
 assert(mailbox.take(pending)&&pending.epoch==mailbox.epoch());
 const auto fresh=RecoveryMailbox::probe(pending,[](uint32_t,void*out,size_t){return true;});
 mailbox.finish(fresh);
 assert(mailbox.ready(key(14,71,1056),ready,RecoverySpans)==1);
 assert(ready[0].address==updated.address);
 std::puts("PASS async recovery: bounded hints, source-only read, no stale publication, nonblocking producer, epoch retirement");
}

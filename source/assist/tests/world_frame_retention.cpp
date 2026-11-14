// Actual producer, read scope and transport; synthetic bytes/Win32 I/O only.
#define WORLD_FRAME_PLATFORM_FIXTURE
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"

namespace retention {
using namespace fixture;
void leases(){
 auto leases=std::make_unique<worldframe::DemandLeases>();std::vector<uint32_t> pages;
 CHECK(!leases->retain(0xFFFF,10)&&!leases->retain(0x10001,10)&&!leases->retain(0x80000000,10));
 CHECK(leases->retain(0x10000,10));leases->append(1010,pages);CHECK(pages.size()==1);pages.clear();
 leases->append(1011,pages);CHECK(pages.empty()&&leases->size()==0);
 CHECK(leases->retain(0x10000,0xFFFFFFF0u));CHECK(leases->retain(0x10000,4));leases->append(1004,pages);CHECK(pages.size()==1);pages.clear();
 leases->append(1005,pages);CHECK(pages.empty());
 leases->clear();for(uint32_t i=0;i<worldframe::MaxPages;++i)CHECK(leases->retain(0x10000+i*4096,100));
 CHECK(!leases->retain(0x10000000,100));CHECK(leases->retain(0x10000000,1101));CHECK(leases->size()==1);
 leases->append(1101,pages);CHECK(pages.size()==1&&pages[0]==0x10000000);leases->clear();pages.clear();leases->append(1102,pages);CHECK(pages.empty());
 std::puts("PASS dependency leases: bounded membership, renewal, expiry, wrap, full-capacity expiry recovery and explicit clear");
}
void demandGap(bool paused=false){
 using namespace assist;currentPid=GamePid;fixture::clock=2000;retireWorldFrame();Memory m;setup(m);Read read{&m};
 constexpr uint32_t extra=0x760100;m.put(extra,0x12345678u);
 auto capture=[&](int frame){m.put(0xA8ED84,frame);snap.frame=frame;fixture::clock+=16;return captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base);};
 CHECK(capture(50));{SampleReadScope scope;uint32_t value=0;CHECK(!readWorldSample(extra,&value,4));}
 CHECK(capture(51));{SampleReadScope scope;uint32_t value=0;CHECK(readWorldSample(extra,&value,4)&&value==0x12345678u&&scope.complete());}
 if(paused)fixture::clock+=2000;
 CHECK(capture(52));CHECK(capture(53)); // A derived cache skips its raw reads.
 m.put(extra,0x55667788u);CHECK(capture(54));
 {SampleReadScope scope;uint32_t value=0;CHECK(readWorldSample(extra,&value,4)&&value==0x55667788u&&scope.complete());}
 std::puts("PASS dependency gap: retained demand acquires new bytes after skipped reads");
}
void pendingDemand(){
 using namespace assist;currentPid=GamePid;fixture::clock=8000;retireWorldFrame();Memory m;setup(m);Read read{&m};snap.frame=50;
 constexpr uint32_t extra=0x760100;m.put(extra,0x11223344u);
 CHECK(captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base));{SampleReadScope scope;uint32_t value=0;CHECK(!readWorldSample(extra,&value,4));}
 ++fixture::clock;m.put(0xA8ED84,51);snap.frame=51;m.put(0xA8EC78+16,-1);
 CHECK(!captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base));
 fixture::clock+=2000;m.put(0xA8EC78+16,3);
 CHECK(captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base));
 {SampleReadScope scope;uint32_t value=0;CHECK(readWorldSample(extra,&value,4)&&value==0x11223344u&&scope.complete());}
 std::puts("PASS pending demand: failed captures never acknowledge/forget an unfulfilled dependency");
}
void requestPriority(){
 using namespace fixture;currentPid=HostPid;fixture::clock=1000;Memory memory;gameMemory=&memory;
 external::ProcessReader reader;CHECK(reader.open(GamePid));auto client=worldframe::acquireClient(GamePid,reader.generation());CHECK(!client->acquire(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));auto source=publication(1,50,123);source->key.connectionLow=uint32_t(reader.generation());server.observe(source->key);server.publish(*source);
 currentPid=HostPid;CHECK(reader.beginWorldFrame(7,source));client->request({0x14000});const uint32_t first=channel()->demandRevision;
 ++fixture::clock;uint32_t value=0;const auto beforeRpm=rpmCalls;CHECK(!reader.read(0x13000,&value,4)&&!reader.worldFrameComplete());reader.endWorldFrame();
 CHECK(channel()->demandRevision==first+1);currentPid=GamePid;auto pages=server.requests();CHECK(std::find(pages.begin(),pages.end(),0x13000)!=pages.end());CHECK(rpmCalls==beforeRpm&&source->key.tick==1000);
 // A repeated failure with unchanged dependencies must not republish at 1 kHz.
 currentPid=HostPid;const auto after=channel()->demandRevision;CHECK(reader.beginWorldFrame(7,source));CHECK(!reader.read(0x13000,&value,4));reader.endWorldFrame();CHECK(channel()->demandRevision==after);
 // Successful reads retain ordinary coalescing; they cannot mark an old frame
 // fresh or request extra game reads merely to refresh a cached display.
 client->request({0x15000});CHECK(channel()->demandRevision==after);fixture::clock+=2;client->subscribe(1,worldframe::Base);CHECK(channel()->demandRevision==after+1);
 reader.close();client.reset();server.disconnect();gameMemory=nullptr;
 std::puts("PASS missing dependency priority: failed ProcessReader publishes within 1 ms without next consumer tick; unchanged failures deduplicate; successful requests coalesce; no extra live read/restamp");
}
void failedRetry(){
 using namespace assist;currentPid=GamePid;fixture::clock=3000;retireWorldFrame();Memory m;setup(m);Read read{&m};snap.frame=50;
 m.put(0xA8EC78+16,-1);auto before=worldlife::capture(read);CHECK(!captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));
 const auto reads=m.calls.size();m.put(0xA8EC78+16,3);fixture::clock+=15;CHECK(!captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));CHECK(m.calls.size()==reads);
 ++fixture::clock;CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));CHECK(worldFrameReady&&worldFrame.key.frame==50&&worldFrame.key.tick==fixture::clock);
 const auto serial=worldFrame.key.serial;const auto after=m.calls.size();fixture::clock+=1000;CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Combat));CHECK(worldFrame.key.serial==serial&&m.calls.size()==after);
 std::puts("PASS failed capture: bounded same-frame retry, then successful publication stays immutable");
}
void failureBackoff(){
 using namespace assist;currentPid=GamePid;fixture::clock=6000;retireWorldFrame();Memory m;setup(m);Read read{&m};snap.frame=50;
 m.put(0xA8EC78+16,-1);auto before=worldlife::capture(read);CHECK(!captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));
 for(uint32_t delay:{16u,32u,64u,128u,250u,250u}){
  const auto reads=m.calls.size();fixture::clock+=delay-1;CHECK(!captureWorldFrame(read,before,House,fixture::clock,worldframe::Base)&&m.calls.size()==reads);
  ++fixture::clock;CHECK(!captureWorldFrame(read,before,House,fixture::clock,worldframe::Base)&&m.calls.size()>reads);
 }
 m.put(0xA8EC78+16,3);fixture::clock+=250;CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));
 std::puts("PASS repeated failure: exponential retry capped at 250 ms and recovery without frame advance");
}
void busyPublication(){
 currentPid=HostPid;fixture::clock=4000;auto client=worldframe::acquireClient(GamePid,1);CHECK(!client->acquire(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));auto frame=publication(1,50,0x12345678u);server.observe(frame->key);server.publish(*frame);
 currentPid=HostPid;auto complete=client->acquire(7);CHECK(complete);auto*c=channel();auto&s=c->slots[c->active];++s.seq;
 CHECK(client->acquire(7)==complete);CHECK(complete->key.tick==4000&&complete->key.serial==1);
 CHECK(!client->acquire(8));++s.seq;
 const auto count=s.count;s.count=worldframe::MaxPages+1;CHECK(!client->acquire(7));s.count=count;
 currentPid=GamePid;auto changed=frame->key;++changed.house;server.observe(changed);currentPid=HostPid;++s.seq;CHECK(!client->acquire(7));++s.seq;
 currentPid=GamePid;fixture::clock+=501;changed=frame->key;++changed.frame;server.observe(changed);currentPid=HostPid;++s.seq;CHECK(!client->acquire(7));++s.seq;
 currentPid=GamePid;server.observe(frame->key);currentPid=HostPid;++s.seq;CHECK(client->acquire(7)==complete&&complete->key.tick==4000);++s.seq;
 fixture::clock+=1001;++s.seq;CHECK(!client->acquire(7));++s.seq;
 currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!client->acquire(7));
 std::puts("PASS busy publication: complete retained token, unchanged stamp, strict identity/age/retirement");
}
void failedPublication(){
 using namespace assist;currentPid=GamePid;fixture::clock=5000;retireWorldFrame();worldFrameServer.disconnect();
 currentPid=HostPid;auto client=worldframe::acquireClient(GamePid,1);CHECK(!client->acquire(7));
 currentPid=GamePid;CHECK(worldFrameServer.wanted(HostPid,fixture::clock));Memory m;setup(m);Read read{&m};snap.frame=50;
 CHECK(captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base));currentPid=HostPid;auto complete=client->acquire(7);CHECK(complete);
 currentPid=GamePid;++fixture::clock;m.put(0xA8ED84,51);snap.frame=51;m.put(0xA8EC78+16,-1);
 CHECK(!captureWorldFrame(read,worldlife::capture(read),House,fixture::clock,worldframe::Base));CHECK(!worldFrameReady);
 currentPid=HostPid;CHECK(client->acquire(7)==complete&&client->accepts(complete));CHECK(complete->key.frame==50&&complete->key.tick==5000);
 currentPid=GamePid;retireWorldFrame();currentPid=HostPid;CHECK(!client->acquire(7));
 std::puts("PASS failed new capture: same-world old publication retained without restamping, explicit retirement clears it");
}
}
int main(int argc,char**argv){
 if(argc!=2)return 2;const std::string name=argv[1];
 if(name=="priority")retention::requestPriority();else if(name=="demand")retention::demandGap();else if(name=="retry")retention::failedRetry();
 else if(name=="busy")retention::busyPublication();else if(name=="publication")retention::failedPublication();else if(name=="paused-demand")retention::demandGap(true);else if(name=="pending-demand")retention::pendingDemand();else if(name=="leases")retention::leases();else if(name=="backoff")retention::failureBackoff();else return 2;
 std::printf("{\"case\":\"%s\",\"checks\":%zu,\"pass\":true}\n",name.c_str(),fixture::checks);
}

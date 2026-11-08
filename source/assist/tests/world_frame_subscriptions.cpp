// Reuse the real transport/Runtime fixture's OS boundary; execute only the
// subscription lifetime regression below, never an imitation of Server/Client.
#define WORLD_FRAME_PLATFORM_FIXTURE
#define main world_frame_core_unused_main
#include "world_frame_core.cpp"
#undef main

namespace subscription {
using namespace fixture;
struct Checks {uint32_t redundantPublications=0;size_t reopenedPages=0,wrapPages=0;bool runtimeResume=false;size_t resumeReads=0,repeatedExports=0;};
Checks result;
void membership(){
 CHECK(mappedBytes==0);fixture::clock=30000;currentPid=HostPid;
 auto client=worldframe::acquireClient(GamePid,91);CHECK(!client->acquire(7));client->subscribe(1,worldframe::Base);client->request({0x740000});
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));CHECK(server.requests().size()==1);
 const auto revision=channel()->demandRevision;
 for(unsigned i=0;i<50;++i){fixture::clock+=3;currentPid=HostPid;client->request({0x740000});currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));CHECK(server.requests().size()==1);}
 result.redundantPublications=channel()->demandRevision-revision;
 currentPid=HostPid;client.reset();++fixture::clock;
 client=worldframe::acquireClient(GamePid,91);client->acquire(7);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));result.reopenedPages=server.requests().size();
 server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
 fixture::clock=0xFFFFFFFFu-100;client=worldframe::acquireClient(GamePid,92);client->acquire(7);client->subscribe(1,worldframe::Visual);client->request({0x740000});
 currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));CHECK(server.requests().size()==1);
 fixture::clock+=251;currentPid=HostPid;client->subscribe(2,worldframe::Base);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));result.wrapPages=server.requests().size();
 server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
}
void runtimeResume(){
 using namespace assist;CHECK(mappedBytes==0);fixture::clock=40000;currentPid=HostPid;
 auto client=worldframe::acquireClient(GamePid,93);client->acquire(7);client->subscribe(1,worldframe::Base);
 currentPid=GamePid;retireWorldFrame();CHECK(worldFrameServer.wanted(HostPid,fixture::clock));
 Memory memory;setup(memory);Read read{&memory};auto before=worldlife::capture(read);snap.frame=before.frame;
 CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));currentPid=HostPid;auto pinned=client->acquire(7);CHECK(pinned);
 fixture::clock+=1001;currentPid=GamePid;CHECK(!worldFrameServer.wanted(HostPid,fixture::clock));
 memory.put(0xA8ED84,int(51));memory.put(ActorA+0x6C,int(25));before=worldlife::capture(read);snap.frame=51;
 CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));const auto sourceKey=worldFrame.key;
 memory.put(ActorA+0x6C,int(90)); // A resume must export the old sealed 25.
 ++fixture::clock;currentPid=HostPid;client->acquire(7);currentPid=GamePid;CHECK(worldFrameServer.wanted(HostPid,fixture::clock));
 const auto readsBefore=memory.calls.size();CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));result.resumeReads=memory.calls.size()-readsBefore;
 currentPid=HostPid;auto resumed=client->acquire(7);int hp=0;
 result.runtimeResume=resumed&&resumed->key.sameFrame(sourceKey)&&resumed->key.serial==sourceKey.serial&&resumed->key.tick==sourceKey.tick&&resumed->read(ActorA+0x6C,&hp,4)&&hp==25;
 CHECK(pinned->read(ActorA+0x6C,&hp,4)&&hp==100);
 currentPid=GamePid;const auto slot=channel()->active;const auto seq=slot>=0?channel()->slots[slot].seq:0;
 const auto allocations=dataCreates;
 for(unsigned i=0;i<100;++i)CHECK(captureWorldFrame(read,before,House,fixture::clock,worldframe::Base));
 result.repeatedExports=(channel()->active!=slot)||(slot>=0&&channel()->slots[slot].seq!=seq)||(dataCreates!=allocations);
 retireWorldFrame();worldFrameServer.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
}
}

int main(int argc,char**argv){
 using namespace fixture;
 const bool requireFixed=argc>1&&std::string(argv[1])=="--expect-fixed";
 CHECK(mappedBytes==0);currentPid=HostPid;fixture::clock=10000;
 auto client=worldframe::acquireClient(GamePid,77);CHECK(!client->acquire(7));
 std::vector<uint32_t>visualPages;for(unsigned i=0;i<256;++i)visualPages.push_back(0x900000+i*4096);
 client->subscribe(1,worldframe::Visual);client->request(visualPages);
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));CHECK(server.requests().size()==visualPages.size());

 // A remaining Base-only consumer keeps the connection live after the last
 // visual reader stops requesting its much larger page set.
 fixture::clock+=251;currentPid=HostPid;client->subscribe(2,worldframe::Base);
 currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));CHECK(server.demandMask()==worldframe::Base);
 const auto activeExpiredPages=server.requests().size();
 Memory memory;setup(memory);worldframe::Snapshot fresh,stale;fresh.reset(key());stale.reset(key());
 worldframe::CaptureGraph<Read>baseline(fresh,Read{&memory}),remaining(stale,Read{&memory});
 CHECK(baseline.capture(worldframe::Base,{}));CHECK(remaining.capture(worldframe::Base,server.requests()));
 const auto activeExtraReads=stale.counters.calls-fresh.counters.calls;

 // Then every Host reader becomes idle while an internal automatic feature
 // can still capture local worlds. It must not retain remote demand or copy
 // new payloads into a channel that has no live consumer.
 fixture::clock+=3;currentPid=HostPid;client->request(visualPages);currentPid=GamePid;CHECK(server.requests().size()==visualPages.size());
 auto first=publication(1,50,0x11223344);first->key.connectionLow=77;server.observe(first->key);server.publish(*first);
 currentPid=HostPid;auto pinned=client->acquire(7);CHECK(pinned);
 fixture::clock+=1001;currentPid=GamePid;CHECK(!server.wanted(HostPid,fixture::clock));
 const auto timedOutPages=server.requests().size();const auto priorCreates=dataCreates;
 auto next=publication(2,51,0x55667788);next->key.connectionLow=77;server.observe(next->key);server.publish(*next);
 const auto lateCreates=dataCreates-priorCreates;
 const bool latePublished=channel()->active>=0&&channel()->slots[channel()->active].key.serial==2;
 uint32_t value=0;CHECK(pinned->read(0x10000,&value,4)&&value==0x11223344);

 // Polling is a heartbeat, not renewal of every former page dependency.
 ++fixture::clock;currentPid=HostPid;client->acquire(7);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));
 const auto resumedOldPages=server.requests().size();
 currentPid=HostPid;fixture::clock+=3;client->subscribe(3,worldframe::Base);client->request({0x750000,0x751000});
 currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));const auto renewed=server.requests();
 CHECK(renewed.size()==2&&std::find(renewed.begin(),renewed.end(),0x750000)!=renewed.end()&&std::find(renewed.begin(),renewed.end(),0x751000)!=renewed.end());
 next->key.serial=3;next->key.tick=fixture::clock;server.observe(next->key);server.publish(*next);
 currentPid=HostPid;auto recovered=client->acquire(7);CHECK(recovered&&recovered->key.serial==3&&recovered->read(0x10000,&value,4)&&value==0x55667788);
 CHECK(pinned->read(0x10000,&value,4)&&value==0x11223344);
 currentPid=GamePid;server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
 subscription::membership();subscription::runtimeResume();const auto&extra=subscription::result;
 const bool fixed=!activeExpiredPages&&!activeExtraReads&&!timedOutPages&&!lateCreates&&!latePublished&&!resumedOldPages&&!extra.redundantPublications&&!extra.reopenedPages&&!extra.wrapPages&&extra.runtimeResume&&!extra.resumeReads&&!extra.repeatedExports;
 std::printf("{\"new_contract_passed\":%s,\"active_expired_pages\":%zu,\"active_extra_capture_reads\":%u,\"base_capture_reads\":%u,\"timed_out_pages\":%zu,\"inactive_payload_allocations\":%zu,\"inactive_publication\":%s,\"resumed_old_pages\":%zu,\"fresh_renewal_pages\":%zu,\"redundant_demand_publications\":%u,\"reopened_old_pages\":%zu,\"wrap_expired_pages\":%zu,\"paused_runtime_resumed\":%s,\"resume_live_reads\":%zu,\"repeated_same_frame_exports\":%zu,\"checks\":%zu,\"windows_executed\":false,\"game_executed\":false}\n",
  fixed?"true":"false",activeExpiredPages,activeExtraReads,fresh.counters.calls,timedOutPages,lateCreates,latePublished?"true":"false",resumedOldPages,renewed.size(),extra.redundantPublications,extra.reopenedPages,extra.wrapPages,extra.runtimeResume?"true":"false",extra.resumeReads,extra.repeatedExports,checks);
 return requireFixed&&!fixed?1:0;
}

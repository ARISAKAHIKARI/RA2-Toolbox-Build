// Real Client/Server/ProcessReader, controlled Win32 mappings and clock.
#define WORLD_FRAME_PLATFORM_FIXTURE
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"

namespace polling {
using namespace fixture;
void demands(){
 currentPid=HostPid;fixture::clock=5000;auto client=worldframe::acquireClient(GamePid,76);
 client->subscribe(1,worldframe::Visual);const auto before=worldframe::demandProbeSweeps;
 for(unsigned n=0;n<1000;++n){fixture::clock=5000+n;client->subscribe(1,worldframe::Visual);CHECK(!client->acquire(7));}
 const auto sweeps=worldframe::demandProbeSweeps-before;
#ifdef EXPECT_POLL_FAST
 CHECK(sweeps==3);
#else
 CHECK(sweeps==2000);
#endif
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Visual));
 fixture::clock=6250;CHECK(!client->acquire(7));CHECK(!channel()->demandMask);
 std::printf("{\"case\":\"demand_renewals\",\"calls\":2000,\"full_sweeps\":%llu}\n",(unsigned long long)sweeps);
}
void cadence(bool moving,uint32_t start){
 currentPid=HostPid;fixture::clock=start;external::ProcessReader reader;CHECK(reader.open(GamePid));
 reader.setWorldDemand(worldframe::Visual);CHECK(!reader.beginWorldFrame(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));worldframe::Snapshot source;
 fillPages(source,128,1,uint32_t(reader.generation()));server.observe(source.key);server.publish(source);
 currentPid=HostPid;worldframe::Token retained;
 unsigned full=0,fast=0,published=1,seen=0;const auto maps=dataMaps,opens=dataOpens;
 for(unsigned n=0;n<1000;++n){
  fixture::clock=start+n;currentPid=GamePid;
  if(moving&&n&&n%17==0){fillPages(source,128,++published,uint32_t(reader.generation()));server.observe(source.key);server.publish(source);}
  if(n%4==0)server.observe(source.key);
  currentPid=HostPid;
#ifdef EXPECT_POLL_FAST
  if(reader.unchangedWorldFrame(retained,fixture::clock)){++fast;continue;}
#endif
  ++full;CHECK(reader.open(GamePid));CHECK(reader.beginWorldFrame(7));auto next=reader.worldFrameToken();CHECK(next);
  if(next!=retained)++seen;retained=next;reader.endWorldFrame();
 }
 CHECK(retained&&seen==published&&retained->key.serial==published);
 CHECK(retained->key.tick==start+(moving?986:0));
#ifdef EXPECT_POLL_FAST
 CHECK(full==(moving?59u:20u));CHECK(fast==1000-full);
#else
 CHECK(full==1000&&!fast);
#endif
 std::printf("{\"case\":\"%s\",\"start\":%u,\"polls\":1000,\"full_paths\":%u,\"fast_paths\":%u,\"publications\":%u,\"seen\":%u,\"data_maps\":%zu,\"data_opens\":%zu}\n",moving?"moving":"paused",start,full,fast,published,seen,dataMaps-maps,dataOpens-opens);
 currentPid=GamePid;server.disconnect();currentPid=HostPid;reader.close();
}
#ifdef EXPECT_POLL_FAST
void identity(){
 currentPid=HostPid;fixture::clock=70000;auto client=worldframe::acquireClient(GamePid,77);CHECK(!client->unchanged({},fixture::clock));CHECK(!client->acquire(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));worldframe::Snapshot source;fillPages(source,2,1,77);server.observe(source.key);server.publish(source);
 currentPid=HostPid;auto first=client->acquire(7);CHECK(first&&client->unchanged(first,fixture::clock));auto*c=channel();auto&s=c->slots[c->active];
 const auto heartbeat=c->heartbeat;const auto key=first->key;
 for(unsigned n=0;n<1000;++n)CHECK(client->unchanged(first,fixture::clock));
 CHECK(c->heartbeat==heartbeat&&!std::memcmp(&first->key,&key,sizeof key));
 ++s.seq;CHECK(!client->unchanged(first,fixture::clock));++s.seq;
 ++c->observationSeq;CHECK(!client->unchanged(first,fixture::clock));++c->observationSeq;
 ++c->connectionSeq;CHECK(!client->unchanged(first,fixture::clock));++c->connectionSeq;
 ++s.data.allocation;CHECK(!client->unchanged(first,fixture::clock));--s.data.allocation;
 ++c->observed.frame;CHECK(!client->unchanged(first,fixture::clock));--c->observed.frame;
 fixture::clock+=501;CHECK(!client->unchanged(first,fixture::clock));
 currentPid=GamePid;server.observe(source.key);currentPid=HostPid;CHECK(client->unchanged(first,fixture::clock));
 currentPid=GamePid;fillPages(source,2,2,77);server.observe(source.key);server.publish(source);currentPid=HostPid;CHECK(!client->unchanged(first,fixture::clock));auto second=client->acquire(7);CHECK(second&&client->unchanged(second,fixture::clock));
 currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!client->unchanged(second,fixture::clock));
 auto replacement=worldframe::acquireClient(GamePid,78);CHECK(!replacement->acquire(7));CHECK(!client->unchanged(second,fixture::clock));
 currentPid=GamePid;server.disconnect();currentPid=HostPid;
 std::puts("{\"case\":\"identity_and_seqlocks\",\"passed\":true}");
}
#endif
}
int main(){polling::demands();polling::cadence(false,10000);polling::cadence(true,20000);polling::cadence(false,0xFFFFFF80u);
#ifdef EXPECT_POLL_FAST
 polling::identity();
#endif
 std::printf("PASS polling production path: checks=%zu; controlled mappings/clock, not Windows timing\n",fixture::checks);
}

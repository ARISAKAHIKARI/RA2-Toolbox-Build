// Exercise actual Client/Server with the established controlled Win32 boundary.
#define WORLD_FRAME_PLATFORM_FIXTURE
#define main inherited_world_frame_core_main
#include "world_frame_core.cpp"
#undef main
#include <thread>

namespace deadline {
using namespace fixture;
static FILE*traceFile=nullptr;static std::vector<uint32_t>lastTracePages;static size_t scenarios=0,events=0;
uint64_t visits(){
#ifdef WORLD_FRAME_SCAN_PROBE
 return worldframe::subscriptionProbeVisits;
#else
 return 0;
#endif
}
uint64_t sweeps(){
#ifdef WORLD_FRAME_SCAN_PROBE
 return worldframe::subscriptionProbeSweeps;
#else
 return 0;
#endif
}
void word(uint32_t value){if(traceFile)for(unsigned shift=0;shift<32;shift+=8){const uint8_t byte=uint8_t(value>>shift);CHECK(std::fwrite(&byte,1,1,traceFile)==1);}}
void trace(const std::vector<uint32_t>&pages){
 ++events;const auto*c=channel();CHECK(c);word(fixture::clock);word(c->demandRevision);word(c->demandCount);word(c->demandMask);word(c->heartbeat);word(uint32_t(c->active));
 const bool changed=pages!=lastTracePages;word(changed);if(changed){word(uint32_t(pages.size()));for(uint32_t p:pages)word(p);lastTracePages=pages;}
}
std::vector<uint32_t> pages(size_t count,unsigned first=0){std::vector<uint32_t>out;for(size_t i=0;i<count;++i)out.push_back(0x10000000+uint32_t(first+i)*4096);return out;}
struct Context {
 std::shared_ptr<worldframe::Client>client;worldframe::Server server;uint32_t started;uint64_t generation;
 explicit Context(uint32_t tick,uint64_t gen=91):started(tick),generation(gen){
  ++scenarios;CHECK(mappedBytes==0);fixture::clock=tick;currentPid=HostPid;client=worldframe::acquireClient(GamePid,gen);CHECK(!client->acquire(7));client->subscribe(1,worldframe::Base);currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));sync();
 }
 ~Context(){currentPid=GamePid;server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);}
 void at(uint32_t elapsed){fixture::clock=started+elapsed;}
 const std::vector<uint32_t>&sync(){currentPid=GamePid;server.wanted(HostPid,fixture::clock);const auto&out=server.requests();trace(out);return out;}
 void request(const std::vector<uint32_t>&wanted){currentPid=HostPid;client->request(wanted);sync();}
 void poll(){currentPid=HostPid;client->acquire(7);sync();}
 void subscribe(uintptr_t id,uint32_t demand){currentPid=HostPid;client->subscribe(id,demand);sync();}
 void expect(std::vector<uint32_t>expected){auto actual=sync();std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());if(actual!=expected)std::fprintf(stderr,"scenario=%zu elapsed=%u actual=%zu expected=%zu\n",scenarios,uint32_t(fixture::clock-started),actual.size(),expected.size());CHECK(actual==expected);}
};
void steady(size_t count){
 Context c(10000);auto wanted=pages(count);c.request(wanted);const auto firstVisits=visits(),firstSweeps=sweeps();const auto revision=channel()->demandRevision;
 for(unsigned i=1;i<=500;++i){c.at(i*2);c.request(wanted);c.expect(wanted);}
 CHECK(channel()->demandRevision==revision);
 const auto walked=visits()-firstVisits,scanned=sweeps()-firstSweeps;
#ifdef WORLD_FRAME_SCAN_PROBE
#ifdef EXPECT_DEADLINE
 CHECK(walked<=count*4&&scanned<=4);
#else
 CHECK(walked==count*500&&scanned==500);
#endif
#endif
 c.at(1251);c.poll();c.expect({});
 std::printf("{\"case\":\"steady\",\"pages\":%zu,\"calls\":500,\"virtual_ms\":1000,\"entry_visits\":%llu,\"full_sweeps\":%llu,\"unchanged_membership_publications\":0}\n",count,(unsigned long long)walked,(unsigned long long)scanned);
}
void boundaries(uint32_t start){
 {Context c(start);const auto a=pages(1);c.request(a);c.at(250);c.request({});c.expect(a);c.at(251);c.request({});c.expect(a);c.at(252);c.request({});c.expect({});}
 {Context c(start);const auto a=pages(1);c.request(a);c.at(250);c.request(a);c.at(500);c.request({});c.expect(a);c.at(501);c.request({});c.expect(a);c.at(502);c.request({});c.expect({});}
 {Context c(start);c.at(200);const auto a=pages(1);c.request(a);c.at(251);c.request({});c.expect(a);c.at(450);c.request({});c.expect(a);c.at(451);c.request({});c.expect(a);c.at(452);c.request({});c.expect({});}
 {Context c(start);c.request(pages(1));c.at(1);c.request(pages(1,1));c.expect(pages(1));c.at(2);c.poll();c.expect(pages(2));c.at(3);c.request(pages(1,2));c.expect(pages(2));c.at(4);c.subscribe(2,worldframe::Base);c.expect(pages(3));}
 {Context c(start);c.request(pages(2));c.at(249);c.request(pages(1));c.at(251);c.request({});c.expect(pages(1));c.at(500);c.poll();c.expect(pages(1));c.at(502);c.request({});c.expect({});}
}
void capacity(){
 Context c(30000);const auto all=pages(worldframe::MaxPages),half=pages(worldframe::MaxPages/2),fresh=pages(worldframe::MaxPages/2,worldframe::MaxPages);
 c.request(all);c.expect(all);c.at(249);c.request(half);c.at(251);c.request(fresh);auto expected=half;expected.insert(expected.end(),fresh.begin(),fresh.end());c.expect(expected);
 c.at(252);c.request({0,0xFFFF,0x15001,0x80000000u});c.expect(expected);
 c.at(500);c.request({});c.expect(fresh);c.at(501);c.request(half);c.expect(fresh);c.at(502);c.request(pages(1,50000));expected=half;expected.push_back(0x10000000+50000u*4096);c.expect(expected);
}
void staggeredExpiry(){
 Context c(40000);c.request(pages(1));const auto firstVisits=visits(),firstSweeps=sweeps();
 for(unsigned i=1;i<=400;++i){c.at(i*2);c.request(pages(1,i));c.expect(pages(std::min(i+1,126u),i>125?i-125:0));}
 const auto walked=visits()-firstVisits,scanned=sweeps()-firstSweeps;
#ifdef WORLD_FRAME_SCAN_PROBE
#ifdef EXPECT_DEADLINE
 CHECK(walked==34650&&scanned==275);
#else
 CHECK(walked==42525&&scanned==400);
#endif
#endif
 std::printf("{\"case\":\"staggered_expiry\",\"calls\":400,\"virtual_ms\":800,\"entry_visits\":%llu,\"full_sweeps\":%llu,\"final_pages\":126}\n",(unsigned long long)walked,(unsigned long long)scanned);
}
void idleAndReattach(){
 Context c(50000);c.request(pages(3));c.at(249);c.request(pages(1));c.at(251);c.poll();c.at(500);c.poll();c.expect({});
 c.at(502);c.request(pages(5));c.at(504);currentPid=HostPid;c.client.reset();c.client=worldframe::acquireClient(GamePid,c.generation);CHECK(!c.client->acquire(7));c.sync();c.expect({});
 c.at(506);c.request(pages(1,10));auto old=c.client;c.at(508);currentPid=HostPid;c.client=worldframe::acquireClient(GamePid,c.generation+1);CHECK(!c.client->acquire(7));c.sync();c.expect({});
 const auto heartbeat=channel()->heartbeat,revision=channel()->demandRevision;c.at(510);currentPid=HostPid;old->request(pages(4));old->subscribe(2,worldframe::Visual);CHECK(!old->acquire(7));CHECK(channel()->heartbeat==heartbeat&&channel()->demandRevision==revision);old.reset();c.sync();
 c.request(pages(2,30));c.expect(pages(2,30));c.at(2000);c.sync();CHECK(c.server.requests().empty());c.poll();c.expect({});
}
void randomTrace(){
 Context c(0xFFFFFF00u);uint32_t random=0xA13D52E7;
 auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
 for(unsigned i=0;i<4000;++i){fixture::clock+=next()%5;if(i%71==0)fixture::clock+=251;if(i%193==0)fixture::clock+=1001;if(i%509==0)fixture::clock+=0x80000001u;
  switch(next()%4){
   case 0:{std::vector<uint32_t>wanted;const size_t n=next()%17;for(size_t j=0;j<n;++j)wanted.push_back(0x10000000+(next()%512)*4096);if(i%11==0)wanted.push_back(0x15001);c.request(wanted);break;}
   case 1:c.poll();break;
   case 2:c.subscribe(next()%8,1u<<(next()%6));break;
   default:c.request({});break;
  }
 }
}
void concurrentReaders(){
 Context c(90000);std::array<std::vector<uint32_t>,4>sets;for(unsigned i=0;i<sets.size();++i)sets[i]=pages(128,i*128);
 c.request(pages(512)); // Fix wire order before concurrent same-time renewals.
 currentPid=HostPid;std::array<std::thread,4>threads;
 for(unsigned i=0;i<threads.size();++i)threads[i]=std::thread([&,i](){for(unsigned n=0;n<50;++n)c.client->request(sets[i]);});
 for(auto&t:threads)t.join();c.at(2);c.request({});c.expect(pages(512));c.at(251);c.poll();c.expect(pages(512));c.at(253);c.poll();c.expect({});
}
void tokenLifetime(){
 Context c(100000,1);auto first=publication(1,50,0x11223344);currentPid=GamePid;c.server.observe(first->key);c.server.publish(*first);currentPid=HostPid;auto pinned=c.client->acquire(7);CHECK(pinned);
 c.request(pages(1024));for(unsigned i=1;i<=200;++i){c.at(i*2);c.request(pages(1));}c.at(652);c.poll();
 uint32_t value=0;CHECK(pinned->read(0x10000,&value,4)&&value==0x11223344);currentPid=GamePid;c.server.retire();currentPid=HostPid;CHECK(!c.client->acquire(7));CHECK(pinned->read(0x10000,&value,4)&&value==0x11223344);
}
}
int main(int argc,char**argv){
 if(argc>1){deadline::traceFile=std::fopen(argv[1],"wb");CHECK(deadline::traceFile);}
 for(size_t count:{128u,4096u,8192u})deadline::steady(count);
 deadline::boundaries(1000);deadline::boundaries(0xFFFFFF80u);deadline::capacity();deadline::staggeredExpiry();deadline::idleAndReattach();deadline::randomTrace();deadline::concurrentReaders();deadline::tokenLifetime();
 if(deadline::traceFile)CHECK(std::fclose(deadline::traceFile)==0);
 std::printf("{\"passed\":true,\"scenarios\":%zu,\"events\":%zu,\"checks\":%zu,\"windows_executed\":false,\"game_tested\":false}\n",deadline::scenarios,deadline::events,fixture::checks);
}

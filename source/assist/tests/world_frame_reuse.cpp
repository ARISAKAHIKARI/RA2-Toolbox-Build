// Production Client/Server/Snapshot plus the existing Win32 mapping model.
// No original broad test entry point runs. Allocation counts cover acquire only.
#include <cstddef>
#include <cstdlib>
#include <new>
namespace allocation {
thread_local bool enabled=false,failPage=false,failOwner=false;
thread_local size_t pages=0,pageBytes=0,large=0,small=0,smallBytes=0;
void reset(){pages=pageBytes=large=small=smallBytes=0;failPage=failOwner=false;}
}
extern "C" void*__real_malloc(size_t);
extern "C" void*__wrap_malloc(size_t bytes){
 if(allocation::enabled){++allocation::pages;allocation::pageBytes+=bytes;if(allocation::failPage){allocation::failPage=false;return nullptr;}}
 return __real_malloc(bytes);
}
void*operator new(size_t bytes){
 if(allocation::enabled){if(bytes>=131072)++allocation::large;else{++allocation::small;allocation::smallBytes+=bytes;}
  if(allocation::failOwner){allocation::failOwner=false;throw std::bad_alloc();}}
 if(auto*p=__real_malloc(bytes))return p;throw std::bad_alloc();
}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
#define WORLD_FRAME_PLATFORM_FIXTURE
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
#include "../../external_reader/source/scene_lighting.h"
#include <thread>

namespace reuse {
using namespace fixture;
struct PaletteReader {worldframe::Token token;worldframe::Token worldFrameToken()const{return token;}bool worldFrameComplete()const{return bool(token);}};
struct PaletteRead {static constexpr bool SealedWorldReads=true;PaletteReader*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->token->read(p,out,n);}};
void paletteIdentity(){
 Memory memory;const uint32_t convert=0x700000,data=0x710000;
 memory.put(0xA8B230,Scenario);memory.put(0xA8ED84,50);memory.put(convert,0x7E5358u);memory.put(convert+4,2);memory.put(convert+0x16C,1);memory.put(convert+0x170,data);
 auto pool=std::make_shared<worldframe::SnapshotPool>();PaletteReader reader;external::lighting::State first,second;
 auto capture=[&](worldframe::Snapshot&s,uint16_t word){std::array<uint16_t,256>words;words.fill(word);memory.put(data,words);s.reset(key());Read read{&memory};
  for(uint32_t at:{0x8A0DD0u,0xA8B230u,0xA8ED84u,convert,data})CHECK(s.prefetch(read,at&~4095u,4096));CHECK(s.seal());};
 auto source=worldframe::SnapshotPool::acquire(pool);auto*address=source.get();capture(*source,0x1234);reader.token=source;
 external::lighting::converterMetadata(PaletteRead{&reader},convert,first);CHECK(first.table&&first.table->front()==0x1234);
 reader.token.reset();source.reset();auto replacement=worldframe::SnapshotPool::acquire(pool);CHECK(replacement.get()==address);capture(*replacement,0x4321);reader.token=replacement;
 external::lighting::converterMetadata(PaletteRead{&reader},convert,second);CHECK(second.table&&second.table!=first.table&&second.table->front()==0x4321);
 std::puts("{\"case\":\"real_palette_cache_same_address_new_publication\",\"passed\":true}");
}
void weakIdentity(){
 auto pool=std::make_shared<worldframe::SnapshotPool>();std::weak_ptr<worldframe::SnapshotPool>poolWeak=pool;
 auto first=worldframe::SnapshotPool::acquire(pool);auto*address=first.get();fillPages(*first,2,1);
 std::weak_ptr<const worldframe::Snapshot>weak=first;first.reset();CHECK(weak.expired());
 auto next=worldframe::SnapshotPool::acquire(pool);CHECK(next.get()==address);fillPages(*next,2,2);
 CHECK(!weak.lock()&&next->key.serial==2);pool.reset();CHECK(!poolWeak.expired());
 next.reset();CHECK(poolWeak.expired()&&weak.expired()); // An old weak cannot pin idle pages through the deleter.
 std::puts("{\"case\":\"weak_identity_and_pool_lifetime\",\"passed\":true}");
}
void warmAcquires(){
 for(uint32_t count:{28u,512u,4096u}){
  currentPid=HostPid;fixture::clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));
  currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));worldframe::Snapshot source;worldframe::Token pinned;
  for(uint32_t serial=1;serial<=3;++serial){fillPages(source,count,serial);server.observe(source.key);server.publish(source);currentPid=HostPid;auto token=client->acquire(7);CHECK(token);if(serial==1)pinned=token;currentPid=GamePid;}
  const auto opens=dataOpens,maps=dataMaps,creates=dataCreates;size_t large=0,small=0,smallBytes=0,pages=0,same=0;
  std::vector<std::weak_ptr<const worldframe::Snapshot>>weak;
  for(uint32_t serial=4;serial<24;++serial){
   currentPid=GamePid;++fixture::clock;fillPages(source,count,serial);server.observe(source.key);server.publish(source);currentPid=HostPid;
   allocation::reset();allocation::enabled=true;auto token=client->acquire(7);allocation::enabled=false;
   CHECK(token&&token->key.serial==serial);large+=allocation::large;small+=allocation::small;smallBytes+=allocation::smallBytes;pages+=allocation::pages;
   for(const auto&old:weak)CHECK(old.expired());weak.push_back(token);
   allocation::reset();allocation::enabled=true;for(int n=0;n<100;++n)CHECK(client->acquire(7)==token);allocation::enabled=false;same+=allocation::large+allocation::small+allocation::pages;
  }
  uint32_t value=0;CHECK(pinned->read(0x10000000,&value,4)&&value==0x01010101u);CHECK(dataOpens==opens&&dataMaps==maps&&dataCreates==creates);
  CHECK(!large&&!pages&&small==20&&!same);
  std::printf("{\"case\":\"steady_client\",\"pages\":%u,\"new_publications\":20,\"snapshot_allocations\":%zu,\"page_allocations\":%zu,\"control_block_allocations\":%zu,\"control_block_bytes\":%zu,\"same_frame_acquires\":2000,\"same_frame_allocations\":%zu,\"retained_token_unchanged\":true}\n",count,large,pages,small,smallBytes,same);
 }
}
void saturatedAndRetired(){
 currentPid=HostPid;fixture::clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));worldframe::Snapshot source;
 std::vector<worldframe::Token>held;
 for(uint32_t serial=1;serial<=9;++serial){
  currentPid=GamePid;fillPages(source,3,serial);server.observe(source.key);server.publish(source);currentPid=HostPid;
  auto token=client->acquire(7);CHECK(token&&token->key.serial==serial);held.push_back(token);
  for(size_t i=0;i<held.size();++i){uint32_t value=0;CHECK(held[i]->read(0x10000000,&value,4)&&value==uint32_t(i+1)*0x01010101u);}
 }
 std::weak_ptr<const worldframe::Snapshot>old=held[0];currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!client->acquire(7));
 held.erase(held.begin());CHECK(old.expired());
 // The old tokens return to their retired pool, never the fresh connection.
 auto replacement=worldframe::acquireClient(GamePid,11);CHECK(!replacement->acquire(7));currentPid=GamePid;CHECK(server.wanted(HostPid,fixture::clock));
 fillPages(source,3,10,11);server.observe(source.key);server.publish(source);currentPid=HostPid;auto next=replacement->acquire(7);CHECK(next&&next->key.connectionLow==11);
 CHECK(!replacement->accepts(held.back()));held.clear();uint32_t value=0;CHECK(next->read(0x10000000,&value,4)&&value==0x0A0A0A0Au);
 std::puts("{\"case\":\"nine_pinned_publications_and_retirement\",\"passed\":true}");
}
void failures(){
 currentPid=HostPid;fixture::clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));currentPid=GamePid;
 worldframe::Server server;CHECK(server.wanted(HostPid,fixture::clock));worldframe::Snapshot source;
 fillPages(source,1,1);server.observe(source.key);server.publish(source);currentPid=HostPid;auto first=client->acquire(7);CHECK(first);
 currentPid=GamePid;fillPages(source,513,2);server.observe(source.key);server.publish(source);currentPid=HostPid;
 allocation::reset();allocation::enabled=true;allocation::failPage=true;auto failed=client->acquire(7);allocation::enabled=false;CHECK(!failed&&allocation::pages==1);
 uint32_t value=0;CHECK(first->read(0x10000000,&value,4)&&value==0x01010101u);
 auto grown=client->acquire(7);CHECK(grown&&grown->pages.size()==513&&grown->key.serial==2);
 currentPid=GamePid;fillPages(source,2,3);server.observe(source.key);server.publish(source);currentPid=HostPid;
 auto*c=channel();auto mapped=payload(c->slots[c->active],unsigned(c->active));auto*wire=reinterpret_cast<worldframe::Page*>(mapped->bytes.data()+sizeof(worldframe::DataHeader));
 const uint32_t second=wire[1].address;wire[1].address=wire[0].address;CHECK(!client->acquire(7));wire[1].address=second;
 auto recovered=client->acquire(7);CHECK(recovered&&recovered->key.serial==3&&recovered->pages.size()==2);
 currentPid=GamePid;fillPages(source,2,4);server.observe(source.key);server.publish(source);currentPid=HostPid;
 auto*idle=grown.get();grown.reset(); // Reuse storage, so the injected new failure must be the control block.
 allocation::reset();allocation::enabled=true;allocation::failOwner=true;bool threw=false;try{client->acquire(7);}catch(const std::bad_alloc&){threw=true;}allocation::enabled=false;CHECK(threw);
 CHECK(allocation::large==0&&allocation::small==1&&allocation::pages==0);
 auto afterOwnerFailure=client->acquire(7);CHECK(afterOwnerFailure&&afterOwnerFailure.get()==idle&&afterOwnerFailure->key.serial==4);
 CHECK(recovered->read(0x10000000,&value,4)&&value==0x03030303u);
 std::puts("{\"case\":\"growth_duplicate_page_and_control_block_failure_recovery\",\"passed\":true}");
}
void concurrentReturns(){
 auto pool=std::make_shared<worldframe::SnapshotPool>();std::atomic<unsigned>done{0},failures{0};
 auto worker=[pool,&done,&failures](unsigned offset){
  for(unsigned n=1;n<=300;++n){auto token=worldframe::SnapshotPool::acquire(pool);worldframe::Page page;page.address=0x10000;page.full=1;page.bytes.fill(uint8_t(n+offset));worldframe::Key k;k.serial=n+offset;
   if(!token->import(k,{},&page,1)){++failures;continue;}std::weak_ptr<const worldframe::Snapshot>weak=token;auto retained=weak.lock();token.reset();
   uint8_t value=0;if(!retained||!retained->read(0x10000,&value,1)||value!=uint8_t(n+offset))++failures;retained.reset();if(!weak.expired())++failures;
  }++done;
 };
 std::thread a(worker,0),b(worker,300),c(worker,600),d(worker,900);a.join();b.join();c.join();d.join();CHECK(done==4&&!failures);
 std::puts("{\"case\":\"concurrent_acquire_last_release\",\"publications\":1200,\"passed\":true}");
}
}
int main(){reuse::paletteIdentity();reuse::weakIdentity();reuse::warmAcquires();reuse::saturatedAndRetired();reuse::failures();reuse::concurrentReturns();fixture::mappingInterleavings();
 std::printf("{\"case\":\"seqlock_interleavings\",\"passed\":true,\"checks\":%zu}\n",fixture::checks);
}

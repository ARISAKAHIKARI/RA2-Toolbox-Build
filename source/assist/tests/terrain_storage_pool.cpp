// Real Sampler/SharedSceneProvider with the existing sealed-page fixture.
#define main existingSharedSceneTests
#include "shared_scene_sampling.cpp"
#undef main
#include <atomic>
#include <iostream>
#include <tuple>
using namespace external;
struct PoolRead {
 Reader*r;bool operator()(uint32_t at,void*out,size_t n)const{return r->read(at,out,n);}
 void prefetch(const std::vector<uint32_t>&,size_t)const{}
};
static auto cellKey(const radar::CellArt&c){return std::tie(c.cell,c.type,c.overlayType,c.overlaySourceType,c.smudgeType,c.rubbleType,c.flags,c.pixelFX,c.land,c.smudgeData,c.overlay,c.sub,c.level,c.overlayFrame,c.sourceHidden,c.sourceColor);}
static bool equal(const radar::Terrain&a,const radar::Terrain&b){
 if(!a.geo.same(b.geo)||a.reveal!=b.reveal||a.wholeWorld!=b.wholeWorld||a.colors!=b.colors||a.baseColors!=b.baseColors||a.shade!=b.shade||a.art.size()!=b.art.size())return false;
 for(size_t i=0;i<a.art.size();++i)if(cellKey(a.art[i])!=cellKey(b.art[i]))return false;return true;
}
static uint64_t digest(const radar::Terrain&t){uint64_t h=1469598103934665603ull;auto fold=[&](uint32_t x){h=(h^x)*1099511628211ull;};
 for(auto c:t.colors)fold(c);for(auto c:t.baseColors)fold(c);for(auto c:t.shade){fold(uint8_t(c.shroud));fold(uint8_t(c.fog));}for(const auto&c:t.art){fold(c.cell);fold(c.type);fold(c.land);fold(c.level);fold(c.sourceColor);}return h;
}
int main(){
 setup();unsigned serial=1;Reader reader(token(serial,10,100));radar::Sampler<PoolRead>sampler(PoolRead{&reader});sampler.wholeWorld(true);
 auto freshToken=[&](uint32_t omitted=0){reader.current=token(++serial,9+serial,100+serial*16,omitted);reader.complete=true;reader.reads=0;};
 auto capture=[&](bool real=true){freshToken();auto revision=sampler.revision();assert(sampler.step(1,16384,real,true,false,true)&&reader.complete&&sampler.revision()!=revision);return sampler.terrainFrame(100+serial*16).terrain;};
 auto oracle=[&](bool real=true){radar::Sampler<PoolRead>fresh(PoolRead{&reader});fresh.wholeWorld(true);reader.complete=true;assert(fresh.step(1,16384,real,true,false,true)&&reader.complete);return fresh.terrainFrame(0).terrain;};
 auto first=capture();const auto*address=first.get();const auto original=digest(*first);std::weak_ptr<const radar::Terrain>old=first;
 put(Cell+0xEC,4);auto second=capture();assert(digest(*first)==original&&second!=first);first.reset();assert(old.expired());
 // Recycle the first raw allocation, but never its expired control block.
 constexpr size_t nextIndex=Edge+1;constexpr uint32_t nextCell=Cell+0x400;
 std::memcpy(memory.data()+nextCell,memory.data()+Cell,0x148);put(nextCell+0x24,int16_t(33));put(nextCell+0x26,int16_t(33));put(Cells+Edge*4,uint32_t(0));put(Cells+nextIndex*4,nextCell);
 auto third=capture();assert(third.get()==address&&old.expired()&&!old.lock());assert(old.owner_before(third)||third.owner_before(old));
 assert(third->colors[Edge]==0&&third->baseColors[Edge]==0&&third->art[Edge].cell==0&&third->art[Edge].overlay==-1&&third->shade[Edge].shroud==127);
 assert(equal(*third,*oracle()));second.reset();third.reset();
 // Every in-flight immutable reader pins its own storage, without a fixed
 // slot limit. Retained hashes survive both new candidates and pool pressure.
 std::vector<std::shared_ptr<const radar::Terrain>>held;std::vector<uint64_t>hashes;
 for(int i=0;i<12;++i){put(nextCell+0xEC,i%12);held.push_back(capture());hashes.push_back(digest(*held.back()));}
 for(size_t i=0;i<held.size();++i)assert(digest(*held[i])==hashes[i]);
 std::atomic<bool>go{false};std::array<std::thread,3>releases;
 for(size_t worker=0;worker<releases.size();++worker){std::vector<std::shared_ptr<const radar::Terrain>>batch;for(size_t i=worker;i<held.size();i+=releases.size())batch.push_back(std::move(held[i]));
  releases[worker]=std::thread([batch=std::move(batch),&go]()mutable{while(!go.load(std::memory_order_acquire))std::this_thread::yield();batch.clear();});}
 go.store(true,std::memory_order_release);for(int i=0;i<4;++i){put(nextCell+0xEC,i);auto next=capture();assert(equal(*next,*oracle()));}for(auto&t:releases)t.join();held.clear();
 // Mode transitions clear the whole prior meaning, including default holes.
 auto diagram=capture(false);assert(diagram->art.empty()&&diagram->baseColors.empty()&&equal(*diagram,*oracle(false)));auto real=capture();assert(real->art.size()==512*512&&equal(*real,*oracle()));
 // Reject a partly written candidate; the last published source stays exact.
 const auto prior=real;const auto before=digest(*prior);put(Cells+Edge*4,Cell);put(Cell+0xEC,8);put(nextCell+0x24,int16_t(90));freshToken();assert(!sampler.step(1,16384,true,true,false,true));assert(sampler.terrainFrame(0).terrain==prior&&digest(*prior)==before);
 put(nextCell+0x24,int16_t(33));auto recovered=capture();assert(equal(*recovered,*oracle())&&digest(*prior)==before);
 freshToken(nextCell&~4095u);assert(!sampler.step(1,16384,true,true,false,true)&&!reader.complete);auto afterMissing=capture();assert(equal(*afterMissing,*oracle()));
 // Multi-step capture retains its writer; it is not returned midway through.
 auto stable=afterMissing;freshToken();auto revision=sampler.revision();assert(sampler.step(1,1,true,true,false,true)&&sampler.revision()==revision&&sampler.terrainFrame(0).terrain==stable);
 assert(sampler.step(1,1,true,true,false,true)&&sampler.revision()!=revision);assert(equal(*sampler.terrainFrame(0).terrain,*oracle()));
 auto retained=sampler.terrainFrame(0).terrain;const auto retainedHash=digest(*retained);sampler.reset();assert(digest(*retained)==retainedHash);auto reset=capture();assert(equal(*reset,*oracle())&&digest(*retained)==retainedHash);
 std::weak_ptr<const radar::Terrain>late;
 {radar::Sampler<PoolRead>temporary(PoolRead{&reader});temporary.wholeWorld(true);assert(temporary.step(1,16384,true,true,false,true));auto value=temporary.terrainFrame(0).terrain;late=value;retained=std::move(value);}
 assert(!late.expired());retained.reset();assert(late.expired());
 std::cout<<"PASS actual terrain storage: raw-address reuse with fresh control block; expired weak owner; held readers/12 publications; concurrent releases; real/diagram defaults; partial and missing-page rejection/recovery; multi-step capture; reset and late-reader lifetime\n";
}

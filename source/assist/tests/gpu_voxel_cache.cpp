#include "../../external_reader/source/gpu_voxel_cache.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external::gpuvoxel;
 PacketCache cache(1024*1024);std::array<uint32_t,256>palette{};palette.fill(0xff112233);
 auto token=std::make_shared<const uint8_t>(0);auto light=std::make_shared<const PacketCache::VplSnapshot>();
 auto key=PacketCache::key(token,light,palette,.25,.5,.1,2,2,false,false,nullptr);auto packet=std::make_shared<Prepared>();packet->triangles.resize(5000);
 cache.insert(key,packet);assert(cache.find(key)==packet);
 auto other=key;other.palette[5]^=1;assert(!cache.find(other));other=key;other.parameters[26]^=1;assert(!cache.find(other));
 other=key;other.geometry=std::make_shared<const uint8_t>(0);assert(!cache.find(other));
 other=key;other.vpl=std::make_shared<const PacketCache::VplSnapshot>();assert(!cache.find(other));
 other=key;other.geometry.reset();cache.insert(other,packet);assert(!cache.find(other));
 // Retaining an old geometry's token makes allocator address reuse unable to
 // give a replacement model the same ownership identity while cached.
 auto old=key.geometry;token.reset();assert(cache.find(key)==packet);key.geometry=std::make_shared<const uint8_t>(0);assert(key.geometry!=old&&!cache.find(key));
 auto alias=key;alias.geometry=std::shared_ptr<const uint8_t>(old.get(),[](const uint8_t*){});assert(alias.geometry.get()==old.get()&&!cache.find(alias));
 for(int n=0;n<100;++n){auto k=key;k.parameters[0]=uint32_t(n);cache.insert(k,packet);assert(cache.bytes()<=1024*1024&&cache.entries()<=256);}
 auto oversized=std::make_shared<Prepared>();oversized->triangles.resize(20000);auto k=key;k.parameters[0]=999;cache.insert(k,oversized);assert(!cache.find(k));
 cache.clear();assert(!cache.entries()&&!cache.bytes()&&!cache.hits()&&!cache.builds());
 std::puts("PASS: immutable GPU packet exact keys, palette/full-pose/light/geometry ownership isolation including same-address control blocks, 100 LRU admissions remain bounded, oversized packet bypass and reset");
}

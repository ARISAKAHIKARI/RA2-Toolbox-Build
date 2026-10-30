#include "../../external_reader/source/gpu_memory_policy.h"
#include "../../shared/source/frame_pool.h"
#include <cassert>
#include <cstdio>
#include <vector>
int main(){
 using external::gpumemory::score;
 assert(score(true,true,true,false,true,false)>score(true,true,true,true,false,true));
 assert(score(true,false,true,true,false,false)>score(true,false,true,false,true,false));
 for(bool read:{false,true})for(bool coherent:{false,true})for(bool cached:{false,true})for(bool local:{false,true})
  assert(score(true,read,false,coherent,cached,local)==-1);
 assert(score(false,false,false,false,false,true)>score(false,false,true,true,true,false));
 struct Frame {std::vector<uint32_t> pixels;};asyncwork::FramePool<Frame> pool;
 auto a=pool.acquire();a->pixels.assign(1024,0x12345678);auto*buffer=a->pixels.data();auto*first=a.get();
 std::shared_ptr<const std::vector<uint32_t>> alias(a,&a->pixels);a.reset();
 auto b=pool.acquire(),c=pool.acquire(),temporary=pool.acquire();assert(b.get()!=first&&c.get()!=first&&temporary.get()!=first);
 assert(alias->front()==0x12345678);alias.reset();
 auto reused=pool.acquire();assert(reused.get()==first&&reused->pixels.data()==buffer&&reused->pixels.size()==1024);
 reused->pixels.assign(1024,0xabcdef01);pool.clear();assert(reused->pixels.front()==0xabcdef01);
 std::puts("PASS cached CPU readback selection, coherent uploads, device-local GPU buffers, immutable alias ownership and reusable output storage; no GPU loaded");
}

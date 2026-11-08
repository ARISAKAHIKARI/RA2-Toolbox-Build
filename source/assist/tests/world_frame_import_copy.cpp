// Compare complete imported wire pages against the frozen pre.1 implementation.
#include "../../shared/source/world_frame.h"
#include BASELINE_WORLD_FRAME_HEADER
#include <cstdio>
#include <cstdlib>

namespace fixture {
size_t checks=0,cases=0;
void require(bool value,int line){++checks;if(!value){std::fprintf(stderr,"FAIL import copy at %d\n",line);std::abort();}}
#define CHECK(value) ::fixture::require(bool(value),__LINE__)
struct Allocation {
 size_t calls=0,failAt=0,live=0;
 static void*allocate(size_t n,void*context)noexcept{auto&self=*static_cast<Allocation*>(context);if(++self.calls==self.failAt)return nullptr;void*p=std::malloc(n);if(p)++self.live;return p;}
 static void release(void*p,void*context)noexcept{if(p)--static_cast<Allocation*>(context)->live;std::free(p);}
};
worldframe::Key key(){worldframe::Key k;k.pid=1000;k.owner=900;k.match=7;k.serial=55;k.frame=101;k.tick=1000;k.generationLow=3;k.connectionLow=4;return k;}
baseline_worldframe::Key oldKey(const worldframe::Key&k){baseline_worldframe::Key old;static_assert(sizeof old==sizeof k,"key layout");std::memcpy(&old,&k,sizeof k);return old;}
std::vector<worldframe::Page> pages(size_t count,int mode){
 std::vector<worldframe::Page>result(count);
 for(size_t i=0;i<count;++i){auto&p=result[i];p.address=0x10000000+uint32_t(i%32)*4096+uint32_t(i/32)*0x08000000;p.full=mode==0||(mode==2&&(i&1));
  for(size_t n=0;n<p.bytes.size();++n)p.bytes[n]=uint8_t(i*23+n*17);
  for(size_t n=0;n<p.valid.size();++n){p.valid[n]=n%3?0x00FF00FF00FF00FFull:0;p.failed[n]=n%5?0xFF000000FF000000ull:0;}
 }return result;
}
void equal(const baseline_worldframe::Snapshot&old,const worldframe::Snapshot&current){
 static_assert(sizeof(baseline_worldframe::Page)==sizeof(worldframe::Page),"page layout");
 CHECK(old.sealed()==current.sealed());CHECK(old.overflow()==current.overflow());CHECK(old.pages.size()==current.pages.size());CHECK(old.pages.capacity()==current.pages.capacity());
 CHECK(!std::memcmp(&old.key,&current.key,sizeof current.key));CHECK(!std::memcmp(&old.counters,&current.counters,sizeof current.counters));
 for(size_t i=0;i<old.pages.size();++i){const auto&a=old.pages[i];const auto&b=current.pages[i];CHECK(!std::memcmp(&a,&b,sizeof b));
  if(!current.sealed())continue;
  for(size_t at=0;at<4096;at+=17){std::array<uint8_t,24>x,y;x.fill(0xEF);y=x;const uint32_t address=b.address+uint32_t(at);
   CHECK(old.read(address,x.data(),x.size())==current.read(address,y.data(),y.size()));CHECK(x==y);CHECK(old.observed(address,x.size())==current.observed(address,y.size()));}
 }
}
bool compare(const std::vector<worldframe::Page>&input,size_t count,bool nullInput=false){
 std::vector<baseline_worldframe::Page>oldInput(input.size());if(!input.empty())std::memcpy(oldInput.data(),input.data(),input.size()*sizeof(worldframe::Page));
 baseline_worldframe::Snapshot old;worldframe::Snapshot current;worldframe::Counters stats{8,4096,12,1,2,3,4,0};baseline_worldframe::Counters oldStats;std::memcpy(&oldStats,&stats,sizeof stats);
 const auto k=key();const bool a=old.import(oldKey(k),oldStats,nullInput?nullptr:oldInput.data(),count),b=current.import(k,stats,nullInput?nullptr:input.data(),count);CHECK(a==b);equal(old,current);++cases;return b;
}
void contents(){
 for(int mode:{0,1,2})for(size_t count:{0u,1u,3u,96u})CHECK(compare(pages(count,mode),count));
 auto input=pages(3,2);worldframe::Snapshot retained;CHECK(retained.import(key(),{},input.data(),input.size()));const auto original=input;
 for(auto&p:input){p.address=0;p.full=0;p.valid.fill(0);p.failed.fill(0);p.bytes.fill(0);}
 for(size_t i=0;i<original.size();++i)CHECK(!std::memcmp(&retained.pages[i],&original[i],sizeof(worldframe::Page)));
 for(uint32_t invalid:{0u,0xF000u,0x10001u,0x80000000u}){auto bad=original;bad[1].address=invalid;CHECK(!compare(bad,bad.size()));}
 {auto bad=original;bad[2].address=bad[0].address;CHECK(!compare(bad,bad.size()));}
 {auto bad=original;bad[1].full=2;CHECK(!compare(bad,bad.size()));}
 CHECK(!compare(original,1,true));CHECK(compare(original,0,true));CHECK(!compare(original,size_t(worldframe::MaxPages)+1));
}
void allocation(){
 const auto input=pages(3,2);std::vector<baseline_worldframe::Page>oldInput(input.size());std::memcpy(oldInput.data(),input.data(),input.size()*sizeof(worldframe::Page));
 Allocation a,b;{
  baseline_worldframe::Snapshot old({Allocation::allocate,Allocation::release,&a});worldframe::Snapshot current({Allocation::allocate,Allocation::release,&b});
  a.failAt=b.failAt=1;CHECK(!old.import(oldKey(key()),{},oldInput.data(),2));CHECK(!current.import(key(),{},input.data(),2));equal(old,current);CHECK(a.calls==b.calls&&a.live==b.live);
  a.failAt=b.failAt=0;CHECK(old.import(oldKey(key()),{},oldInput.data(),2));CHECK(current.import(key(),{},input.data(),2));equal(old,current);
  auto*oldStorage=old.pages.data();auto*newStorage=current.pages.data();a.failAt=a.calls+1;b.failAt=b.calls+1;
  CHECK(!old.import(oldKey(key()),{},oldInput.data(),3));CHECK(!current.import(key(),{},input.data(),3));equal(old,current);CHECK(old.pages.data()==oldStorage&&current.pages.data()==newStorage);CHECK(a.calls==b.calls&&a.live==b.live);
  a.failAt=b.failAt=0;CHECK(old.import(oldKey(key()),{},oldInput.data(),3));CHECK(current.import(key(),{},input.data(),3));equal(old,current);CHECK(a.calls==b.calls&&a.live==b.live);
 }CHECK(a.live==0&&b.live==0);
}
void captureStillInitialized(){
 worldframe::Snapshot current;baseline_worldframe::Snapshot old;current.reset(key());old.reset(oldKey(key()));auto denied=[](uint32_t,void*,size_t){return false;};
 CHECK(!old.prefetch(denied,0x20010,16));CHECK(!current.prefetch(denied,0x20010,16));CHECK(old.seal()&&current.seal());equal(old,current);
 for(uint8_t byte:current.pages[0].bytes)CHECK(byte==0);
}
}
int main(){fixture::contents();fixture::allocation();fixture::captureStillInitialized();std::printf("PASS import copy: %zu cases / %zu checks; complete bytes, full/valid/failed, immutable copy, malformed input, growth failure/recovery, initialized capture\n",fixture::cases,fixture::checks);}

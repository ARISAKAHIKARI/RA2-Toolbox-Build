#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef IMMUTABLE_ATLAS_HEADER
#define IMMUTABLE_ATLAS_HEADER "../../external_reader/source/immutable_atlas_plan.h"
#endif
#include IMMUTABLE_ATLAS_HEADER
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
using namespace external::immutableatlas;
using Owner=std::shared_ptr<std::vector<uint8_t>>;
static Owner owner(size_t bytes,unsigned seed){auto p=std::make_shared<std::vector<uint8_t>>(bytes);for(size_t n=0;n<bytes;++n)(*p)[n]=uint8_t(seed*37+n*11);return p;}
static Source source(const Owner&p,uint64_t signature=0,bool generated=false){return {p,p->data(),p->size(),generated,signature};}
static void upload(const std::vector<Source>&input,const Plan&p,std::vector<uint8_t>&atlas){
 for(size_t i:p.dirty){const auto&s=input[i];const size_t at=size_t(p.offsets[i])*4,bytes=(s.bytes+3)/4*4;assert(at+bytes<=atlas.size());
  // A generated source is written by its GPU stage rather than an upload;
  // this deterministic stand-in verifies the same address/lifetime contract.
  if(s.generated)for(size_t n=0;n<s.bytes;++n)atlas[at+n]=uint8_t(static_cast<const uint8_t*>(s.data)[n]^uint8_t(s.signature));
  else std::memcpy(atlas.data()+at,s.data,s.bytes);
  std::fill(atlas.begin()+at+s.bytes,atlas.begin()+at+bytes,0);
 }
}
static void equal(const std::vector<Source>&input,const Plan&p,const std::vector<uint8_t>&atlas){
 assert(p.offsets.size()==input.size()&&p.dirtyFlags.size()==input.size());uint64_t bytes=0;
 for(size_t i=0;i<input.size();++i){const auto&s=input[i];const size_t at=size_t(p.offsets[i])*4;assert(at+(s.bytes+3)/4*4<=atlas.size());
  for(size_t n=0;n<s.bytes;++n)assert(atlas[at+n]==uint8_t(static_cast<const uint8_t*>(s.data)[n]^(s.generated?uint8_t(s.signature):0)));
  assert(p.changed(i)==(std::find(p.dirty.begin(),p.dirty.end(),i)!=p.dirty.end()));
 }
 for(size_t i:p.dirty)if(!input[i].generated)bytes+=(input[i].bytes+3)/4*4;assert(bytes==p.bytes);
 auto retained=p.retained;std::sort(retained.begin(),retained.end(),[](const auto&a,const auto&b){return a.offset<b.offset;});uint64_t end=0;
 for(const auto&r:retained){assert(r.offset>=end);end=uint64_t(r.offset)+r.words;assert(end*4<=atlas.size());}
}
static void pressure(){
 constexpr size_t Count=8192,Bytes=1024,Budget=Count*Bytes;
 std::vector<Owner>owners;std::vector<Source>all;for(size_t n=0;n<Count;++n){owners.push_back(owner(Bytes,unsigned(n)));all.push_back(source(owners.back()));}
 Plan initial;assert(build(all,{},initial,Budget));std::vector<uint8_t>atlas(Budget);upload(all,initial,atlas);equal(all,initial,atlas);
 auto next=owner(Bytes,Count);std::vector<Source>input{source(next)};for(size_t n=0;n<Count;n+=2)input.push_back(all[n]);input.push_back(input[0]);
 Plan p;auto start=std::chrono::steady_clock::now();assert(build(input,initial.retained,p,Budget));double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();upload(input,p,atlas);equal(input,p,atlas);
#ifdef EXPECT_PRESSURE_RECLAIM
 assert(p.dirty.size()==1&&p.dirty[0]==0&&p.bytes==Bytes);
 for(size_t n=1;n+1<input.size();++n)assert(p.offsets[n]==initial.offsets[(n-1)*2]);
 assert(p.offsets.front()==p.offsets.back()&&p.changed(0)&&!p.changed(input.size()-1));
#endif
 std::cout<<"{\"case\":\"full_live_owner_atlas\",\"resident_sources\":"<<Count<<",\"active_unique\":"<<Count/2+1<<",\"budget_bytes\":"<<Budget<<",\"dirty_sources\":"<<p.dirty.size()<<",\"uploaded_bytes\":"<<p.bytes<<",\"planner_ms\":"<<ms<<",\"all_source_bytes_equal\":true}\n";
 // With room available the cache must retain inactive bindings, so a camera
 // returning to them does not lose residency merely by hiding them once.
 Plan roomy;assert(build(input,initial.retained,roomy,2*Budget));assert(roomy.retained.size()==initial.retained.size()+1&&roomy.dirty.size()==1);
}
static void fragmented(){
 std::vector<Owner>o;std::vector<Source>all;for(unsigned n=0;n<4;++n){o.push_back(owner(64,n));all.push_back(source(o.back()));}
 Plan warm;assert(build(all,{},warm,256));auto large=owner(128,9);std::vector<Source>input{source(large),all[0],all[2]};Plan p;assert(build(input,warm.retained,p,256));
 // Reclaiming inactive slots still leaves two 64B holes. The 128B item needs
 // a genuine repack; every active item then has to be written at its new slot.
 assert(p.dirty.size()==3&&p.bytes==256);std::vector<uint8_t>atlas(256);upload(input,p,atlas);equal(input,p,atlas);
 // A new generated revision aliases the old owner, but must not reuse bytes.
 auto generated=owner(64,11);input={source(generated,1,true),all[0],all[2]};assert(build(input,{},warm,256));upload(input,warm,atlas);
 input[0]=source(generated,2,true);assert(build(input,warm.retained,p,256));upload(input,p,atlas);equal(input,p,atlas);assert(p.changed(0)&&!p.changed(1)&&!p.changed(2)&&p.bytes==0);
}
static void streams(){
 constexpr size_t Budget=64*1024;std::vector<Owner>owners;for(unsigned n=0;n<512;++n)owners.push_back(owner(128+n%16,n));
 std::vector<Resident>resident;std::vector<uint8_t>atlas(Budget);std::mt19937 random(121108);size_t checks=0;
 for(unsigned frame=0;frame<240;++frame){std::vector<Source>input;for(unsigned n=0;n<220;++n){unsigned id=random()%owners.size();input.push_back(source(owners[id],frame%3,(id%5)==0));}
  Plan p;assert(build(input,resident,p,Budget));upload(input,p,atlas);equal(input,p,atlas);resident=std::move(p.retained);checks+=input.size();
 }
 std::cout<<"{\"case\":\"randomized_pressure_stream\",\"frames\":240,\"source_checks\":"<<checks<<",\"generated_revisions_and_aliases\":true,\"all_source_bytes_equal\":true}\n";
}
int main(){pressure();fragmented();streams();}

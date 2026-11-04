#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
namespace external {namespace immutableatlas {
struct Source {std::shared_ptr<const void> owner;const void* data=nullptr;size_t bytes=0;bool generated=false;uint64_t signature=0;};
struct Resident {std::weak_ptr<const void> owner;const void* address=nullptr;uint32_t offset=0,words=0;uint64_t signature=0;};
struct Plan {
 std::vector<Resident> retained;std::vector<uint32_t> offsets;std::vector<size_t> dirty;std::vector<uint8_t> dirtyFlags;uint64_t bytes=0;
 bool changed(size_t n)const{return n<dirtyFlags.size()&&dirtyFlags[n]!=0;}
};
inline bool identical(const Resident&r,const Source&s){return r.address==s.owner.get()&&!r.owner.owner_before(s.owner)&&!s.owner.owner_before(r.owner)&&r.words==(s.bytes+3)/4&&r.signature==s.signature;}
// Address indexes are accelerators only: control-block identity, word length
// and generation signature are always checked before reusing GPU storage.
inline bool build(const std::vector<Source>&input,const std::vector<Resident>&resident,Plan&out,uint64_t maximum){
 out={};std::vector<Resident> active;std::vector<size_t> indices,original;
 std::unordered_multimap<const void*,size_t> activeIndex;activeIndex.reserve(input.size());
 uint64_t total=0;indices.reserve(input.size());
 for(size_t i=0;i<input.size();++i){const auto&s=input[i];if(!s.owner||(!s.data&&!s.generated)||!s.bytes||s.bytes>maximum)return false;
  size_t n=active.size();auto range=activeIndex.equal_range(s.owner.get());
  for(auto it=range.first;it!=range.second;++it)if(identical(active[it->second],s)){n=it->second;break;}
  if(n==active.size()){Resident r{s.owner,s.owner.get(),0,uint32_t((s.bytes+3)/4),s.signature};active.push_back(r);original.push_back(i);activeIndex.emplace(s.owner.get(),n);total+=r.words;}
  indices.push_back(n);
 }
 if(total>maximum/4)return false;
 for(const auto&r:resident)if(!r.owner.expired())out.retained.push_back(r);
 std::sort(out.retained.begin(),out.retained.end(),[](const Resident&a,const Resident&b){return a.offset<b.offset;});
 std::unordered_multimap<const void*,size_t> retainedIndex;retainedIndex.reserve(out.retained.size());
 struct Gap{uint64_t offset,end;};std::vector<Gap> gaps;uint64_t cursor=0;
 for(size_t n=0;n<out.retained.size();++n){const auto&r=out.retained[n];if(r.offset<cursor||uint64_t(r.offset)+r.words>maximum/4)return false;
  if(cursor<r.offset)gaps.push_back({cursor,r.offset});cursor=uint64_t(r.offset)+r.words;retainedIndex.emplace(r.address,n);}
 if(cursor<maximum/4)gaps.push_back({cursor,maximum/4});
 std::vector<bool> dirty(active.size(),false);bool repack=false;
 for(size_t n=0;n<active.size();++n){const auto&s=input[original[n]];auto range=retainedIndex.equal_range(s.owner.get());bool found=false;
  for(auto it=range.first;it!=range.second;++it)if(identical(out.retained[it->second],s)){active[n].offset=out.retained[it->second].offset;found=true;break;}
  if(found)continue;
  auto gap=std::find_if(gaps.begin(),gaps.end(),[&](const Gap&g){return g.end-g.offset>=active[n].words;});
  if(gap==gaps.end()){repack=true;break;}active[n].offset=uint32_t(gap->offset);gap->offset+=active[n].words;
  if(gap->offset==gap->end)gaps.erase(gap);out.retained.push_back(active[n]);dirty[n]=true;
 }
 if(repack){out.retained=active;uint32_t offset=0;for(size_t n=0;n<active.size();++n){active[n].offset=out.retained[n].offset=offset;offset+=active[n].words;dirty[n]=true;}}
 out.offsets.reserve(input.size());out.dirtyFlags.assign(input.size(),0);std::vector<bool> seen(active.size(),false);
 for(size_t i=0;i<input.size();++i){const size_t n=indices[i];out.offsets.push_back(active[n].offset);if(dirty[n]&&!seen[n]){seen[n]=true;out.dirty.push_back(i);out.dirtyFlags[i]=1;if(!input[i].generated)out.bytes+=uint64_t(active[n].words)*4;}}
 return true;
}
}}

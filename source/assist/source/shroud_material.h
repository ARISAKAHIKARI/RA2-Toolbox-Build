#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>
namespace assist {namespace shroudmaterials {
// Native DrawShroud/GetData consume the raw alpha plane, not RGBA artwork.
// Keep the engine's allocations, SHP references and headers in place. Only
// verified alpha bytes are replaced, with originals retained for restoration.
class Controller {
 struct Plane {uint32_t address=0;std::vector<uint8_t> original,replacement;};
 struct Source {uint32_t base=0;std::vector<uint8_t> header;std::vector<Plane> planes;};
 std::vector<Source> owned_;std::array<uint32_t,3> refs_{},resolved_{};unsigned mode_=0;uint32_t checked_=0;int status_=0;bool changed_=false;
 static uint16_t u16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
 static uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
 static bool range(uint32_t p,size_t n){return p>=0x10000&&n&&uint64_t(p)+n<=0x80000000ull;}
 template<class Read>static bool identity(const Source&s,Read read){std::vector<uint8_t> bytes(s.header.size());return read(s.base,bytes.data(),bytes.size())&&bytes==s.header;}
 // 0: lazy resource not loaded; -1: unsupported/corrupt; 1: captured.
 template<class Read>static int capture(uint32_t ref,Read read,Source&out){
  if(!ref)return 0;
  std::array<uint8_t,8> head{};if(!range(ref,8)||!read(ref,head.data(),head.size()))return -1;
  uint32_t base=ref;if(u16(head.data())==0xffff){uint8_t loaded=0;if(!read(ref+16,&loaded,1)||!loaded)return 0;
   if(!read(ref+12,&base,4)||!base)return 0;
   if(!range(base,8)||!read(base,head.data(),head.size()))return -1;}
  const unsigned frames=u16(head.data()+6);if(u16(head.data())||u16(head.data()+2)!=60||u16(head.data()+4)!=30||frames<47||frames>128)return -1;
  Source source;source.base=base;source.header.resize(8+frames*24);if(!read(base,source.header.data(),source.header.size()))return -1;
  if(std::memcmp(source.header.data(),head.data(),8))return -1;
  size_t bytes=0;std::set<std::pair<uint32_t,size_t>> seen;
  for(unsigned f=0;f<47;++f){const auto*h=source.header.data()+8+f*24;unsigned x=u16(h),y=u16(h+2),w=u16(h+4),height=u16(h+6);uint32_t offset=u32(h+20);
   if(!w||!height||!offset)continue;
   // Native alpha leaves ignore SHP compression flags. Reject compressed art
   // rather than treating RLE lengths/control bytes as material alpha.
   if(x+w>60||y+height>30||(u32(h+8)&2)||offset<source.header.size()||offset>2*1024*1024||!range(base+offset,size_t(w)*height))return -1;
   const size_t count=size_t(w)*height;if((bytes+=count)>128*1024)return -1;
   const uint32_t address=base+offset;
   if(!seen.insert({address,count}).second)continue;
   for(const auto&existing:source.planes)
    if(uint64_t(address)<uint64_t(existing.address)+existing.original.size()&&uint64_t(existing.address)<uint64_t(address)+count)return -1;
   Plane plane;plane.address=base+offset;plane.original.resize(count);if(!read(plane.address,plane.original.data(),count))return -1;
   source.planes.push_back(std::move(plane));
  }
  if(source.planes.empty()||!identity(source,read))return -1;
  out=std::move(source);return 1;
 }
 template<class Read>bool intact(Read read)const{
  for(const auto&s:owned_){if(!identity(s,read))return false;for(const auto&p:s.planes){std::vector<uint8_t> bytes(p.replacement.size());if(!read(p.address,bytes.data(),bytes.size())||bytes!=p.replacement)return false;}}
  return true;
 }
public:
 bool active()const{return !owned_.empty();}bool changed()const{return changed_;}int status()const{return status_;}
 template<class Read,class Write>bool restore(Read read,Write write){
  changed_=false;bool ok=true;std::vector<Source> retained;
  for(auto&s:owned_){if(!identity(s,read))continue;bool restored=true;
   for(const auto&p:s.planes){std::vector<uint8_t> bytes(p.original.size());
    if(!read(p.address,bytes.data(),bytes.size())){restored=false;continue;}
    if(bytes==p.original)continue;
    bool ours=true;for(size_t i=0;i<bytes.size();++i)if(bytes[i]!=p.original[i]&&bytes[i]!=p.replacement[i]){ours=false;break;}
    // Mixed original/replacement bytes also cover a partial failed write.
    if(!ours||!write(p.address,p.original.data(),p.original.size()))restored=false;else changed_=true;
   }
   if(!restored){ok=false;retained.push_back(std::move(s));}
  }
  owned_=std::move(retained);if(ok){mode_=0;status_=0;refs_={};resolved_={};checked_=0;}else status_=-2;return ok;
 }
 // mode 1 = transparent, 2 = half. No source/loader/ownership change occurs.
 template<class Read,class Write>int update(unsigned mode,uint32_t now,Read read,Write write){
  changed_=false;if(!mode)return restore(read,write)?0:-2;if(mode>2)return -2;
  std::array<uint32_t,3> refs{},resolved{};const uint32_t slots[]={0x89E790,0x89E794,0x89E760};
  for(size_t i=0;i<refs.size();++i){if(!read(slots[i],&refs[i],4))return status_=-2;
   if(refs[i]){std::array<uint8_t,8> head{};if(!range(refs[i],8)||!read(refs[i],head.data(),head.size()))return status_=-2;
    resolved[i]=refs[i];if(u16(head.data())==0xffff){uint8_t loaded=0;if(!read(refs[i]+16,&loaded,1)||!read(refs[i]+12,&resolved[i],4))return status_=-2;if(!loaded)resolved[i]=0;}}
  }
  const bool same=mode==mode_&&refs==refs_&&resolved==resolved_;
  if(same&&uint32_t(now-checked_)<(status_==1?1000u:100u)&&status_>0)return status_;
  if(same&&status_==1&&intact(read)){checked_=now;return status_;}
  if(!restore(read,write))return -2;
  bool restored=changed_;
  std::vector<Source> sources;std::set<uint32_t> bases;
  for(auto ref:refs){Source source;int result=capture(ref,read,source);if(result<0){changed_=restored;return status_=-2;}if(result&&bases.insert(source.base).second)sources.push_back(std::move(source));}
  // A corrupt offset must not overwrite another resource's header or planes.
  for(size_t i=0;i<sources.size();++i)for(size_t j=i+1;j<sources.size();++j){
   const auto&a=sources[i];const auto&b=sources[j];
   auto overlaps=[](uint32_t x,size_t xn,uint32_t y,size_t yn){return uint64_t(x)<uint64_t(y)+yn&&uint64_t(y)<uint64_t(x)+xn;};
   if(overlaps(a.base,a.header.size(),b.base,b.header.size())){changed_=restored;return status_=-2;}
   for(const auto&p:a.planes){if(overlaps(p.address,p.original.size(),b.base,b.header.size())){changed_=restored;return status_=-2;}
    for(const auto&q:b.planes)if(overlaps(p.address,p.original.size(),q.address,q.original.size())){changed_=restored;return status_=-2;}}
   for(const auto&p:b.planes)if(overlaps(p.address,p.original.size(),a.base,a.header.size())){changed_=restored;return status_=-2;}
  }
  for(auto&s:sources)for(auto&p:s.planes){p.replacement=p.original;for(auto&value:p.replacement)if(value<=127)value=mode==1?127:uint8_t((unsigned(value)+128)/2);}
  owned_=std::move(sources);mode_=mode;refs_=refs;resolved_=resolved;checked_=now;
  for(const auto&s:owned_)for(const auto&p:s.planes)if(p.replacement!=p.original&&!write(p.address,p.replacement.data(),p.replacement.size())){restore(read,write);changed_=true;return status_=-2;}
  changed_=restored||!owned_.empty();return status_=owned_.empty()?2:1;
 }
};
}}

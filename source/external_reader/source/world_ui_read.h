#pragma once
#include "game_view.h"
#include "optional_read.h"
#include "resource_read.h"
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <memory>
namespace external {namespace worldui {
template<class Reader>struct WorldRead {
 Reader*reader;
 bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}
 template<class R>static auto nullable(R&r,uint32_t p,void*out,size_t n,int)->decltype(r.readOptional(p,out,n)){return r.readOptional(p,out,n);}
 template<class R>static bool nullable(R&r,uint32_t p,void*out,size_t n,long){return r.read(p,out,n);}
 bool optional(uint32_t p,void*out,size_t n)const{return nullable(*reader,p,out,n,0);}
};
// Explicit UI fields over immutable world bytes. This is not a read-failure
// fallback: only selection, hover and local presentation controls can use the
// live reader. Actor identity/health/location/orders and all other data stay
// sealed. Existing before/after UI checks therefore remain real live checks.
template<class World,class Live>class Read {
 World world_;Live live_;std::array<uint32_t,6>selected_{};
 std::vector<uint32_t>uiBytes_,selectedPointers_;std::map<uint32_t,uint32_t>uiOwners_;bool ready_=false;
 struct Checks {bool good=true;std::map<std::pair<uint32_t,size_t>,std::vector<uint8_t>>bytes;};
 std::shared_ptr<Checks>checks_=std::make_shared<Checks>();
 bool input(uint32_t p,void*out,size_t n)const{
  if(!checks_->good||!live_(p,out,n))return checks_->good=false;
  auto key=std::make_pair(p,n);auto found=checks_->bytes.find(key);
  if(found!=checks_->bytes.end())return checks_->good=std::memcmp(found->second.data(),out,n)==0;
  const auto*bytes=static_cast<const uint8_t*>(out);checks_->bytes.emplace(key,std::vector<uint8_t>(bytes,bytes+n));return true;
 }
 bool flag(uint32_t p,void*out)const{
  const uint32_t object=uiOwners_.at(p);
  if(p==object+0x83){*static_cast<uint8_t*>(out)=std::binary_search(selectedPointers_.begin(),selectedPointers_.end(),object);return true;}
  if(!input(p,out,1))return false;if(!*static_cast<uint8_t*>(out))return true;
  uint32_t sampled=0,current=0;
  return world_(object+0x10,&sampled,4)&&input(object+0x10,&current,4)&&sampled&&sampled==current;
 }
 static bool within(uint32_t p,size_t n,uint32_t base,size_t size){return p>=base&&uint64_t(p)+n<=uint64_t(base)+size;}
 bool ui(uint32_t p,size_t n)const{
  return within(p,n,0xA8ECB8,24)||(selected_[4]&&within(p,n,selected_[1],size_t(selected_[4])*4))||
   within(p,n,0x843108,1)||within(p,n,0xB0EA80,12)||within(p,n,0xAC4CF4,1)||
   within(p,n,0xAC4CCC,4)||within(p,n,0xAC4BF0,4);
 }
public:
 Read(World world,Live live):world_(world),live_(live){}
 bool beginPointers(std::vector<uint32_t>objects){
  ready_=false;uiBytes_.clear();uiOwners_.clear();selectedPointers_.clear();checks_=std::make_shared<Checks>();
  if(!input(0xA8ECB8,selected_.data(),sizeof selected_)||selected_[2]>8192||selected_[4]>selected_[2]||selected_[4]>8192||
    (selected_[4]&&(selected_[1]<0x10000||uint64_t(selected_[1])+selected_[4]*4>0x80000000ull)))return false;
  std::vector<uint32_t>selected(selected_[4]);
  if(selected_[4]&&!input(selected_[1],selected.data(),selected.size()*4))return false;
  selectedPointers_=selected;std::sort(selectedPointers_.begin(),selectedPointers_.end());
  if(std::adjacent_find(selectedPointers_.begin(),selectedPointers_.end())!=selectedPointers_.end())return false;
  for(auto p:selected){uint32_t sampled=0,current=0;if(p<0x10000||!world_(p+0x10,&sampled,4)||!input(p+0x10,&current,4)||!sampled||sampled!=current)return false;}
  objects.insert(objects.end(),selected.begin(),selected.end());
  for(auto p:objects)if(p>=0x10000&&p<=0x7ffff800u)for(auto offset:{0x83u,0x431u}){uiBytes_.push_back(p+offset);uiOwners_[p+offset]=p;}
  std::sort(uiBytes_.begin(),uiBytes_.end());uiBytes_.erase(std::unique(uiBytes_.begin(),uiBytes_.end()),uiBytes_.end());ready_=true;return true;
 }
 // Production callers provide the already projected scene membership. This
 // keeps display-rate input indexing proportional to active objects, with no
 // new Techno roster walk and no per-actor live read of the selected flag.
 template<class Object>bool begin(const std::vector<Object>&objects){std::vector<uint32_t>pointers;pointers.reserve(objects.size());for(const auto&o:objects)pointers.push_back(o.pointer);return beginPointers(std::move(pointers));}
 bool begin(){GameView game(world_);std::vector<uint32_t>objects;return game.vecChecked(0xA8EC78,objects,32768)&&beginPointers(std::move(objects));}
 bool operator()(uint32_t p,void*out,size_t n)const{
  if(!n)return true;if(!ready_||!out)return false;
  if(ui(p,n))return input(p,out,n);
  if(n==1&&std::binary_search(uiBytes_.begin(),uiBytes_.end(),p))return flag(p,out);
  if(!world_(p,out,n))return false;
  auto at=std::lower_bound(uiBytes_.begin(),uiBytes_.end(),p);
  while(at!=uiBytes_.end()&&uint64_t(*at)<uint64_t(p)+n){if(!flag(*at,static_cast<uint8_t*>(out)+(*at-p)))return false;++at;}
  return true;
 }
 bool stable()const{
  if(!ready_||!checks_->good)return false;std::vector<uint8_t>after;
  for(const auto&entry:checks_->bytes){after.resize(entry.first.second);if(!live_(entry.first.first,after.data(),after.size())||after!=entry.second)return false;}return true;
 }
 bool optional(uint32_t p,void*out,size_t n)const{return optionalread::bytes(world_,p,out,n);}
 bool resource(uint32_t p,void*out,size_t n)const{return live_(p,out,n);}
 bool local(uint32_t p,void*out,size_t n)const{return live_(p,out,n);}
};
}}

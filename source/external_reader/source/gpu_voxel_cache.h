#pragma once
#include "gpu_voxel.h"
#include "render_memory_budget.h"
#include <list>
#include <map>
#include <unordered_map>
namespace external { namespace gpuvoxel {
class PacketCache {
public:
 using Geometry=std::shared_ptr<const uint8_t>;
 using VplSnapshot=std::array<uint8_t,256*256>;
 struct Key {
  Geometry geometry;std::shared_ptr<const VplSnapshot> vpl;
  std::array<uint32_t,256> palette{};
  std::array<uint32_t,33> parameters{};
 };
private:
 struct Entry {Key key;std::shared_ptr<const Prepared> packet;uint64_t used=0;size_t bytes=0;};
 std::list<Entry> entries_;using Iterator=std::list<Entry>::iterator;
 std::unordered_multimap<size_t,Iterator>index_;
 std::map<std::shared_ptr<const VplSnapshot>,size_t,std::owner_less<std::shared_ptr<const VplSnapshot>>>tables_;
 size_t bytes_=0,budget_;uint64_t serial_=0,hits_=0,builds_=0;
 static constexpr size_t TableBytes=sizeof(VplSnapshot)+8*sizeof(void*);
 static size_t hash(const Key&k){size_t h=reinterpret_cast<uintptr_t>(k.geometry.get())^reinterpret_cast<uintptr_t>(k.vpl.get());for(auto p:k.parameters)h=(h^p)*size_t(1099511628211ull);return h;}
 template<class T>static bool owner(const std::shared_ptr<T>&a,const std::shared_ptr<T>&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
 static bool same(const Key&a,const Key&b){return owner(a.geometry,b.geometry)&&owner(a.vpl,b.vpl)&&a.parameters==b.parameters&&a.palette==b.palette;}
public:
 explicit PacketCache(size_t budget=rendermemory::PacketBytes):budget_(std::min<size_t>(budget,rendermemory::PacketBytes)){}
 void clear(){entries_.clear();index_.clear();tables_.clear();bytes_=0;serial_=hits_=builds_=0;}
 size_t bytes()const{return bytes_;}size_t entries()const{return entries_.size();}uint64_t hits()const{return hits_;}uint64_t builds()const{return builds_;}
 std::shared_ptr<const Prepared> find(const Key&key){
  if(!key.geometry)return {}; // manually mutable geometry has no capture identity
  auto range=index_.equal_range(hash(key));for(auto p=range.first;p!=range.second;++p){auto e=p->second;
   if(same(e->key,key)){e->used=++serial_;++hits_;entries_.splice(entries_.end(),entries_,e);return e->packet;}}return {};
 }
 void insert(Key key,std::shared_ptr<const Prepared> packet){
  ++builds_;if(!key.geometry||!packet)return;
  const size_t cost=sizeof(Entry)+8*sizeof(void*)+sizeof(Prepared)+packet->triangles.capacity()*sizeof(Triangle);
  if(cost>budget_)return;
  auto tableCost=[&]{return key.vpl&&!tables_.count(key.vpl)?TableBytes:size_t(0);};
  while(!entries_.empty()&&bytes_+cost+tableCost()>budget_){auto old=entries_.begin();auto range=index_.equal_range(hash(old->key));
   for(auto p=range.first;p!=range.second;++p)if(p->second==old){index_.erase(p);break;}
   if(old->key.vpl){auto table=tables_.find(old->key.vpl);if(!--table->second){bytes_-=TableBytes;tables_.erase(table);}}
   bytes_-=old->bytes;entries_.erase(old);
  }
  if(bytes_+cost+tableCost()>budget_)return;bytes_+=cost+tableCost();if(key.vpl)++tables_[key.vpl];
  const auto bucket=hash(key);entries_.push_back({std::move(key),std::move(packet),++serial_,cost});index_.emplace(bucket,std::prev(entries_.end()));
 }
 static Key key(Geometry geometry,std::shared_ptr<const VplSnapshot>vpl,const std::array<uint32_t,256>&palette,double angle,double turret,double pitch,float offset,int scale,bool projectile,bool shadow,const voxelposture::State*posture,int bodyFrame=0,int turretFrame=0,double gunPitch=0,std::array<float,2>recoil={}){
  Key out;out.geometry=std::move(geometry);out.vpl=std::move(vpl);out.palette=palette;size_t next=0;
  auto number=[&](const auto&value){static_assert(sizeof(value)==4||sizeof(value)==8,"packed GPU pose scalar");std::memcpy(out.parameters.data()+next,&value,sizeof value);next+=sizeof(value)/4;};
  number(angle);number(turret);number(pitch);number(offset);number(uint32_t(scale));number(uint32_t(255));number(uint32_t(projectile)|(uint32_t(shadow)<<1)|(posture?4u:0u));
  const auto state=posture?*posture:voxelposture::State{};
  for(float value:state.ramp)number(value);for(float value:{state.sideways,state.forwards,state.scaleX,state.scaleY})number(value);number(uint32_t(state.active)|(uint32_t(state.available)<<1));
  number(uint32_t(bodyFrame));number(uint32_t(turretFrame));number(gunPitch);number(recoil[0]);number(recoil[1]);return out;
 }
};
}}

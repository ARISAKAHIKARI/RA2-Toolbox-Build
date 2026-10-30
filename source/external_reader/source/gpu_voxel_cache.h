#pragma once
#include "gpu_voxel.h"
namespace external { namespace gpuvoxel {
class PacketCache {
public:
 using Geometry=std::shared_ptr<const uint8_t>;
 using VplSnapshot=std::array<uint8_t,256*256>;
 struct Key {
  Geometry geometry;std::shared_ptr<const VplSnapshot> vpl;
  std::array<uint32_t,256> palette{};
  std::array<uint32_t,29> parameters{};
 };
private:
 struct Entry {Key key;std::shared_ptr<const Prepared> packet;uint64_t used=0;size_t bytes=0;};
 std::vector<Entry> entries_;size_t bytes_=0,budget_;uint64_t serial_=0,hits_=0,builds_=0;
 template<class T>static bool owner(const std::shared_ptr<T>&a,const std::shared_ptr<T>&b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
 static bool same(const Key&a,const Key&b){return owner(a.geometry,b.geometry)&&owner(a.vpl,b.vpl)&&a.parameters==b.parameters&&a.palette==b.palette;}
public:
 explicit PacketCache(size_t budget=64*1024*1024):budget_(std::min<size_t>(budget,64*1024*1024)){}
 void clear(){entries_.clear();bytes_=0;serial_=hits_=builds_=0;}
 size_t bytes()const{return bytes_;}size_t entries()const{return entries_.size();}uint64_t hits()const{return hits_;}uint64_t builds()const{return builds_;}
 std::shared_ptr<const Prepared> find(const Key&key){
  if(!key.geometry)return {}; // manually mutable geometry has no capture identity
  for(auto&e:entries_)if(same(e.key,key)){e.used=++serial_;++hits_;return e.packet;}return {};
 }
 void insert(Key key,std::shared_ptr<const Prepared> packet){
  ++builds_;if(!key.geometry||!packet)return;
  // Conservatively account a complete semantic VPL snapshot for each entry,
  // even though entries sharing the same table share its allocation.
  const size_t cost=sizeof(Entry)+sizeof(Prepared)+packet->triangles.capacity()*sizeof(Triangle)+(key.vpl?sizeof(VplSnapshot):0);
  if(cost>budget_)return;
  while(!entries_.empty()&&(bytes_+cost>budget_||entries_.size()>=256)){
   auto old=std::min_element(entries_.begin(),entries_.end(),[](const Entry&a,const Entry&b){return a.used<b.used;});bytes_-=old->bytes;entries_.erase(old);
  }
  bytes_+=cost;entries_.push_back({std::move(key),std::move(packet),++serial_,cost});
 }
 static Key key(Geometry geometry,std::shared_ptr<const VplSnapshot>vpl,const std::array<uint32_t,256>&palette,double angle,double turret,double pitch,float offset,int scale,bool projectile,bool shadow,const voxelposture::State*posture,int bodyFrame=0,int turretFrame=0){
  Key out;out.geometry=std::move(geometry);out.vpl=std::move(vpl);out.palette=palette;size_t next=0;
  auto number=[&](const auto&value){static_assert(sizeof(value)==4||sizeof(value)==8,"packed GPU pose scalar");std::memcpy(out.parameters.data()+next,&value,sizeof value);next+=sizeof(value)/4;};
  number(angle);number(turret);number(pitch);number(offset);number(uint32_t(scale));number(uint32_t(255));number(uint32_t(projectile)|(uint32_t(shadow)<<1)|(posture?4u:0u));
  const auto state=posture?*posture:voxelposture::State{};
  for(float value:state.ramp)number(value);for(float value:{state.sideways,state.forwards,state.scaleX,state.scaleY})number(value);number(uint32_t(state.active)|(uint32_t(state.available)<<1));
  number(uint32_t(bodyFrame));number(uint32_t(turretFrame));return out;
 }
};
}}

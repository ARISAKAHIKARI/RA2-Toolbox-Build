#pragma once
#include "voxel_preview.h"
#include "render_memory_budget.h"
#include <list>
#include <map>
#include <set>
namespace external { namespace voxelgeometry {
// Only successful immutable captures have an Identity. HVA transforms depend
// on geometry and animation frames, not facing, palette, VPL, recoil or camera.
// Those later stages still run for every requested presentation.
class Cache {
public:
 using Identity=std::shared_ptr<const uint8_t>;
private:
 struct Key {Identity identity;int body=0,turret=0;};
 struct Less {bool operator()(const Key&a,const Key&b)const{
  if(a.identity.owner_before(b.identity))return true;if(b.identity.owner_before(a.identity))return false;
  if(a.identity.get()!=b.identity.get())return std::less<const uint8_t*>{}(a.identity.get(),b.identity.get());
  return a.body!=b.body?a.body<b.body:a.turret<b.turret;
 }};
 struct Entry {Key key;std::shared_ptr<const voxel::Model>model;size_t bytes=0;uint64_t used=0;};
 struct Trait {Key key;bool indexed=false;uint64_t used=0;};
 std::list<Entry>models_;std::list<Trait>traits_;
 std::map<Key,typename std::list<Entry>::iterator,Less>modelIndex_;
 std::map<Key,typename std::list<Trait>::iterator,Less>traitIndex_;
 size_t bytes_=0,budget_;uint64_t serial_=0,hits_=0,builds_=0,faceChecks_=0,evictions_=0;
 static constexpr size_t TraitBytes=sizeof(Trait)+sizeof(Key)+10*sizeof(void*);
 static size_t cost(const voxel::Model&m){
  size_t n=sizeof(Entry)+sizeof(Key)+sizeof(voxel::Model)+10*sizeof(void*)+m.faces.capacity()*sizeof(voxel::Face);
  // Transformed faces retain immutable Motion owners. Charge them even when
  // another cache entry also retains the same owner; never undercount them.
  std::set<const voxel::Motion*>seen;for(const auto&f:m.faces)if(f.motion&&seen.insert(f.motion.get()).second)
   n+=sizeof(voxel::Motion)+f.motion->changes.capacity()*sizeof(voxelposture::Matrix);
  return n;
 }
 void room(size_t cost){while(bytes_+cost>budget_&&(!models_.empty()||!traits_.empty())){
  if(!models_.empty()&&(traits_.empty()||models_.front().used<traits_.front().used)){
   auto e=models_.begin();bytes_-=e->bytes;modelIndex_.erase(e->key);models_.erase(e);
  }else{auto e=traits_.begin();bytes_-=TraitBytes;traitIndex_.erase(e->key);traits_.erase(e);}++evictions_;
 }}
public:
 explicit Cache(size_t budget=rendermemory::PacketBytes/4):budget_(std::min(budget,rendermemory::PacketBytes/4)){}
 void clear(){models_.clear();traits_.clear();modelIndex_.clear();traitIndex_.clear();bytes_=0;serial_=hits_=builds_=faceChecks_=evictions_=0;}
 size_t bytes()const{return bytes_;}size_t entries()const{return models_.size();}
 uint64_t hits()const{return hits_;}uint64_t builds()const{return builds_;}uint64_t faceChecks()const{return faceChecks_;}uint64_t evictions()const{return evictions_;}
 bool indexed(const voxel::Model&geometry,const Identity&identity){
  Key key{identity};if(identity){auto found=traitIndex_.find(key);if(found!=traitIndex_.end()){
   auto e=found->second;e->used=++serial_;traits_.splice(traits_.end(),traits_,e);return e->indexed;}}
  bool result=true;for(const auto&face:geometry.faces){++faceChecks_;if(!face.hasNormal&&face.normalIndex<253){result=false;break;}}
  if(identity&&TraitBytes<=budget_){room(TraitBytes);traits_.push_back({std::move(key),result,++serial_});auto e=std::prev(traits_.end());traitIndex_.emplace(e->key,e);bytes_+=TraitBytes;}
  return result;
 }
 std::shared_ptr<const voxel::Model> animated(const voxel::Model&geometry,const Identity&identity,int body,int turret){
  if(body<0||turret<0)return {};
  Key key{identity,body,turret};if(identity){auto found=modelIndex_.find(key);if(found!=modelIndex_.end()){
   auto e=found->second;e->used=++serial_;models_.splice(models_.end(),models_,e);++hits_;return e->model;}}
  auto model=std::make_shared<voxel::Model>();++builds_;if(!voxel::animated(geometry,body,turret,*model))return {};
  if(identity){const size_t n=cost(*model);if(n<=budget_){room(n);models_.push_back({std::move(key),model,n,++serial_});auto e=std::prev(models_.end());modelIndex_.emplace(e->key,e);bytes_+=n;}}
  return model;
 }
};
}}

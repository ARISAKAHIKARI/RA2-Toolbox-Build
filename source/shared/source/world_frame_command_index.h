#pragma once
#include "world_frame.h"
#include "../../assist/source/object_index.h"
#include <memory>
#include <new>
#include <vector>
#include <utility>

namespace worldframe {
// Build an identity index ONLY from a complete sealed command CaptureGraph.
// The public Main/worker handoff swaps owners, not 16K keys or ~640 KiB
// hash tables. This is an identity CANDIDATE, not a native execution grant.
struct CommandRoster {
 struct Member {uint32_t pointer=0,id=0;};
 Key key{};
 std::unique_ptr<assist::ObjectIndex> objects;
 // Canonical ordered Techno membership, projected by the Worker from the
 // same sealed graph as the UID hash. Feature consumers must not reread the
 // live Techno vector to rediscover this list.
 std::vector<Member> members;
 std::vector<uint32_t> globalVehicles;
 uint32_t items=0;
 int count=-1;
 bool global=false;
 void swap(CommandRoster&other)noexcept{
  using std::swap;
  swap(key,other.key);objects.swap(other.objects);
  members.swap(other.members);globalVehicles.swap(other.globalVehicles);
  swap(items,other.items);swap(count,other.count);swap(global,other.global);
 }
 void invalidate(){key={};items=0;count=-1;global=false;members.clear();globalVehicles.clear();}
 bool current(const Key&frame)const{
  return objects&&count>=0&&key.sameFrame(frame)&&!boundaryOnlySource(frame);
 }
 bool build(const Snapshot&snapshot,bool globalWanted,std::vector<uint32_t>&scratch){
  invalidate();
  if(!snapshot.sealed()||snapshot.overflow()||boundaryOnlySource(snapshot.key))return false;
  struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
  Vec all{};
  if(!snapshot.read(0xA8EC78,&all,sizeof all)||all.count<0||all.count>16384||
     all.capacity<all.count||all.capacity>262144||
     (all.count&&all.items<0x10000))return false;
  // The source's actual Techno roster, not recursive queue/Passengers, is
  // the only membership authority for worldObjects/byId.
  scratch.resize(size_t(all.count));
  if(all.count&&!snapshot.read(all.items,scratch.data(),size_t(all.count)*4))return false;
  if(!objects)objects.reset(new(std::nothrow)assist::ObjectIndex);
  if(!objects)return false;
  objects->clear();members.clear();globalVehicles.clear();
  members.reserve(size_t(all.count));
  if(globalWanted)globalVehicles.reserve(size_t(all.count));
  for(uint32_t pointer:scratch){
   if(pointer<0x10000)continue;
   uint32_t id=0;
   if(!snapshot.read(pointer+0x10,&id,4)){invalidate();return false;}
   if(!id)continue;
   objects->add(pointer,id);members.push_back({pointer,id});
   if(globalWanted){
    uint32_t vt=0,owner=0;
    if(!snapshot.read(pointer,&vt,4)||!snapshot.read(pointer+0x21C,&owner,4)){
     invalidate();return false;
    }
    if(vt==0x7F5C70u&&owner==snapshot.key.house)globalVehicles.push_back(pointer);
   }
  }
  key=snapshot.key;items=all.items;count=all.count;global=globalWanted;
  return true;
 }
};
} // namespace worldframe

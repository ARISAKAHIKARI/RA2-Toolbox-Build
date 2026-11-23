#pragma once
#include "execution_proof.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace assist::cooperative {
struct CaptureStamp {
 uint32_t serial=0,match=0,tick=0;
 int frame=-1;
 bool operator==(const CaptureStamp&other)const{
  return serial==other.serial&&match==other.match&&tick==other.tick&&frame==other.frame;
 }
};
// Runtime-local provenance for the Host's value-only aircraft assignments.
// This stays out of the shared-memory protocol.
struct CapturedIdentities {
 CaptureStamp stamp{};
 uint32_t house=0;
 std::vector<executionproof::Actor> actors;
 std::vector<executionproof::Identity> targets;
 template<class Identity>
 static Identity identity(const std::vector<Identity>&objects,uint32_t id){
  const auto it=std::lower_bound(objects.begin(),objects.end(),id,[](const auto&object,uint32_t uid){return object.id<uid;});
  return it!=objects.end()&&it->id==id?*it:Identity{};
 }
 executionproof::Ticket boarding(uint32_t actor,uint32_t destination)const{
  return {stamp.match,house,stamp.tick,identity(actors,actor),{},identity(targets,destination)};
 }
 executionproof::Ticket attack(uint32_t actor,uint32_t target)const{
  return {stamp.match,house,stamp.tick,identity(actors,actor),identity(targets,target),{}};
 }
};
class CaptureHistory {
 std::array<CapturedIdentities,8> frames_{};
 size_t next_=0;
public:
 void keep(CapturedIdentities frame){
  const auto byID=[](const auto&a,const auto&b){return a.id<b.id;};
  std::sort(frame.actors.begin(),frame.actors.end(),byID);
  std::sort(frame.targets.begin(),frame.targets.end(),byID);
  frames_[next_++]=std::move(frame);
  if(next_==frames_.size())next_=0;
 }
 const CapturedIdentities*find(const CaptureStamp&stamp)const{
  if(stamp.serial&&stamp.match)
   for(const auto&frame:frames_)if(frame.stamp==stamp)return &frame;
  return nullptr;
 }
};
}

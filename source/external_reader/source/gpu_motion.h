#pragma once
#include "vulkan_scene_types.h"
#include <map>
namespace external { namespace gpumotion {
// Guides describe current pixel -> previous pixel, in output pixel units.
// Depth is reversed orthographic renderer Z, not physical perspective depth.
struct Guides {
 int width=0,height=0;
 uint64_t sequence=0;
 bool resetHistory=true;
 std::vector<float> motion; // interleaved x,y
 std::vector<float> depth;
 std::vector<float> confidence; // 0 holds original pixels; 1 permits neural effect
};
struct Plan {
 bool reset=true;
 std::array<float,2> base{{0,0}};
 std::vector<std::array<float,2>> draws;
 std::vector<uint8_t> eligible;
};
class History {
 using Draw=vulkancompute::DrawJob;
 using Key=vulkancompute::MotionIdentity;
 struct Snapshot {
  std::weak_ptr<const void> geometry;const void* address=nullptr;
  int width=0,height=0,originY=0,bottom=0,rasterScale=0;
  float left=0,top=0,scaleX=0,scaleY=0,lift=0;
  uint32_t flags=0;
 };
 std::map<Key,Snapshot> previous_;
 vulkancompute::MotionView view_;
 int width_=0,height_=0;bool valid_=false;uint64_t sequence_=0;
 static std::shared_ptr<const void> geometry(const Draw&d){
  if(d.gpuRaster)return {d.gpuRaster,d.gpuRaster.get()};
  if(d.source.pixels)return {d.source.pixels,d.source.pixels.get()};
  return {};
 }
 static Snapshot snapshot(const Draw&d){
  auto owner=geometry(d);return {owner,owner.get(),d.source.width,d.source.height,d.originY,d.depthBottom,d.rasterScale,
   d.left,d.top,d.scaleX,d.scaleY,d.lift,uint32_t(d.shadow)|(uint32_t(d.writeDepth)<<1)|(uint32_t(d.flat)<<2)|(uint32_t(d.skipZ)<<3)};
 }
 static bool known(const Draw&d){return d.motion.known&&d.motion.object>=0x10000&&d.motion.type>=0x10000&&d.motion.kind<=6&&d.motion.part.size()<=16&&bool(geometry(d));}
 static bool sameShape(const Snapshot&a,const Draw&d){
  const auto b=snapshot(d);return a.address==b.address&&!a.geometry.expired()&&!a.geometry.owner_before(b.geometry)&&!b.geometry.owner_before(a.geometry)&&
   a.width==b.width&&a.height==b.height&&a.originY==b.originY&&a.bottom==b.bottom&&a.rasterScale==b.rasterScale&&
   a.scaleX==b.scaleX&&a.scaleY==b.scaleY&&a.lift==b.lift&&a.flags==b.flags;
 }
 static bool validView(const vulkancompute::FrameJob&f){
  const auto&v=f.motion;return f.width>0&&f.height>0&&f.width<=8192&&f.height<=8192&&uint64_t(f.width)*f.height<=16*1024*1024&&
   v.context&&v.frame>=0&&std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.scaleX)&&std::isfinite(v.scaleY)&&
   v.scaleX>0&&v.scaleY>0&&v.scaleX<=8&&v.scaleY<=8&&std::abs(v.x)<=1e7&&std::abs(v.y)<=1e7&&
   std::isfinite(v.depthFar)&&std::isfinite(v.depthNear)&&v.depthNear>v.depthFar;
 }
 bool continuation(const vulkancompute::FrameJob&f)const{
  const auto&v=f.motion;return valid_&&f.width==width_&&f.height==height_&&view_.context.get()==v.context.get()&&
   !view_.context.owner_before(v.context)&&!v.context.owner_before(view_.context)&&v.scaleX==view_.scaleX&&v.scaleY==view_.scaleY&&
   v.depthFar==view_.depthFar&&v.depthNear==view_.depthNear&&v.terrainRevision==view_.terrainRevision&&
   uint32_t(v.tick-view_.tick)<=250&&v.frame>=view_.frame&&int64_t(v.frame)-view_.frame<=120&&
   std::abs(v.x-view_.x)<=384&&std::abs(v.y-view_.y)<=384;
 }
public:
 void clear(){previous_.clear();view_={};width_=height_=0;valid_=false;sequence_=0;}
 bool prepare(const vulkancompute::FrameJob&f,Plan&out)const{
  out={};if(!validView(f)||f.draws.size()>vulkancompute::MaximumSceneDraws)return false;
  out.draws.resize(f.draws.size(),{{0,0}});out.eligible.resize(f.draws.size());
  bool continuing=continuation(f);out.reset=!continuing;
  if(continuing){out.base={{float((f.motion.x-view_.x)*f.motion.scaleX),float((f.motion.y-view_.y)*f.motion.scaleY)}};}
  std::map<Key,unsigned> counts;for(const auto&d:f.draws)if(known(d))++counts[d.motion];
  size_t identified=0;
  for(size_t i=0;i<f.draws.size();++i){const auto&d=f.draws[i];
   if(!known(d)||counts[d.motion]!=1){out.reset=true;continue;}
   ++identified;out.eligible[i]=1;
   auto old=previous_.find(d.motion);
   if(!continuing||old==previous_.end()||!sameShape(old->second,d)){out.reset=true;continue;}
   auto delta=std::array<float,2>{{old->second.left-d.left,old->second.top-d.top}};
   if(!std::isfinite(delta[0])||!std::isfinite(delta[1])||std::abs(delta[0])>4096||std::abs(delta[1])>4096){out.reset=true;out.eligible[i]=0;continue;}
   out.draws[i]=delta;
  }
  if(continuing&&(identified!=previous_.size()||identified!=f.draws.size()))out.reset=true;
  // Reset discards prior-frame correspondence. Do not publish invented
  // velocities for new poses, teleports or a different map/context.
  if(out.reset){out.base={{0,0}};std::fill(out.draws.begin(),out.draws.end(),std::array<float,2>{{0,0}});}
  return true;
 }
 void commit(const vulkancompute::FrameJob&f){
  if(!validView(f)){clear();return;}
  previous_.clear();std::map<Key,unsigned> counts;for(const auto&d:f.draws)if(known(d))++counts[d.motion];
  for(const auto&d:f.draws)if(known(d)&&counts[d.motion]==1)previous_.emplace(d.motion,snapshot(d));
  view_=f.motion;width_=f.width;height_=f.height;valid_=true;++sequence_;
 }
 uint64_t nextSequence()const{return sequence_+1;}
};
}}

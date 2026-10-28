#pragma once
#include "radar_shroud_mask.h"
#include <memory>
#include <numeric>

// Native SHROUD/FOG stamp planning. CPU work is source discovery plus one
// source-coordinate lookup per row/column/sample, never per covered pixel.
// Integer GPU kernels consume the exact double-coordinate decisions below.
namespace external { namespace gpushroud {
constexpr uint32_t BinSize=32,StampWords=12;
constexpr size_t MaximumCoordinates=8*1024*1024,MaximumReferences=8*1024*1024;
struct Source {
 std::shared_ptr<const std::vector<uint8_t>> indices;
 int width=0,height=0;
};
struct Stamp {
 std::array<uint32_t,4> rect{}; // left,top,right,bottom, output pixel bounds
 std::array<uint32_t,4> source{}; // Source index (GPU patches to word offset),w,h,fog
 std::array<uint32_t,4> coordinates{}; // X offset,Y offset,reserved,reserved
};
static_assert(sizeof(Stamp)==StampWords*4,"GPU shroud std430 stamp layout");
struct Plan {
 int width=0,height=0,samplesX=1,samplesY=1;
 uint32_t binColumns=0;
 std::vector<Source> sources;
 std::vector<Stamp> stamps;
 std::vector<int32_t> coordinates;
 std::vector<uint32_t> ranges,references;
 size_t outputBytes()const{return size_t(width)*height;}
 size_t outputWords()const{return (outputBytes()+3)/4;}
};
struct Params {
 uint32_t width=0,height=0,samplesX=1,samplesY=1;
 uint32_t stampOffset=0,coordinateOffset=0,rangeOffset=0,referenceOffset=0;
 uint32_t binColumns=0,outputOffset=0;
};
static_assert(sizeof(Params)==40,"GPU shroud push constants");
struct Packed {
 std::vector<uint32_t> sources,metadata;
 Params params;
};
inline bool pack(const Plan&plan,Packed&out,const std::vector<uint32_t>*sourceOffsets=nullptr){
 out={};if(plan.width<1||plan.height<1||plan.width>8192||plan.height>8192||uint64_t(plan.width)*plan.height>16*1024*1024||
   plan.samplesX<1||plan.samplesX>4||plan.samplesY<1||plan.samplesY>4||plan.sources.size()>94||plan.stamps.size()>131072||
   plan.coordinates.size()>MaximumCoordinates||plan.references.size()>MaximumReferences||
   plan.binColumns!=uint32_t((plan.width+31)/32)||plan.ranges.size()!=size_t(plan.binColumns)*((plan.height+31)/32)+1||
   plan.ranges.empty()||plan.ranges.front()!=0||plan.ranges.back()!=plan.references.size()||
   (sourceOffsets&&sourceOffsets->size()!=plan.sources.size()))return false;
 std::vector<uint32_t> offsets;
 for(const auto&source:plan.sources){if(!source.indices||source.width<1||source.width>60||source.height<1||source.height>30||source.indices->size()!=size_t(source.width)*source.height)return false;
  offsets.push_back(uint32_t(out.sources.size()));if(!sourceOffsets)for(uint8_t value:*source.indices)out.sources.push_back(value);
 }
 if(sourceOffsets)offsets=*sourceOffsets;
 for(const auto&stamp:plan.stamps){
  if(stamp.rect[0]>=stamp.rect[2]||stamp.rect[1]>=stamp.rect[3]||stamp.rect[2]>uint32_t(plan.width)||stamp.rect[3]>uint32_t(plan.height)||stamp.source[0]>=plan.sources.size()||stamp.source[3]>1)return false;
  const auto&source=plan.sources[stamp.source[0]];
  if(stamp.source[1]!=uint32_t(source.width)||stamp.source[2]!=uint32_t(source.height)||
    uint64_t(stamp.coordinates[0])+uint64_t(stamp.rect[2]-stamp.rect[0])*plan.samplesX>plan.coordinates.size()||
    uint64_t(stamp.coordinates[1])+uint64_t(stamp.rect[3]-stamp.rect[1])*plan.samplesY>plan.coordinates.size())return false;
  auto copy=stamp;copy.source[0]=offsets[stamp.source[0]];
  for(uint32_t value:copy.rect)out.metadata.push_back(value);for(uint32_t value:copy.source)out.metadata.push_back(value);for(uint32_t value:copy.coordinates)out.metadata.push_back(value);
 }
 out.params={uint32_t(plan.width),uint32_t(plan.height),uint32_t(plan.samplesX),uint32_t(plan.samplesY),0,uint32_t(out.metadata.size()),0,0,plan.binColumns,0};
 for(int32_t value:plan.coordinates)out.metadata.push_back(uint32_t(value));
 out.params.rangeOffset=uint32_t(out.metadata.size());out.metadata.insert(out.metadata.end(),plan.ranges.begin(),plan.ranges.end());
 out.params.referenceOffset=uint32_t(out.metadata.size());out.metadata.insert(out.metadata.end(),plan.references.begin(),plan.references.end());
 for(size_t n=1;n<plan.ranges.size();++n)if(plan.ranges[n]<plan.ranges[n-1]||plan.ranges[n]>plan.references.size())return false;
 for(auto reference:plan.references)if(reference>=plan.stamps.size())return false;
 return true;
}
class Builder {
 std::array<std::array<std::shared_ptr<const voxel::Image>,47>,2> images_{};
 static bool same(const voxel::Image&a,const voxel::Image&b){return a.canvasWidth==b.canvasWidth&&a.canvasHeight==b.canvasHeight&&a.width==b.width&&a.height==b.height&&a.x==b.x&&a.y==b.y&&a.indices==b.indices;}
 static int edge(double value,int maximum,bool upper){value=upper?std::ceil(value):std::floor(value);return int(std::clamp(value,0.,double(maximum)));}
public:
 void reset(){images_={};}
 template<class Load>bool build(const radar::Geometry&g,const std::vector<radar::shroudmask::Cell>&cells,int w,int h,double spanX,double spanY,bool fog,Load load,std::shared_ptr<const Plan>&out,double originX=0,double originY=0){
  out.reset();if(!g.valid()||w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||cells.size()>65536||
   !std::isfinite(originX)||!std::isfinite(originY)||std::abs(originX)>1e7||std::abs(originY)>1e7||!std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  std::array<std::array<bool,47>,2> needed{};
  for(const auto&cell:cells){if(cell.index>=512*512||!g.legal(int(cell.index%512),int(cell.index/512)))return false;if(cell.shade.shroud==127)continue;
   int a=radar::shroudmask::frame(cell.shade.shroud),b=radar::shroudmask::frame(cell.shade.fog);if(a<0||a>=47||b<0||b>=47)return false;needed[0][a]=true;if(fog)needed[1][b]=true;
  }
  auto plan=std::make_shared<Plan>();plan->width=w;plan->height=h;plan->binColumns=uint32_t((w+31)/32);
  plan->samplesX=std::clamp(int(std::ceil(spanX/w/12.)),1,4);plan->samplesY=std::clamp(int(std::ceil(spanY/h/12.)),1,4);
  while(uint64_t(w)*h*plan->samplesX*plan->samplesY>8*1024*1024&&(plan->samplesX>1||plan->samplesY>1)){
   if(plan->samplesX>=plan->samplesY&&plan->samplesX>1)--plan->samplesX;else --plan->samplesY;
  }
  std::array<std::array<uint32_t,47>,2> source{};
  for(int kind=0;kind<2;++kind)for(int f=0;f<47;++f)if(needed[kind][f]){
   voxel::Image image;if(!load(kind!=0,f,image)||!radar::shroudmask::Raster::valid(image))return false;
   auto&cached=images_[kind][f];if(!cached||!same(*cached,image))cached=std::make_shared<const voxel::Image>(std::move(image));
   source[kind][f]=uint32_t(plan->sources.size());plan->sources.push_back({std::shared_ptr<const std::vector<uint8_t>>(cached,&cached->indices),cached->width,cached->height});
  }
  auto ordered=cells;std::stable_sort(ordered.begin(),ordered.end(),[](const radar::shroudmask::Cell&a,const radar::shroudmask::Cell&b){return a.index%512+a.index/512<b.index%512+b.index/512;});
  for(int kind=0;kind<(fog?2:1);++kind)for(const auto&cell:ordered){
   if(cell.shade.shroud==127)continue;int f=radar::shroudmask::frame(kind?cell.shade.fog:cell.shade.shroud);const auto&image=*images_[kind][f];mapbounds::TileOrigin origin;
   if(!g.shroudOrigin(int(cell.index%512),int(cell.index/512),origin))continue;
   const double left=double(origin.x)+30+image.x-originX,top=double(origin.y)+15+image.y-originY;
   int x0=edge(left*w/spanX,w,false),x1=edge((left+image.width)*w/spanX,w,true),y0=edge(top*h/spanY,h,false),y1=edge((top+image.height)*h/spanY,h,true);
   if(x0>=x1||y0>=y1)continue;
   const size_t count=size_t(x1-x0)*plan->samplesX+size_t(y1-y0)*plan->samplesY;
   if(plan->coordinates.size()+count>MaximumCoordinates||plan->stamps.size()>=131072)return false;
   Stamp stamp;stamp.rect={{uint32_t(x0),uint32_t(y0),uint32_t(x1),uint32_t(y1)}};stamp.source={{source[kind][f],uint32_t(image.width),uint32_t(image.height),uint32_t(kind)}};
   stamp.coordinates[0]=uint32_t(plan->coordinates.size());
   for(int px=x0;px<x1;++px)for(int sx=0;sx<plan->samplesX;++sx){double ix=std::floor((px+(sx+.5)/plan->samplesX)*spanX/w-left);plan->coordinates.push_back(ix<0||ix>=image.width?-1:int32_t(ix));}
   stamp.coordinates[1]=uint32_t(plan->coordinates.size());
   for(int py=y0;py<y1;++py)for(int sy=0;sy<plan->samplesY;++sy){double iy=std::floor((py+(sy+.5)/plan->samplesY)*spanY/h-top);plan->coordinates.push_back(iy<0||iy>=image.height?-1:int32_t(iy));}
   plan->stamps.push_back(stamp);
  }
  const size_t bins=size_t(plan->binColumns)*((h+31)/32);plan->ranges.assign(bins+1,0);
  for(const auto&s:plan->stamps)for(uint32_t by=s.rect[1]/32;by<=(s.rect[3]-1)/32;++by)for(uint32_t bx=s.rect[0]/32;bx<=(s.rect[2]-1)/32;++bx)++plan->ranges[size_t(by)*plan->binColumns+bx+1];
  for(size_t n=1;n<plan->ranges.size();++n){if(uint64_t(plan->ranges[n])+plan->ranges[n-1]>MaximumReferences)return false;plan->ranges[n]+=plan->ranges[n-1];}
  plan->references.resize(plan->ranges.back());auto next=plan->ranges;
  for(uint32_t n=0;n<plan->stamps.size();++n){const auto&s=plan->stamps[n];for(uint32_t by=s.rect[1]/32;by<=(s.rect[3]-1)/32;++by)for(uint32_t bx=s.rect[0]/32;bx<=(s.rect[2]-1)/32;++bx)plan->references[next[size_t(by)*plan->binColumns+bx]++]=n;}
  out=std::move(plan);return true;
 }
 template<class Load>bool build(const radar::Terrain&terrain,int w,int h,double spanX,double spanY,bool fog,Load load,std::shared_ptr<const Plan>&out,double originX=0,double originY=0){
  out.reset();if(terrain.reveal||terrain.shade.size()!=512*512)return false;std::vector<radar::shroudmask::Cell> cells;
  for(size_t n=0;n<terrain.shade.size();++n)if(terrain.geo.shroudSourceVisible(int(n%512),int(n/512))&&terrain.shade[n].shroud!=127)cells.push_back({uint32_t(n),terrain.shade[n]});
  return build(terrain.geo,cells,w,h,spanX,spanY,fog,load,out,originX,originY);
 }
};
// Upload-layout oracle, deliberately separate from the production Raster's
// stamp-first loops. Actual GPU execution is checked by vulkan_shroud.cpp.
inline bool reference(const Plan&plan,std::vector<uint8_t>&out){
 Packed packed;if(!pack(plan,packed))return false;out.assign(plan.outputBytes(),2);
 for(int py=0;py<plan.height;++py)for(int px=0;px<plan.width;++px){
  unsigned sum=0;const uint32_t bin=uint32_t(py/32)*plan.binColumns+uint32_t(px/32);
  for(int sy=0;sy<plan.samplesY;++sy)for(int sx=0;sx<plan.samplesX;++sx){unsigned value=2;
   for(uint32_t ref=plan.ranges[bin];ref<plan.ranges[bin+1];++ref){const auto&s=plan.stamps[plan.references[ref]];if(uint32_t(px)<s.rect[0]||uint32_t(px)>=s.rect[2]||uint32_t(py)<s.rect[1]||uint32_t(py)>=s.rect[3])continue;
    int ix=plan.coordinates[s.coordinates[0]+(uint32_t(px)-s.rect[0])*plan.samplesX+sx],iy=plan.coordinates[s.coordinates[1]+(uint32_t(py)-s.rect[1])*plan.samplesY+sy];
    if(ix<0||iy<0||ix>=int(s.source[1])||iy>=int(s.source[2]))continue;unsigned incoming=(*plan.sources[s.source[0]].indices)[size_t(iy)*s.source[1]+ix];if(incoming>127)continue;
    value=s.source[3]?(value==127?incoming:unsigned(std::max(0,int(value)+int(incoming)-127))):incoming;
   }sum+=value;
  }out[size_t(py)*plan.width+px]=uint8_t((sum+unsigned(plan.samplesX*plan.samplesY)/2)/unsigned(plan.samplesX*plan.samplesY));
 }return true;
}
}}

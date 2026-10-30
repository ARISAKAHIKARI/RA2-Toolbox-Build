#pragma once
#include "voxel_preview.h"
namespace external { namespace materialblend {
inline uint32_t pixel(uint32_t a,uint32_t b,unsigned weight){
 weight=std::min(255u,weight);uint32_t out=0;
 for(int s:{0,8,16,24})out|=((unsigned((a>>s)&255)*(255-weight)+unsigned((b>>s)&255)*weight+127)/255)<<s;
 return out;
}
inline float depth(const voxel::Image&i,int x,int y,float lift){
 size_t at=size_t(y)*i.width+x;if(i.depth.size()==i.pixels.size())return i.depth[at];
 if(i.flat)return float(i.y+y)/i.rasterScale-14+lift;
 int bottom=i.depthBottom==std::numeric_limits<int>::min()?i.y+i.height-1:i.depthBottom;
 return float(bottom)/i.rasterScale-14+lift+float((bottom-i.y-y)/3)/i.rasterScale;
}
inline bool image(const voxel::Image&a,const voxel::Image&b,unsigned weight,float lift,voxel::Image&out){
 if(weight>=255){out=b;return true;}if(!weight){out=a;return true;}
 if(a.rasterScale!=1||b.rasterScale!=1||a.shadow||b.shadow||a.objectPalette!=b.objectPalette||
    a.flat!=b.flat||a.writeDepth!=b.writeDepth||a.depthBias!=b.depthBias||a.attachments.size()!=b.attachments.size()||
    a.width<1||a.height<1||b.width<1||b.height<1||a.pixels.size()!=size_t(a.width)*a.height||b.pixels.size()!=size_t(b.width)*b.height)return false;
 const int l=std::min(a.x,b.x),t=std::min(a.y,b.y),r=std::max(a.x+a.width,b.x+b.width),bt=std::max(a.y+a.height,b.y+b.height);
 if(r-l>2048||bt-t>2048||std::abs(l)>4096||std::abs(t)>4096)return false;
 voxel::Image next=b;next.x=l;next.y=t;next.width=r-l;next.height=bt-t;next.attachments.clear();next.nativeBlend=0;next.blendSource.reset();next.sourcePalette.reset();
 next.pixels.assign(size_t(next.width)*next.height,0);next.depth.assign(next.pixels.size(),-std::numeric_limits<float>::infinity());next.indices.resize(next.pixels.size());
 for(int y=t;y<bt;++y)for(int x=l;x<r;++x){
  int ax=x-a.x,ay=y-a.y,bx=x-b.x,by=y-b.y;
  bool av=ax>=0&&ay>=0&&ax<a.width&&ay<a.height,bv=bx>=0&&by>=0&&bx<b.width&&by<b.height;
  size_t ai=av?size_t(ay)*a.width+ax:0,bi=bv?size_t(by)*b.width+bx:0,to=size_t(y-t)*next.width+x-l;
  uint32_t ac=av?a.pixels[ai]:0,bc=bv?b.pixels[bi]:0;next.pixels[to]=pixel(ac,bc,weight);
  if(!(next.pixels[to]>>24))continue;
  float ad=(ac>>24)?depth(a,ax,ay,lift):-std::numeric_limits<float>::infinity();
  float bd=(bc>>24)?depth(b,bx,by,lift):-std::numeric_limits<float>::infinity();
  next.depth[to]=std::isnan(ad)||std::isnan(bd)?std::numeric_limits<float>::quiet_NaN():std::max(ad,bd);
  // Fullbright palette entries may not blend into ordinary shaded material.
  uint8_t ia=av&&a.indices.size()==a.pixels.size()?a.indices[ai]:0,ib=bv&&b.indices.size()==b.pixels.size()?b.indices[bi]:0;
  if((ia>=240&&ia<=254)!=(ib>=240&&ib<=254))return false;
  next.indices[to]=weight<128?ia:ib;
 }
 for(size_t n=0;n<a.attachments.size();++n){
  const auto&aa=a.attachments[n];const auto&bb=b.attachments[n];voxel::Image part;
  // Shadow coverage and structural production components stay on observed
  // frames if their independent composition is not safe to dissolve.
  if(aa.shadow||bb.shadow)part=weight<128?aa:bb;
  else if(!image(aa,bb,weight,lift,part))part=weight<128?aa:bb;
  next.attachments.push_back(std::move(part));
 }
 out=std::move(next);return true;
}
}}

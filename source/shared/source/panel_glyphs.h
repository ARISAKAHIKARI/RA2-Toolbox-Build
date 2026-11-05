#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "screen_sprite_scale.h"
// Immutable screen annotations belong to the completed scene, not the newest
// sampler frame. Only their anchors scale with the panel; native PIPS pixels
// retain their readable minimum of one physical output pixel.
namespace panelglyphs {
struct Glyph {
 double u=0,v=0;int x=0,y=0,width=0,height=0;
 std::shared_ptr<const std::vector<uint32_t>> pixels;
 bool operator==(const Glyph&o)const{return u==o.u&&v==o.v&&x==o.x&&y==o.y&&width==o.width&&height==o.height&&pixels==o.pixels;}
};
struct Annotations {double worldWidth=0,worldHeight=0;std::vector<Glyph> glyphs;};
inline bool same(const std::shared_ptr<const Annotations>&a,const std::shared_ptr<const Annotations>&b){
 if(a==b)return true;if(!a||!b)return (!a||a->glyphs.empty())&&(!b||b->glyphs.empty());
 return a->worldWidth==b->worldWidth&&a->worldHeight==b->worldHeight&&a->glyphs==b->glyphs;
}
struct Draw {int x=0,y=0,width=0,height=0,sourceWidth=0,sourceHeight=0;std::shared_ptr<const std::vector<uint32_t>>pixels;};
inline std::vector<Draw> place(const std::shared_ptr<const Annotations>&annotations,int width,int height){
 std::vector<Draw> result;if(!annotations||width<3||height<3||!std::isfinite(annotations->worldWidth)||!std::isfinite(annotations->worldHeight)||annotations->worldWidth<=0||annotations->worldHeight<=0)return result;
 const int w=width-2,h=height-2;const double sx=std::max(1.,w/annotations->worldWidth),sy=std::max(1.,h/annotations->worldHeight);
 for(const auto&g:annotations->glyphs){if(!g.pixels||g.width<1||g.height<1||g.width>1024||g.height>1024||g.pixels->size()!=size_t(g.width)*g.height||!std::isfinite(g.u)||!std::isfinite(g.v)||std::abs(g.u)>16||std::abs(g.v)>16)continue;
  const double x=1+g.u*w+2+g.x*sx,y=1+g.v*h+2+g.y*sy,dw=g.width*sx,dh=g.height*sy;
  if(!std::isfinite(x)||!std::isfinite(y)||dw>8192||dh>8192||x>=width-1||y>=height-1||x+dw<=1||y+dh<=1)continue;
  const int targetW=int(std::ceil(dw)),targetH=int(std::ceil(dh));
  auto raster=screensprites::cache().get(g.pixels,g.width,g.height,targetW,targetH);
  result.push_back({int(std::floor(x)),int(std::floor(y)),targetW,targetH,raster.width,raster.height,std::move(raster.pixels)});
 }return result;
}
inline uint32_t over(uint32_t src,uint32_t dst){
 const unsigned a=src>>24;if(!a)return dst;if(a==255)return src;uint32_t result=0;for(int shift:{0,8,16,24})result|=std::min(255u,((src>>shift)&255u)+(((dst>>shift)&255u)*(255-a)+127)/255)<<shift;return result;
}
inline void paint(uint32_t*pixels,int width,int height,const std::vector<Draw>&draws){
 if(!pixels||width<3||height<3)return;
 for(const auto&d:draws){const int l=std::max(1,d.x),t=std::max(1,d.y),r=std::min(width-1,d.x+d.width),b=std::min(height-1,d.y+d.height);
  for(int y=t;y<b;++y){const int sy=std::min(d.sourceHeight-1,int((int64_t(y-d.y)*2+1)*d.sourceHeight/(int64_t(d.height)*2)));
   for(int x=l;x<r;++x){const int sx=std::min(d.sourceWidth-1,int((int64_t(x-d.x)*2+1)*d.sourceWidth/(int64_t(d.width)*2)));auto&out=pixels[size_t(y)*width+x];out=over((*d.pixels)[size_t(sy)*d.sourceWidth+sx],out);}
  }
 }
}
}

#pragma once
#include "radar_map.h"
#include <array>

namespace external { namespace radar { namespace shroudmask {
// SHROUD.SHP and FOG.SHP are alpha sprites. Indices 0..127 are the native
// alpha-buffer values (127 clear); 0xFE and other values above 127 skip a
// pixel. The mask is projected on the flat cell, independently of Level.
inline int frame(int8_t value){return value==-2?15:value==-1?0:int(value);}

struct Raster {
 int width=0,height=0;
 std::vector<uint8_t> alpha;
 template<class Image> static bool valid(const Image&image){
  return image.canvasWidth==60&&image.canvasHeight==30&&image.width>0&&image.height>0&&
   image.width<=60&&image.height<=30&&image.indices.size()==size_t(image.width)*image.height;
 }
 template<class Load> bool build(const Terrain&terrain,int w,int h,double spanX,double spanY,bool fog,Load load,double originX=0,double originY=0){
  if(terrain.reveal||!terrain.geo.valid()||terrain.shade.size()!=512*512||w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||!std::isfinite(originX)||!std::isfinite(originY)||
     !std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0)return false;
  std::array<std::array<voxel::Image,47>,2> images;
  std::array<std::array<bool,47>,2> needed{};
  const auto&g=terrain.geo;
  for(size_t n=0;n<terrain.shade.size();++n){int x=int(n%512),y=int(n/512);if(!g.legal(x,y)||!g.flatVisible(x,y))continue;
   auto shade=terrain.shade[n];if(shade.shroud==127)continue;
   int a=frame(shade.shroud),b=frame(shade.fog);
   if(a<0||a>=47||b<0||b>=47)return false;
   needed[0][a]=true;if(fog)needed[1][b]=true;
  }
  for(int kind=0;kind<2;++kind)for(int f=0;f<47;++f)if(needed[kind][f]&&
      (!load(kind!=0,f,images[kind][f])||!valid(images[kind][f])))return false;
  int samplesX=std::clamp(int(std::ceil(spanX/w/12.)),1,4),samplesY=std::clamp(int(std::ceil(spanY/h/12.)),1,4);
  while(size_t(w)*h*samplesX*samplesY>8*1024*1024&&(samplesX>1||samplesY>1)){
   if(samplesX>=samplesY&&samplesX>1)--samplesX;else --samplesY;
  }
  const size_t samples=size_t(samplesX)*samplesY;
  // Unknown/outside physical cells are covered. Clear and partial masks are
  // then stamped in native cell order; the output stores area samples of the
  // original 60x30 source pixels, instead of scaling each sprite in isolation.
  std::vector<uint8_t> surface(size_t(w)*h*samples,2);
  std::vector<size_t> cells;
  for(size_t n=0;n<terrain.shade.size();++n){int x=int(n%512),y=int(n/512);if(g.legal(x,y)&&g.flatVisible(x,y)&&terrain.shade[n].shroud!=127)cells.push_back(n);}
  std::stable_sort(cells.begin(),cells.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});
  for(int kind=0;kind<(fog?2:1);++kind)for(size_t n:cells){
   int x=int(n%512),y=int(n/512),f=frame(kind?terrain.shade[n].fog:terrain.shade[n].shroud);
   const auto&image=images[kind][f];mapbounds::TileOrigin origin;
   if(!g.shroudOrigin(x,y,origin))continue;
   // voxel::sprite stores the frame crop relative to its native centre.
   const double centerX=origin.x+30,centerY=origin.y+15;
   const double left=centerX+image.x-originX,top=centerY+image.y-originY;
   int x0=std::clamp(int(std::floor(left*w/spanX)),0,w),x1=std::clamp(int(std::ceil((left+image.width)*w/spanX)),0,w);
   int y0=std::clamp(int(std::floor(top*h/spanY)),0,h),y1=std::clamp(int(std::ceil((top+image.height)*h/spanY)),0,h);
   for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px)for(int sy=0;sy<samplesY;++sy)for(int sx=0;sx<samplesX;++sx){
    double nx=(px+(sx+.5)/samplesX)*spanX/w,ny=(py+(sy+.5)/samplesY)*spanY/h;
    int ix=int(std::floor(nx-left)),iy=int(std::floor(ny-top));if(ix<0||iy<0||ix>=image.width||iy>=image.height)continue;
    uint8_t value=image.indices[size_t(iy)*image.width+ix];if(value>127)continue;
    auto&target=surface[(size_t(py)*w+px)*samples+size_t(sy)*samplesX+sx];
    if(!kind)target=value;
    else target=target==127?value:uint8_t(std::max(0,int(target)+int(value)-127));
   }
  }
  std::vector<uint8_t> result(size_t(w)*h);
  for(size_t n=0;n<result.size();++n){unsigned sum=0;for(size_t i=0;i<samples;++i)sum+=surface[n*samples+i];result[n]=uint8_t((sum+samples/2)/samples);}
  width=w;height=h;alpha=std::move(result);return true;
 }
 void apply(std::vector<uint32_t>&pixels)const{
  if(pixels.size()!=alpha.size())return;
  for(size_t n=0;n<pixels.size();++n){unsigned a=alpha[n];if(a>=127)continue;
   uint32_t p=pixels[n];if(!(p>>24))continue;uint32_t v=p&0xff000000u;
   for(int shift:{0,8,16})v|=uint32_t((((p>>shift)&255u)*a+63)/127)<<shift;
   pixels[n]=v;
  }
 }
};
}}}

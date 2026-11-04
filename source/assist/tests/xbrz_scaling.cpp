#include "../../external_reader/source/xbrz_adapter.h"
#include "../../third_party/xbrz/xbrz.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace external;
static uint32_t straight(uint32_t p){
 unsigned alpha=p>>24;if(!alpha)return 0;uint32_t out=alpha<<24;
 for(int shift:{0,8,16})out|=std::min(255u,(((p>>shift)&255)*255+alpha/2)/alpha)<<shift;return out;
}
static uint32_t premultiply(uint32_t p){
 unsigned alpha=p>>24;uint32_t out=alpha<<24;
 for(int shift:{0,8,16})out|=((((p>>shift)&255)*alpha+127)/255)<<shift;return out;
}
static std::vector<uint32_t> reference(const std::vector<uint32_t>&input,int w,int h,int factor){
 std::vector<uint32_t> source(input.size()),output(input.size()*factor*factor);
 for(size_t i=0;i<input.size();++i)source[i]=straight(input[i]);
 xbrz::scale(factor,source.data(),output.data(),w,h,xbrz::ColorFormat::argbUnbuffered);
 for(auto&p:output)p=premultiply(p);return output;
}
// Standard bilinear reconstruction is intentionally independent of adapter
// sample(): crop alignment and block halo errors must be visible to this test.
static uint32_t bilinear(const std::vector<uint32_t>&image,int width,int height,double x,double y){
 int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;
 auto get=[&](int xx,int yy){return image[size_t(std::clamp(yy,0,height-1))*width+std::clamp(xx,0,width-1)];};
 uint32_t a=get(ix,iy),b=get(ix+1,iy),c=get(ix,iy+1),d=get(ix+1,iy+1),out=0;
 for(int shift:{0,8,16,24}){double top=((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx;
  double bottom=((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx;
  out|=uint32_t(std::floor(top*(1-fy)+bottom*fy+.5))<<shift;
 }return out;
}
static void assertNear(uint32_t actual,uint32_t expected){
 for(int shift:{0,8,16,24})assert(std::abs(int((actual>>shift)&255)-int((expected>>shift)&255))<=1);
}
int main(){
 namespace adapter=xbrzadapter;
 // Edges, thin diagonals, opaque color steps and partially transparent samples
 // exercise the actual upstream rule set, including straight-alpha conversion.
 const int sw=180,sh=164;std::vector<uint32_t> art(size_t(sw)*sh);
 for(int y=0;y<sh;++y)for(int x=0;x<sw;++x){
  unsigned a=(x/13+y/11)%7==0?0:((x/17+y/19)%5==0?128:255);
  unsigned r=(x*7+y*3)&255,g=(x*3+y*11)&255,b=(x*13+y*5)&255;
  if((x-y+sw)%23<2){r=230;g=180;b=40;}art[size_t(y)*sw+x]=premultiply(a<<24|r<<16|g<<8|b);
 }
 for(int factor:{2,3,4}){
  adapter::ScaledImage actual;assert(adapter::scale(art,sw,sh,factor,actual));
  assert(actual.width==sw*factor&&actual.height==sh*factor&&actual.factor==factor);
  auto expected=reference(art,sw,sh,factor);assert(actual.pixels==expected);
  for(auto p:actual.pixels){unsigned alpha=p>>24;for(int shift:{0,8,16})assert(((p>>shift)&255)<=alpha);}
  for(double y:{0.,.5,12.125,80.75,163.75,164.})for(double x:{0.,.5,14.25,90.125,179.75,180.})
   assertNear(adapter::sample(actual,x,y),bilinear(expected,sw*factor,sh*factor,x*factor-.5,y*factor-.5));
 }
 // Crop crosses several 128-output-pixel block boundaries on both axes. A
 // full-image upstream pass is the oracle; internal block edges must agree.
 auto fetch=[&](int x,int y){return x<0||y<0||x>=sw||y>=sh?0u:art[size_t(y)*sw+x];};
 constexpr double originX=4.25,originY=5.5,spanX=160,spanY=144;
 for(double ratio:{1.25,1.5,2.,2.5,3.,4.}){
  int width=int(spanX*ratio),height=int(spanY*ratio),factor=int(std::ceil(ratio));
  std::vector<uint32_t> actual;assert(adapter::sampleRegion(fetch,originX,originY,spanX,spanY,width,height,actual));
  assert(actual.size()==size_t(width)*height);auto full=reference(art,sw,sh,factor);
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){
   double px=(originX+(x+.5)*spanX/width)*factor-.5,py=(originY+(y+.5)*spanY/height)*factor-.5;
   assertNear(actual[size_t(y)*width+x],bilinear(full,sw*factor,sh*factor,px,py));
  }
 }
 // The retained terrain cache is anchored to the SOURCE grid, so static
 // frames and small camera pans reuse the same xBRZ neighborhoods. This crop
 // crosses both native 256-cell boundaries and is checked against one full
 // upstream pass rather than against the uncached adapter implementation.
 const int terrainWidth=600,terrainHeight=560;
 std::vector<uint32_t> terrain(size_t(terrainWidth)*terrainHeight);
 for(int yy=0;yy<terrainHeight;++yy)for(int xx=0;xx<terrainWidth;++xx)terrain[size_t(yy)*terrainWidth+xx]=art[size_t(yy%sh)*sw+xx%sw];
 auto fullTerrain=reference(terrain,terrainWidth,terrainHeight,2);
 adapter::RegionCache region;size_t cacheFetches=0;
 auto terrainFetch=[&](int xx,int yy){++cacheFetches;return xx<0||yy<0||xx>=terrainWidth||yy>=terrainHeight?0u:terrain[size_t(yy)*terrainWidth+xx];};
 auto cachedCrop=[&](double x,double y){std::vector<uint32_t> actual;
  assert(region.sampleRegion(terrainFetch,x,y,80,64,160,128,actual));
  for(int yy=0;yy<128;++yy)for(int xx=0;xx<160;++xx)
   assertNear(actual[size_t(yy)*160+xx],bilinear(fullTerrain,terrainWidth*2,terrainHeight*2,(x+(xx+.5)*.5)*2-.5,(y+(yy+.5)*.5)*2-.5));
  return actual;
 };
 auto initialCrop=cachedCrop(244.25,245.5);assert(region.builds()==4&&cacheFetches>0);
 const auto initialBuilds=region.builds(),initialHits=region.hits();cacheFetches=0;
 assert(cachedCrop(244.25,245.5)==initialCrop);assert(cacheFetches==0&&region.builds()==initialBuilds&&region.hits()>initialHits);
 cachedCrop(244.5,246.);cachedCrop(245.25,246.5);
 assert(cacheFetches==0&&region.builds()==initialBuilds); // fractional and integer pan share all four tiles
 // A changed integer xBRZ factor needs its own entries; bounded LRU eviction
 // under camera travel cannot grow an unlimited reconstructed map cache.
 adapter::RegionCache travel;
 for(int tile=0;tile<20;++tile){std::vector<uint32_t> result;
  assert(travel.sampleRegion([](int,int){return 0xff304050u;},tile*256.+20,20,4,4,16,16,result));
  assert(travel.residentPixels()<=adapter::MaxPixels&&travel.entries()<=20);
  assert(std::all_of(result.begin(),result.end(),[](uint32_t value){return value==0xff304050u;}));
 }
 assert(travel.builds()==20&&travel.entries()<20);
 // Public allocation guards reject malformed or excessive input before any
 // scan or source fetch, and leave the caller free to show the original scene.
 adapter::ScaledImage invalid;std::vector<uint32_t> one{0xff123456};
 assert(!adapter::scale(one,0,1,2,invalid));assert(!adapter::scale(one,1,1,1,invalid));
 assert(!adapter::scale(one,1,1,5,invalid));assert(!adapter::scale(one,8193,1,2,invalid));
 assert(!adapter::scale(one,4096,4097,2,invalid));assert(!adapter::scale(one,2,2,2,invalid));
 std::vector<uint32_t> out;int calls=0;auto counted=[&](int,int){++calls;return 0xffffffffu;};
 const double nan=std::numeric_limits<double>::quiet_NaN();
 assert(!adapter::sampleRegion(counted,0,0,4,4,17,16,out));
 assert(!adapter::sampleRegion(counted,nan,0,4,4,8,8,out));
 assert(!adapter::sampleRegion(counted,0,0,4,4,8193,8,out));
 assert(!adapter::sampleRegion(counted,0,0,4,4,0,8,out));assert(calls==0);
 // Cache key is image CONTENT, not allocation address: mutation and a changed
 // factor must not return old pixels. Identical content can reuse a result.
 adapter::Cache cache;std::vector<uint32_t> source(16,0xff203040);
 auto first=cache.get(source,4,4,2);assert(first&&first->pixels==reference(source,4,4,2));
 auto same=cache.get(source,4,4,2);assert(same==first);
 source[5]=0xffe08020;auto changed=cache.get(source,4,4,2);assert(changed&&changed->pixels==reference(source,4,4,2));
 auto three=cache.get(source,4,4,3);assert(three&&three->factor==3&&three->pixels==reference(source,4,4,3));
 // Sustained animation or palette churn is bounded rather than accumulating
 // every historical frame. The newest frame still has the upstream output.
 std::vector<uint32_t> frame(160*160,0xff203040);
 for(unsigned n=0;n<16;++n){frame[17]=0xff000000u|n*0x000b0703u;auto current=cache.get(frame,160,160,4);
  assert(current&&cache.residentPixels()<=adapter::CachePixels&&cache.entries()<=128);
 }
 auto newest=cache.get(frame,160,160,4);assert(newest&&newest->pixels==reference(frame,160,160,4));
 assert(adapter::sample(*newest,nan,0)==0);
 std::puts("PASS: xBRZ 1.9 upstream identity, premultiplied alpha, arbitrary crop/ratio and halo seams, allocation guards, content-verified sprite cache and fixed-source terrain cache reuse/eviction");
}

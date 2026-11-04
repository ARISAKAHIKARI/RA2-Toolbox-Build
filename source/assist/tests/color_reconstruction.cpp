#include "../../external_reader/source/camera_raster.h"
#include "../../third_party/xbrz/xbrz.h"
#include <cassert>
#include <cstdio>
using namespace external;
static voxel::Image columns(uint32_t left,uint32_t right){
 voxel::Image image;image.width=image.height=2;image.pixels={left,right,left,right};image.depth={0,0,0,0};return image;
}
static camera::Surface draw(const voxel::Image&image,const lighting::State&light,float scale,
                            colorprecision::Mode mode=colorprecision::Mode::SmoothRGB,uint32_t background=0xff000000){
 camera::Surface out;assert(out.reset(16,16,background));out.precision(mode);out.blit(image,0,0,scale,scale,10,light,1,0,true);return out;
}
static uint32_t straight(uint32_t p){
 unsigned a=p>>24;if(!a)return 0;uint32_t out=a<<24;
 for(int shift:{0,8,16})out|=std::min(255u,(((p>>shift)&255)*255+a/2)/a)<<shift;return out;
}
static uint32_t over(uint32_t p,uint32_t background){
 unsigned a=p>>24;uint32_t out=0xff000000u;
 for(int shift:{0,8,16}){unsigned c=(((p>>shift)&255)*a+127)/255;out|=std::min(255u,c+((background>>shift)&255)*(255-a)/255)<<shift;}return out;
}
// Independent algorithm oracle: illuminate source samples first, call the
// unchanged upstream API, and composite its straight-alpha output. This is
// deliberately not a second implementation of the xBRZ neighborhood rules.
static std::vector<uint32_t> upstream(const voxel::Image&image,const lighting::State&light,
                                      colorprecision::Mode mode,int factor,uint32_t background){
 camera::Light transform(light,mode);std::vector<uint32_t> input(image.pixels.size()),scaled(input.size()*factor*factor);
 bool indexed=image.indices.size()==input.size();
 for(size_t n=0;n<input.size();++n)input[n]=straight(transform.apply(image.pixels[n],indexed?image.indices[n]:0,image.objectPalette));
 xbrz::scale(factor,input.data(),scaled.data(),image.width,image.height,xbrz::ColorFormat::argbUnbuffered);
 for(auto&p:scaled)p=over(p,background);return scaled;
}
static void matchesUpstream(const voxel::Image&image,const lighting::State&light,colorprecision::Mode mode,int factor){
 constexpr uint32_t background=0xff182838;auto out=draw(image,light,float(factor),mode,background);
 auto expected=upstream(image,light,mode,factor,background);int width=image.width*factor;
 for(int y=0;y<image.height*factor;++y)for(int x=0;x<width;++x){
  // Native transparency remains a depth/geometry admission rule: xBRZ may
  // soften an admitted edge but cannot add painted pixels outside the cutout.
  bool admitted=image.pixels[size_t(y/factor)*image.width+x/factor]>>24;
  assert(out.color[size_t(y)*out.width()+x]==(admitted?expected[size_t(y)*width+x]:background));
 }
}
int main(){
 const auto native=colorprecision::Mode::NativeSteps,smooth=colorprecision::Mode::SmoothRGB;
 lighting::State light;
 // Mature xBRZ consumes the live-lit RGB8 result. Low-light rounding and
 // overbright clipping are source inputs; no HDR reconstruction is claimed.
 auto edge=columns(0xff404040,0xffa0a0a0),dark=columns(0xff010101,0xff020202),highlight=columns(0xff720000,0xff8e0000);
 for(const auto*image:{&edge,&dark,&highlight})for(auto mode:{native,smooth})for(int brightness:{0,10,100,333,1000,1500,2000}){
  light={};light.intensity=brightness;matchesUpstream(*image,light,mode,2);
 }
 auto translucent=columns(0x80402010,0xc0603048);
 for(int brightness:{0,333,1000,2000}){light={};light.intensity=brightness;matchesUpstream(translucent,light,smooth,3);}
 light={};light.intensity=333;light.tint={{1250,750,1500}};matchesUpstream(dark,light,smooth,3);
 light={};light.intensity=2000;camera::Light bright(light,smooth);
 assert(bright.apply(highlight.pixels[0],0,false)==0xffe40000&&bright.apply(highlight.pixels[1],0,false)==0xffff0000);
 // Identical RGB with ordinary/emissive SHP indices must be illuminated using
 // each original material BEFORE xBRZ receives the resulting color image.
 auto emissive=columns(0xff606060,0xff606060);emissive.objectPalette=true;emissive.indices={1,245,1,245};
 for(int brightness:{0,100,900,2000}){light={};light.intensity=brightness;matchesUpstream(emissive,light,smooth,2);}
 emissive.objectPalette=false;light.intensity=900;matchesUpstream(emissive,light,smooth,2);
 emissive.objectPalette=true;emissive.indices={245,245,245,245};emissive.pixels={0xff606060,0xff686868,0xff606060,0xff686868};
 light.intensity=0;matchesUpstream(emissive,light,smooth,3);
 // Source identity alone cannot cache a lit frame. Exercise the same Surface
 // and the same vector addresses across light, palette, and index changes.
 camera::Surface reused;reused.precision(smooth);
 auto verifyReuse=[&]{assert(reused.reset(16,16,0xff182838));reused.blit(emissive,0,0,2,2,10,light,1,0,true);auto fresh=draw(emissive,light,2,smooth,0xff182838);assert(reused.color==fresh.color&&reused.depth==fresh.depth);};
 verifyReuse();light.intensity=1700;emissive.indices={1,245,1,245};verifyReuse();emissive.pixels[0]=0xff2080a0;verifyReuse();emissive.indices[0]=245;verifyReuse();
 // Ordinary SHP/TMP transparency is still nearest geometry admission, even at
 // fractional scales. Mature filtering cannot paint depth into an empty texel.
 auto cutout=columns(0,0xffffffff);light.intensity=2000;light.tint={{2000,2000,2000}};
 for(float scale:{1.25f,1.5f,2.f,2.5f,3.f,4.f}){
  auto scaled=draw(cutout,light,scale,smooth,0xff646464);
  for(int y=0;y<int(2*scale);++y)for(int x=0;x<int(2*scale);++x)if(int(std::floor((x+.5)/scale))==0){
   assert(scaled.color[size_t(y)*scaled.width()+x]==0xff646464);
   assert(scaled.depth[size_t(y)*scaled.width()+x]==-std::numeric_limits<float>::infinity());
  }
 }
 // True supersampled VXL coverage stays on its floating point resolve path,
 // preserving coverage and per-sample emissive gains independently of xBRZ.
 cutout.rasterScale=2;light={};light.intensity=2000;
 auto coverage=draw(cutout,light,1,smooth,0xff646464);
 assert(coverage.color[0]==0xffb2b2b2&&coverage.depth[0]==10);
 light.intensity=0;assert(draw(cutout,light,1,smooth,0xff646464).color[0]==0xff323232);
 emissive=columns(0xff606060,0xff606060);emissive.objectPalette=true;emissive.indices={1,245,1,245};emissive.rasterScale=2;
 assert(draw(emissive,light,1).color[0]==0xff303030);
 // Shadows are native coverage attenuation, independent of RGB precision.
 cutout.shadow=true;cutout.writeDepth=true;light.intensity=1000;
 for(int raster:{1,2})for(float scale:{1.f,1.5f,2.f}){
  cutout.rasterScale=raster;auto old=draw(cutout,light,scale,native,0xff646464),enhanced=draw(cutout,light,scale,smooth,0xff646464);
  assert(old.color==enhanced.color&&old.depth==enhanced.depth);
 }
 // A failed depth test must commit neither reconstructed color nor coverage.
 auto blocked=draw(edge,light,2);blocked.depth.assign(blocked.depth.size(),20);auto before=blocked.color;
 blocked.blit(highlight,0,0,1.5,1.5,10,light,1,0,true);assert(blocked.color==before);
 std::puts("PASS: live-lit RGB8 xBRZ matches unmodified upstream, precision/emissive/cache inputs, native cutout admission, VXL float coverage, and unchanged shadow/depth rules");
}

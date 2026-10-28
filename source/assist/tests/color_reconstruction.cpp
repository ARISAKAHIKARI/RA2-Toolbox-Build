#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdio>
using namespace external;
static voxel::Image columns(uint32_t left,uint32_t right){
 voxel::Image image;image.width=image.height=2;image.pixels={left,right,left,right};image.depth={0,0,0,0};return image;
}
static camera::Surface draw(const voxel::Image&image,const lighting::State&light,float scale,
                            colorprecision::Mode mode=colorprecision::Mode::SmoothRGB,uint32_t background=0xff000000){
 camera::Surface out;assert(out.reset(8,8,background));out.precision(mode);out.blit(image,0,0,scale,scale,10,light,1,0,true);return out;
}
static uint32_t prequantizedMiddle(const voxel::Image&image,const lighting::State&light){
 camera::Light transform(light,colorprecision::Mode::SmoothRGB);
 return pixelupgrade::adaptivePixel(transform.apply(image.pixels[0],1,false),transform.apply(image.pixels[1],1,false),
                                    transform.apply(image.pixels[2],1,false),transform.apply(image.pixels[3],1,false),.5,.5);
}
int main(){
 const auto native=colorprecision::Mode::NativeSteps,smooth=colorprecision::Mode::SmoothRGB;
 lighting::State light;
 // A material edge keeps its position when the live light changes. The old
 // post-light guide classified the dark 6/16 output as a smooth gradient (9).
 auto edge=columns(0xff404040,0xffa0a0a0);
 for(int brightness:{0,10,100,500,1000,1500,2000}){
  light.intensity=brightness;auto out=draw(edge,light,2);
  assert(out.color[1*8+1]==camera::Light(light,smooth).apply(edge.pixels[0],1,false));
 }
 light.intensity=100;assert(draw(edge,light,2).color[9]==0xff060606);
 // Low-contrast source shades are filtered without an intermediate RGB8
 // round: (1+2)/2 * .333 = .4995, not average(round(.333),round(.666)).
 auto dark=columns(0xff010101,0xff020202);light.intensity=333;
 assert(prequantizedMiddle(dark,light)==0xff010101);
 assert(draw(dark,light,1.5).color[9]==0xff000000);
 light.tint={{1250,750,1500}};
 assert(draw(dark,light,1.5).color[9]==0xff010001);
 // Reconstruct overbright source contributions before the LDR clamp.
 // 114*2 and 142*2 average to 256; clipping 284 first incorrectly gives 242.
 light={};light.intensity=2000;auto highlight=columns(0xff720000,0xff8e0000);
 assert(prequantizedMiddle(highlight,light)==0xfff20000);
 assert(draw(highlight,light,1.5).color[9]==0xffff0000);
 // NativeSteps still follows the existing quantize-then-filter contract,
 // including its old edge decisions and truncating illumination arithmetic.
 for(const auto*image:{&edge,&dark,&highlight})for(int brightness:{100,333,1000,2000}){
  light={};light.intensity=brightness;camera::Light transform(light,native);auto out=draw(*image,light,1.5,native);
  uint32_t expected=pixelupgrade::adaptivePixel(transform.apply(image->pixels[0],1,false),transform.apply(image->pixels[1],1,false),
                                               transform.apply(image->pixels[2],1,false),transform.apply(image->pixels[3],1,false),.5,.5);
  assert(out.color[9]==expected);
 }
 // Identical source RGB can still represent ordinary and emissive SHP
 // indices. Their material boundary must not interpolate a new light class.
 auto emissive=columns(0xff606060,0xff606060);emissive.objectPalette=true;emissive.indices={1,245,1,245};
 light={};light.intensity=900;auto out=draw(emissive,light,2);
 assert(out.color[9]==0xff565656&&out.color[10]==0xff606060);
 emissive.objectPalette=false;out=draw(emissive,light,2);assert(out.color[9]==0xff565656&&out.color[10]==0xff565656);
 emissive.objectPalette=true;emissive.indices={245,245,245,245};emissive.pixels={0xff606060,0xff686868,0xff606060,0xff686868};
 light.intensity=0;assert(draw(emissive,light,1.5).color[9]==0xff646464);
 // Ordinary SHP/TMP alpha cutouts remain nearest geometry, even at a
 // fractional scale and under maximal light. No transparent fringe is added.
 auto cutout=columns(0,0xffffffff);light.intensity=2000;light.tint={{2000,2000,2000}};
 for(float scale:{1.5f,2.f,3.f}){
  auto scaled=draw(cutout,light,scale,smooth,0xff646464);
  for(int y=0;y<int(2*scale);++y)for(int x=0;x<int(2*scale);++x)
   assert(scaled.color[size_t(y)*8+x]==(x<int(scale)?0xff646464:0xffffffff));
 }
 // True supersampled VXL coverage must stay fractional. Resolve first,
 // clamp to premultiplied alpha, then composite and round once: 177.5 -> 178.
 cutout.rasterScale=2;light={};light.intensity=2000;
 auto coverage=draw(cutout,light,1,smooth,0xff646464);
 assert(coverage.color[0]==0xffb2b2b2&&coverage.depth[0]==10);
 light.intensity=0;assert(draw(cutout,light,1,smooth,0xff646464).color[0]==0xff323232);
 // Supersampled ordinary/emissive samples also retain their own gains; a
 // coverage resolve cannot assign the blended RGB a single palette index.
 emissive=columns(0xff606060,0xff606060);emissive.objectPalette=true;emissive.indices={1,245,1,245};emissive.rasterScale=2;
 assert(draw(emissive,light,1).color[0]==0xff303030);
 // Shadows are coverage attenuation, independent of the RGB precision mode.
 cutout.shadow=true;cutout.writeDepth=true;light.intensity=1000;
 for(int raster:{1,2})for(float scale:{1.f,1.5f,2.f}){
  cutout.rasterScale=raster;auto old=draw(cutout,light,scale,native,0xff646464),enhanced=draw(cutout,light,scale,smooth,0xff646464);
  assert(old.color==enhanced.color&&old.depth==enhanced.depth);
 }
 // A rejected Z test never commits a reconstructed color or coverage.
 auto blocked=draw(edge,light,2);blocked.depth.assign(blocked.depth.size(),20);auto before=blocked.color;
 blocked.blit(highlight,0,0,1.5,1.5,10,light,1,0,true);assert(blocked.color==before);
 std::puts("PASS: unlit material edges, one final RGB round and highlight clamp, native reconstruction compatibility, emissive semantics, cutouts, VXL fractional coverage and unchanged shadow/depth rules");
}

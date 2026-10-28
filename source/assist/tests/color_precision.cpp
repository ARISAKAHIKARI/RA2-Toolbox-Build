#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdio>
#include <set>
using namespace external;
static uint32_t legacy(uint32_t pixel,uint8_t index,bool object,const lighting::State&s){
 if(object&&index>=240&&index<=254)return pixel;
 int brightness=std::clamp(s.intensity<0?s.ambient*10:s.intensity,0,2000);
 int shade=std::min(254,(261*brightness)>>11),level=std::min(62,127*shade*62/32258);
 uint32_t out=pixel&0xff000000;unsigned alpha=pixel>>24;
 for(int c=0;c<3;++c){int shift=16-c*8;float gain=level/31.f*std::clamp(s.tint[c],0,2000)/1000.f;out|=unsigned(std::clamp(((pixel>>shift)&255)*gain,0.f,float(alpha)))<<shift;}
 return out;
}
static uint32_t roundtrip565(uint32_t pixel){
 unsigned r=((pixel>>16)&255)>>3,g=((pixel>>8)&255)>>2,b=(pixel&255)>>3;
 return (pixel&0xff000000)|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);
}
int main(){
 const auto native=colorprecision::Mode::NativeSteps,smooth=colorprecision::Mode::SmoothRGB;
 lighting::State s;std::set<uint32_t> oldRamp,newRamp;
 for(int i=0;i<=1000;++i){s.intensity=i;oldRamp.insert(camera::Light(s,native).apply(0xffffffff,1,false));newRamp.insert(camera::Light(s,smooth).apply(0xffffffff,1,false));}
 assert(oldRamp.size()==32&&newRamp.size()==256);
 s.intensity=500;assert(camera::Light(s,smooth).apply(0xffffffff,1,false)==0xff808080);
 assert(camera::Light(s,native).apply(0xffffffff,1,false)==0xff7b7b7b);
 // Original compatibility arithmetic and truncation remain bit-for-bit.
 for(int b=-100;b<=2200;b+=17)for(int tint=-50;tint<=2100;tint+=91){s.intensity=b;s.tint={{tint,1000,2000-tint/2}};
  for(uint32_t p:{0xff804020u,0x80402010u,0u})assert(camera::Light(s).apply(p,1,true)==legacy(p,1,true,s));
 }
 s={};s.intensity=375;s.tint={{1250,750,500}};uint32_t p=0x80604020;
 auto lit=camera::Light(s,smooth).apply(p,1,true);assert((lit>>24)==128);
 for(int sh:{0,8,16})assert(((lit>>sh)&255)<=(lit>>24));
 for(unsigned index=240;index<=254;++index)assert(camera::Light(s,smooth).apply(p,uint8_t(index),true)==p);
 assert(camera::Light(s,smooth).apply(p,239,true)!=p&&camera::Light(s,smooth).apply(p,245,false)!=p);
 s.intensity=2000;s.tint={{2000,2000,2000}};assert(camera::Light(s,smooth).apply(p,1,true)==0x80808080);
 s.intensity=0;assert(camera::Light(s,smooth).apply(p,1,true)==0x80000000);
 assert(camera::Light(s,smooth).apply(0,1,false)==0);
 // High-precision lighting preserves unquantized RGB; expanding a 565-only
 // source cannot reconstruct the original channel values.
 s={};uint32_t original=0xff818283,from565=roundtrip565(original);assert(original!=from565);
 assert(camera::Light(s,smooth).apply(original,1,false)==original);
 assert(camera::Light(s,smooth).apply(from565,1,false)==from565);
 std::set<uint32_t> rgbColors,rgb565Colors;
 for(unsigned c=0;c<256;++c){uint32_t red=0xff000000|(c<<16);rgbColors.insert(camera::Light(s,smooth).apply(red,1,false));rgb565Colors.insert(roundtrip565(red));}
 assert(rgbColors.size()==256&&rgb565Colors.size()==32);
 voxel::Image image;image.width=image.height=1;image.pixels={0xffffffff};image.depth={0};
 camera::Surface a,b;assert(a.reset(2,2)&&b.reset(2,2));b.precision(smooth);s.intensity=500;
 a.blit(image,0,0,1,1,10,s);b.blit(image,0,0,1,1,10,s);
 assert(a.color[0]==0xff7b7b7b&&b.color[0]==0xff808080);
 assert(a.depth==b.depth&&a.precision()==native&&b.precision()==smooth);
 assert(b.reset(2,2)&&b.precision()==smooth);b.precision(native);assert(b.precision()==native);
 // Shadow attenuation and coverage are independent of the light ramp mode.
 image.shadow=true;image.pixels={0xffffffff};image.depth={1};a.reset(2,2,0xff808080);b.reset(2,2,0xff808080);b.precision(smooth);
 a.blit(image,0,0,1,1,0,s);b.blit(image,0,0,1,1,0,s);assert(a.color==b.color&&a.color[0]==0xff404040);
 image.shadow=false;image.objectPalette=true;image.indices={245};image.pixels={0xff804020};
 std::vector<uint32_t> preview;camera::previewPixels(image,s,128,preview,smooth);assert(preview[0]==0x80402010);
 std::puts("PASS: 256-level smooth RGB light ramp, exact native compatibility, retained source RGB versus 565 loss, independent surface modes, alpha/emissive/depth/shadow preservation");
}

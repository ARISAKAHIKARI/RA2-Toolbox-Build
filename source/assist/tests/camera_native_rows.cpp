#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;uint64_t hash=1469598103934665603ull;unsigned cases=0;
 auto fold=[&](uint32_t v){hash=(hash^v)*1099511628211ull;};
 for(auto precision:{colorprecision::Mode::NativeSteps,colorprecision::Mode::SmoothRGB})
 for(int scale:{1,2,4})for(int zmode=0;zmode<3;++zmode)for(unsigned flags=0;flags<32;++flags)
 for(float fraction:{0.f,1.f/256,.5f,255.f/256}){
  camera::Surface canvas;assert(canvas.reset(48,36,0xff6183a7));canvas.precision(precision);
  for(size_t n=0;n<canvas.depth.size();++n)canvas.depth[n]=float(int(n%31)-24)/4;
  voxel::Image image;image.rasterScale=scale;image.width=13;image.height=9;image.x=-4*scale;image.y=-3*scale;
  image.pixels.resize(117);image.indices.resize(117);image.objectPalette=true;image.flat=zmode==1;
  image.shadow=(flags&1)!=0;image.writeDepth=(flags&2)!=0;image.skipZ=(flags&4)!=0;image.depthBias=-1.25f;
  if(flags&8)image.depthBottom=2*scale;if(zmode==0)image.depth.resize(117);
  for(unsigned n=0;n<117;++n){unsigned a=n%5==0?0:n%5==1?124:255;
   image.pixels[n]=(a<<24)|((n*13%(a+1))<<16)|((n*17%(a+1))<<8)|(n*23%(a+1));image.indices[n]=n%7?20:245;
   if(zmode==0)image.depth[n]=n%17?float(int(n%11)-5):std::numeric_limits<float>::quiet_NaN();}
  auto attachment=image;attachment.attachments.clear();attachment.y+=2*scale;attachment.shadow=true;attachment.flat=true;attachment.skipZ=false;attachment.writeDepth=false;
  image.attachments.push_back(std::move(attachment));image.shadowsAfter=(flags&16)!=0;
  lighting::State light;if(flags&8){light.intensity=731;light.tint={{1200,800,1400}};}
  canvas.blit(image,17.25f+fraction,13.5f-fraction,float(scale),float(scale),1.75f,light,2,3,true);
  for(uint32_t p:canvas.color)fold(p);for(float z:canvas.depth){uint32_t bits;std::memcpy(&bits,&z,4);fold(bits);}++cases;
 }
 // Captured from the pre-optimization scalar renderer at 09f172611a0466fb.
 // Includes RGBA AND depth, fractional origins, 1/2/4x material density,
 // tinted/emissive pixels, NaN/no-Z paths and ordered shadow attachments.
 assert(cases==2304&&hash==12758203375126593659ull);
 std::printf("PASS native row scalar oracle: cases=%u RGBA/depth hash=%llu\n",cases,(unsigned long long)hash);
}

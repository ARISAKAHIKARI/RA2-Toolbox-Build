#ifndef RA2_RASTER_HEADER
#define RA2_RASTER_HEADER "../../external_reader/source/camera_raster.h"
#endif
#include RA2_RASTER_HEADER
#include <cstdio>
using namespace external;

// Compile this identical fixture against the previous rasterizer and the new
// one. Hash every ARGB, float-Z, native-Z and A element after each ordered draw.
// This is a regression comparison, not a native-game visual oracle.
int main(){
 uint64_t hash=1469598103934665603ull;size_t draws=0;
 auto fold=[&](uint32_t v){hash=(hash^v)*1099511628211ull;};
 for(unsigned flags=0;flags<1024;++flags){
  camera::Surface surface;surface.reset(79,61,0x92334961);
  surface.nativeView(9,1,1,79,67,3);surface.outputRowOrigin(2);surface.writeRows(2,59);
  auto alpha=std::make_shared<std::vector<uint8_t>>(surface.color.size());
  for(size_t n=0;n<alpha->size();++n)(*alpha)[n]=uint8_t(n*17+flags);
  surface.alpha(alpha);
  for(size_t n=0;n<surface.color.size();++n){surface.nativeZ[n]=uint16_t(32640+n%180);surface.depth[n]=float(int(n%73)-19);}
  voxel::Image image;image.width=37;image.height=29;image.x=-5;image.y=-3;
  image.flat=flags&1;image.skipZ=flags&2;image.writeDepth=!(flags&4);image.objectPalette=flags&8;
  image.nativeDepth.enabled=true;image.nativeDepth.gradient=nativesurface::Gradient(flags%4);
  image.nativeDepth.tileEqual=flags&16;image.nativeDepth.voxel=flags&32;
  image.depthBias=.75f;image.depthBottom=(flags&64)?39:std::numeric_limits<int>::min();
  const size_t count=size_t(image.width)*image.height;image.pixels.resize(count);image.indices.resize(count);
  for(size_t n=0;n<count;++n){unsigned a=n%11<4?0:n%7==0?91:255;
   image.pixels[n]=(a<<24)|((uint32_t(n)*0x170b03)&0x3f3f3f);image.indices[n]=uint8_t(n*19);
  }
  if(flags&128){image.depth.resize(count);for(size_t n=0;n<count;++n)image.depth[n]=n%13?float(int(n%23)-11):std::numeric_limits<float>::quiet_NaN();}
  if(flags&256){image.nativeShape.resize(count);image.nativeShapeCoverage.resize(count);
   for(size_t n=0;n<count;++n){image.nativeShape[n]=int(n%39)-19;image.nativeShapeCoverage[n]=n%5!=0;}}
  lighting::State light;light.intensity=int(flags*13%2300)-100;light.tint={{1327,701,1999}};
  light.rgb555=flags&512;light.scalar=flags&32;light.paletteRows=(flags&64)?1:53;
  if(flags&8){auto table=std::make_shared<std::vector<uint32_t>>(light.paletteRows*256);
   for(size_t n=0;n<table->size();++n)(*table)[n]=uint16_t(n*137+101);light.table=table;}
  camera::Material material;material.nativeZAdjust=-7;material.clipBottom=(flags&16)?19.25f:1e7f;material.bridgeSplit=flags&64;
  for(unsigned mode=0;mode<8;++mode){
   // First two calls select the new kernel (integer/fractional placement).
   // Remaining calls exercise ordered fallbacks and overlap/equal-Z behavior.
   auto draw=image;auto mat=material;
   if(mode==2)draw.shadow=true;
   if(mode==3){draw.nativeBlend=2;draw.blendSource=std::make_shared<const std::vector<uint32_t>>(draw.pixels);}
   if(mode==4)mat.translucency=2;
   if(mode==5)mat.mixStrength=127;
   if(mode==6){mat.displace=true;mat.cloakWords=-3;draw.nativeDepth.voxel=true;}
   if(mode==7){auto part=image;part.x+=4;part.y+=3;draw.attachments.push_back(std::move(part));}
   const float scale=mode==7?1.25f:1.f;
   surface.blit(draw,mode==1?20.25f:20.f,mode==1?17.75f:17.f,scale,scale,31.5f,light,1.25f);
   for(auto v:surface.color)fold(v);for(float v:surface.depth){uint32_t bits;std::memcpy(&bits,&v,4);fold(bits);}
   for(auto v:surface.nativeZ)fold(v);for(size_t n=0;n<surface.color.size();++n)fold(surface.alphaAt(n));++draws;
  }
 }
 std::printf("{\"ordered_draws\":%zu,\"four_plane_digest\":\"%llu\",\"gpu_executed\":false}\n",draws,(unsigned long long)hash);
}

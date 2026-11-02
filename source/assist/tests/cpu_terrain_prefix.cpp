#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_record.h"
#include RA2_REFERENCE_RECORD
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace external;
double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(){
 constexpr int Width=600,Height=300,Count=10000;camera::GpuSceneRecord after;camera::BeforeGpuSceneRecord before;
 auto base=std::make_shared<const std::vector<uint32_t>>(Width*Height,0xff193347u);auto depth=std::make_shared<const std::vector<float>>(Width*Height,-std::numeric_limits<float>::infinity());
 auto fog=std::make_shared<std::vector<uint8_t>>(Width*Height,127);auto tile=std::make_shared<voxel::Image>();tile->width=60;tile->height=30;tile->pixels.assign(1800,0xff2266aau);tile->nativeDepth.enabled=true;tile->nativeDepth.gradient=nativesurface::Gradient::Flat;
 auto actor=std::make_shared<voxel::Image>();actor->width=actor->height=8;actor->pixels.assign(64,0xffee3344);actor->skipZ=true;actor->writeDepth=false;
 camera::Material material;material.nativeZ=true;lighting::State light;
 auto record=[&](auto&r,int n){r.begin(Width,Height,base,depth,colorprecision::Mode::NativeSteps,{}, {},{},n>0);r.nativeView(.1,.1,true);r.fog(fog);
  if(!n){for(int y=0;y<100;++y)for(int x=0;x<100;++x)assert(r.append(*tile,tile,x*6.f,y*3.f,.1f,.1f,float(y),light,1,0,false,{},material));r.retainPrefix();}
  assert(r.append(*actor,actor,float(n*7%580),120,1,1,1000,light,1,0,false));};
 camera::Surface oldSurface,newSurface;double oldMs=0,newMs=0;unsigned hits=0;
 for(int n=0;n<32;++n){record(before,n);record(after,n);const double a=clockMs();assert(before.replay(oldSurface));oldMs+=clockMs()-a;const double b=clockMs();assert(after.replay(newSurface));newMs+=clockMs()-b;
  assert(oldSurface.color==newSurface.color&&oldSurface.depth==newSurface.depth&&oldSurface.nativeZ==newSurface.nativeZ&&*oldSurface.alphaPlane()==*newSurface.alphaPlane());hits+=after.cpuPrefixReused();if(n)assert(after.cpuPrefixReused());
 }
 // A real native alpha change must replay the terrain with the new palette
 // admission. Recoloring old output after the native pass would be incorrect.
 fog=std::make_shared<std::vector<uint8_t>>(*fog);std::fill(fog->begin(),fog->begin()+Width*80,63);record(before,32);record(after,32);assert(before.replay(oldSurface)&&after.replay(newSurface)&&!after.cpuPrefixReused());assert(oldSurface.color==newSurface.color&&oldSurface.nativeZ==newSurface.nativeZ);
 record(before,33);record(after,33);assert(before.replay(oldSurface)&&after.replay(newSurface)&&after.cpuPrefixReused()&&oldSurface.color==newSurface.color);
 // Precision/coordinate changes are explicit keys even for a caller that
 // retains the same prefix. New owners may never alias a previous prefix.
 after.nativeView(.2,.1,true);assert(after.replay(newSurface)&&!after.cpuPrefixReused());
 base=std::make_shared<const std::vector<uint32_t>>(*base);record(after,34);assert(after.replay(newSurface)&&!after.cpuPrefixReused());
 std::printf("PASS actual GpuSceneRecord CPU replay: %d static TMP calls + moving actor at %dx%d from 6000x3000 source; 32 RGBA/Z/nativeZ/A comparisons equal; prefix hits %u/31 eligible. Mean %.6f -> %.6f ms; alpha/scale/base-owner invalidation. Cache plane bytes=%zu, maximum bounded plane bytes=%zu.\n",Count,Width,Height,hits,oldMs/32,newMs/32,size_t(Width)*Height*11,size_t(2)*1024*1024*11);
}

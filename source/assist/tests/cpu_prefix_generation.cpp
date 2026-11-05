#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_record.h"
#include RA2_REFERENCE_RECORD
#include <cassert>
#include <cstdio>
using namespace external;
template<class T>static bool same(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(!a.size()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(T)));}
static bool same(const camera::Surface&a,const camera::Surface&b){if(!same(a.color,b.color)||!same(a.depth,b.depth)||!same(a.nativeZ,b.nativeZ))return false;for(size_t n=0;n<a.color.size();++n)if(a.alphaAt(n)!=b.alphaAt(n))return false;return true;}
static std::shared_ptr<const voxel::Image> image(uint32_t color){auto i=std::make_shared<voxel::Image>();i->width=i->height=8;i->pixels.assign(64,color);i->nativeDepth.enabled=true;i->nativeDepth.gradient=nativesurface::Gradient::Flat;i->flat=true;return i;}
template<class R>static void draw(R&r,const std::shared_ptr<const voxel::Image>&i,float x,float y){camera::Material m;m.nativeZ=true;assert(r.append(*i,i,x,y,1,1,y,{},1,0,false,{},m));}
int main(){
 constexpr int W=64,H=48;auto base=std::make_shared<const std::vector<uint32_t>>(W*H,0xff123456);auto z=std::make_shared<const std::vector<float>>(W*H,-10000);
 auto fog=std::make_shared<const std::vector<uint8_t>>(W*H,127);auto a=image(0xffff0000),b=image(0xff00ff00);
 camera::GpuSceneRecord current,cold;camera::BeforeGpuSceneRecord before;camera::Surface now,reference,old;
 auto begin=[&](auto&r,bool keep=false){r.begin(W,H,base,z,colorprecision::Mode::NativeSteps,{}, {},{},keep);r.nativeView(1,1,true);r.fog(fog);};
 begin(current);begin(before);draw(current,a,6,8);draw(before,a,6,8);current.retainPrefix();before.retainPrefix();assert(current.replay(now)&&before.replay(old)&&same(now,old));
 // Retaining again captures an extended static prefix. Owners/dimensions are
 // unchanged, so an image key without the prefix generation is insufficient.
 draw(current,b,30,15);draw(before,b,30,15);current.retainPrefix();before.retainPrefix();
 begin(cold);draw(cold,a,6,8);draw(cold,b,30,15);cold.retainPrefix();assert(cold.replay(reference));
 assert(current.replay(now)&&before.replay(old));assert(same(now,reference));assert(!same(old,reference));
 assert(!current.cpuPrefixReused());
 // Ordinary begin-with-retained-prefix still reuses the completed new prefix.
 begin(current,true);assert(current.replay(now)&&current.cpuPrefixReused()&&same(now,reference));
 puts("PASS extended/re-retained CPU prefix: old production negative control misses new draw; current RGBA/floatZ-bits/nativeZ/A equals cold complete replay; ordinary retained begin still hits cache");
}

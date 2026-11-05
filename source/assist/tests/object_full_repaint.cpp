#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef OBJECT_CANVAS_HEADER
#define OBJECT_CANVAS_HEADER "../../external_reader/source/object_canvas.h"
#endif
#include OBJECT_CANVAS_HEADER
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
using namespace external;
static std::shared_ptr<const voxel::Image> image(int w,int h,uint32_t color,bool cloak=false){
 auto p=std::make_shared<voxel::Image>();p->width=w;p->height=h;p->nativeDepth.enabled=true;p->nativeDepth.voxel=cloak;p->nativeDepth.gradient=nativesurface::Gradient::Flat;p->pixels.resize(size_t(w)*h);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)p->pixels[size_t(y)*w+x]=color^uint32_t((x*3+y*7)&31);return p;
}
static uint64_t hash(const camera::NativeCanvas&out){uint64_t h=1469598103934665603ull;for(int y=0;y<out.height();++y)for(int x=0;x<out.width();++x){float d=out.depth(x,y);uint32_t bits;std::memcpy(&bits,&d,sizeof bits);for(uint32_t v:{out.pixel(x,y),bits,uint32_t(out.nativeDepth(x,y)),uint32_t(out.nativeAlphaAt(x,y))})h=(h^v)*1099511628211ull;}return h;}
static void run(int w,int h,bool cloak){
 camera::NativeCanvas base;assert(base.reset(w,h));auto tile=image(64,32,0xff408060),actor=image(48,48,0xffbb8844),hidden=image(48,48,0xff5070a0,true);
 auto ground=[&](int intensity){lighting::State light;light.intensity=intensity;size_t id=0;for(int y=0;y<h;y+=32)for(int x=0;x<w;x+=64)base.terrainSource(id++,tile,float(x),float(y),0,light,0);};ground(1000);
 std::vector<camera::ObjectCanvas::Draw>draws;for(int n=0;n<64;++n){camera::ObjectCanvas::Draw d;d.image=actor;d.x=float(80+n%16*(w-160)/16);d.y=float(80+n/16*(h-160)/4);d.base=100;d.lift=0;d.material.nativeZ=true;draws.push_back(d);}
 if(cloak){auto d=draws[0];d.image=hidden;d.x=127.25f;d.y=127.75f;d.material.displace=true;d.material.cloakWords=-w-129;draws.insert(draws.begin()+1,d);}
 camera::ObjectCanvas renderer;camera::NativeCanvas out;assert(renderer.compose(base,draws,out));out={};double samples[8];size_t changed=0,reused=0;uint64_t hashes[2]={};
 for(int n=0;n<8;++n){ground(n%2?1000:800);auto at=std::chrono::steady_clock::now();assert(renderer.compose(base,draws,out,true));samples[n]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-at).count();changed+=renderer.stats().changed;reused+=renderer.stats().reused;
  if(n<2)hashes[n]=hash(out);out={};}
 std::sort(std::begin(samples),std::end(samples));assert(hashes[0]!=hashes[1]);std::printf("FULL %dx%d cloak=%d median_ms=%.6f changed_mean=%.3f reused_mean=%.3f hashes=%llu,%llu\n",w,h,int(cloak),(samples[3]+samples[4])/2,changed/8.,reused/8.,(unsigned long long)hashes[0],(unsigned long long)hashes[1]);
}
int main(){run(4096,2048,false);run(4096,2048,true);run(2560,1440,false);run(2560,1440,true);}

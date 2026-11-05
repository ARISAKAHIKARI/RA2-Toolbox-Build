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
using Draw=camera::ObjectCanvas::Draw;
static std::shared_ptr<voxel::Image> image(int w,int h,uint32_t color,bool voxel=false){
 auto out=std::make_shared<voxel::Image>();out->width=w;out->height=h;out->pixels.resize(size_t(w)*h);out->nativeDepth.enabled=true;out->nativeDepth.voxel=voxel;out->nativeDepth.gradient=nativesurface::Gradient::Flat;
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)out->pixels[size_t(y)*w+x]=color^uint32_t((x*3+y*7)&31);return out;
}
static camera::NativeCanvas ground(int w,int h){
 camera::NativeCanvas out;assert(out.reset(w,h));auto tile=image(64,32,0xff408060);size_t id=0;
 for(int y=0;y<h;y+=32)for(int x=0;x<w;x+=64){out.terrainSource(id++,tile,float(x),float(y),0,{},0);}
 return out;
}
static void equal(const camera::NativeCanvas&a,const camera::NativeCanvas&b){
 assert(a.width()==b.width()&&a.height()==b.height());for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x){
  if(a.pixel(x,y)!=b.pixel(x,y)||a.depth(x,y)!=b.depth(x,y)||a.nativeDepth(x,y)!=b.nativeDepth(x,y)||a.nativeAlphaAt(x,y)!=b.nativeAlphaAt(x,y)){
   std::fprintf(stderr,"mismatch (%d,%d) rgba %08x/%08x depth %f/%f native %u/%u\n",x,y,a.pixel(x,y),b.pixel(x,y),a.depth(x,y),b.depth(x,y),a.nativeDepth(x,y),b.nativeDepth(x,y));assert(false);
  }
 }
}
static Draw object(std::shared_ptr<const voxel::Image>im,float x,float y,int offset=0,bool cloak=false){
 Draw d;d.image=std::move(im);d.x=x;d.y=y;d.base=100;d.lift=0;d.material.nativeZ=true;d.material.displace=cloak;d.material.cloakWords=offset;return d;
}
static void correctness(){
 auto base=ground(1024,768);auto body=image(50,45,0xffc07020),cloak=image(49,43,0xff7060c0,true);
 std::vector<Draw>draws{object(body,120.25f,120.75f),object(cloak,127.25f,125.75f,-129,true),object(body,126,120),object(cloak,250.5f,251.5f,1024+131,true),object(cloak,380.25f,378.75f,-1024-255,true),object(body,760,600)};
 camera::ObjectCanvas warm;camera::NativeCanvas out;unsigned comparisons=0;
 auto check=[&]{camera::ObjectCanvas cold;camera::NativeCanvas reference;assert(cold.compose(base,draws,reference));assert(warm.compose(base,draws,out,true));equal(out,reference);++comparisons;};
 check();check();assert(warm.stats().changed==0);auto old=out;camera::ObjectCanvas frozenRenderer;camera::NativeCanvas frozen;assert(frozenRenderer.compose(base,draws,frozen));
 draws[1].x+=4;draws[1].y+=5;check();equal(old,frozen);assert(warm.stats().changed>0);
#ifdef EXPECT_SCOPED_CLOAK
 assert(warm.stats().reused>0&&warm.stats().changed<48);
#endif
 // A late opaque writer in the displacement source must not be read early.
 draws[2].x+=3;draws[2].y+=1;check();draws[0].light.intensity=550;check();
 // Read dependencies can cross rows, be negative, or leave the framebuffer.
 for(int offset:{-1,1,-1025,1025,-1000000,1000000,1024*600+200}){draws[1].material.cloakWords=offset;check();}
 // Multiple cloaks form transitive dependencies; move either end and order.
 draws[1].material.cloakWords=120;draws[3].material.cloakWords=-128;draws[4].material.cloakWords=-128;check();
 std::swap(draws[0],draws[4]);check();draws.erase(draws.begin()+1);check();
 auto attached=std::make_shared<voxel::Image>(*cloak);auto part=*cloak;part.x=-35;part.y=5;part.shadow=true;part.airborneShadow=true;attached->attachments.push_back(part);attached->shadowsAfter=true;
 draws.push_back(object(attached,510.25f,126.75f,-1024+129,true));draws.back().drop=15;check();draws.back().drop=0;check();draws.back().material.clipBottom=9;check();
 auto alpha=std::make_shared<camera::NativeCanvas::AlphaPages>();(*alpha)[257]=std::make_shared<const std::vector<uint8_t>>(128*128,63);base.nativeAlpha(alpha);check();
 for(int scale:{2,3}){auto scaled=std::make_shared<voxel::Image>(*attached);scaled->rasterScale=scale;scaled->x=-17;scaled->y=-13;
  draws.push_back(object(scaled,127.25f,255.75f,-1024-129,true));check();draws.back().x+=3;draws.back().y-=2;check();draws.pop_back();check();
 }
 base=ground(517,333);draws={object(cloak,505.25f,126.5f,7,true),object(cloak,120.5f,253.75f,-521,true),object(body,3,129)};check();draws[2].x+=1;check();draws[0].x-=4;check();
 // Same-frame base mutation outside any actor's destination still changes a
 // reader. No retained TMP sources are needed for this source-only case.
 camera::NativeCanvas sparse;assert(sparse.reset(1024,768));auto one=image(1,1,0xff00aa22);sparse.blit(*one,20,20,1,1,0,{},0);
 base=sparse;draws={object(cloak,500,500,20*1024+20-(500*1024+500),true)};warm.clear();check();
 const uint32_t originalSource=base.pixel(20,20);assert(out.pixel(500,500)==nativepacked::blend(originalSource,0,0));
 auto altered=image(1,1,0xffdd3377);altered->skipZ=true;base.blit(*altered,20,20,1,1,0,{},0);assert(base.pixel(20,20)!=originalSource);check();
 assert(out.pixel(500,500)==nativepacked::blend(base.pixel(20,20),0,0));
 // Removal restores its old destination; clearing the last cloak returns to
 // independent row workers without retaining stale read dependencies.
 draws.clear();check();draws={object(body,40,40)};check();
 // A pinned front plus the authoritative shared budget may refuse a COW
 // update. It must not publish a partial frame; after release a cold retry
 // must recover, including dependencies formerly outside a local replay.
 auto budget=std::make_shared<camera::NativeCanvas::Budget>();budget->maximum=4;camera::NativeCanvas tight;assert(tight.reset(512,128,budget));
 for(int n=0;n<4;++n)tight.terrainSource(size_t(n),body,float(n*128),0,0,{},0);
 std::vector<Draw>tightDraws{object(cloak,120,10,130,true)};camera::ObjectCanvas bounded;camera::NativeCanvas front;assert(bounded.compose(tight,tightDraws,front));tightDraws[0].x+=1;
 camera::NativeCanvas rejected;assert(!bounded.compose(tight,tightDraws,rejected,true));assert(rejected.failed());front={};rejected={};
 assert(bounded.compose(tight,tightDraws,rejected,true));std::vector<uint32_t>recovered;std::vector<float>recoveredDepth;std::vector<uint16_t>recoveredZ;
 for(int y=0;y<128;++y)for(int x=0;x<512;++x){recovered.push_back(rejected.pixel(x,y));recoveredDepth.push_back(rejected.depth(x,y));recoveredZ.push_back(rejected.nativeDepth(x,y));}
 camera::ObjectCanvas tightCold;camera::NativeCanvas tightReference;rejected={};bounded.clear();assert(tightCold.compose(tight,tightDraws,tightReference));assert(tightReference.pageCount()==4);
 for(int y=0;y<128;++y)for(int x=0;x<512;++x){size_t n=size_t(y)*512+x;assert(recovered[n]==tightReference.pixel(x,y)&&recoveredDepth[n]==tightReference.depth(x,y)&&recoveredZ[n]==tightReference.nativeDepth(x,y));}
 std::printf("PASS cloak replay: %u full RGBA/depth/native-Z/alpha comparisons; boundary moves, late source writers, signed/wrapped/outside offsets, chained cloaks, attachment/drop/clip, source-only base mutation, removal and retained front\n",comparisons);
}
static void benchmark(){
 auto base=ground(4096,2048);auto body=image(48,48,0xffbb8844),cloak=image(48,48,0xff5070a0,true);std::vector<Draw>draws;
 for(int n=0;n<64;++n)draws.push_back(object(body,float(80+n%16*240),float(80+n/16*400)));
 draws.push_back(object(cloak,180,180,-129,true));camera::ObjectCanvas renderer;camera::NativeCanvas out;assert(renderer.compose(base,draws,out));out={};
 double samples[12];size_t changed=0,reused=0;for(int n=0;n<12;++n){draws.back().x=float(180+(n+1)%2);auto at=std::chrono::steady_clock::now();assert(renderer.compose(base,draws,out,true));samples[n]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-at).count();changed+=renderer.stats().changed;reused+=renderer.stats().reused;out={};}
 std::sort(std::begin(samples),std::end(samples));std::printf("BENCH 4096x2048 4096 TMPs 64 actors one moving cloak: median_ms=%.6f changed_pages_mean=%.3f reused_pages_mean=%.3f\n",(samples[5]+samples[6])/2,changed/12.,reused/12.);
}
int main(int argc,char**argv){if(argc<2||std::strcmp(argv[1],"--benchmark"))correctness();if(argc<2||std::strcmp(argv[1],"--correctness"))benchmark();}

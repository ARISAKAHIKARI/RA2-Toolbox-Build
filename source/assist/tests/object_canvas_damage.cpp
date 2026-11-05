#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/object_canvas.h"
#include <cassert>
#include <cstdio>
using namespace external;
static unsigned comparisons=0,partialFrames=0;
static std::shared_ptr<voxel::Image> image(int w,int h,uint32_t color){
 auto p=std::make_shared<voxel::Image>();p->width=w;p->height=h;p->pixels.resize(size_t(w)*h);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)p->pixels[size_t(y)*w+x]=((x+y)%7)?color:0;
 p->nativeDepth.enabled=true;p->nativeDepth.gradient=nativesurface::Gradient::Flat;return p;
}
static void equal(const camera::NativeCanvas&a,const camera::NativeCanvas&b){
 assert(a.pageKeys()==b.pageKeys());for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)
  assert(a.pixel(x,y)==b.pixel(x,y)&&a.depth(x,y)==b.depth(x,y)&&a.nativeDepth(x,y)==b.nativeDepth(x,y)&&a.nativeAlphaAt(x,y)==b.nativeAlphaAt(x,y));
 ++comparisons;
}
static uint64_t hash(const camera::NativeCanvas&a){uint64_t v=1469598103934665603ull;for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)v=(v^a.pixel(x,y))*1099511628211ull;return v;}
static void changes(bool terrain,bool retained){
 auto tile=image(64,32,0xff407030),body=image(23,29,0xffbb6633),top=image(19,27,0x80402010);
 auto attached=std::make_shared<voxel::Image>(*body);auto shadow=*body;shadow.shadow=true;shadow.writeDepth=false;shadow.x=-17;shadow.y=18;attached->attachments.push_back(shadow);
 camera::NativeCanvas base;assert(base.reset(384,256));if(terrain){size_t n=0;for(int y=0;y<256;y+=32)for(int x=0;x<384;x+=64)base.terrainSource(n++,tile,float(x),float(y),0,{},0);}
 camera::ObjectCanvas warm;camera::NativeCanvas front;std::vector<uint32_t>incremental;
 std::vector<camera::ObjectCanvas::Draw>d(3);d[0].image=attached;d[0].x=41.25f;d[0].y=38.5f;d[0].base=100;d[0].material.nativeZ=true;d[0].id=1;
 d[1]=d[0];d[1].image=top;d[1].x=49;d[1].y=41;d[1].base=105;d[1].id=2;d[2]=d[0];d[2].x=254.5f;d[2].y=112.25f;d[2].id=3;
 auto check=[&]{
  const uint64_t frozen=hash(front);if(!retained)front={};
  camera::ObjectCanvas cold;camera::NativeCanvas next,reference;assert(warm.compose(base,d,next,true));assert(cold.compose(base,d,reference));equal(next,reference);
  if(warm.stats().partialPages){++partialFrames;assert(warm.stats().restoredPixels<(warm.stats().changed+warm.stats().partialPages)*16384);}
  std::vector<uint32_t>full;assert(reference.present(96,64,full));if(incremental.empty())assert(next.present(96,64,incremental));else assert(next.presentDirty(96,64,incremental));assert(full==incremental);
  if(retained&&front.width())assert(hash(front)==frozen);front=std::move(next);
 };
 check();for(int n=0;n<15;++n){d[0].x+=.75f;d[0].y+=.25f;d[0].drop=float(n%3)*3;check();}
 std::reverse(d.begin(),d.end());check();d[1].light.intensity=650;check();d[0].material.translucency=2;check();d[0].material.mixStrength=80;check();
 for(auto mode:{colorprecision::Mode::SmoothRGB,colorprecision::Mode::NativeSteps}){base.precision(mode);check();d[0].x+=1.5f;check();}
 d[1].image=body;check();d.erase(d.begin()+1);check();d[0].x=127.75f;d[0].y=127.75f;check();d[0].x=129.25f;check();
 auto cloak=image(20,18,0xff8090a0);cloak->nativeDepth.voxel=true;auto q=d.front();q.image=cloak;q.material.displace=true;q.material.cloakWords=-384-3;q.x=250;q.y=115;d.push_back(q);check();
 for(int n=0;n<6;++n){d[0].x+=1;d.back().x+=1.5f;d.back().y+=.75f;check();}d.pop_back();check();d[0].x+=1;check();
 auto alpha=std::make_shared<camera::NativeCanvas::AlphaPages>();(*alpha)[0]=std::make_shared<const std::vector<uint8_t>>(16384,70);base.nativeAlpha(alpha);check();d[0].x+=1;check();
 if(terrain){auto changed=image(64,32,0xffaa7722);base.terrainSource(0,changed,0,0,0,{},0);check();d[0].x+=1;check();}
 base.restrictPaint(0,0,256,200);check();d[0].y+=1;check();base.restrictPaint(0,0,384,256);check();d.clear();check();
}
static void clipping(){
 for(bool indexed:{false,true})for(int scale:{1,2})for(int phase=0;phase<8;++phase){
  auto p=image(31,29,0xff995533);p->rasterScale=scale;if(indexed)p->indices.assign(p->pixels.size(),18);
  camera::Surface full,part;assert(full.reset(128,128));assert(part.reset(128,128));
  const auto beforeColor=part.color;const auto beforeDepth=part.depth;const auto beforeZ=part.nativeZ;
  camera::Material m;m.nativeZ=true;m.translucency=phase%4;m.mixStrength=phase%2?120:0;
  full.blit(*p,21.25f+phase,18.5f,1,1,100,{},1,0,false,m);part.writeRect(27,22,42,37);part.blit(*p,21.25f+phase,18.5f,1,1,100,{},1,0,false,m);
  for(int y=0;y<128;++y)for(int x=0;x<128;++x){const size_t n=size_t(y)*128+x;const bool inside=x>=27&&x<42&&y>=22&&y<37;
   assert(part.color[n]==(inside?full.color[n]:beforeColor[n])&&part.depth[n]==(inside?full.depth[n]:beforeDepth[n])&&part.nativeZ[n]==(inside?full.nativeZ[n]:beforeZ[n]));}
  ++comparisons;
 }
}
int main(){for(bool terrain:{false,true})for(bool retained:{false,true})changes(terrain,retained);clipping();assert(partialFrames>50);assert(camera::ObjectCanvas::terrainPrefixPoolBytes()==0);
 std::printf("PASS local damage: %u exact comparisons, %u partial frames; RGBA/float-Z/native-Z/alpha, COW retained fronts, dirty resolves, shadows/translucency/order/removal/terrain/clip/cloak and horizontal blit sentinels\n",comparisons,partialFrames);}

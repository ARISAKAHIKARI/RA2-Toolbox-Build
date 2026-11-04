#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/gpu_scene_record.h"
#include RA2_REFERENCE_RECORD_HEADER
#include <cassert>
#include <chrono>
#include <cstdio>
#include <future>
using namespace external;
static size_t checks=0,segments=0,barriers=0;
template<class T>static bool same(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(!a.size()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
static void equal(const camera::Surface&a,const camera::Surface&b){assert(same(a.color,b.color)&&same(a.depth,b.depth)&&same(a.nativeZ,b.nativeZ));for(size_t n=0;n<a.color.size();++n)assert(a.alphaAt(n)==b.alphaAt(n));++checks;}
static std::shared_ptr<const voxel::Image> image(unsigned flags,int pose){
 auto out=std::make_shared<voxel::Image>();out->width=72;out->height=65;out->x=-23;out->y=-19;out->rasterScale=flags&1?2:1;const size_t count=size_t(out->width)*out->height;
 out->pixels.resize(count);out->indices.resize(count);out->objectPalette=true;out->flat=flags&2;out->writeDepth=!(flags&4);out->skipZ=flags&8;
 out->nativeDepth.enabled=!(flags&16);out->nativeDepth.gradient=nativesurface::Gradient(flags%4);out->nativeDepth.tileEqual=flags&32;out->nativeDepth.voxel=true;out->nativeDepth.shape=flags&64;
 for(size_t n=0;n<count;++n){unsigned a=n%11<3?0:n%13==0?95:255;out->pixels[n]=(a<<24)|((uint32_t(n+pose*31)*0x317b53)&0x3f3f3f);out->indices[n]=uint8_t(n*19+pose*17);}
 if(flags&32){out->depth.resize(count);for(size_t n=0;n<count;++n)out->depth[n]=n%17?float(int(n%31)-15):std::numeric_limits<float>::quiet_NaN();}
 if(flags&64){out->nativeShape.resize(count);out->nativeShapeCoverage.resize(count);for(size_t n=0;n<count;++n){out->nativeShape[n]=int(n%29)-14;out->nativeShapeCoverage[n]=n%7!=0;}}
 return out;
}
template<class R>static void build(R&record,unsigned flags,int frame,int w,int h,int units,bool boundaries){
 auto base=std::make_shared<std::vector<uint32_t>>(size_t(w)*h);auto depth=std::make_shared<std::vector<float>>(base->size());auto a=std::make_shared<std::vector<uint8_t>>(base->size());
 for(size_t n=0;n<base->size();++n){(*base)[n]=0xff000000u|((n*173+frame*317)&0xffffff);(*depth)[n]=float(int(n%79)-80);(*a)[n]=uint8_t(n*17+frame);}
 record.begin(w,h,base,depth,(flags&128)?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps);record.nativeView(.9375f,1.25f,true);record.fog(a);
 if(flags&16){auto effects=std::make_shared<gpueffects::Plan>();effects->width=w;effects->height=h;gpueffects::Command command;auto bits=[](float f){uint32_t out;std::memcpy(&out,&f,4);return out;};command.words[0]=gpueffects::Alpha;command.words[4]=3;command.words[5]=17;command.words[6]=std::min(w,43);command.words[7]=std::min(h,57);command.words[8]=bits(3.f);command.words[9]=bits(17.f);command.words[10]=command.words[11]=bits(20.f);command.words[14]=command.words[15]=2;command.indices=std::make_shared<const std::vector<uint8_t>>(std::initializer_list<uint8_t>{32,97,159,255});effects->commands.push_back(command);record.effects(effects);}
 lighting::State light;light.intensity=700+frame*53;light.tint={{1300,870,1010}};light.rgb555=flags&2;light.scalar=flags&4;
 if(flags&8){auto table=std::make_shared<std::vector<uint32_t>>(53*256);for(size_t n=0;n<table->size();++n)(*table)[n]=uint16_t(n*137+frame);light.table=table;}
 const auto source=image(flags,frame);float scale=flags&1?1.5f:1.25f;
 for(int unit=0;unit<units;++unit){std::shared_ptr<const voxel::Image> part=source;camera::Material mat;mat.clipBottom=(flags&32)?37.25f:1e7f;mat.bridgeSplit=flags&64;mat.nativeZAdjust=int(flags%11)-5;
  if(boundaries&&unit==units/3){mat.displace=true;mat.cloakWords=(flags&4)?w*2+3:-(w*2+3);mat.translucency=2;}
  if(boundaries&&unit==units*2/3){auto mutablePart=std::make_shared<voxel::Image>(*source);mutablePart->rasterScale=1;part=mutablePart;}
  bool smooth=boundaries&&unit==units*2/3;
  if(boundaries&&unit%17==0){auto mutablePart=std::make_shared<voxel::Image>(*part);auto shadow=*source;shadow.shadow=true;shadow.x+=4;shadow.y+=5;mutablePart->attachments.push_back(std::move(shadow));auto gun=*source;gun.x-=9;gun.y-=37;gun.skipZ=true;mutablePart->attachments.push_back(std::move(gun));part=mutablePart;}
  if(boundaries&&unit%19==0){auto mutablePart=std::make_shared<voxel::Image>(*part);mutablePart->nativeBlend=2;mutablePart->blendSource=std::make_shared<const std::vector<uint32_t>>(part->pixels);part=mutablePart;}
  if(boundaries&&unit%13==0)mat.mixStrength=127;
  if(boundaries&&unit%11==0)mat.translucency=1;
  float x=float(17+(unit%20)*(w/20))+float((frame+unit)%7)/256,y=float(23+(unit/20)*(h/std::max(8,(units+19)/20)))+float((frame+unit)%11)/256;
  // Attachment borders and displacement sources cross worker rows.
  assert(record.append(*part,part,x,y,scale,scale*.875f,float(unit),light,1.25f,3,smooth,{},mat));
  if(boundaries&&unit%23==0)record.annotationDot(int(x),int(y),0xff00ff);
 }
}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
static void check(unsigned flags,int frame,int w,int h,int units,bool boundaries){camera::GpuSceneRecord current;record_reference::GpuSceneRecord prior;build(current,flags,frame,w,h,units,boundaries);build(prior,flags,frame,w,h,units,boundaries);camera::Surface a,b;camera::cpureplay::Stats stats;assert(current.replay(a,&stats));assert(prior.replay(b));equal(a,b);segments+=stats.rowSegments;barriers+=stats.barriers;if(units<8)assert(!stats.rowSegments);}
static void deferred(){
 camera::GpuSceneRecord current;record_reference::GpuSceneRecord prior;camera::Surface a,b;
 for(bool invalid:{false,true}){build(current,64,3,641,401,120,false);build(prior,64,3,641,401,120,false);
  auto packet=std::make_shared<gpuvoxel::Prepared>();packet->width=packet->height=8;gpuvoxel::Triangle triangle;triangle.vertex[0]={{1,1,1,0}};triangle.vertex[1]={{7,1,2,0}};triangle.vertex[2]={{1,7,3,0}};triangle.material={{0xffaa3344,71,0,0}};packet->triangles.push_back(triangle);
  auto image=std::make_shared<voxel::Image>();image->width=8;image->height=invalid?9:8;image->gpuRaster=packet;image->skipZ=true;
  assert(current.append(*image,image,40,70,1.25f,1.25f,100,{},1,0,false)&&prior.append(*image,image,40,70,1.25f,1.25f,100,{},1,0,false));
  camera::cpureplay::Stats stats;const bool x=current.replay(a,&stats),y=prior.replay(b);assert(x==y&&x==!invalid&&stats.barriers==1);if(x)equal(a,b);
 }
 // Failed deferred work cannot contaminate the next complete replay.
 build(current,32,4,641,401,120,true);build(prior,32,4,641,401,120,true);assert(current.replay(a)&&prior.replay(b));equal(a,b);
}
int main(){xbrzadapter::gpuPresentation(false);
 for(unsigned flags=0;flags<256;flags+=7)check(flags,flags%5,641,401,120,true);
 for(unsigned flags=0;flags<8;++flags)check(flags,flags,79,61,3,true);
 // Multiple renderers contend for the existing bounded pool. The loser
 // executes serially rather than queuing or changing any output semantics.
 auto one=std::async(std::launch::async,[]{camera::GpuSceneRecord current;record_reference::GpuSceneRecord prior;build(current,127,2,1280,720,160,true);build(prior,127,2,1280,720,160,true);camera::Surface a,b;assert(current.replay(a)&&prior.replay(b));assert(same(a.color,b.color)&&same(a.depth,b.depth)&&same(a.nativeZ,b.nativeZ));});
 check(126,3,1280,720,160,true);one.get();
 deferred();assert(segments&&barriers);std::printf("parity full_planes=%zu row_segments=%zu barriers=%zu; concurrent renderers, deferred failure/recovery, alpha effects and low-load serial PASS\n",checks,segments,barriers);
 for(int units:{3,160,400})for(bool boundaries:{false,true}){camera::GpuSceneRecord current;record_reference::GpuSceneRecord prior;camera::Surface a,b;std::vector<double> oldTimes,newTimes;
  for(int frame=0;frame<7;++frame){build(current,64,frame,1280,720,units,boundaries);build(prior,64,frame,1280,720,units,boundaries);double oldMs,newMs;if(frame%2){double t=now();assert(prior.replay(b));oldMs=now()-t;t=now();assert(current.replay(a));newMs=now()-t;}else{double t=now();assert(current.replay(a));newMs=now()-t;t=now();assert(prior.replay(b));oldMs=now()-t;}equal(a,b);if(frame){oldTimes.push_back(oldMs);newTimes.push_back(newMs);}}
  std::printf("dynamic_replay units=%d boundaries=%u before_ms=%.6f after_ms=%.6f ratio=%.4f\n",units,unsigned(boundaries),median(oldTimes),median(newTimes),median(newTimes)/median(oldTimes));
 }
 std::puts("PASS full-target RGBA/floatZ-bits/nativeZ/A unchanged. Every timed replay redraws changing frames; full base initialization, fog and barriers included; scene preparation excluded.");
}

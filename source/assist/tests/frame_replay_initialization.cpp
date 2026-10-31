#ifndef RA2_FRAME_REPLAY_HEADER
#define RA2_FRAME_REPLAY_HEADER "../../external_reader/source/gpu_scene_record.h"
#endif
#include RA2_FRAME_REPLAY_HEADER
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace external;
struct Fixture {
 camera::GpuSceneRecord record;
 camera::Surface output;
 int w,h;
 Fixture(int width,int height,unsigned flags,int objects):w(width),h(height){
  auto base=std::make_shared<std::vector<uint32_t>>(size_t(w)*h);
  for(size_t n=0;n<base->size();++n)(*base)[n]=0xff000000u|((uint32_t(n)*0x10103u)&0x7f7f7fu);
  camera::GpuDepthPlan axes;std::shared_ptr<std::vector<float>> depth;
  if(flags&2){
   axes.width=(w+1)/2;axes.height=(h+1)/2;depth=std::make_shared<std::vector<float>>(size_t(axes.width)*axes.height);
   auto xs=std::make_shared<std::vector<uint32_t>>(w),ys=std::make_shared<std::vector<uint32_t>>(h);
   for(int x=0;x<w;++x)(*xs)[x]=x/2;for(int y=0;y<h;++y)(*ys)[y]=y/2;axes.xs=xs;axes.ys=ys;
  }else depth=std::make_shared<std::vector<float>>(size_t(w)*h);
  for(size_t n=0;n<depth->size();++n)(*depth)[n]=float(int(n%57)-70);
  std::shared_ptr<const std::vector<vulkancompute::TileJob>>tiles;
  if(flags&16)tiles=std::make_shared<const std::vector<vulkancompute::TileJob>>(1);
  record.begin(w,h,(flags&16)?nullptr:base,depth,colorprecision::Mode::NativeSteps,tiles,
   [base](std::vector<uint32_t>&out){out=*base;return true;},axes);
  record.nativeView(1,1,(flags&1)!=0);
  if(flags&4){auto alpha=std::make_shared<std::vector<uint8_t>>(size_t(w)*h);for(size_t n=0;n<alpha->size();++n)(*alpha)[n]=uint8_t(n%128);record.fog(alpha);}
  for(int n=0;n<objects;++n){
   auto image=std::make_shared<voxel::Image>();image->width=image->height=40;image->flat=true;image->objectPalette=true;
   image->pixels.assign(1600,0xffa08040u+uint32_t(n));image->indices.assign(1600,40);image->nativeDepth.enabled=(flags&1)!=0;
   image->nativeDepth.gradient=nativesurface::Gradient::Vertical;image->nativeDepth.voxel=(flags&8)&&n%7==0;
   image->shadow=n%5==0;image->writeDepth=n%4!=0;
   for(size_t at=0;at<image->pixels.size();at+=11)image->pixels[at]=0;
   if(n%3==0){auto part=*image;part.x=13;part.y=25;part.shadow=true;part.attachments.clear();image->attachments.push_back(std::move(part));}
   camera::Material material;material.nativeZAdjust=-2-n%9;material.displace=image->nativeDepth.voxel;material.cloakWords=3;
   material.translucency=n%3==0?2:0;material.clipBottom=n%4==0?25.f:1e7f;
   lighting::State light;light.intensity=800+n%300;
   if(!record.append(*image,image,float((n*83)%(w+30)-15),float((n*59)%(h+20)-10),1,1,float(n),light,1,0,false,{},material))std::abort();
  }
 }
 void draw(){if(!record.replay(output))std::abort();}
 uint64_t digest()const{
  uint64_t value=1469598103934665603ull;auto fold=[&](uint32_t n){value=(value^n)*1099511628211ull;};
  for(auto n:output.color)fold(n);for(float f:output.depth){uint32_t n;std::memcpy(&n,&f,4);fold(n);}
  for(auto n:output.nativeZ)fold(n);for(size_t n=0;n<output.color.size();++n)fold(output.alphaAt(n));return value;
 }
};
int main(int argc,char**){
 if(argc>1){
  std::printf("{\"timings\":[");bool comma=false;
  for(auto size:{std::pair<int,int>{1280,720},{2560,1440}})for(int objects:{0,160}){
   Fixture fixture(size.first,size.second,7,objects);for(int n=0;n<3;++n)fixture.draw();
   std::vector<double>samples;for(int n=0;n<15;++n){auto start=std::chrono::steady_clock::now();fixture.draw();samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
   std::sort(samples.begin(),samples.end());if(comma)std::printf(",");comma=true;
   std::printf("{\"width\":%d,\"height\":%d,\"objects\":%d,\"median_ms\":%.6f,\"digest\":\"%llu\"}",size.first,size.second,objects,samples[7],(unsigned long long)fixture.digest());
  }
  std::puts("]}");return 0;
 }
 // Same Surface reused across changing frame layouts and all combinations of
 // native/float depth, compact/full base depth, A, cloak and lazy base pixels.
 std::printf("{\"digests\":[");for(unsigned flags=0;flags<32;++flags){Fixture fixture(192,128,flags,24);fixture.draw();auto first=fixture.digest();fixture.output.reset(11,13);fixture.draw();if(fixture.digest()!=first)std::abort();std::printf("%s\"%llu\"",flags?",":"",(unsigned long long)first);}
 std::puts("],\"gpu_executed\":false}");
}

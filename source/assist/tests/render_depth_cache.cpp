#include "../../external_reader/source/viewport_canvas.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace external;assert(!xbrzadapter::gpuState().resolve);
 camera::NativeCanvas background;assert(background.reset(1024,768));
 voxel::Image terrain;terrain.width=1024;terrain.height=768;terrain.pixels.assign(1024*768,0xff607080);terrain.depth.resize(terrain.pixels.size());
 for(size_t n=0;n<terrain.depth.size();++n)terrain.depth[n]=float(n%997);
 lighting::State light;light.intensity=1000;background.blit(terrain,0,0,1,1,0,light);
 auto color=std::make_shared<const std::vector<uint32_t>>(160*90,0xff607080);camera::ViewportCanvas view;
 xbrzadapter::gpuPresentation(true);uint64_t checks=0;std::shared_ptr<const std::vector<float>> plane;uint64_t key=0;
 auto prepare=[&](double x,double y,uint64_t revision){
  assert(view.prepare(background,*color,160,90,x,y,320,180,++key,colorprecision::Mode::NativeSteps,1,color,{}, {},revision));
  const auto*f=view.recordedGpuFrame();assert(f&&f->baseDepth&&f->baseDepthXs&&f->baseDepthYs);
  for(int yy=0;yy<90;++yy)for(int xx=0;xx<160;++xx){
   const auto actual=(*f->baseDepth)[size_t((*f->baseDepthYs)[yy])*f->baseDepthWidth+(*f->baseDepthXs)[xx]];
   assert(actual==background.depth(int(std::floor(x+(xx+.5)*2)),int(std::floor(y+(yy+.5)*2))));++checks;
  }
  return f->baseDepth;
 };
 plane=prepare(200.25,180.5,10);
 for(int n=1;n<=20;++n)assert(prepare(200.25+n*8,180.5+n*4,10)==plane);
 auto moved=prepare(900,700,10);assert(moved!=plane);
 terrain.depth.assign(terrain.depth.size(),5000);background.blit(terrain,0,0,1,1,0,light);
 auto changed=prepare(200.25,180.5,11);assert(changed!=plane&&*changed!=*plane);
 view.reset();assert(prepare(200.25,180.5,11)!=changed);
 xbrzadapter::gpuPresentation(false);assert(!xbrzadapter::gpuState().resolve);
 std::printf("PASS %llu exact nearest-Z samples, 20 pans reuse one immutable native depth plane, revision/coverage/reset invalidation; CPU plan only, no Vulkan execution\n",(unsigned long long)checks);
}

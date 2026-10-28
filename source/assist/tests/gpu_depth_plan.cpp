#include "../../external_reader/source/viewport_canvas.h"
#include <cassert>
#include <cstdio>
using namespace external;
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";
 camera::NativeCanvas background;assert(background.reset(32,24));lighting::State light;light.intensity=1000;light.tint={{1000,1000,1000}};
 voxel::Image terrain;terrain.width=32;terrain.height=24;terrain.pixels.resize(32*24);terrain.depth.resize(32*24);
 for(int y=0;y<24;++y)for(int x=0;x<32;++x){terrain.pixels[size_t(y)*32+x]=0xff000000u|(uint32_t(x*7)<<16)|(uint32_t(y*9)<<8)|uint32_t((x+y)*4);terrain.depth[size_t(y)*32+x]=float((x*3+y*5)%11-4);}
 background.blit(terrain,0,0,1,1,0,light);
 voxel::Image actor;actor.width=actor.height=5;actor.x=actor.y=-2;actor.pixels.assign(25,0xffe83311);actor.depth.resize(25);for(size_t n=0;n<25;++n)actor.depth[n]=float(n%3);
 auto image=std::make_shared<const voxel::Image>(actor);
 struct Crop {double x,y,w,h;int width,height;};
 const Crop crops[]={{0,0,8,6,8,6},{3.25,2.5,8,6,12,9},{-.75,-.25,8,6,16,12},{26.125,19.25,8,6,16,12},{4,4,1,1,2,2},{4.25,4.75,1,1,2,2}};
 uint64_t depthChecks=0,colorChecks=0;unsigned maxColorError=0;size_t cases=0,gpuFrames=0;
 for(const auto&crop:crops){
  std::vector<uint32_t> base;assert(background.presentRegion(crop.width,crop.height,base,crop.x,crop.y,crop.w,crop.h,1));auto baseOwner=std::make_shared<const std::vector<uint32_t>>(base);
  const float ax=float(crop.x+crop.w*.5),ay=float(crop.y+crop.h*.5);
  xbrzadapter::gpuPresentation(false);camera::ViewportCanvas cpu;assert(cpu.prepare(background,*baseOwner,crop.width,crop.height,crop.x,crop.y,crop.w,crop.h,1,colorprecision::Mode::NativeSteps,1,baseOwner));cpu.blit(*image,ax,ay,1,1,1,light,1,0,image);std::vector<uint32_t> expected;assert(cpu.finish(expected,true));
  auto&state=xbrzadapter::gpuState();if(state.resolve&&!state.resolve->diagnostics().available)state.resolve.reset();xbrzadapter::gpuPresentation(true);camera::ViewportCanvas gpu;
  auto prepare=[&]{return gpu.prepare(background,*baseOwner,crop.width,crop.height,crop.x,crop.y,crop.w,crop.h,1,colorprecision::Mode::NativeSteps,1,baseOwner);};
  assert(prepare()&&gpu.gpuRecording());const auto*frame=gpu.recordedGpuFrame();assert(frame&&frame->basePixels==baseOwner);
  const int x0=int(std::floor(crop.x)),y0=int(std::floor(crop.y));const int nw=int(std::ceil(crop.x+crop.w))-x0,nh=int(std::ceil(crop.y+crop.h))-y0;
  assert(frame->baseDepthWidth==nw&&frame->baseDepthHeight==nh&&frame->baseDepth->size()==size_t(nw)*nh&&frame->baseDepthXs->size()==size_t(crop.width)&&frame->baseDepthYs->size()==size_t(crop.height));
  auto plane=frame->baseDepth;auto xs=frame->baseDepthXs,ys=frame->baseDepthYs;
  std::vector<float> original;assert(background.depthRegion(x0,y0,nw,nh,original)&&*plane==original);
  for(int y=0;y<crop.height;++y)for(int x=0;x<crop.width;++x){
   const int ix=std::clamp(int(std::floor(crop.x+(x+.5)*crop.w/crop.width))-x0,0,nw-1),iy=std::clamp(int(std::floor(crop.y+(y+.5)*crop.h/crop.height))-y0,0,nh-1);
   assert((*xs)[x]==uint32_t(ix)&&(*ys)[y]==uint32_t(iy)&&(*plane)[size_t((*ys)[y])*nw+(*xs)[x]]==original[size_t(iy)*nw+ix]);++depthChecks;
  }
  assert(prepare());frame=gpu.recordedGpuFrame();assert(frame&&frame->baseDepth==plane&&frame->baseDepthXs==xs&&frame->baseDepthYs==ys); // no full output-Z owner/copy
  gpu.blit(*image,ax,ay,1,1,1,light,1,0,image);std::vector<uint32_t> actual;assert(gpu.finish(actual,true));
  const bool used=xbrzadapter::gpuUsed();if(required&&!used){std::fprintf(stderr,"GPU native Z unavailable: %s\n",state.resolve?state.resolve->diagnostics().reason.c_str():"no backend");return 1;}
  if(used){++gpuFrames;assert(state.resolve->diagnostics().readbacks==1);}
  assert(actual.size()==expected.size());for(size_t n=0;n<actual.size();++n)for(int shift:{0,8,16,24}){const unsigned error=unsigned(std::abs(int((actual[n]>>shift)&255)-int((expected[n]>>shift)&255)));maxColorError=std::max(maxColorError,error);assert(error<=(used?1u:0u));++colorChecks;}
  // A mode toggle with the SAME key must rebuild output-layout Z for the old
  // CPU path; retaining the compact plane here would corrupt occlusion.
  xbrzadapter::gpuPresentation(false);assert(prepare()&&!gpu.gpuRecording());gpu.blit(*image,ax,ay,1,1,1,light,1,0,image);std::vector<uint32_t> toggled;assert(gpu.finish(toggled,true)&&toggled==expected);++cases;
 }
 xbrzadapter::gpuPresentation(false);
 // Public recorder guards reject a bad nearest index before either CPU replay
 // or GPU submission; a valid compact1x1 plane expands only during replay.
 auto one=std::make_shared<const std::vector<float>>(1,3.f);auto indices=std::make_shared<const std::vector<uint32_t>>(2,0u);auto pixels=std::make_shared<const std::vector<uint32_t>>(4,0xff102030);
 camera::GpuSceneRecord record;record.begin(2,2,pixels,one,colorprecision::Mode::NativeSteps,{}, {},{1,1,indices,indices});camera::Surface expanded;assert(record.valid()&&record.replay(expanded)&&expanded.depth==std::vector<float>(4,3.f));
 auto bad=std::make_shared<const std::vector<uint32_t>>(2,1u);record.begin(2,2,pixels,one,colorprecision::Mode::NativeSteps,{}, {},{1,1,bad,indices});assert(!record.valid()&&!record.replay(expanded));
 std::printf("PASS: native terrain Z plans cases=%zu depth_checks=%llu color_channel_checks=%llu gpu_frames=%zu max_error=%u; fractional/clamped1/1.5/2x,1x1plane,stable immutable axes/plane, CPU toggle and same-frame replay\n",cases,(unsigned long long)depthChecks,(unsigned long long)colorChecks,gpuFrames,maxColorError);
}

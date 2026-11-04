#include "../../external_reader/source/xbrz_adapter.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

int main(){
 using namespace external::xbrzadapter;
 const bool required=std::getenv("RA2_TEST_VULKAN_REQUIRED")!=nullptr;
 auto fetch=[](int x,int y){
  const unsigned alpha=unsigned((x*13+y*7)&255);
  return (alpha<<24)|((unsigned((x*19+y*3)&255)*alpha/255)<<16)|
   ((unsigned((x*5-y*11)&255)*alpha/255)<<8)|(unsigned((x*3+y*17)&255)*alpha/255);
 };
 RegionCache cpu,gpu;std::vector<uint32_t> expected,actual;unsigned used=0,comparisons=0,maxError=0;
 for(double ratio:{1.25,1.5,2.5})for(int pan:{0,1,9}){
  const int w=173,h=109;const double x=240.25+pan,y=251.5+pan;
  gpuPresentation(false);assert(cpu.sampleRegion(fetch,x,y,w/ratio,h/ratio,w,h,expected));
  gpuPresentation(true);assert(gpu.sampleRegion(fetch,x,y,w/ratio,h/ratio,w,h,actual));
  assert(gpuAttempted());if(required)assert(gpuUsed());used+=gpuUsed();
  assert(actual.size()==expected.size());
  for(size_t i=0;i<actual.size();++i){
   for(unsigned shift:{0u,8u,16u,24u}){const unsigned a=(actual[i]>>shift)&255,b=(expected[i]>>shift)&255;
    const unsigned error=a>b?a-b:b-a;maxError=std::max(maxError,error);assert(error<=1);++comparisons;}
   const auto p=actual[i];assert((p&255)<=(p>>24)&&((p>>8)&255)<=(p>>24)&&((p>>16)&255)<=(p>>24));
  }
  const auto builds=gpu.builds();gpuPresentation(true);
  assert(gpu.sampleRegion(fetch,x,y,w/ratio,h/ratio,w,h,actual));assert(gpu.builds()==builds);
  if(gpuUsed()){const auto&d=gpuState().resolve->diagnostics();assert(d.uploads==0&&d.uploadedBytes==0&&d.readbacks==1);}
 }
 // Exact integer ratios keep the existing copy path and must not submit GPU work.
 gpuPresentation(true);assert(gpu.sampleRegion(fetch,0,0,40,30,80,60,actual));assert(!gpuAttempted()&&!gpuUsed());
 // An off/on transition retains the same backend, including sticky failure state.
 auto* identity=gpuState().resolve.get();gpuPresentation(false);assert(gpuState().resolve.get()==identity);
 gpuPresentation(true);assert(gpuState().resolve.get()==identity);
 std::printf("PASS: Vulkan RegionCache integration used=%u/9 comparisons=%u max_channel_error=%u; warm uploads0/readback1, integer bypass, bounded toggle lifecycle\n",used,comparisons,maxError);
}

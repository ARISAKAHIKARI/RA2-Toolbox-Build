#include "../../external_reader/source/vulkan_area.h"
#include <cassert>
#include <cstdio>
#include <string>
int main(int argc,char**argv){
 using namespace external;const bool required=argc>1&&std::string(argv[1])=="--require";
 assert(gpuarea::atlasBytes(2048,128ull*1024*1024)==128ull*1024*1024);
 if(sizeof(void*)>=8){assert(!gpuarea::atlasBytes(2049,128ull*1024*1024));assert(gpuarea::atlasBytes(2049,512ull*1024*1024)==256ull*1024*1024);assert(gpuarea::atlasBytes(8192,512ull*1024*1024)==512ull*1024*1024);assert(!gpuarea::atlasBytes(8193,1024ull*1024*1024));}
 if(sizeof(void*)<8)return 0;
 gpuarea::Plan plan;plan.nativeWidth=plan.nativeHeight=48*128;plan.width=plan.height=48;plan.spanX=plan.nativeWidth;plan.spanY=plan.nativeHeight;
 for(unsigned y=0;y<48;++y)for(unsigned x=0;x<48;++x){uint32_t color=0xff000000u|((x*5)<<16)|((y*5)<<8)|((x+y)*2);
  plan.pages.push_back({x,y,std::make_shared<const std::vector<uint32_t>>(gpuarea::PagePixels,color),1});}
 assert(plan.pages.size()==2304&&gpuarea::valid(plan));vulkancompute::AreaResolve gpu;std::vector<uint32_t>out;
 if(!gpu.render(plan,out)){std::printf("LARGE_GPU_UNAVAILABLE %s\n",gpu.diagnostics().reason.c_str());return required?1:0;}
 auto check=[&]{assert(out.size()==48*48);for(unsigned y=0;y<48;++y)for(unsigned x=0;x<48;++x)assert(out[y*48+x]==(*plan.pages[y*48+x].pixels)[0]);};
 check();assert(gpu.nativeAtlasBytes()>=uint64_t(2304)*gpuarea::PagePixels*4&&gpu.prefixBuilds()==2304&&gpu.diagnostics().readbacks==1);
 assert(gpu.render(plan,out));check();assert(!gpu.prefixBuilds()&&!gpu.diagnostics().uploads&&gpu.diagnostics().readbacks==1);
 auto changed=std::make_shared<std::vector<uint32_t>>(gpuarea::PagePixels,0xff774411);plan.pages[1337].pixels=changed;++plan.pages[1337].version;
 assert(gpu.render(plan,out));check();assert(gpu.prefixBuilds()==1&&gpu.diagnostics().uploads==1);
 std::printf("PASS GPU area beyond2048: 2304 unique native pages, device-aware raw banks, exact output, warm0uploads, changed1upload, one final readback; software=%u device=%s\n",unsigned(gpu.diagnostics().softwareDevice),gpu.diagnostics().device.c_str());
}

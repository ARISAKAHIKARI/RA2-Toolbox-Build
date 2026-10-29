#include "../../external_reader/source/vulkan_area.h"
#include "gpu_shroud_cases.h"
#include <cassert>
#include <cstdio>
#include <new>
#include <random>
#include <string>
using namespace external;
static std::shared_ptr<const std::vector<uint32_t>> page(std::mt19937&r){auto p=std::make_shared<std::vector<uint32_t>>(gpuarea::PagePixels);for(auto&v:*p)v=r();return p;}
int main(int argc,char**argv){bool required=argc>1&&std::string(argv[1])=="--require";vulkancompute::AreaResolve gpu;std::mt19937 random(20261029);gpuarea::Plan p;p.nativeWidth=513;p.nativeHeight=389;p.width=73;p.height=51;p.spanX=512.625;p.spanY=388.375;p.background=0x053d2811;
 for(uint32_t y=0;y<4;++y)for(uint32_t x=0;x<5;++x)if((x+2*y)%4!=2)p.pages.push_back({x,y,page(random)});
 std::vector<uint32_t>out{0x12345678};auto invalid=p;invalid.width=0;assert(!gpu.render(invalid,out)&&out==std::vector<uint32_t>{0x12345678});
 if(!gpu.render(p,out)){std::printf("VULKAN_UNAVAILABLE %s\n",gpu.diagnostics().reason.c_str());return required?1:0;}
 uint64_t checks=0,frames=0;unsigned maxError=0;auto check=[&](){std::vector<uint32_t>expected;assert(gpuarea::reference(p,expected));assert(out.size()==expected.size());for(size_t n=0;n<out.size();++n)for(int s:{0,8,16,24}){unsigned error=unsigned(std::abs(int((out[n]>>s)&255)-int((expected[n]>>s)&255)));maxError=std::max(maxError,error);if(error>1){std::fprintf(stderr,"Mismatch frame%llu at%zu shift%d actual%08x expected%08x\n",(unsigned long long)frames,n,s,out[n],expected[n]);assert(false);}++checks;}assert(gpu.diagnostics().readbacks==1);++frames;};check();assert(gpu.prefixBuilds()==p.pages.size());
 for(auto dims:std::vector<std::array<int,2>>{{1,1},{7,3},{31,17},{128,63},{511,389},{731,519},{160,80}}){p.width=dims[0];p.height=dims[1];assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0&&gpu.diagnostics().dispatches==1);check();}
 for(int n=0;n<12;++n){p.spanX=513.-double(n+1)/32;p.spanY=389.-double(n+1)/37;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0);check();}
 p.pages[1].pixels=page(random);assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1&&gpu.diagnostics().uploads==1);check();
 // A new shared owner at an identical vector address must rebuild its slot.
 alignas(std::vector<uint32_t>) unsigned char storage[sizeof(std::vector<uint32_t>)];auto reused=[&](){auto image=page(random);auto*v=new(storage) std::vector<uint32_t>(*image);return std::shared_ptr<const std::vector<uint32_t>>(v,[](const auto*q){const_cast<std::vector<uint32_t>*>(q)->~vector();});};p.pages[2].pixels=reused();assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1);check();p.pages[2].pixels.reset();p.pages[2].pixels=reused();assert(gpu.render(p,out));assert(gpu.prefixBuilds()==1);check();
 p.pages.erase(p.pages.begin()+3);assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0);check();
 auto fog=std::make_shared<std::vector<uint8_t>>(size_t(p.width)*p.height);for(size_t n=0;n<fog->size();++n)(*fog)[n]=uint8_t(n%128);p.fog=fog;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0);check();
 // Native SHROUD/FOG raster stays GPU-resident and shares one final fence.
 auto g=shroudcases::geometry();auto cells=shroudcases::cells(g);gpushroud::Builder builder;std::shared_ptr<const gpushroud::Plan>mask;assert(builder.build(g,cells,p.width,p.height,p.spanX,p.spanY,true,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},mask));p.fog.reset();p.mask=mask;std::vector<uint8_t>alpha;assert(gpushroud::reference(*mask,alpha));auto referencePlan=p;referencePlan.mask.reset();referencePlan.fog=std::make_shared<const std::vector<uint8_t>>(alpha);assert(gpu.render(p,out));auto saved=p;p=referencePlan;check();p=saved;assert(gpu.render(p,out));assert(gpu.prefixBuilds()==0&&gpu.diagnostics().uploads==0&&gpu.diagnostics().dispatches==1);p=referencePlan;check();
 const auto before=out;invalid=p;invalid.pages.push_back(invalid.pages.front());assert(!gpu.render(invalid,out)&&out==before);invalid=p;invalid.spanX=std::numeric_limits<double>::quiet_NaN();assert(!gpu.render(invalid,out)&&out==before);
 std::printf("PASS actual GPU sparse area frames=%llu channels=%llu max_error=%u; shrink/upscale/fractional/sparse/deleted/owner-reuse/current native fog, warm0uploads0prefixbuilds onefinalreadback; software=%u device=%s\n",(unsigned long long)frames,(unsigned long long)checks,maxError,unsigned(gpu.diagnostics().softwareDevice),gpu.diagnostics().device.c_str());
}

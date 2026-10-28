#include "../../external_reader/source/gpu_scene_record.h"
#include "gpu_voxel_cases.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
 using namespace external;const bool required=argc>1&&std::string(argv[1])=="--require";
 vulkancompute::FrameResolve gpu;constexpr int W=224,H=160;
 auto background=std::make_shared<const std::vector<uint32_t>>(W*H,0xff284061u);auto depth=std::make_shared<const std::vector<float>>(W*H,-1000.f);
 auto native=std::make_shared<voxel::Image>();native->width=7;native->height=7;native->flat=true;native->pixels.assign(49,0xff723cab);for(size_t n=0;n<49;++n)if(n%5==0)native->pixels[n]=0;
 native->indices.assign(49,42);native->objectPalette=true;
 auto cases=gpuvoxeltest::cases();const auto packet=std::make_shared<const gpuvoxel::Prepared>(cases.front().prepared);
 auto mesh=std::make_shared<voxel::Image>();mesh->gpuRaster=packet;mesh->width=packet->width;mesh->height=packet->height;mesh->x=packet->x;mesh->y=packet->y;mesh->rasterScale=packet->rasterScale;mesh->depthBottom=packet->depthBottom;mesh->objectPalette=true;mesh->writeDepth=false;
 camera::GpuSceneRecord record;record.begin(W,H,background,depth,colorprecision::Mode::SmoothRGB);
 lighting::State light;light.intensity=1000;
 assert(record.append(*native,native,20,40,1.5,1.5,0,light,1,0,true));assert(record.append(*native,native,60,60,1.5,1.5,0,light,1,0,true));
 assert(record.append(*mesh,mesh,110,80,1,1,0,light,1,0,true));assert(record.append(*mesh,mesh,155,100,1,1,0,light,1,0,true));
 assert(record.frame().draws.size()==4);assert(record.frame().draws[0].reconstructionSource==record.frame().draws[1].reconstructionSource);assert(record.frame().draws[2].gpuRaster==record.frame().draws[3].gpuRaster);
 std::vector<uint32_t> actual;if(!gpu.renderFrame(record.frame(),actual)){std::cout<<"VULKAN_SHARED_UNAVAILABLE "<<gpu.diagnostics().reason<<std::endl;return required?1:0;}
 assert(gpu.diagnostics().xbrzBuilds==1&&gpu.diagnostics().voxelRasters==1&&gpu.diagnostics().readbacks==1);camera::Surface expected;assert(record.replay(expected));
 unsigned maximum=0;for(size_t n=0;n<actual.size();++n)for(unsigned shift:{0u,8u,16u,24u}){unsigned a=(actual[n]>>shift)&255,b=(expected.color[n]>>shift)&255,e=a>b?a-b:b-a;maximum=std::max(maximum,e);assert(e<=1);}
 auto first=actual;assert(gpu.renderFrame(record.frame(),actual)&&actual==first);assert(gpu.diagnostics().uploads==0&&gpu.diagnostics().uploadedBytes==0&&gpu.diagnostics().xbrzBuilds==0&&gpu.diagnostics().voxelRasters==0&&gpu.diagnostics().readbacks==1);
 std::cout<<"PASS shared GPU assets: two SHP instances one xBRZ build; two VXL instances one resident raster; warm0uploads0builds one readback; max_channel_error="<<maximum<<std::endl;
}

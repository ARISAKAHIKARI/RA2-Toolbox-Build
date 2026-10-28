#include "../../external_reader/source/gpu_scene_record.h"
#include "gpu_voxel_cases.h"
#include <iostream>
#include <cstdlib>
int main(int argc,char**argv){
 using namespace external;
 const bool required=argc>1&&std::string(argv[1])=="--require";vulkancompute::FrameResolve gpu;
 constexpr int W=224,H=160;auto background=std::make_shared<std::vector<uint32_t>>(W*H);auto depth=std::make_shared<std::vector<float>>(W*H,-1000.f);
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){(*background)[y*W+x]=0xff000000u|((x&255)<<16)|((y&255)<<8)|((x+y)&255);if(x>130&&y>80)(*depth)[y*W+x]=10000;}
 uint64_t comparisons=0;unsigned frames=0,maximumError=0;bool available=true;
 for(auto&test:gpuvoxeltest::cases()){
  const unsigned n=frames;auto packet=std::make_shared<const gpuvoxel::Prepared>(std::move(test.prepared));
  auto image=std::make_shared<voxel::Image>();image->gpuRaster=packet;image->width=packet->width;image->height=packet->height;image->x=packet->x;image->y=packet->y;image->rasterScale=packet->rasterScale;image->depthBottom=packet->depthBottom;image->objectPalette=true;image->writeDepth=false;
  auto expectedImage=test.expected;expectedImage.depth.clear();expectedImage.writeDepth=false;
  if(n%5==4){image->skipZ=true;expectedImage.depth.assign(expectedImage.pixels.size(),std::numeric_limits<float>::quiet_NaN());}
  if(n%6==0){auto shadow=*image;shadow.x+=3*shadow.rasterScale;shadow.shadow=true;shadow.flat=true;shadow.depthBias=2;shadow.skipZ=false;shadow.attachments.clear();image->attachments.push_back(shadow);
   auto expectedShadow=expectedImage;expectedShadow.x=shadow.x;expectedShadow.shadow=true;expectedShadow.flat=true;expectedShadow.depthBias=2;expectedShadow.depth.clear();expectedImage.attachments.push_back(std::move(expectedShadow));}
  lighting::State light;light.intensity=int(600+(n%6)*140);light.tint={{650,1050,900}};
  const auto precision=n%2?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps;
  const float sx=n%3?1.5f:float(packet->rasterScale),sy=n%3?1.25f:float(packet->rasterScale);
  const bool smooth=packet->rasterScale>1;
  camera::Surface expected;assert(expected.reset(W,H));expected.color=*background;expected.depth=*depth;expected.precision(precision);expected.blit(expectedImage,112,90,sx,sy,100,light,1,0,smooth);
  camera::GpuSceneRecord record;record.begin(W,H,background,depth,precision);assert(record.append(*image,image,112,90,sx,sy,100,light,1,0,smooth));assert(record.valid());
  // The very same deferred packets must also produce the CPU fallback image.
  camera::Surface fallback;assert(record.replay(fallback)&&fallback.color==expected.color&&fallback.depth==expected.depth);
  if(!available){++frames;continue;}
  std::vector<uint32_t> actual;if(!gpu.renderFrame(record.frame(),actual)){std::cout<<"VULKAN_VOXEL_SCENE_UNAVAILABLE "<<gpu.diagnostics().reason<<std::endl;if(required)return 1;available=false;++frames;continue;}
  assert(actual.size()==expected.color.size());
  for(size_t pixel=0;pixel<actual.size();++pixel)for(unsigned shift:{0u,8u,16u,24u}){unsigned a=(actual[pixel]>>shift)&255,b=(expected.color[pixel]>>shift)&255,error=a>b?a-b:b-a;maximumError=std::max(maximumError,error);
   if(error>1){std::cerr<<"GPU voxel scene mismatch case="<<frames<<" pixel="<<pixel<<" shift="<<shift<<" actual="<<std::hex<<actual[pixel]<<" expected="<<expected.color[pixel]<<std::dec<<std::endl;std::abort();}++comparisons;}
  assert(gpu.diagnostics().readbacks==1);auto first=actual;
  assert(gpu.renderFrame(record.frame(),actual)&&actual==first&&gpu.diagnostics().uploadedBytes==0&&gpu.diagnostics().uploads==0&&gpu.diagnostics().readbacks==1);
  ++frames;
 }
 if(!available)std::cout<<"PASS GPU voxel same-frame CPU fallback cases="<<frames<<"; GPU device unavailable, no GPU execution claim"<<std::endl;
 else std::cout<<"PASS actual GPU voxel -> resident color/index -> lit scene cases="<<frames<<" channel_comparisons="<<comparisons<<" max_channel_error="<<maximumError<<" warm_uploads=0 readbacks_per_frame=1; VPL/index32/emissive/supersampling/shadows/standingZ/projectileNoZ/CPUfallback"<<std::endl;
}

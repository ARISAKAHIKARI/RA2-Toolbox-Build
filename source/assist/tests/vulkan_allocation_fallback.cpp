// Force a real allocation-budget fault after GPU pipeline/device allocation;
// verify all non-pending GPU resources release BEFORE ordinary CPU replay.
#define RA2_VULKAN_MAPPED_BUDGET_BYTES (64ull*1024)
#include "../../external_reader/source/vulkan_scene.h"
#include "../../external_reader/source/vulkan_area.h"
#include "../../external_reader/source/camera_raster.h"
#include "../../external_reader/source/native_canvas.h"
#include <cassert>
#include <iostream>
using namespace external;
class Probe:public vulkancompute::FrameResolve {
public:bool liveDevice()const{return device_!=VK_NULL_HANDLE;}bool disabled()const{return disabled_;}
};
class AreaProbe:public vulkancompute::AreaResolve {
public:bool liveDevice()const{return device_!=VK_NULL_HANDLE;}bool disabled()const{return disabled_;}
};
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";
 // Optional neural guides must not disable a GPU capable of the raw scene.
 // This source/staging footprint admits ordinary RGBA but not its guide planes.
 {
  Probe optional;vulkancompute::FrameJob small;small.width=64;small.height=48;small.collectGuides=true;
  small.basePixels=std::make_shared<const std::vector<uint32_t>>(64*48,0xff314759u);small.baseDepth=std::make_shared<const std::vector<float>>(64*48,0);
  small.motion.context=std::make_shared<const int>(1);small.motion.depthFar=-100;small.motion.depthNear=100;small.motion.frame=1;small.motion.tick=100;
  std::vector<uint32_t> current;const bool rendered=optional.renderFrame(small,current);
  if(!rendered&&(optional.diagnostics().reason.find("unavailable")!=std::string::npos||optional.diagnostics().reason.find("No compatible")!=std::string::npos)){
   std::cout<<"VULKAN_GUIDE_ALLOCATION_UNAVAILABLE "<<optional.diagnostics().reason<<std::endl;return required?1:0;
  }
  assert(rendered&&current==*small.basePixels&&!optional.guides()&&!optional.disabled()&&optional.liveDevice());
  assert(optional.guideAllocationFailures()==1&&optional.diagnostics().readbacks==1);
  for(int n=0;n<3;++n){assert(optional.renderFrame(small,current)&&current==*small.basePixels&&!optional.guides());assert(optional.guideAllocationFailures()==1&&!optional.disabled());}
  small.collectGuides=false;assert(optional.renderFrame(small,current));small.collectGuides=true;assert(optional.renderFrame(small,current));assert(optional.guideAllocationFailures()==2);
  small.width=32;small.height=16;small.basePixels=std::make_shared<const std::vector<uint32_t>>(32*16,0xff314759u);small.baseDepth=std::make_shared<const std::vector<float>>(32*16,0);
  assert(optional.renderFrame(small,current)&&current==*small.basePixels&&optional.guides()&&optional.guides()->resetHistory&&optional.guideAllocationFailures()==2);
  std::cout<<"PASS Vulkan optional-guide mapped-budget fallback: same-frame raw GPU output retained, no repeated allocation attempts until off/resize, smaller guide frame recovers"<<std::endl;
 }
 Probe gpu;
 vulkancompute::FrameJob frame;frame.width=256;frame.height=256;
 auto source=std::make_shared<const std::vector<uint32_t>>(256*256,0xff7f4523);frame.basePixels=source;
 std::vector<uint32_t> output{0x12345678};assert(!gpu.renderFrame(frame,output));assert(output==std::vector<uint32_t>{0x12345678});
 const auto&d=gpu.diagnostics();if(d.reason.find("unavailable")!=std::string::npos||d.reason.find("No compatible")!=std::string::npos){std::cout<<"VULKAN_ALLOCATION_UNAVAILABLE "<<d.reason<<std::endl;return required?1:0;}
 assert(gpu.disabled()&&!gpu.liveDevice());assert(d.allocatedBufferBytes==0&&d.mappedBufferBytes==0&&d.readbacks==0);
 camera::Surface cpu;assert(cpu.reset(256,256));cpu.color=*source;assert(cpu.color==*source);
 const auto reason=d.reason;for(int n=0;n<3;++n){assert(!gpu.renderFrame(frame,output)&&!gpu.liveDevice());assert(gpu.diagnostics().allocatedBufferBytes==0&&gpu.diagnostics().mappedBufferBytes==0&&gpu.diagnostics().reason==reason);}
 // The minimap has its own raw atlas, prefix and mask stage. Its non-pending
 // resources must also release when a large final readback exceeds the same
 // mapped budget; the caller can then execute the original CPU area replay.
 AreaProbe area;gpuarea::Plan plan;plan.nativeWidth=plan.nativeHeight=256;plan.width=plan.height=200;plan.spanX=plan.spanY=256;
 auto page=std::make_shared<const std::vector<uint32_t>>(gpuarea::PagePixels,0xff3c7a29);plan.pages.push_back({0,0,page});
 output={0x13579bdf};assert(!area.render(plan,output));assert(output==std::vector<uint32_t>{0x13579bdf});
 const auto&ad=area.diagnostics();assert(area.disabled()&&!area.liveDevice());assert(ad.allocatedBufferBytes==0&&ad.mappedBufferBytes==0&&ad.readbacks==0);
 if(ad.reason.find("shaderFloat64")!=std::string::npos){std::cout<<"VULKAN_AREA_ALLOCATION_UNAVAILABLE "<<ad.reason<<std::endl;return required?1:0;}
 assert(ad.reason.find("allocation")!=std::string::npos);const auto areaReason=ad.reason;
 camera::NativeCanvas canvas;assert(canvas.reset(256,256));voxel::Image patch;patch.width=patch.height=128;patch.flat=true;patch.writeDepth=false;patch.pixels=*page;
 canvas.blit(patch,0,0,1,1,0,lighting::State{});std::vector<uint32_t> replay;assert(canvas.present(200,200,replay,256,256)&&replay.size()==200*200);
 for(int y=0;y<200;++y)for(int x=0;x<200;++x)assert(replay[size_t(y)*200+x]==(x<100&&y<100?0xff3c7a29u:plan.background));
 for(int n=0;n<3;++n){assert(!area.render(plan,output)&&!area.liveDevice());assert(area.diagnostics().allocatedBufferBytes==0&&area.diagnostics().mappedBufferBytes==0&&area.diagnostics().reason==areaReason);}
 std::cout<<"PASS Vulkan area forced mapped-budget fault: raw atlas/prefix resources released before exact sparse CPU replay; sticky no-retry; output preserved; no queued work destroyed"<<std::endl;
 std::cout<<"PASS Vulkan forced mapped-budget fault: all device buffers released before exact CPU replay; sticky no-retry; output preserved; no queued work destroyed"<<std::endl;
}

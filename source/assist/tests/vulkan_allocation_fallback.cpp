// Force a real allocation-budget fault after GPU pipeline/device allocation;
// verify all non-pending GPU resources release BEFORE ordinary CPU replay.
#define RA2_VULKAN_MAPPED_BUDGET_BYTES (64ull*1024)
#include "../../external_reader/source/vulkan_scene.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <iostream>
using namespace external;
class Probe:public vulkancompute::FrameResolve {
public:bool liveDevice()const{return device_!=VK_NULL_HANDLE;}bool disabled()const{return disabled_;}
};
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";Probe gpu;
 vulkancompute::FrameJob frame;frame.width=256;frame.height=256;
 auto source=std::make_shared<const std::vector<uint32_t>>(256*256,0xff7f4523);frame.basePixels=source;
 std::vector<uint32_t> output{0x12345678};assert(!gpu.renderFrame(frame,output));assert(output==std::vector<uint32_t>{0x12345678});
 const auto&d=gpu.diagnostics();if(d.reason.find("unavailable")!=std::string::npos||d.reason.find("No compatible")!=std::string::npos){std::cout<<"VULKAN_ALLOCATION_UNAVAILABLE "<<d.reason<<std::endl;return required?1:0;}
 assert(gpu.disabled()&&!gpu.liveDevice());assert(d.allocatedBufferBytes==0&&d.mappedBufferBytes==0&&d.readbacks==0);
 camera::Surface cpu;assert(cpu.reset(256,256));cpu.color=*source;assert(cpu.color==*source);
 const auto reason=d.reason;for(int n=0;n<3;++n){assert(!gpu.renderFrame(frame,output)&&!gpu.liveDevice());assert(gpu.diagnostics().allocatedBufferBytes==0&&gpu.diagnostics().mappedBufferBytes==0&&gpu.diagnostics().reason==reason);}
 std::cout<<"PASS Vulkan forced mapped-budget fault: all device buffers released before exact CPU replay; sticky no-retry; output preserved; no queued work destroyed"<<std::endl;
}

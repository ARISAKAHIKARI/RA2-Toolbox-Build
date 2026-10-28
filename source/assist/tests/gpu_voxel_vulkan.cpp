#include "../../external_reader/source/vulkan_voxel_backend.h"
#include "gpu_voxel_cases.h"
#include <iostream>
#include <cstdlib>
using namespace external;
// This harness reads raw actor output only to compare the real shader against
// the CPU oracle. Production Stage records into FrameResolve's resident atlas
// and deliberately exposes no per-actor submission or readback API.
class Probe:public vulkancompute::Resolve {
 gpuvoxel::Stage stage_{*this};Buffer colors_,indices_,result_;
public:
 ~Probe(){if(inFlight_&&vkGetFenceStatus_&&vkGetFenceStatus_(device_,fence_)!=VK_SUCCESS)return;destroy(colors_);destroy(indices_);destroy(result_);}
 bool run(std::shared_ptr<const gpuvoxel::Prepared>packet,std::vector<uint32_t>&colors,std::vector<uint32_t>&indices){
  if(!initialize())return false;
  const size_t pixels=size_t(packet->width)*packet->height;
  const VkDeviceSize colorBytes=(pixels+8)*4,indexBytes=(pixels+16)*4,totalBytes=colorBytes+indexBytes;
  if(!buffer(colors_,colorBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false)||!buffer(indices_,indexBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false)||!buffer(result_,totalBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true))return false;
  if(!stage_.prepare({{packet,4,8}},colors_.buffer,colorBytes,indices_.buffer,indexBytes))return false;
  assert(vkResetCommandPool_(device_,commandPool_,0)==VK_SUCCESS);assert(vkResetFences_(device_,1,&fence_)==VK_SUCCESS);
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;assert(vkBeginCommandBuffer_(command_,&begin)==VK_SUCCESS);
  vkCmdFillBuffer_(command_,colors_.buffer,0,colorBytes,0xdeadbeefu);vkCmdFillBuffer_(command_,indices_.buffer,0,indexBytes,0xdeadbeefu);
  VkMemoryBarrier initial{VK_STRUCTURE_TYPE_MEMORY_BARRIER};initial.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;initial.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&initial,0,nullptr,0,nullptr);
  stage_.record(command_);
  VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&after,0,nullptr,0,nullptr);
  VkBufferCopy copyColor{0,0,colorBytes},copyIndex{0,colorBytes,indexBytes};vkCmdCopyBuffer_(command_,colors_.buffer,result_.buffer,1,&copyColor);vkCmdCopyBuffer_(command_,indices_.buffer,result_.buffer,1,&copyIndex);
  VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  assert(vkEndCommandBuffer_(command_)==VK_SUCCESS);VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;assert(vkQueueSubmit_(queue_,1,&submit,fence_)==VK_SUCCESS);inFlight_=true;
  auto waited=vkWaitForFences_(device_,1,&fence_,VK_TRUE,10000000000ull);if(waited!=VK_SUCCESS){quarantined().store(true);return fatal("GPU voxel test fence failed");}inFlight_=false;
  if(!result_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=result_.memory;range.size=VK_WHOLE_SIZE;assert(vkInvalidateMappedMemoryRanges_(device_,1,&range)==VK_SUCCESS);}
  const auto* data=static_cast<const uint32_t*>(result_.mapped);const auto* index=data+colorBytes/4;
  for(size_t n=0;n<4;++n)assert(data[n]==0xdeadbeefu&&data[pixels+4+n]==0xdeadbeefu);
  for(size_t n=0;n<8;++n)assert(index[n]==0xdeadbeefu&&index[pixels+8+n]==0xdeadbeefu);
  colors.assign(data+4,data+pixels+4);indices.assign(index+8,index+pixels+8);
  assert(stage_.stats().jobs==1&&stage_.stats().uploadedBytes==packet->triangles.size()*sizeof(gpuvoxel::Triangle)&&stage_.stats().dispatches==(packet->triangles.empty()?1u:3u));
  stage_.commit();return true;
 }
};
int main(int argc,char**argv){
 const bool required=argc>1&&std::string(argv[1])=="--require";Probe probe;std::vector<uint32_t> rgba,indices;uint64_t pixels=0;unsigned count=0;
 for(auto&test:gpuvoxeltest::cases()){
  auto packet=std::make_shared<const gpuvoxel::Prepared>(std::move(test.prepared));
  if(!probe.run(packet,rgba,indices)){std::cout<<"VULKAN_VOXEL_UNAVAILABLE "<<probe.diagnostics().reason<<std::endl;return required?1:0;}
  assert(rgba.size()==test.expected.pixels.size());
  for(size_t n=0;n<rgba.size();++n){if(rgba[n]!=test.expected.pixels[n]||indices[n]!=test.expected.indices[n]){std::cerr<<"GPU voxel mismatch case="<<count<<" pixel="<<n<<" xy="<<n%packet->width<<","<<n/packet->width<<" rgba="<<std::hex<<rgba[n]<<" expected="<<test.expected.pixels[n]<<" index="<<indices[n]<<" expected="<<unsigned(test.expected.indices[n])<<std::dec<<std::endl;std::abort();}}
  pixels+=rgba.size();++count;
 }
 std::cout<<"PASS actual GPU voxel raster cases="<<count<<" exact_color_index_pixels="<<pixels<<" software="<<probe.diagnostics().softwareDevice<<" device="<<probe.diagnostics().device<<"; four bindings, output-offset sentinels, exact depth ties, shader writes resident atlases"<<std::endl;
}

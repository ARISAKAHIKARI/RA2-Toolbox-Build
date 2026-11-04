#include "gpu_shroud_cases.h"
#include "../../external_reader/source/vulkan_shroud_backend.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace {
class Rig:public external::vulkancompute::Resolve {
 Buffer target_,result_;
 static constexpr uint32_t GuardWords=7;
 bool begin(){
  if(vkResetCommandPool_(device_,commandPool_,0)!=VK_SUCCESS||vkResetFences_(device_,1,&fence_)!=VK_SUCCESS)return false;
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;return vkBeginCommandBuffer_(command_,&begin)==VK_SUCCESS;
 }
public:
 uint64_t submissions=0,readbacks=0;
 ~Rig(){if(!inFlight_||(vkGetFenceStatus_&&vkGetFenceStatus_(device_,fence_)==VK_SUCCESS)){destroy(target_);destroy(result_);}}
 bool available(){return initialize();}
 bool execute(external::gpushroud::Stage&stage,const std::shared_ptr<const external::gpushroud::Plan>&plan,std::vector<uint8_t>&out){
  const size_t bytes=plan->outputWords()*4+64;
  if(!buffer(target_,bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false)||!buffer(result_,bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true)||!stage.prepare(plan,target_.buffer,GuardWords,target_.size)||!begin())return false;
  vkCmdFillBuffer_(command_,target_.buffer,0,bytes,0xa5a5a5a5u);
  VkMemoryBarrier fill{VK_STRUCTURE_TYPE_MEMORY_BARRIER};fill.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;fill.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&fill,0,nullptr,0,nullptr);
  stage.record(command_);
  VkMemoryBarrier copy{VK_STRUCTURE_TYPE_MEMORY_BARRIER};copy.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;copy.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&copy,0,nullptr,0,nullptr);
  VkBufferCopy region{0,0,bytes};vkCmdCopyBuffer_(command_,target_.buffer,result_.buffer,1,&region);
  VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  if(vkEndCommandBuffer_(command_)!=VK_SUCCESS)return false;VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;
  if(vkQueueSubmit_(queue_,1,&submit,fence_)!=VK_SUCCESS)return false;inFlight_=true;++submissions;
  const uint64_t wait=AllowSoftware&&diagnostics_.softwareDevice?10000000000ull:100000000ull;
  if(vkWaitForFences_(device_,1,&fence_,VK_TRUE,wait)!=VK_SUCCESS)return fatal("Shroud GPU test timeout");inFlight_=false;stage.commit();
  if(!result_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=result_.memory;range.size=VK_WHOLE_SIZE;if(vkInvalidateMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return false;}
  const auto*data=static_cast<const uint8_t*>(result_.mapped);
  for(size_t n=0;n<GuardWords*4;++n)assert(data[n]==0xa5);
  for(size_t n=GuardWords*4+plan->outputWords()*4;n<bytes;++n)assert(data[n]==0xa5);
  out.assign(data+GuardWords*4,data+GuardWords*4+plan->outputBytes());++readbacks;return true;
 }
 bool abandon(external::gpushroud::Stage&stage,const std::shared_ptr<const external::gpushroud::Plan>&plan){
  if(!buffer(target_,plan->outputWords()*4+64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false)||!stage.prepare(plan,target_.buffer,GuardWords,target_.size)||!begin())return false;
  stage.record(command_);return vkEndCommandBuffer_(command_)==VK_SUCCESS; // deliberately no queue submission or commit
 }
};
}
int main(int argc,char**argv){
 using namespace external;const bool required=argc>1&&std::string(argv[1])=="--require";Rig rig;
 if(!rig.available()){std::printf("VULKAN_UNAVAILABLE %s\n",rig.diagnostics().reason.c_str());return required?1:0;}
 gpushroud::Stage stage(rig);gpushroud::Builder builder;auto g=shroudcases::geometry();auto cells=shroudcases::cells(g);uint64_t pixels=0,frames=0;
 std::shared_ptr<const gpushroud::Plan> last;
 for(const auto&c:shroudcases::cases()){
  std::shared_ptr<const gpushroud::Plan>plan;radar::shroudmask::Raster expected;
  auto load=[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);};
  assert(expected.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,load,c.x,c.y));assert(builder.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,load,plan,c.x,c.y));
  std::vector<uint8_t>actual;assert(rig.execute(stage,plan,actual));assert(actual==expected.alpha);assert(stage.dispatches()==1&&stage.copies()==1);pixels+=actual.size();++frames;
  assert(rig.execute(stage,plan,actual));assert(actual==expected.alpha);assert(stage.dispatches()==0&&stage.copies()==1&&stage.uploads()==0&&stage.uploadedBytes()==0);pixels+=actual.size();++frames;last=plan;
 }
 auto abandoned=std::make_shared<const gpushroud::Plan>(*last);assert(rig.abandon(stage,abandoned));std::vector<uint8_t>actual,expected;assert(gpushroud::reference(*abandoned,expected));
 assert(rig.execute(stage,abandoned,actual));assert(actual==expected&&stage.dispatches()==1);pixels+=actual.size();++frames;
 // Replacing bytes at otherwise identical frame coordinates cannot reuse a
 // stale source or packed mask; previous owners remain live through last.
 const auto c=shroudcases::cases().back();std::shared_ptr<const gpushroud::Plan>changed;auto changedLoad=[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i,1);};
 assert(builder.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,changedLoad,changed,c.x,c.y));assert(gpushroud::reference(*changed,expected));assert(rig.execute(stage,changed,actual));assert(actual==expected&&stage.dispatches()==1&&stage.uploads()>0);pixels+=actual.size();++frames;
 assert(rig.submissions==frames&&rig.readbacks==frames);
 stage.release();stage.release(); // idempotent non-inflight cleanup before owner/dtor
 std::printf("PASS Vulkan native shroud frames=%llu exact_alpha_pixels=%llu warm_dispatches=0 warm_source_uploads=0 one_readback=1 canaries/abandoned_record/changed_source=pass software=%u device=%s\n",(unsigned long long)frames,(unsigned long long)pixels,unsigned(rig.diagnostics().softwareDevice),rig.diagnostics().device.c_str());
}

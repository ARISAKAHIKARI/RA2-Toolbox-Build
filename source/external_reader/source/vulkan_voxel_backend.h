#pragma once
#include "gpu_voxel.h"
#include "gpu_voxel_spirv.h"
#include "vulkan_compute.h"
namespace external { namespace gpuvoxel {
// Record-only prepass owned by the scene coordinator. It never submits a
// queue, waits on a fence or reads pixels back to the CPU. All actor results
// stay in the same color/material atlases consumed by final scene composition.
class Stage {
 static constexpr VkDeviceSize TriangleBudget=96ull*1024*1024;
 vulkancompute::Resolve&core_;
 using Buffer=vulkancompute::Resolve::Buffer;
 VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;
 VkPipelineLayout layout_=VK_NULL_HANDLE;
 std::array<VkPipeline,3> pipelines_{};
 VkDescriptorPool pool_=VK_NULL_HANDLE;VkDescriptorSet set_=VK_NULL_HANDLE;
 Buffer triangles_,scratch_;
 std::vector<Params> commands_;
 bool initialized_=false,prepared_=false;
 void barrier(VkCommandBuffer command,VkPipelineStageFlags from,VkPipelineStageFlags to,VkAccessFlags source,VkAccessFlags destination){
  VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=source;b.dstAccessMask=destination;
  core_.vkCmdPipelineBarrier_(command,from,to,0,1,&b,0,nullptr,0,nullptr);
 }
 bool pipeline(const uint32_t*words,size_t bytes,VkPipeline&out){
  VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=bytes;info.pCode=words;VkShaderModule module=VK_NULL_HANDLE;
  if(core_.vkCreateShaderModule_(core_.device_,&info,nullptr,&module)!=VK_SUCCESS)return core_.fatal("Vulkan voxel shader creation failed");
  VkComputePipelineCreateInfo c{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};c.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;c.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;c.stage.module=module;c.stage.pName="main";c.layout=layout_;
  auto result=core_.vkCreateComputePipelines_(core_.device_,VK_NULL_HANDLE,1,&c,nullptr,&out);core_.vkDestroyShaderModule_(core_.device_,module,nullptr);
  return result==VK_SUCCESS?true:core_.fatal("Vulkan voxel pipeline creation failed");
 }
public:
 struct Job {std::shared_ptr<const Prepared> raster;uint32_t colorOffset=0,indexOffset=0;};
 struct Stats {uint64_t jobs=0,triangles=0,candidatePixels=0,outputPixels=0,uploadedBytes=0,dispatches=0;} stats_;
 explicit Stage(vulkancompute::Resolve&core):core_(core){}
 Stage(const Stage&)=delete;Stage&operator=(const Stage&)=delete;
 ~Stage(){close();}
 const Stats&stats()const{return stats_;}
 void close(){
  if(core_.abandoned_)return;
  // Share the coordinator's bounded timeout quarantine. In-flight Vulkan
  // commands may still reference every buffer, descriptor and pipeline here.
  if(core_.inFlight_&&(!core_.vkGetFenceStatus_||core_.vkGetFenceStatus_(core_.device_,core_.fence_)!=VK_SUCCESS))return;
  if(!core_.device_)return;
  core_.destroy(triangles_);core_.destroy(scratch_);
  if(pool_)core_.vkDestroyDescriptorPool_(core_.device_,pool_,nullptr);
  for(auto&p:pipelines_)if(p){core_.vkDestroyPipeline_(core_.device_,p,nullptr);p=VK_NULL_HANDLE;}
  if(layout_)core_.vkDestroyPipelineLayout_(core_.device_,layout_,nullptr);
  if(descriptorLayout_)core_.vkDestroyDescriptorSetLayout_(core_.device_,descriptorLayout_,nullptr);
  pool_=VK_NULL_HANDLE;set_=VK_NULL_HANDLE;layout_=VK_NULL_HANDLE;descriptorLayout_=VK_NULL_HANDLE;initialized_=prepared_=false;commands_.clear();
 }
 bool initialize(){
  if(initialized_)return !core_.disabled_&&core_.device_;if(!core_.initialize())return false;
  const auto&limits=core_.properties_.limits;
  if(limits.maxPerStageDescriptorStorageBuffers<4||limits.maxDescriptorSetStorageBuffers<4||limits.maxPushConstantsSize<sizeof(Params)||limits.maxComputeWorkGroupInvocations<64||limits.maxComputeWorkGroupSize[0]<64)return core_.fatal("Vulkan voxel device limits unsupported");
  VkDescriptorSetLayoutBinding bindings[4]{};for(uint32_t n=0;n<4;++n){bindings[n].binding=n;bindings[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[n].descriptorCount=1;bindings[n].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo d{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};d.bindingCount=4;d.pBindings=bindings;
  if(core_.vkCreateDescriptorSetLayout_(core_.device_,&d,nullptr,&descriptorLayout_)!=VK_SUCCESS)return core_.fatal("Vulkan voxel descriptor layout failed");
  VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params)};
  VkPipelineLayoutCreateInfo l{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};l.setLayoutCount=1;l.pSetLayouts=&descriptorLayout_;l.pushConstantRangeCount=1;l.pPushConstantRanges=&range;
  if(core_.vkCreatePipelineLayout_(core_.device_,&l,nullptr,&layout_)!=VK_SUCCESS)return core_.fatal("Vulkan voxel pipeline layout failed");
  if(!pipeline(DepthShader,sizeof(DepthShader),pipelines_[0])||!pipeline(WinnerShader,sizeof(WinnerShader),pipelines_[1])||!pipeline(ResolveShader,sizeof(ResolveShader),pipelines_[2]))return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4};VkDescriptorPoolCreateInfo p{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};p.maxSets=1;p.poolSizeCount=1;p.pPoolSizes=&size;
  if(core_.vkCreateDescriptorPool_(core_.device_,&p,nullptr,&pool_)!=VK_SUCCESS)return core_.fatal("Vulkan voxel descriptor pool failed");
  VkDescriptorSetAllocateInfo a{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};a.descriptorPool=pool_;a.descriptorSetCount=1;a.pSetLayouts=&descriptorLayout_;
  if(core_.vkAllocateDescriptorSets_(core_.device_,&a,&set_)!=VK_SUCCESS)return core_.fatal("Vulkan voxel descriptor allocation failed");
  initialized_=true;return true;
 }
 bool prepare(const std::vector<Job>&jobs,VkBuffer colors,VkDeviceSize colorBytes,VkBuffer indices,VkDeviceSize indexBytes){
  commands_.clear();stats_={};prepared_=false;
  if(jobs.empty())return true;
  if(core_.inFlight_||jobs.size()>4096||!colors||!indices||!colorBytes||!indexBytes)return core_.fail("Invalid Vulkan voxel batch");
  uint64_t totalTriangles=0,maximumPixels=0;
  for(const auto&job:jobs){
   if(!job.raster)return core_.fail("Missing Vulkan voxel packet");const auto&p=*job.raster;
   const uint64_t pixels=uint64_t(std::max(0,p.width))*std::max(0,p.height);
   if(p.width<1||p.height<1||p.width>8192||p.height>8192||pixels>resolutionsampling::MaxPixels||p.triangles.size()>MaxFaces*2||uint64_t(job.colorOffset)+pixels>colorBytes/4||uint64_t(job.indexOffset)+pixels>indexBytes/4)return core_.fail("Invalid Vulkan voxel output region");
   for(const auto&t:p.triangles){for(const auto&v:t.vertex)for(int n=0;n<3;++n)if(!std::isfinite(v[n])||std::abs(v[n])>1e7)return core_.fail("Invalid Vulkan voxel vertex");if(t.material[1]>255)return core_.fail("Invalid Vulkan voxel index");
    int left=std::max(0,int(std::floor(std::min({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]})))),right=std::min(p.width-1,int(std::ceil(std::max({t.vertex[0][0],t.vertex[1][0],t.vertex[2][0]}))));
    int top=std::max(0,int(std::floor(std::min({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]})))),bottom=std::min(p.height-1,int(std::ceil(std::max({t.vertex[0][1],t.vertex[1][1],t.vertex[2][1]}))));
    if(right>=left&&bottom>=top)stats_.candidatePixels+=uint64_t(right-left+1)*(bottom-top+1);
    if(stats_.candidatePixels>128ull*1024*1024)return core_.fail("Vulkan voxel raster work budget exceeded");
   }
   totalTriangles+=p.triangles.size();maximumPixels=std::max(maximumPixels,pixels);
   stats_.outputPixels+=pixels;
  }
  // The scratch Z/winner region is reused between sequential actors. It does
  // not grow with the number of actors in a scene or retain completed poses.
  if(totalTriangles*sizeof(Triangle)>TriangleBudget||maximumPixels*8>128ull*1024*1024)return core_.fail("Vulkan voxel batch budget exceeded");
  if(!initialize())return false;
  const VkDeviceSize triangleBytes=std::max<VkDeviceSize>(sizeof(Triangle),totalTriangles*sizeof(Triangle)),scratchBytes=maximumPixels*8;
  if(triangleBytes>core_.properties_.limits.maxStorageBufferRange||scratchBytes>core_.properties_.limits.maxStorageBufferRange||colorBytes>core_.properties_.limits.maxStorageBufferRange||indexBytes>core_.properties_.limits.maxStorageBufferRange)return core_.fail("Vulkan voxel storage range unsupported");
  if(!core_.buffer(triangles_,triangleBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,true)||!core_.buffer(scratch_,scratchBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true))return core_.fatal("Vulkan voxel frame allocation failed");
  auto* destination=static_cast<Triangle*>(triangles_.mapped);uint32_t offset=0;
  for(const auto&job:jobs){const auto&p=*job.raster;const uint32_t pixels=uint32_t(p.width)*p.height;
   if(!p.triangles.empty())std::memcpy(destination+offset,p.triangles.data(),p.triangles.size()*sizeof(Triangle));
   commands_.push_back({offset,uint32_t(p.triangles.size()),0,pixels,job.colorOffset,job.indexOffset,uint32_t(p.width),uint32_t(p.height)});offset+=uint32_t(p.triangles.size());
  }
  if(!core_.flush(triangles_))return core_.fatal("Vulkan voxel triangle upload flush failed");
  VkDescriptorBufferInfo info[4]{{triangles_.buffer,0,triangleBytes},{scratch_.buffer,0,scratchBytes},{colors,0,colorBytes},{indices,0,indexBytes}};VkWriteDescriptorSet writes[4]{};
  for(uint32_t n=0;n<4;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=set_;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[n].pBufferInfo=&info[n];}
  core_.vkUpdateDescriptorSets_(core_.device_,4,writes,0,nullptr);
  stats_.jobs=jobs.size();stats_.triangles=totalTriangles;stats_.uploadedBytes=totalTriangles*sizeof(Triangle);prepared_=true;return true;
 }
 void record(VkCommandBuffer command){
  if(!prepared_||commands_.empty()||core_.disabled_||core_.inFlight_)return;
  bool prior=false;
  for(const auto&p:commands_){const uint64_t pixels=uint64_t(p.width)*p.height;
   if(prior)barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
   core_.vkCmdFillBuffer_(command,scratch_.buffer,VkDeviceSize(p.depthOffset)*4,pixels*4,0x007fffffu);
   core_.vkCmdFillBuffer_(command,scratch_.buffer,VkDeviceSize(p.winnerOffset)*4,pixels*4,0xffffffffu);
   barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
   core_.vkCmdBindDescriptorSets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set_,0,nullptr);
   core_.vkCmdPushConstants_(command,layout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&p);
   for(unsigned pass=0;pass<3;++pass){
    core_.vkCmdBindPipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines_[pass]);
    if(pass<2){if(p.triangleCount){core_.vkCmdDispatch_(command,(p.triangleCount+63)/64,1,1);++stats_.dispatches;}}
    else{uint32_t groups=uint32_t((pixels+63)/64),x=std::min(groups,core_.properties_.limits.maxComputeWorkGroupCount[0]);core_.vkCmdDispatch_(command,x,(groups+x-1)/x,1);++stats_.dispatches;}
    barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
   }
   prior=true;
  }
 }
 void commit(){prepared_=false;commands_.clear();}
};
}}

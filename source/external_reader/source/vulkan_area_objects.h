#pragma once
#include "gpu_area.h"
#include "gpu_draw_encoding.h"
#include "immutable_atlas_plan.h"
#include "vulkan_voxel_backend.h"
#include "vulkan_scene_spirv.h"
#include <set>
namespace external { namespace gpuarea {
// GPL-3.0 adaptation of Vera's retained atlas / native ordered composition.
// Color/Z is evaluated at native pixels on changed pages, before AreaResolve.
// Uses the SAME shader body and DrawJob encoding as the main viewport.
class ObjectStage {
 using Core=vulkancompute::Resolve;using Buffer=Core::Buffer;using Source=immutableatlas::Source;using UploadPlan=immutableatlas::Plan;
 static constexpr VkDeviceSize Bytes=128ull*1024*1024;
 Core&core_;gpuvoxel::Stage voxels_;Buffer colors_,materials_,commands_,colorUpload_,materialUpload_,nativeZ_;
 std::vector<immutableatlas::Resident>colorResident_,materialResident_;UploadPlan colorPlan_,materialPlan_;
 std::vector<VkBufferCopy>colorCopies_,materialCopies_;std::array<uint32_t,32>params_{};
 VkDescriptorSetLayout layout_=VK_NULL_HANDLE;VkPipelineLayout pipelineLayout_=VK_NULL_HANDLE;VkDescriptorPool pool_=VK_NULL_HANDLE;
 VkDescriptorSet set_=VK_NULL_HANDLE;VkPipeline pipeline_=VK_NULL_HANDLE;bool ready_=false,banked_=false;uint32_t jobs_=0;
 uint64_t uploads_=0,uploaded_=0,sceneDispatches_=0;std::vector<uint32_t>displaced_;std::map<std::pair<uint32_t,uint32_t>,uint32_t>alphaOffsets_;
 template<class T>static Source source(const std::shared_ptr<const std::vector<T>>&v,uint64_t revision=0){return v?Source{std::shared_ptr<const void>(v,v.get()),v->data(),v->size()*sizeof(T),false,revision}:Source{};}
 bool fail(const char*why){return core_.fail(why);}
 void barrier(VkCommandBuffer command,VkPipelineStageFlags from,VkPipelineStageFlags to,VkAccessFlags src,VkAccessFlags dst){
  VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;core_.vkCmdPipelineBarrier_(command,from,to,0,1,&b,0,nullptr,0,nullptr);
 }
 bool initialize(bool banked){
  if(ready_)return banked==banked_;banked_=banked;const uint32_t bindings=banked?8:5;
  if(core_.properties_.limits.maxPerStageDescriptorStorageBuffers<bindings||core_.properties_.limits.maxDescriptorSetStorageBuffers<bindings||core_.properties_.limits.maxPushConstantsSize<128)return fail("GPU native-page object limits unavailable");
  VkDescriptorSetLayoutBinding b[8]{};for(uint32_t n=0;n<bindings;++n){b[n].binding=!banked&&n==4?7:n;b[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;b[n].descriptorCount=1;b[n].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ds.bindingCount=bindings;ds.pBindings=b;
  if(core_.vkCreateDescriptorSetLayout_(core_.device_,&ds,nullptr,&layout_)!=VK_SUCCESS)return fail("GPU native-page object layout failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,128};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&layout_;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;
  if(core_.vkCreatePipelineLayout_(core_.device_,&pl,nullptr,&pipelineLayout_)!=VK_SUCCESS)return fail("GPU native-page object pipeline layout failed");
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=banked?sizeof(vulkancompute::ScenePagesBankedShader):sizeof(vulkancompute::ScenePagesShader);sm.pCode=banked?vulkancompute::ScenePagesBankedShader:vulkancompute::ScenePagesShader;
  VkShaderModule shader=VK_NULL_HANDLE;if(core_.vkCreateShaderModule_(core_.device_,&sm,nullptr,&shader)!=VK_SUCCESS)return fail("GPU native-page shader failed");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=shader;cp.stage.pName="main";cp.layout=pipelineLayout_;
  auto status=core_.vkCreateComputePipelines_(core_.device_,VK_NULL_HANDLE,1,&cp,nullptr,&pipeline_);core_.vkDestroyShaderModule_(core_.device_,shader,nullptr);if(status!=VK_SUCCESS)return fail("GPU native-page pipeline failed");
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,bindings};VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
  if(core_.vkCreateDescriptorPool_(core_.device_,&pool,nullptr,&pool_)!=VK_SUCCESS)return fail("GPU native-page descriptor pool failed");
  VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=pool_;alloc.descriptorSetCount=1;alloc.pSetLayouts=&layout_;
  if(core_.vkAllocateDescriptorSets_(core_.device_,&alloc,&set_)!=VK_SUCCESS)return fail("GPU native-page descriptor set failed");
  if(!core_.buffer(colors_,Bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true)||!core_.buffer(materials_,Bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true))return fail("GPU native-page material allocation failed");
  ready_=true;return true;
 }
 bool upload(const std::vector<Source>&sources,const UploadPlan&plan,Buffer&staging,std::vector<VkBufferCopy>&copies){
  copies.clear();if(!plan.bytes)return true;if(!core_.buffer(staging,plan.bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true,false,Bytes))return false;
  VkDeviceSize at=0;for(size_t n:plan.dirty){const auto&s=sources[n];if(s.generated)continue;const VkDeviceSize bytes=(s.bytes+3)/4*4;
   std::memcpy(static_cast<char*>(staging.mapped)+at,s.data,s.bytes);if(bytes>s.bytes)std::memset(static_cast<char*>(staging.mapped)+at+s.bytes,0,size_t(bytes-s.bytes));
   VkBufferCopy copy{at,VkDeviceSize(plan.offsets[n])*4,bytes};if(!copies.empty()&&copies.back().srcOffset+copies.back().size==copy.srcOffset&&copies.back().dstOffset+copies.back().size==copy.dstOffset)copies.back().size+=bytes;else copies.push_back(copy);at+=bytes;
  }return core_.flush(staging);
 }
public:
 explicit ObjectStage(Core&core):core_(core),voxels_(core){}
 ~ObjectStage(){release();}
 void release(){
  if(core_.inFlight_)return;voxels_.close();
  if(core_.device_){for(auto*b:{&colors_,&materials_,&commands_,&colorUpload_,&materialUpload_,&nativeZ_})core_.destroy(*b);
   if(pipeline_)core_.vkDestroyPipeline_(core_.device_,pipeline_,nullptr);if(pool_)core_.vkDestroyDescriptorPool_(core_.device_,pool_,nullptr);
   if(pipelineLayout_)core_.vkDestroyPipelineLayout_(core_.device_,pipelineLayout_,nullptr);if(layout_)core_.vkDestroyDescriptorSetLayout_(core_.device_,layout_,nullptr);
  }alphaOffsets_.clear();pipeline_=VK_NULL_HANDLE;pool_=VK_NULL_HANDLE;pipelineLayout_=VK_NULL_HANDLE;layout_=VK_NULL_HANDLE;set_=VK_NULL_HANDLE;ready_=false;jobs_=0;colorResident_.clear();materialResident_.clear();
 }
 bool prepare(const Plan&p,const std::vector<uint32_t>&slots,const std::vector<size_t>&dirty,const std::array<Buffer*,4>&banks,bool banked){
  jobs_=0;sceneDispatches_=0;displaced_.clear();uploads_=uploaded_=0;std::vector<size_t>pages;for(auto n:dirty)if(p.nativeSurface||!p.pages[n].draws.empty())pages.push_back(n);if(pages.empty())return true;
  if(!p.objects||p.objects->size()>65536||pages.size()>core_.properties_.limits.maxComputeWorkGroupCount[2])return fail("GPU native-page job budget exceeded");
  if(!ready_&&!initialize(banked)){release();return false;}const auto&draws=*p.objects;uint32_t maximumSlot=0;for(uint32_t slot:slots)maximumSlot=std::max(maximumSlot,slot+1);const uint64_t nativeBytes=p.nativeSurface?uint64_t(maximumSlot)*PagePixels*2:4;
  if(!core_.buffer(nativeZ_,nativeBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false,false,std::min<uint64_t>(core_.properties_.limits.maxStorageBufferRange,uint64_t(MaximumPages)*PagePixels*2)))return fail("GPU native Z atlas unavailable");
  std::vector<Source>colors,materials;std::vector<std::array<size_t,8>>sources;std::vector<std::array<int,4>>bounds;
  uint64_t candidates=0;for(const auto&d:draws){const auto&t=d.source;if(p.nativeSurface&&!d.nativeDepth.enabled&&!d.skipZ)return fail("Native page source lacks depth metadata; CPU fallback");uint64_t pixels=uint64_t(t.width)*t.height;
   if(t.width<1||t.height<1||t.width>8192||t.height>8192||pixels>16*1024*1024||(!t.pixels&&!d.gpuRaster)||(t.pixels&&t.pixels->size()!=pixels)||(t.depth&&t.depth->size()!=pixels)||(t.indices&&t.indices->size()!=pixels)||
      d.rasterScale<1||d.rasterScale>4||d.sampling!=vulkancompute::Sampling::Nearest||!std::isfinite(d.left)||!std::isfinite(d.top)||!std::isfinite(d.scaleX)||!std::isfinite(d.scaleY)||d.scaleX<=0||d.scaleY<=0||d.scaleX>8||d.scaleY>8||!std::isfinite(d.depthBase)||!std::isfinite(d.lift)||d.mixStrength>255||d.translucency>3||!std::isfinite(d.clipBottom)||d.nativeBlend>3||(d.nativeBlend&&d.smoothRGB))return fail("Invalid native-page draw");
   for(float gain:d.gains)if(!std::isfinite(gain)||gain<0||gain>16)return fail("Invalid native-page lighting");
   std::array<size_t,8>s{{colors.size(),size_t(-1),size_t(-1),size_t(-1),size_t(-1),size_t(-1),size_t(-1),size_t(-1)}};
   if(d.gpuRaster){if(d.gpuRaster->width!=t.width||d.gpuRaster->height!=t.height)return false;auto owner=std::shared_ptr<const void>(d.gpuRaster,d.gpuRaster.get());colors.push_back({owner,nullptr,size_t(pixels)*4,true,1});s[2]=materials.size();materials.push_back({owner,nullptr,size_t(pixels)*4,true,2});}
   else colors.push_back(source(t.pixels));
   if(d.nativeShape){s[5]=materials.size();materials.push_back(source(d.nativeShape));}if(d.nativeCoverage){s[6]=materials.size();materials.push_back(source(d.nativeCoverage));}if(t.depth){s[1]=materials.size();materials.push_back(source(t.depth));}if(t.indices&&!d.gpuRaster){s[2]=materials.size();materials.push_back(source(t.indices));}
   if(d.palette.table){if(d.palette.table->size()!=size_t(d.palette.rows)*256)return false;s[7]=materials.size();materials.push_back(source(d.palette.table));}
   if(d.sourcePalette){if(d.sourcePalette->size()!=256||s[2]==size_t(-1))return false;s[4]=colors.size();colors.push_back(source(d.sourcePalette));}
   sources.push_back(s);auto b=vulkancompute::drawBounds(d,p.nativeWidth,p.nativeHeight);bounds.push_back(b);candidates+=uint64_t(b[2]-b[0])*(b[3]-b[1]);if(candidates>128ull*1024*1024)return fail("GPU native-page candidate budget exceeded");
  }
  std::vector<size_t>depthSources;for(auto n:pages){bool depth=!p.nativeSurface&&bool(p.pages[n].depth);depthSources.push_back(depth?materials.size():size_t(-1));if(depth)materials.push_back(source(p.pages[n].depth,p.pages[n].version));}
  std::vector<size_t>alphaSources;for(const auto&page:p.pages){alphaSources.push_back(page.nativeAlpha?materials.size():size_t(-1));if(page.nativeAlpha)materials.push_back(source(page.nativeAlpha));}
  if(!immutableatlas::build(colors,colorResident_,colorPlan_,Bytes)||!immutableatlas::build(materials,materialResident_,materialPlan_,Bytes))return fail("GPU native-page material budget exceeded");
  auto offset=[](const UploadPlan&plan,size_t n){return n==size_t(-1)?Missing:plan.offsets[n];};
  std::vector<uint32_t>metadata(draws.size()*64);for(size_t n=0;n<draws.size();++n){const auto&s=sources[n];std::array<uint32_t,8>at{{offset(colorPlan_,s[0]),offset(materialPlan_,s[1]),offset(materialPlan_,s[2]),Missing,offset(colorPlan_,s[4]),offset(materialPlan_,s[5]),offset(materialPlan_,s[6]),offset(materialPlan_,s[7])}};vulkancompute::encodeDraw(draws[n],at,bounds[n],metadata.data()+n*64);}
  params_={};params_[0]=p.nativeWidth;params_[1]=p.nativeHeight;params_[6]=params_[7]=params_[15]=Missing;params_[8]=uint32_t(metadata.size());params_[10]=uint32_t(pages.size());params_[20]=0;params_[21]=uint32_t(draws.size());params_[24]=1;params_[27]=p.nativeSurface;
  alphaOffsets_.clear();for(size_t n=0;n<p.pages.size();++n)alphaOffsets_[{p.pages[n].x,p.pages[n].y}]=offset(materialPlan_,alphaSources[n]);
  for(size_t j=0;j<pages.size();++j){auto n=pages[j];metadata.insert(metadata.end(),{slots[n],p.pages[n].x,p.pages[n].y,offset(materialPlan_,depthSources[j]),Missing,offset(materialPlan_,alphaSources[n])});}
  const size_t bins=pages.size()*16;std::vector<uint32_t>counts(bins),starts(bins+1);
  auto visit=[&](size_t job,uint32_t index,auto fn){const auto&page=p.pages[pages[job]];const auto&b=bounds[index];int ox=int(page.x)*128,oy=int(page.y)*128;
   int l=std::max(0,b[0]-ox),t=std::max(0,b[1]-oy),r=std::min(128,b[2]-ox),bt=std::min(128,b[3]-oy);if(l>=r||t>=bt)return;
   for(int y=t/32;y<=(bt-1)/32;++y)for(int x=l/32;x<=(r-1)/32;++x)fn(job*16+y*4+x);
  };
  for(size_t j=0;j<pages.size();++j)for(auto n:p.pages[pages[j]].draws)visit(j,n,[&](size_t bin){++counts[bin];});
  uint64_t refs=0;for(size_t i=0;i<bins;++i){starts[i]=uint32_t(refs);refs+=counts[i];if(refs>2*1024*1024)return fail("GPU native-page bin budget exceeded");}starts[bins]=uint32_t(refs);
  auto next=starts;std::vector<uint32_t>references(size_t(refs),0);for(size_t j=0;j<pages.size();++j)for(auto n:p.pages[pages[j]].draws)visit(j,n,[&](size_t bin){references[next[bin]++]=n;});
  if(!p.nativeSurface)vulkancompute::encodeReadOnlyRuns(draws,references,starts);params_[3]=uint32_t(metadata.size());for(size_t i=0;i<bins;++i){metadata.push_back(starts[i]);metadata.push_back(counts[i]);}params_[4]=uint32_t(metadata.size());metadata.insert(metadata.end(),references.begin(),references.end());
  if(p.nativeSurface){params_[31]=uint32_t(metadata.size());size_t columns=(p.nativeWidth+127)/128,rows=(p.nativeHeight+127)/128;metadata.resize(metadata.size()+columns*rows*2,0);
   for(size_t n=0;n<p.pages.size();++n){const auto&page=p.pages[n];metadata[params_[31]+(page.y*columns+page.x)*2]=slots[n]+1;}
   for(size_t j=0;j<pages.size();++j){const auto&page=p.pages[pages[j]];metadata[params_[31]+(page.y*columns+page.x)*2+1]=uint32_t(j+1);}
   for(uint32_t n=0;n<draws.size();++n)if(draws[n].displace)displaced_.push_back(n);
  }
  if(metadata.size()>16*1024*1024||!core_.buffer(commands_,metadata.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true))return fail("GPU native-page commands unavailable");
  std::memcpy(commands_.mapped,metadata.data(),metadata.size()*4);if(!core_.flush(commands_)||!upload(colors,colorPlan_,colorUpload_,colorCopies_)||!upload(materials,materialPlan_,materialUpload_,materialCopies_))return fail("GPU native-page upload failed");
  std::vector<gpuvoxel::Stage::Job>voxels;std::set<std::pair<uint32_t,uint32_t>>seen;
  for(size_t n=0;n<draws.size();++n)if(draws[n].gpuRaster){const auto&s=sources[n];auto key=std::make_pair(colorPlan_.offsets[s[0]],materialPlan_.offsets[s[2]]);if((colorPlan_.changed(s[0])||materialPlan_.changed(s[2]))&&seen.insert(key).second)voxels.push_back({draws[n].gpuRaster,key.first,key.second});}
  if(!voxels_.prepare(voxels,colors_.buffer,Bytes,materials_.buffer,Bytes))return false;
  VkDescriptorBufferInfo info[8]{{colors_.buffer,0,Bytes},{materials_.buffer,0,Bytes},{commands_.buffer,0,commands_.size}};
  for(unsigned n=0;n<4;++n){auto*bank=banks[n]&&banks[n]->buffer?banks[n]:banks[0];info[3+n]={bank->buffer,0,bank->size};}
  info[7]={nativeZ_.buffer,0,nativeZ_.size};VkWriteDescriptorSet writes[8]{};const uint32_t bindings=banked?8:5;for(uint32_t n=0;n<bindings;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=set_;writes[n].dstBinding=!banked&&n==4?7:n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[n].pBufferInfo=&info[!banked&&n==4?7:n];}core_.vkUpdateDescriptorSets_(core_.device_,bindings,writes,0,nullptr);
  jobs_=uint32_t(pages.size());uploads_=colorCopies_.size()+materialCopies_.size();uploaded_=colorPlan_.bytes+materialPlan_.bytes+metadata.size()*4;return true;
 }
 void record(VkCommandBuffer command){if(!jobs_)return;
  if(!colorCopies_.empty())core_.vkCmdCopyBuffer_(command,colorUpload_.buffer,colors_.buffer,uint32_t(colorCopies_.size()),colorCopies_.data());
  if(!materialCopies_.empty())core_.vkCmdCopyBuffer_(command,materialUpload_.buffer,materials_.buffer,uint32_t(materialCopies_.size()),materialCopies_.data());
  barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
  voxels_.record(command);barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
  core_.vkCmdBindPipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);core_.vkCmdBindDescriptorSets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout_,0,1,&set_,0,nullptr);
  const uint32_t count=params_[21];auto dispatch=[&](uint32_t first,uint32_t last,bool serial){params_[20]=first;params_[21]=last;params_[24]=(sceneDispatches_?0u:1u)|(serial?2u:0u);core_.vkCmdPushConstants_(command,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof params_,params_.data());core_.vkCmdDispatch_(command,serial?1:16,serial?1:16,serial?1:jobs_);++sceneDispatches_;barrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);};
  uint32_t first=0;for(uint32_t index:displaced_){if(index>first||!sceneDispatches_)dispatch(first,index,false);dispatch(index,index+1,true);first=index+1;}dispatch(first,count,false);
 }
 void commit(){if(!jobs_)return;colorResident_=std::move(colorPlan_.retained);materialResident_=std::move(materialPlan_.retained);voxels_.commit();}
 bool coversNativeZ(size_t slots)const{return nativeZ_.size>=uint64_t(slots)*PagePixels*2;}
 VkDescriptorBufferInfo nativeZInfo()const{return {nativeZ_.buffer,0,nativeZ_.size};}
 VkDescriptorBufferInfo nativeAlphaInfo()const{return {materials_.buffer,0,materials_.size};}
 uint32_t alphaOffset(uint32_t x,uint32_t y)const{auto at=alphaOffsets_.find({x,y});return at==alphaOffsets_.end()?Missing:at->second;}
 uint64_t jobs()const{return jobs_;}uint64_t uploads()const{return uploads_;}uint64_t uploadedBytes()const{return uploaded_+(jobs_?voxels_.stats().uploadedBytes:0);}
 uint64_t dispatches()const{return jobs_?sceneDispatches_+voxels_.stats().dispatches:0;}uint64_t rasters()const{return jobs_?voxels_.stats().jobs:0;}
};
}}

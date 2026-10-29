#pragma once
#include "vulkan_scene_types.h"
#include "gpu_motion.h"
#include "vulkan_scene_spirv.h"
#include "vulkan_voxel_backend.h"
#include "vulkan_xbrz_backend.h"
#include "gpu_xbrz_selfcheck.h"
#include "vulkan_shroud_backend.h"
#include <set>
namespace external { namespace vulkancompute {
// Per-pixel ordered composition: a pixel is owned by one invocation, while
// 32x32 bins restrict it to sprites which can affect that region. No atomics,
// inter-draw barriers or per-sprite framebuffer readbacks are needed.
class FrameResolve : public Resolve {
 struct Source {std::shared_ptr<const void> owner;const void* data=nullptr;size_t bytes=0;bool generated=false;uint64_t signature=0;};
 struct Resident {std::weak_ptr<const void> owner;const void* address=nullptr;uint32_t offset=0,words=0;uint64_t signature=0;};
 struct UploadPlan {std::vector<Resident> retained;std::vector<uint32_t> offsets;std::vector<size_t> dirty;VkDeviceSize bytes=0;};
 struct SceneParams {uint32_t width,height,drawsOffset,binsOffset,refsOffset,binColumns,depthOffset,fogOffset,clipLeft,clipTop,clipRight,clipBottom,depthWidth,depthXOffset,depthYOffset,guideOffset,baseMotionX,baseMotionY,depthFar,depthScale;};
 static constexpr VkDeviceSize MaterialBytes=96ull*1024*1024;
 gpuvoxel::Stage voxels_{*this};
 gpuxbrz::Stage xbrz_{*this};
 gpushroud::Stage shroud_{*this};
 static constexpr uint32_t Missing=0xffffffffu,DrawWords=32,MaxDraws=65536,MaxRefs=2*1024*1024;
 Buffer materials_,materialUpload_,commands_;
 gpumotion::History motionHistory_;std::shared_ptr<const gpumotion::Guides> guides_;bool guidesBuffered_=false;
 int guideRejectedWidth_=0,guideRejectedHeight_=0;uint64_t guideAllocationFailures_=0;
 std::vector<Resident> colorResident_,materialResident_;
 VkDescriptorSetLayout sceneDescriptorLayout_=VK_NULL_HANDLE;VkPipelineLayout scenePipelineLayout_=VK_NULL_HANDLE;
 VkDescriptorPool sceneDescriptorPool_=VK_NULL_HANDLE;VkDescriptorSet sceneDescriptors_=VK_NULL_HANDLE;
 VkPipeline scenePipeline_=VK_NULL_HANDLE,scene64Pipeline_=VK_NULL_HANDLE;
 bool sceneReady_=false,xbrzChecked_=false,xbrzChecking_=false,xbrzRejected_=false;
 template<class T>static Source source(const std::shared_ptr<const std::vector<T>>& v){return v?Source{std::shared_ptr<const void>(v,v.get()),v->data(),v->size()*sizeof(T)}:Source{};}
 static bool identical(const Resident&r,const Source&s){return r.address==s.owner.get()&&!r.owner.owner_before(s.owner)&&!s.owner.owner_before(r.owner)&&r.words==(s.bytes+3)/4&&r.signature==s.signature;}
 bool plan(const std::vector<Source>&input,const std::vector<Resident>&resident,UploadPlan&out,VkDeviceSize maximum){
  std::vector<Resident> active;std::vector<size_t> indices;uint64_t total=0;
  for(const auto&s:input){if(!s.owner||(!s.data&&!s.generated)||!s.bytes||s.bytes>maximum)return fail("Invalid immutable GPU source");size_t n=0;for(;n<active.size();++n)if(identical(active[n],s))break;
   if(n==active.size()){Resident r{s.owner,s.owner.get(),0,uint32_t((s.bytes+3)/4),s.signature};active.push_back(r);total+=r.words;}indices.push_back(n);
  }
  if(total>maximum/4)return fail("GPU scene texture budget exceeded");
  out.retained=resident;out.retained.erase(std::remove_if(out.retained.begin(),out.retained.end(),[](const Resident&r){return r.owner.expired();}),out.retained.end());
  bool repack=false;std::vector<bool>dirty(active.size(),false);
  for(size_t n=0;n<active.size();++n){const auto&original=input[std::find(indices.begin(),indices.end(),n)-indices.begin()];auto found=std::find_if(out.retained.begin(),out.retained.end(),[&](const Resident&r){return identical(r,original);});
   if(found!=out.retained.end()){active[n].offset=found->offset;continue;}
   std::sort(out.retained.begin(),out.retained.end(),[](const Resident&a,const Resident&b){return a.offset<b.offset;});uint64_t offset=0;
   for(const auto&r:out.retained){if(offset+active[n].words<=r.offset)break;offset=uint64_t(r.offset)+r.words;}
   if(offset+active[n].words>maximum/4){repack=true;break;}
   active[n].offset=uint32_t(offset);out.retained.push_back(active[n]);dirty[n]=true;
  }
  if(repack){out.retained=active;uint32_t offset=0;for(size_t n=0;n<active.size();++n){active[n].offset=out.retained[n].offset=offset;offset+=active[n].words;dirty[n]=true;}}
  out.offsets.reserve(input.size());std::vector<bool>seen(active.size(),false);
  for(size_t i=0;i<input.size();++i){const size_t n=indices[i];out.offsets.push_back(active[n].offset);if(dirty[n]&&!seen[n]){seen[n]=true;out.dirty.push_back(i);if(!input[i].generated)out.bytes+=VkDeviceSize(active[n].words)*4;}}
  return true;
 }
 bool uploadPlan(const std::vector<Source>&input,const UploadPlan&plan,Buffer&staging,std::vector<VkBufferCopy>&copies,VkDeviceSize maximum){
  if(!plan.bytes)return true;if(!buffer(staging,plan.bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true,false,maximum))return fatal("GPU scene staging allocation failed");
  VkDeviceSize offset=0;for(size_t i:plan.dirty){const auto&s=input[i];if(s.generated)continue;VkDeviceSize bytes=((s.bytes+3)/4)*4;std::memcpy(static_cast<char*>(staging.mapped)+offset,s.data,s.bytes);if(bytes>s.bytes)std::memset(static_cast<char*>(staging.mapped)+offset+s.bytes,0,size_t(bytes-s.bytes));copies.push_back({offset,VkDeviceSize(plan.offsets[i])*4,bytes});offset+=bytes;}
  if(!flush(staging))return fatal("GPU scene staging flush failed");return true;
 }
 bool pipeline(const uint32_t*words,size_t bytes,VkPipeline&out){
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=bytes;sm.pCode=words;VkShaderModule module=VK_NULL_HANDLE;
  if(vkCreateShaderModule_(device_,&sm,nullptr,&module)!=VK_SUCCESS)return fatal("GPU scene shader creation failed");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=module;cp.stage.pName="main";cp.layout=scenePipelineLayout_;
  VkResult status=vkCreateComputePipelines_(device_,VK_NULL_HANDLE,1,&cp,nullptr,&out);vkDestroyShaderModule_(device_,module,nullptr);return status==VK_SUCCESS||fatal("GPU scene pipeline creation failed");
 }
 bool sceneInitialize(){
  if(!initialize())return false;if(sceneReady_)return true;
  if(properties_.limits.maxPerStageDescriptorStorageBuffers<4||properties_.limits.maxDescriptorSetStorageBuffers<4)return fatal("GPU scene storage bindings unavailable");
  VkDescriptorSetLayoutBinding bindings[4]{};for(uint32_t i=0;i<4;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ds.bindingCount=4;ds.pBindings=bindings;if(vkCreateDescriptorSetLayout_(device_,&ds,nullptr,&sceneDescriptorLayout_)!=VK_SUCCESS)return fatal("GPU scene descriptor layout failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(SceneParams)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&sceneDescriptorLayout_;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout_(device_,&pl,nullptr,&scenePipelineLayout_)!=VK_SUCCESS)return fatal("GPU scene pipeline layout failed");
  if(!pipeline(SceneShader,sizeof(SceneShader),scenePipeline_))return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4};VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
  if(vkCreateDescriptorPool_(device_,&pool,nullptr,&sceneDescriptorPool_)!=VK_SUCCESS)return fatal("GPU scene descriptor pool failed");
  VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=sceneDescriptorPool_;alloc.descriptorSetCount=1;alloc.pSetLayouts=&sceneDescriptorLayout_;
  if(vkAllocateDescriptorSets_(device_,&alloc,&sceneDescriptors_)!=VK_SUCCESS)return fatal("GPU scene descriptor allocation failed");
  if(!buffer(materials_,MaterialBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false,true))return fatal("GPU scene material atlas allocation failed");sceneReady_=true;return true;
 }
 bool verifyXbrz(){
  if(xbrzRejected_)return fail("GPU xBRZ device self-check failed; CPU fallback");if(xbrzChecked_||xbrzChecking_)return true;
  xbrzChecking_=true;FrameJob check;check.width=63;check.height=28;
  auto source=std::make_shared<const std::vector<uint32_t>>(gpuxbrz::SelfCheckSource,gpuxbrz::SelfCheckSource+49);
  int left=0;for(int factor:{2,3,4}){check.baseTiles.push_back({source,7,7,left,0,7*factor,7*factor,0,0,1,1,factor});left+=7*factor;}
  std::vector<uint32_t> pixels;bool matches=renderFrame(check,pixels);left=0;
  if(matches)for(int factor:{2,3,4}){const uint32_t*expected=factor==2?gpuxbrz::SelfCheck2:factor==3?gpuxbrz::SelfCheck3:gpuxbrz::SelfCheck4;const int size=7*factor;for(int y=0;y<size;++y)for(int x=0;x<size;++x)if(pixels[size_t(y)*63+left+x]!=expected[size_t(y)*size+x])matches=false;left+=size;}
  xbrzChecking_=false;xbrzChecked_=matches;xbrzRejected_=!matches;
  if(!matches)return fail("GPU xBRZ device self-check failed; CPU fallback");
  diagnostics_.uploads=diagnostics_.uploadedBytes=diagnostics_.dispatches=diagnostics_.readbacks=diagnostics_.xbrzBuilds=diagnostics_.voxelRasters=0;return true;
 }
 static uint32_t bits(float value){uint32_t result;std::memcpy(&result,&value,4);return result;}
 void sceneClose(){
  if(!device_)return;
  if(inFlight_&&vkGetFenceStatus_&&vkGetFenceStatus_(device_,fence_)!=VK_SUCCESS)return;
  destroy(materials_);destroy(materialUpload_);destroy(commands_);
  if(scenePipeline_&&vkDestroyPipeline_)vkDestroyPipeline_(device_,scenePipeline_,nullptr);
  if(scene64Pipeline_&&vkDestroyPipeline_)vkDestroyPipeline_(device_,scene64Pipeline_,nullptr);
  if(sceneDescriptorPool_&&vkDestroyDescriptorPool_)vkDestroyDescriptorPool_(device_,sceneDescriptorPool_,nullptr);
  if(scenePipelineLayout_&&vkDestroyPipelineLayout_)vkDestroyPipelineLayout_(device_,scenePipelineLayout_,nullptr);
  if(sceneDescriptorLayout_&&vkDestroyDescriptorSetLayout_)vkDestroyDescriptorSetLayout_(device_,sceneDescriptorLayout_,nullptr);
  scenePipeline_=scene64Pipeline_=VK_NULL_HANDLE;sceneDescriptorPool_=VK_NULL_HANDLE;sceneDescriptors_=VK_NULL_HANDLE;scenePipelineLayout_=VK_NULL_HANDLE;sceneDescriptorLayout_=VK_NULL_HANDLE;sceneReady_=false;
 }
 void cleanupFailure(){if(!disabled_||inFlight_)return;shroud_.release();voxels_.close();xbrz_.close();sceneClose();colorResident_.clear();materialResident_.clear();Resolve::close();memoryDiagnostics();}
public:
 ~FrameResolve(){sceneClose();}
 bool supportsFloat64(){return initialize()&&features_.shaderFloat64;}
 bool render(int w,int h,const std::vector<TileJob>&jobs,std::vector<uint32_t>&out){guides_.reset();motionHistory_.clear();if(guidesBuffered_&&!inFlight_){destroy(output_);destroy(readback_);guidesBuffered_=false;}for(const auto&job:jobs)if(job.reconstructionFactor){FrameJob frame;frame.width=w;frame.height=h;frame.baseTiles=jobs;return renderFrame(frame,out);}colorResident_.clear();const bool ok=Resolve::render(w,h,jobs,out);if(!ok)cleanupFailure();return ok;}
 std::shared_ptr<const gpumotion::Guides> guides()const{return guides_;}
 uint64_t guideAllocationFailures()const{return guideAllocationFailures_;}
 bool renderFrame(const FrameJob&frame,std::vector<uint32_t>&out,std::vector<uint8_t>*maskOut=nullptr){guides_.reset();const bool ok=renderFrameImpl(frame,out,maskOut);if(!ok){motionHistory_.clear();cleanupFailure();}return ok;}
private:
 bool renderFrameImpl(const FrameJob&frame,std::vector<uint32_t>&out,std::vector<uint8_t>*maskOut){
  const auto started=std::chrono::steady_clock::now();diagnostics_.uploads=diagnostics_.uploadedBytes=diagnostics_.dispatches=diagnostics_.readbacks=diagnostics_.xbrzBuilds=diagnostics_.voxelRasters=0;
  const int w=frame.width,h=frame.height;const uint64_t pixels=uint64_t(std::max(0,w))*std::max(0,h);
  if(disabled_)return false;if(w<1||h<1||w>8192||h>8192||pixels>MaxPixels||frame.draws.size()>MaxDraws||(bool(frame.basePixels)==!frame.baseTiles.empty())||(frame.basePixels&&frame.basePixels->size()!=pixels)||(frame.baseDepth&&frame.baseDepthWidth==0&&frame.baseDepth->size()!=pixels)||(frame.fogAlpha&&frame.fogAlpha->size()!=pixels))return fail("Invalid GPU scene frame");
  if(!frame.collectGuides||w!=guideRejectedWidth_||h!=guideRejectedHeight_)guideRejectedWidth_=guideRejectedHeight_=0;
  gpumotion::Plan guidePlan;bool collectGuides=frame.collectGuides&&!(w==guideRejectedWidth_&&h==guideRejectedHeight_)&&frame.baseDepth&&motionHistory_.prepare(frame,guidePlan);
  if(!collectGuides)motionHistory_.clear();
  bool needs64=false;std::vector<Source>colors,materials;std::vector<std::array<size_t,4>>drawSources;size_t baseSource=0,baseDepth=0,baseDepthX=0,baseDepthY=0,fog=0;
  if(frame.basePixels){baseSource=colors.size();colors.push_back(source(frame.basePixels));}
  uint64_t coordinateCount=0;if(frame.baseTiles.size()>4096)return fail("GPU scene base tile count exceeded");
  std::vector<size_t>tileSources;for(const auto&t:frame.baseTiles){
   if(t.reconstructionFactor&&(t.reconstructionFactor<2||t.reconstructionFactor>4))return fail("Invalid GPU xBRZ factor");
   const uint64_t f=t.reconstructionFactor?t.reconstructionFactor:1;
   if(uint64_t(t.sourceWidth)*f>8192||uint64_t(t.sourceHeight)*f>8192||uint64_t(t.sourceWidth)*t.sourceHeight*f*f>MaxPixels)return fail("GPU xBRZ dimensions exceeded");
   if(!t.pixels||t.sourceWidth<1||t.sourceHeight<1||t.sourceWidth>8192||t.sourceHeight>8192||uint64_t(t.sourceWidth)*t.sourceHeight!=t.pixels->size()||t.left<0||t.top<0||t.width<1||t.height<1||int64_t(t.left)+t.width>w||int64_t(t.top)+t.height>h||!std::isfinite(t.sourceX)||!std::isfinite(t.sourceY)||!std::isfinite(t.stepX)||!std::isfinite(t.stepY))return fail("Invalid GPU scene base tile");coordinateCount+=uint64_t(t.width)+t.height;if(coordinateCount>MaxCoordinates)return fail("GPU scene coordinate budget exceeded");tileSources.push_back(colors.size());if(t.reconstructionFactor){auto owner=std::shared_ptr<const void>(t.pixels,t.pixels.get());const uint64_t signature=(uint64_t(t.reconstructionFactor)<<56)|(uint64_t(t.sourceWidth)<<28)|uint64_t(t.sourceHeight);colors.push_back({owner,nullptr,t.pixels->size()*size_t(f)*size_t(f)*4,true,signature});}else colors.push_back(source(t.pixels));
  }
  for(size_t i=0;i<frame.baseTiles.size();++i)for(size_t k=0;k<i;++k){const auto&a=frame.baseTiles[i];const auto&b=frame.baseTiles[k];if(a.left<b.left+b.width&&b.left<a.left+a.width&&a.top<b.top+b.height&&b.top<a.top+a.height)return fail("GPU scene base tiles overlap");}
  if(frame.baseDepthWidth){if(!frame.baseDepth||frame.baseDepthWidth<1||frame.baseDepthHeight<1||frame.baseDepthWidth>8192||frame.baseDepthHeight>8192||uint64_t(frame.baseDepthWidth)*frame.baseDepthHeight>MaxPixels||frame.baseDepth->size()!=uint64_t(frame.baseDepthWidth)*frame.baseDepthHeight||!frame.baseDepthXs||!frame.baseDepthYs||frame.baseDepthXs->size()!=size_t(w)||frame.baseDepthYs->size()!=size_t(h))return fail("Invalid native GPU depth plane");for(auto x:*frame.baseDepthXs)if(x>=uint32_t(frame.baseDepthWidth))return fail("Invalid GPU depth X lookup");for(auto y:*frame.baseDepthYs)if(y>=uint32_t(frame.baseDepthHeight))return fail("Invalid GPU depth Y lookup");baseDepthX=materials.size();materials.push_back(source(frame.baseDepthXs));baseDepthY=materials.size();materials.push_back(source(frame.baseDepthYs));}
  if(frame.baseDepth){baseDepth=materials.size();materials.push_back(source(frame.baseDepth));}
  if(frame.maskPlan){if(frame.maskPlan->width!=w||frame.maskPlan->height!=h)return fail("GPU mask dimensions disagree");fog=materials.size();auto owner=std::shared_ptr<const void>(frame.maskPlan,frame.maskPlan.get());materials.push_back({owner,nullptr,frame.maskPlan->outputWords()*4,true,3});}
  else if(frame.fogAlpha){fog=materials.size();materials.push_back(source(frame.fogAlpha));}
  uint64_t candidatePixels=0;std::vector<std::array<int,4>> bounds;bounds.reserve(frame.draws.size());
  for(const auto&d:frame.draws){
   const auto&t=d.source;if((!t.pixels&&!d.gpuRaster)||t.width<1||t.height<1||t.width>8192||t.height>8192||uint64_t(t.width)*t.height>MaxPixels||(t.pixels&&uint64_t(t.width)*t.height!=t.pixels->size())||(t.depth&&t.depth->size()!=uint64_t(t.width)*t.height)||(t.indices&&t.indices->size()!=uint64_t(t.width)*t.height)||d.rasterScale<1||d.rasterScale>4||!std::isfinite(d.left)||!std::isfinite(d.top)||!std::isfinite(d.scaleX)||!std::isfinite(d.scaleY)||d.scaleX<=0||d.scaleY<=0||d.scaleX>8||d.scaleY>8||!std::isfinite(d.depthBase)||!std::isfinite(d.lift)||uint32_t(d.sampling)>3)return fail("Invalid GPU scene sprite");
   for(float gain:d.gains)if(!std::isfinite(gain)||gain<0||gain>16)return fail("Invalid GPU scene light");
   std::array<size_t,4> sources{{colors.size(),size_t(-1),size_t(-1),size_t(-1)}};
   if(d.gpuRaster){if(d.gpuRaster->width!=t.width||d.gpuRaster->height!=t.height)return fail("GPU voxel dimensions disagree");auto owner=std::shared_ptr<const void>(d.gpuRaster,d.gpuRaster.get());colors.push_back({owner,nullptr,size_t(t.width)*t.height*4,true,1});sources[2]=materials.size();materials.push_back({owner,nullptr,size_t(t.width)*t.height*4,true,2});}
   else colors.push_back(source(t.pixels));
   if(t.depth){sources[1]=materials.size();materials.push_back(source(t.depth));}if(t.indices&&!d.gpuRaster){sources[2]=materials.size();materials.push_back(source(t.indices));}
   if(d.sampling==Sampling::Reconstructed){if((!d.reconstructed&&!d.reconstructionSource)||(d.reconstructed&&d.reconstructionSource)||d.reconstructionFactor<2||d.reconstructionFactor>4||d.reconstructedWidth!=t.width*d.reconstructionFactor||d.reconstructedHeight!=t.height*d.reconstructionFactor||(d.reconstructed&&d.reconstructed->size()!=uint64_t(d.reconstructedWidth)*d.reconstructedHeight)||(d.reconstructionSource&&d.reconstructionSource->size()!=uint64_t(t.width)*t.height))return fail("Invalid reconstructed GPU scene sprite");sources[3]=colors.size();if(d.reconstructionSource){auto owner=std::shared_ptr<const void>(d.reconstructionSource,d.reconstructionSource.get());uint64_t signature=(uint64_t(d.reconstructionFactor)<<56)|(uint64_t(t.width)<<28)|uint64_t(t.height);colors.push_back({owner,nullptr,size_t(d.reconstructedWidth)*d.reconstructedHeight*4,true,signature});}else colors.push_back(source(d.reconstructed));}
   needs64|=d.sampling!=Sampling::Nearest;drawSources.push_back(sources);
   auto bound=[&](float value,int limit,bool high){double v=std::clamp(double(value),-1e7,1e7);return std::clamp(int(high?std::ceil(v):std::floor(v)),0,limit);};
   int left=bound(d.left,w,false),top=bound(d.top,h,false),right=bound(d.left+t.width*d.scaleX,w,true),bottom=bound(d.top+t.height*d.scaleY,h,true);bounds.push_back({left,top,right,bottom});candidatePixels+=uint64_t(std::max(0,right-left))*std::max(0,bottom-top);
  }
  if(candidatePixels>128ull*1024*1024)return fail("GPU scene candidate budget exceeded");if(!sceneInitialize())return false;
  bool rawTiles=false;for(const auto&t:frame.baseTiles)rawTiles|=t.reconstructionFactor!=0;for(const auto&d:frame.draws)rawTiles|=bool(d.reconstructionSource);
  if((needs64||rawTiles)&&!features_.shaderFloat64)return fail("GPU scene sampling requires shaderFloat64; CPU fallback");
  if(needs64&&!scene64Pipeline_&&!pipeline(Scene64Shader,sizeof(Scene64Shader),scene64Pipeline_))return false;
  if(rawTiles&&!verifyXbrz())return false;
  UploadPlan colorPlan,materialPlan;if(!plan(colors,colorResident_,colorPlan,AtlasBytes)||!plan(materials,materialResident_,materialPlan,MaterialBytes))return false;
  std::vector<gpuxbrz::RawJob>rawJobs;
  for(size_t n=0;n<frame.baseTiles.size();++n){const auto&t=frame.baseTiles[n];if(t.reconstructionFactor&&std::find(colorPlan.dirty.begin(),colorPlan.dirty.end(),tileSources[n])!=colorPlan.dirty.end())rawJobs.push_back({t.pixels,t.sourceWidth,t.sourceHeight,t.reconstructionFactor,colorPlan.offsets[tileSources[n]]});}
  for(size_t n=0;n<frame.draws.size();++n){const auto&d=frame.draws[n];if(d.reconstructionSource&&std::find(colorPlan.dirty.begin(),colorPlan.dirty.end(),drawSources[n][3])!=colorPlan.dirty.end())rawJobs.push_back({d.reconstructionSource,d.source.width,d.source.height,d.reconstructionFactor,colorPlan.offsets[drawSources[n][3]]});}
  {std::set<uint32_t>seen;rawJobs.erase(std::remove_if(rawJobs.begin(),rawJobs.end(),[&](const gpuxbrz::RawJob&job){return !seen.insert(job.outputOffset).second;}),rawJobs.end());}
  if(!xbrz_.prepare(rawJobs))return false;diagnostics_.xbrzAvailable=xbrz_.available();
  const bool maskDirty=frame.maskPlan&&std::find(materialPlan.dirty.begin(),materialPlan.dirty.end(),fog)!=materialPlan.dirty.end();
  if(frame.maskPlan&&!shroud_.prepare(frame.maskPlan,materials_.buffer,materialPlan.offsets[fog],MaterialBytes))return false;
  std::vector<gpuvoxel::Stage::Job>voxelJobs;
  for(size_t n=0;n<frame.draws.size();++n)if(frame.draws[n].gpuRaster){const auto&s=drawSources[n];bool dirty=std::find(colorPlan.dirty.begin(),colorPlan.dirty.end(),s[0])!=colorPlan.dirty.end()||std::find(materialPlan.dirty.begin(),materialPlan.dirty.end(),s[2])!=materialPlan.dirty.end();if(dirty)voxelJobs.push_back({frame.draws[n].gpuRaster,colorPlan.offsets[s[0]],materialPlan.offsets[s[2]]});}
  {std::set<std::pair<uint32_t,uint32_t>>seen;voxelJobs.erase(std::remove_if(voxelJobs.begin(),voxelJobs.end(),[&](const gpuvoxel::Stage::Job&job){return !seen.insert({job.colorOffset,job.indexOffset}).second;}),voxelJobs.end());}
  if(!voxels_.prepare(voxelJobs,atlas_.buffer,AtlasBytes,materials_.buffer,MaterialBytes))return false;
  std::vector<VkBufferCopy>colorCopies,materialCopies;if(!uploadPlan(colors,colorPlan,upload_,colorCopies,AtlasBytes)||!uploadPlan(materials,materialPlan,materialUpload_,materialCopies,MaterialBytes))return false;
  const uint32_t cols=(uint32_t(w)+31)/32,rows=(uint32_t(h)+31)/32;std::vector<std::vector<uint32_t>>bins(size_t(cols)*rows);
  for(size_t n=0;n<bounds.size();++n){const auto&r=bounds[n];if(r[0]>=r[2]||r[1]>=r[3])continue;for(int y=r[1]/32;y<=(r[3]-1)/32;++y)for(int x=r[0]/32;x<=(r[2]-1)/32;++x)bins[size_t(y)*cols+x].push_back(uint32_t(n));}
  size_t refs=0;for(const auto&bin:bins)refs+=bin.size();if(refs>MaxRefs)return fail("GPU scene bin budget exceeded");
  std::vector<uint32_t>metadata(frame.draws.size()*DrawWords);auto offset=[&](const UploadPlan&p,size_t n){return n==size_t(-1)?Missing:p.offsets[n];};
  for(size_t n=0;n<frame.draws.size();++n){const auto&d=frame.draws[n];const auto&s=drawSources[n];auto*dst=metadata.data()+n*DrawWords;
   dst[0]=offset(colorPlan,s[0]);dst[1]=offset(materialPlan,s[1]);dst[2]=offset(materialPlan,s[2]);dst[3]=d.source.width;dst[4]=d.source.height;dst[5]=offset(colorPlan,s[3]);dst[6]=d.reconstructedWidth;dst[7]=d.reconstructedHeight;dst[8]=d.reconstructionFactor;
   dst[9]=(d.shadow?1:0)|(d.writeDepth?2:0)|(d.flat?4:0)|(d.objectPalette?8:0)|(d.smoothRGB?16:0)|(d.skipZ?32:0)|(d.gpuRaster?64:0);dst[10]=uint32_t(d.sampling);dst[11]=d.rasterScale;dst[12]=uint32_t(d.originY);dst[13]=uint32_t(d.depthBottom);
   dst[14]=bits(d.left);dst[15]=bits(d.top);dst[16]=bits(d.scaleX);dst[17]=bits(d.scaleY);dst[18]=bits(d.depthBase);dst[19]=bits(d.lift);for(int c=0;c<3;++c)dst[20+c]=bits(d.gains[c]);for(int c=0;c<4;++c)dst[24+c]=bounds[n][c];
   if(collectGuides){dst[28]=bits(guidePlan.draws[n][0]);dst[29]=bits(guidePlan.draws[n][1]);dst[30]=guidePlan.eligible[n];}
  }
  SceneParams params{uint32_t(w),uint32_t(h),0,uint32_t(metadata.size()),0,cols,frame.baseDepth?materialPlan.offsets[baseDepth]:Missing,(frame.maskPlan||frame.fogAlpha)?materialPlan.offsets[fog]:Missing,uint32_t(std::clamp(frame.clipLeft,0,w)),uint32_t(std::clamp(frame.clipTop,0,h)),uint32_t(frame.clipRight<0?w:std::clamp(frame.clipRight,0,w)),uint32_t(frame.clipBottom<0?h:std::clamp(frame.clipBottom,0,h)),uint32_t(frame.baseDepthWidth),frame.baseDepthWidth?materialPlan.offsets[baseDepthX]:Missing,frame.baseDepthWidth?materialPlan.offsets[baseDepthY]:Missing,Missing,0,0,0,0};
  if(collectGuides&&pixels*16>properties_.limits.maxStorageBufferRange){guideRejectedWidth_=w;guideRejectedHeight_=h;collectGuides=false;motionHistory_.clear();}
  if(collectGuides){params.guideOffset=uint32_t(pixels);params.baseMotionX=bits(guidePlan.base[0]);params.baseMotionY=bits(guidePlan.base[1]);params.depthFar=bits(frame.motion.depthFar);params.depthScale=bits(1.f/(frame.motion.depthNear-frame.motion.depthFar));}
  params.refsOffset=params.binsOffset+uint32_t(bins.size()*2);uint32_t position=0;for(const auto&bin:bins){metadata.push_back(position);metadata.push_back(uint32_t(bin.size()));position+=bin.size();}for(const auto&bin:bins)metadata.insert(metadata.end(),bin.begin(),bin.end());
  if(metadata.empty())metadata.push_back(0);const VkDeviceSize outputBytes=pixels*4,totalOutputBytes=outputBytes*(collectGuides?4:1),maskBytes=maskOut&&frame.maskPlan?frame.maskPlan->outputWords()*4:0;
  if(guidesBuffered_&&!collectGuides){destroy(output_);destroy(readback_);}guidesBuffered_=collectGuides;
  if(!buffer(output_,totalOutputBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,collectGuides)||!buffer(readback_,totalOutputBytes+maskBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true,collectGuides)||!buffer(commands_,metadata.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true)){
   if(collectGuides){++guideAllocationFailures_;destroy(output_);destroy(readback_);guidesBuffered_=false;motionHistory_.clear();FrameJob fallback=frame;fallback.collectGuides=false;const bool raw=renderFrameImpl(fallback,out,maskOut);guideRejectedWidth_=w;guideRejectedHeight_=h;return raw;}
   return fatal("GPU scene framebuffer allocation failed");
  }
  std::memcpy(commands_.mapped,metadata.data(),metadata.size()*4);if(!flush(commands_))return fatal("GPU scene command flush failed");
  std::vector<Coordinate>coords;std::vector<Dispatch>baseDispatches;
  for(size_t n=0;n<frame.baseTiles.size();++n){const auto&t=frame.baseTiles[n];int factor=t.reconstructionFactor?t.reconstructionFactor:1;const int sourceWidth=t.sourceWidth*factor,sourceHeight=t.sourceHeight*factor;Params p{colorPlan.offsets[tileSources[n]],uint32_t(sourceWidth),uint32_t(w),uint32_t(t.left),uint32_t(t.top),uint32_t(t.width),uint32_t(t.height),uint32_t(coords.size()),0};for(int x=0;x<t.width;++x)coords.push_back(coordinate(t.sourceX+x*t.stepX,sourceWidth));p.yOffset=uint32_t(coords.size());for(int y=0;y<t.height;++y)coords.push_back(coordinate(t.sourceY+y*t.stepY,sourceHeight));baseDispatches.push_back({p});}
  if(coords.size()>MaxCoordinates)return fail("GPU scene resolve coordinates exceeded");
  if(coords.empty())destroy(coordinates_);
  if(!coords.empty()){if(!buffer(coordinates_,coords.size()*sizeof(Coordinate),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true))return fatal("GPU scene coordinates allocation failed");std::memcpy(coordinates_.mapped,coords.data(),coords.size()*sizeof(Coordinate));if(!flush(coordinates_))return fatal("GPU scene coordinates flush failed");VkDescriptorBufferInfo infos[3]{{atlas_.buffer,0,AtlasBytes},{coordinates_.buffer,0,coords.size()*sizeof(Coordinate)},{output_.buffer,0,outputBytes}};VkWriteDescriptorSet writes[3]{};for(uint32_t i=0;i<3;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=descriptors_;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];}vkUpdateDescriptorSets_(device_,3,writes,0,nullptr);}
  VkDescriptorBufferInfo infos[4]{{atlas_.buffer,0,AtlasBytes},{materials_.buffer,0,MaterialBytes},{commands_.buffer,0,metadata.size()*4},{output_.buffer,0,totalOutputBytes}};VkWriteDescriptorSet writes[4]{};for(uint32_t i=0;i<4;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=sceneDescriptors_;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];}vkUpdateDescriptorSets_(device_,4,writes,0,nullptr);
  if(vkResetCommandPool_(device_,commandPool_,0)!=VK_SUCCESS||vkResetFences_(device_,1,&fence_)!=VK_SUCCESS)return fatal("GPU scene reset failed");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;if(vkBeginCommandBuffer_(command_,&begin)!=VK_SUCCESS)return fatal("GPU scene record failed");
  if(!colorCopies.empty())vkCmdCopyBuffer_(command_,upload_.buffer,atlas_.buffer,uint32_t(colorCopies.size()),colorCopies.data());if(!materialCopies.empty())vkCmdCopyBuffer_(command_,materialUpload_.buffer,materials_.buffer,uint32_t(materialCopies.size()),materialCopies.data());
  if(!colorCopies.empty()&&frame.basePixels){VkMemoryBarrier copyBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};copyBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;copyBarrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&copyBarrier,0,nullptr,0,nullptr);}
  if(frame.basePixels){VkBufferCopy copy{VkDeviceSize(colorPlan.offsets[baseSource])*4,0,outputBytes};vkCmdCopyBuffer_(command_,atlas_.buffer,output_.buffer,1,&copy);}else vkCmdFillBuffer_(command_,output_.buffer,0,outputBytes,0xff080c08u);
  VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
  if(maskDirty){shroud_.record(command_);VkMemoryBarrier maskBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};maskBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;maskBarrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&maskBarrier,0,nullptr,0,nullptr);}
  if(!rawJobs.empty()&&!xbrz_.record(command_,atlas_.buffer))return false;
  if(!voxelJobs.empty())voxels_.record(command_);
  if(!baseDispatches.empty()){vkCmdBindPipeline_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);vkCmdBindDescriptorSets_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout_,0,1,&descriptors_,0,nullptr);for(const auto&d:baseDispatches){vkCmdPushConstants_(command_,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&d.params);vkCmdDispatch_(command_,(d.params.width+7)/8,(d.params.height+7)/8,1);}VkMemoryBarrier resolve{VK_STRUCTURE_TYPE_MEMORY_BARRIER};resolve.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;resolve.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&resolve,0,nullptr,0,nullptr);}
  vkCmdBindPipeline_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,needs64?scene64Pipeline_:scenePipeline_);vkCmdBindDescriptorSets_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,scenePipelineLayout_,0,1,&sceneDescriptors_,0,nullptr);vkCmdPushConstants_(command_,scenePipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(params),&params);vkCmdDispatch_(command_,(w+7)/8,(h+7)/8,1);
  VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&after,0,nullptr,0,nullptr);VkBufferCopy copy{0,0,totalOutputBytes};vkCmdCopyBuffer_(command_,output_.buffer,readback_.buffer,1,&copy);if(maskBytes){VkMemoryBarrier maskCopy{VK_STRUCTURE_TYPE_MEMORY_BARRIER};maskCopy.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;maskCopy.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&maskCopy,0,nullptr,0,nullptr);VkBufferCopy mask{VkDeviceSize(materialPlan.offsets[fog])*4,totalOutputBytes,maskBytes};vkCmdCopyBuffer_(command_,materials_.buffer,readback_.buffer,1,&mask);}VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  if(vkEndCommandBuffer_(command_)!=VK_SUCCESS)return fatal("GPU scene completion failed");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;if(vkQueueSubmit_(queue_,1,&submit,fence_)!=VK_SUCCESS)return fatal("GPU scene submission failed");inFlight_=true;
  const uint64_t waitBudget=AllowSoftware&&diagnostics_.softwareDevice?10000000000ull:100000000ull;const auto waited=vkWaitForFences_(device_,1,&fence_,VK_TRUE,waitBudget);if(waited!=VK_SUCCESS){quarantined().store(true,std::memory_order_relaxed);return fatal("GPU scene timed out/lost; CPU fallback");}inFlight_=false;
  if(!readback_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=readback_.memory;range.size=VK_WHOLE_SIZE;if(vkInvalidateMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return fatal("GPU scene readback invalidation failed");}
  out.resize(size_t(pixels));std::memcpy(out.data(),readback_.mapped,size_t(outputBytes));if(maskOut){if(frame.maskPlan){maskOut->resize(size_t(pixels));std::memcpy(maskOut->data(),static_cast<char*>(readback_.mapped)+totalOutputBytes,size_t(pixels));}else if(frame.fogAlpha)*maskOut=*frame.fogAlpha;else maskOut->clear();}if(collectGuides){
   auto guides=std::make_shared<gpumotion::Guides>();guides->width=w;guides->height=h;guides->resetHistory=guidePlan.reset;guides->sequence=motionHistory_.nextSequence();
   guides->motion.resize(size_t(pixels)*2);guides->depth.resize(size_t(pixels));guides->confidence.resize(size_t(pixels));
   const auto*packed=reinterpret_cast<const uint32_t*>(static_cast<const char*>(readback_.mapped)+outputBytes);
   for(size_t n=0;n<size_t(pixels);++n){uint32_t z=packed[n*3+2],m0=packed[n*3],m1=packed[n*3+1],depthBits=z&0x7fffffffu;
    std::memcpy(&guides->motion[n*2],&m0,4);std::memcpy(&guides->motion[n*2+1],&m1,4);std::memcpy(&guides->depth[n],&depthBits,4);guides->confidence[n]=(z&0x80000000u)?0.f:1.f;
   }motionHistory_.commit(frame);guides_=std::move(guides);
  }
  colorResident_=std::move(colorPlan.retained);materialResident_=std::move(materialPlan.retained);tiles_.clear();voxels_.commit();xbrz_.commit();if(maskDirty)shroud_.commit();++diagnostics_.frames;diagnostics_.uploads=colorCopies.size()+materialCopies.size()+xbrz_.uploads()+(frame.maskPlan?shroud_.uploads():0);diagnostics_.uploadedBytes=colorPlan.bytes+materialPlan.bytes+voxels_.stats().uploadedBytes+xbrz_.uploadedBytes()+(frame.maskPlan?shroud_.uploadedBytes():0);diagnostics_.dispatches=baseDispatches.size()+1+voxels_.stats().dispatches+xbrz_.dispatches()+(maskDirty?shroud_.dispatches():0);diagnostics_.xbrzBuilds=xbrz_.builds();diagnostics_.voxelRasters=voxels_.stats().jobs;diagnostics_.readbacks=1;diagnostics_.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();diagnostics_.reason.clear();return true;
 }
};
}}

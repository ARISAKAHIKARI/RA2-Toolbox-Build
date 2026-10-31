#pragma once
#include "camera_raster.h"
#include "vulkan_scene.h"
#include <functional>
#include <list>
#include <unordered_map>
namespace external { namespace camera {
// A frame keeps immutable image owners, including their depth/index arrays.
// The original calls are retained too: unsupported/failed GPU work replays the
// same Surface::blit operations in this frame, without a GPU image readback.
struct GpuDepthPlan {
 int width=0,height=0;
 std::shared_ptr<const std::vector<uint32_t>> xs,ys;
};
class GpuSceneRecord {
 struct Replay {
  std::shared_ptr<const voxel::Image> image;
  float ax=0,ay=0,sx=1,sy=1,base=0,lift=1,drop=0;lighting::State light;bool smooth=false;
  int dotX=0,dotY=0;uint32_t dot=0;Material material;
 };
 struct LitSource {
  std::weak_ptr<const voxel::Image> owner;const voxel::Image*address=nullptr;
  std::array<float,3> gains{{1,1,1}};colorprecision::Mode precision=colorprecision::Mode::NativeSteps;
  std::shared_ptr<const std::vector<uint32_t>> pixels;uint64_t used=0;
 };
 vulkancompute::FrameJob frame_;std::shared_ptr<const std::vector<uint32_t>> fallbackBase_;std::function<bool(std::vector<uint32_t>&)> cpuBaseFallback_;std::vector<Replay> replay_;
 std::list<LitSource> litSources_;
 std::unordered_multimap<const voxel::Image*,typename std::list<LitSource>::iterator>litIndex_;
 size_t litBytes_=0;uint64_t litBuilds_=0,litHits_=0;
 uint64_t serial_=0;bool valid_=false;
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 static std::shared_ptr<const voxel::Image> retain(const voxel::Image&i,std::shared_ptr<const voxel::Image> owner){
  // A caller-provided owner must own THIS object, not an unrelated stack copy.
  if(owner&&owner.get()==&i)return owner;
  return std::make_shared<const voxel::Image>(i);
 }
 std::shared_ptr<const std::vector<uint32_t>> litSource(const std::shared_ptr<const voxel::Image>&owner,const Light&light){
  const auto&i=*owner;if(i.gpuRaster||i.pixels.size()!=size_t(i.width)*i.height)return {};
  std::array<float,3> gains;std::copy_n(light.gains,3,gains.begin());
  auto range=litIndex_.equal_range(owner.get());
  for(auto at=range.first;at!=range.second;++at){auto cached=at->second;
   if(!cached->owner.owner_before(owner)&&!owner.owner_before(cached->owner)&&cached->precision==precision_&&cached->gains==gains){
    cached->used=++serial_;++litHits_;litSources_.splice(litSources_.end(),litSources_,cached);return cached->pixels;}}
  ++litBuilds_;
  auto pixels=std::make_shared<std::vector<uint32_t>>(i.pixels.size());const bool indexed=i.indices.size()==i.pixels.size();
  for(size_t n=0;n<i.pixels.size();++n)(*pixels)[n]=light.apply(i.pixels[n],indexed?i.indices[n]:0,i.objectPalette);
  constexpr size_t EntryBytes=sizeof(LitSource)+12*sizeof(void*),Budget=xbrzadapter::CachePixels*4;
  const size_t cost=pixels->size()*4+EntryBytes;
  // Bound actual storage, not 128 materials. With 129 live materials the
  // former count limit evicted the next needed input on every single frame.
  if(cost<=Budget){while(!litSources_.empty()&&litBytes_+cost>Budget){auto oldest=litSources_.begin();
    auto entries=litIndex_.equal_range(oldest->address);for(auto at=entries.first;at!=entries.second;++at)if(at->second==oldest){litIndex_.erase(at);break;}
    litBytes_-=oldest->pixels->size()*4+EntryBytes;litSources_.erase(oldest);
   }
   litBytes_+=cost;litSources_.push_back({owner,owner.get(),gains,precision_,pixels,++serial_});litIndex_.emplace(owner.get(),std::prev(litSources_.end()));
  }
  return pixels;
 }
 bool flatten(const std::shared_ptr<const voxel::Image>&owner,float ax,float ay,float sx,float sy,float base,const lighting::State&state,float lift,float shadowDrop,bool smooth,const vulkancompute::MotionIdentity&motion,Material material={},unsigned nesting=0){
  if(!owner||nesting>16)return false;const auto&i=*owner;
  if(i.width<=0||i.height<=0||i.width>resolutionsampling::MaxDimension||i.height>resolutionsampling::MaxDimension||uint64_t(i.width)*i.height>resolutionsampling::MaxPixels||i.rasterScale<1||i.rasterScale>resolutionsampling::MaxRasterScale||(!i.gpuRaster&&i.pixels.size()!=size_t(i.width)*i.height)||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>8||sy>8)return false;
  auto attachment=[&](const voxel::Image&part){return std::shared_ptr<const voxel::Image>(owner,&part);};
  for(size_t index=0;index<i.attachments.size();++index){const auto&part=i.attachments[index];if(part.shadow&&!i.shadowsAfter&&(!part.airborneShadow||shadowDrop>0)){
   auto key=motion;if(motion.known&&frame_.collectGuides)key.part.push_back(uint32_t(index));if(!flatten(attachment(part),ax,ay+shadowDrop*sy,sx,sy,base,state,lift,0,smooth,key,material.shifted(part.shadow?shadowDrop:0),nesting+1))return false;}}
  const float pxScale=sx/i.rasterScale,pyScale=sy/i.rasterScale,left=ax+i.x*pxScale,top=ay+i.y*pyScale;
  int x0=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),0,frame_.width),y0=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),0,frame_.height);
  int x1=std::clamp(int(std::ceil(std::clamp(left+i.width*pxScale,-1e7f,1e7f))),0,frame_.width),y1=std::clamp(int(std::ceil(std::clamp(top+i.height*pyScale,-1e7f,1e7f))),0,frame_.height);
  if(x0<x1&&y0<y1){
   vulkancompute::DrawJob draw;if(frame_.collectGuides)draw.motion=motion;draw.gpuRaster=i.gpuRaster;draw.source.width=i.width;draw.source.height=i.height;
   if(!i.gpuRaster){draw.source.pixels={owner,&i.pixels};if(i.depth.size()==i.pixels.size())draw.source.depth={owner,&i.depth};if(i.indices.size()==i.pixels.size())draw.source.indices={owner,&i.indices};}
   draw.mixStrength=i.shadow?0:material.mixStrength;draw.translucency=i.shadow?0:material.translucency;draw.clipBottom=ay+material.clipBottom*sy;
   draw.sourcePalette=i.sourcePalette;draw.left=left;draw.top=top;draw.scaleX=pxScale;draw.scaleY=pyScale;draw.depthBase=base+i.depthBias;
   draw.originY=i.y;draw.depthBottom=i.depthBottom==std::numeric_limits<int>::min()?i.y+i.height-1:i.depthBottom;draw.rasterScale=i.rasterScale;draw.lift=lift;
   draw.shadow=i.shadow;draw.writeDepth=i.writeDepth;draw.flat=i.flat;draw.objectPalette=i.objectPalette;draw.smoothRGB=precision_==colorprecision::Mode::SmoothRGB;draw.skipZ=i.skipZ;
   draw.nativeDepth=i.nativeDepth;if(material.shpFlat&&!i.shadow)draw.nativeDepth.gradient=nativesurface::Gradient::Flat;draw.palette=lighting::palette(state,i.objectPalette);draw.palette.rgb555|=material.rgb555;
   draw.nativeWalk=nativesurface::walk(draw.nativeDepth,int(std::floor(top/sy)),(i.height+i.rasterScale-1)/i.rasterScale,int(std::ceil(std::min(float(frame_.height),draw.clipBottom-.5f)/sy)),material.nativeZAdjust,material.bridgeSplit);
   if(i.nativeShape.size()==size_t(i.width)*i.height)draw.nativeShape={owner,&i.nativeShape};
   if(i.nativeShapeCoverage.size()==size_t(i.width)*i.height)draw.nativeCoverage={owner,&i.nativeShapeCoverage};
   draw.displace=material.displace&&i.nativeDepth.voxel;draw.cloakWords=material.cloakWords;draw.nativePitch=int(std::ceil(frame_.width/sx));draw.nativeScaleX=sx;draw.nativeScaleY=sy;
   Light transform(state,precision_);std::copy_n(transform.gains,3,draw.gains.begin());
   const bool reconstruct=smooth&&!i.shadow&&i.rasterScale==1&&(pxScale>1||pyScale>1);
   if(reconstruct){const int factor=xbrzadapter::factor(pxScale,pyScale);if(!factor)return false;
    draw.reconstructionSource=litSource(owner,transform);if(!draw.reconstructionSource)return false;draw.reconstructedWidth=i.width*factor;draw.reconstructedHeight=i.height*factor;
    draw.reconstructionFactor=factor;draw.sampling=vulkancompute::Sampling::Reconstructed;
   }
   else if(smooth&&(!i.shadow||i.rasterScale>1)&&(pxScale!=1||pyScale!=1))draw.sampling=i.rasterScale>1&&pxScale<=1&&pyScale<=1?vulkancompute::Sampling::Bilinear:vulkancompute::Sampling::Adaptive;
   if(draw.sampling==vulkancompute::Sampling::Nearest&&!draw.smoothRGB&&i.nativeBlend>0&&i.nativeBlend<=3&&i.blendSource&&i.blendSource->size()==size_t(i.width)*i.height){draw.source.pixels=i.blendSource;draw.nativeBlend=i.nativeBlend;}
   if(frame_.draws.size()>=65536)return false;frame_.draws.push_back(std::move(draw));
  }
  for(size_t index=0;index<i.attachments.size();++index){const auto&part=i.attachments[index];if((!part.shadow||i.shadowsAfter)&&(!part.airborneShadow||shadowDrop>0)){
   auto key=motion;if(motion.known&&frame_.collectGuides)key.part.push_back(uint32_t(index));if(!flatten(attachment(part),ax,ay+(part.shadow?shadowDrop*sy:0),sx,sy,base,state,lift,0,smooth,key,material.shifted(part.shadow?shadowDrop:0),nesting+1))return false;}}
  return true;
 }
 static bool validDepth(const vulkancompute::FrameJob&frame){
  if(!frame.baseDepth)return false;
  if(frame.baseDepthWidth==0)return frame.baseDepthHeight==0&&frame.baseDepth->size()==size_t(frame.width)*frame.height;
  if(frame.baseDepthWidth<1||frame.baseDepthHeight<1||frame.baseDepth->size()!=size_t(frame.baseDepthWidth)*frame.baseDepthHeight||!frame.baseDepthXs||!frame.baseDepthYs||frame.baseDepthXs->size()!=size_t(frame.width)||frame.baseDepthYs->size()!=size_t(frame.height))return false;
  for(uint32_t x:*frame.baseDepthXs)if(x>=uint32_t(frame.baseDepthWidth))return false;
  for(uint32_t y:*frame.baseDepthYs)if(y>=uint32_t(frame.baseDepthHeight))return false;
  return true;
 }
public:
 GpuSceneRecord()=default;GpuSceneRecord(const GpuSceneRecord&)=delete;GpuSceneRecord&operator=(const GpuSceneRecord&)=delete;
 GpuSceneRecord(GpuSceneRecord&&)=default;GpuSceneRecord&operator=(GpuSceneRecord&&)=default;
 void reset(){frame_={};fallbackBase_.reset();cpuBaseFallback_={};replay_.clear();valid_=false;}
 void clear(){reset();litSources_.clear();litIndex_.clear();litBytes_=serial_=litBuilds_=litHits_=0;}
 void beginObjects(int width,int height,colorprecision::Mode precision){reset();frame_.width=width;frame_.height=height;precision_=precision;valid_=width>0&&height>0&&width<=32768&&height<=32768;}
 void nativeView(float sx,float sy,bool enabled){frame_.nativeScene=enabled;frame_.nativeScaleX=sx;frame_.nativeScaleY=sy;}
 uint64_t litBuilds()const{return litBuilds_;}uint64_t litHits()const{return litHits_;}size_t litBytes()const{return litBytes_;}
 void begin(int width,int height,std::shared_ptr<const std::vector<uint32_t>>base,std::shared_ptr<const std::vector<float>>depth,colorprecision::Mode precision,
            std::shared_ptr<const std::vector<vulkancompute::TileJob>>baseTiles={},std::function<bool(std::vector<uint32_t>&)>cpuBaseFallback={},GpuDepthPlan depthPlan={}){
  reset();frame_.width=width;frame_.height=height;frame_.basePixels=std::move(base);fallbackBase_=frame_.basePixels;frame_.baseDepth=std::move(depth);precision_=precision;cpuBaseFallback_=std::move(cpuBaseFallback);
  frame_.baseDepthWidth=depthPlan.width;frame_.baseDepthHeight=depthPlan.height;frame_.baseDepthXs=std::move(depthPlan.xs);frame_.baseDepthYs=std::move(depthPlan.ys);
  if(baseTiles&&!baseTiles->empty()){frame_.baseTiles=*baseTiles;frame_.basePixels.reset();}
  valid_=((frame_.basePixels&&frame_.basePixels->size()==size_t(width)*height)||!frame_.baseTiles.empty())&&validDepth(frame_);
 }
 bool append(const voxel::Image&i,std::shared_ptr<const voxel::Image>owner,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift,float drop,bool smooth,vulkancompute::MotionIdentity motion={},Material material={}){
  owner=retain(i,std::move(owner));replay_.push_back({owner,ax,ay,sx,sy,base,lift,drop,light,smooth});replay_.back().material=material;
  if(valid_)valid_=flatten(owner,ax,ay,sx,sy,base,light,lift,drop,smooth,motion,material);return valid_;
 }
 void annotationDot(int x,int y,uint32_t color){
  color=0xff000000u|(color&0xffffffu);Replay replay;replay.dotX=x;replay.dotY=y;replay.dot=color;replay_.push_back(replay);
  if(!valid_)return;if(frame_.draws.size()>=65536){valid_=false;return;}vulkancompute::DrawJob draw;draw.source.width=draw.source.height=3;draw.source.pixels=std::make_shared<const std::vector<uint32_t>>(9,color);draw.left=float(x-1);draw.top=float(y-1);draw.skipZ=true;draw.writeDepth=false;frame_.draws.push_back(std::move(draw));
 }
 void clip(int left,int top,int right,int bottom){frame_.clipLeft=left;frame_.clipTop=top;frame_.clipRight=right;frame_.clipBottom=bottom;}
 void fog(std::shared_ptr<const std::vector<uint8_t>>alpha){frame_.fogAlpha=std::move(alpha);}
 void effects(std::shared_ptr<const gpueffects::Plan>plan){frame_.effects=std::move(plan);}
 void motion(vulkancompute::MotionView view,bool enabled){frame_.motion=std::move(view);frame_.collectGuides=enabled;}
 void maskPlan(std::shared_ptr<const gpushroud::Plan>plan){frame_.maskPlan=std::move(plan);}
 void baseTiles(std::vector<vulkancompute::TileJob>jobs){frame_.baseTiles=std::move(jobs);if(!frame_.baseTiles.empty())frame_.basePixels.reset();}
 bool valid()const{return valid_;}
 const vulkancompute::FrameJob&frame()const{return frame_;}
 bool replay(Surface&surface)const{
  // basePixels stays captured for fallback even when GPU base tiles are used.
  if(frame_.width<1||frame_.height<1||uint64_t(frame_.width)*frame_.height>16*1024*1024||!validDepth(frame_))return false;
  const std::vector<uint32_t>*base=fallbackBase_.get();std::vector<uint32_t>generated;
  if(!base||base->size()!=size_t(frame_.width)*frame_.height){if(!cpuBaseFallback_||!cpuBaseFallback_(generated)||generated.size()!=size_t(frame_.width)*frame_.height)return false;base=&generated;}
  if(!frame_.baseDepth||!surface.reset(frame_.width,frame_.height))return false;
  surface.precision(precision_);surface.color=*base;surface.nativeView(0,frame_.nativeScaleX,frame_.nativeScaleY,int(std::ceil(frame_.width/frame_.nativeScaleX)),int(std::ceil(frame_.height/frame_.nativeScaleY)));
  if(frame_.nativeScene){std::shared_ptr<const std::vector<uint8_t>>mask=frame_.fogAlpha;
   if(frame_.maskPlan){auto a=std::make_shared<std::vector<uint8_t>>();if(!gpushroud::reference(*frame_.maskPlan,*a))return false;mask=a;}
   auto a=mask?std::make_shared<std::vector<uint8_t>>(*mask):std::make_shared<std::vector<uint8_t>>(size_t(frame_.width)*frame_.height,127);
   if(frame_.effects)for(const auto&command:frame_.effects->commands)if(command.words[0]==gpueffects::Alpha&&command.indices){const auto&w=command.words;
    for(uint32_t y=w[5];y<w[7];++y)for(uint32_t x=w[4];x<w[6];++x){int xx=int((x-gpueffects::real(w[8]))/gpueffects::real(w[10])),yy=int((y-gpueffects::real(w[9]))/gpueffects::real(w[11]));if(xx<0||yy<0||xx>=int(w[14])||yy>=int(w[15]))continue;size_t at=size_t(y)*frame_.width+x;(*a)[at]=uint8_t(std::min(255u,unsigned((*a)[at])*(*command.indices)[size_t(yy)*w[14]+xx]/127u));}}
   surface.nativeBase(a);
  }
  if(frame_.baseDepthWidth>0){
   // The GPU consumes this compact native plane directly. Only an actual CPU
   // fallback expands it to the output framebuffer's depth layout.
   for(int y=0;y<frame_.height;++y){size_t row=size_t((*frame_.baseDepthYs)[y])*frame_.baseDepthWidth;
    for(int x=0;x<frame_.width;++x)surface.depth[size_t(y)*frame_.width+x]=(*frame_.baseDepth)[row+(*frame_.baseDepthXs)[x]];
   }
  }else surface.depth=*frame_.baseDepth;
  auto paint=[&](Surface&target,const Replay&call,int row){if(call.image)target.blit(*call.image,call.ax,call.ay-row,call.sx,call.sy,call.base,call.light,call.lift,call.drop,call.smooth,call.material);
   else for(int y=std::max(0,call.dotY-row-1);y<std::min(target.height(),call.dotY-row+2);++y)for(int x=std::max(0,call.dotX-1);x<std::min(frame_.width,call.dotX+2);++x)target.color[size_t(y)*frame_.width+x]=call.dot;};
  // Independent rows, identical painter order. Destination-reading cloak
  // calls fence the row team and retain their native global scanline order.
  // Failed GPU meshes are materialized once on the owner, never per row team.
  auto calls=replay_;for(auto&call:calls)if(call.image&&call.image->gpuRaster){voxel::Image image;if(!gpuvoxel::materialize(*call.image,image))return false;call.image=std::make_shared<const voxel::Image>(std::move(image));}
  std::atomic<bool>failed{false};auto segment=[&](size_t first,size_t last){if(first==last)return;const auto alpha=surface.alphaPlane();
   parallelrows::rows(size_t(frame_.height),size_t(frame_.width)*frame_.height,[&](size_t top,size_t bottom){try{if(top==0&&bottom==size_t(frame_.height)){for(size_t n=first;n<last;++n)paint(surface,calls[n],0);return;}
    static thread_local Surface band;const int rows=int(bottom-top);if(!band.reset(frame_.width,rows)){failed=true;return;}band.precision(precision_);band.outputRowOrigin(int(top));band.nativeView(0,frame_.nativeScaleX,frame_.nativeScaleY,int(std::ceil(frame_.width/frame_.nativeScaleX)),int(std::ceil(frame_.height/frame_.nativeScaleY)));
    const size_t offset=top*frame_.width,count=size_t(rows)*frame_.width;std::copy_n(surface.color.data()+offset,count,band.color.data());std::copy_n(surface.depth.data()+offset,count,band.depth.data());std::copy_n(surface.nativeZ.data()+offset,count,band.nativeZ.data());
    if(alpha&&alpha->size()==surface.color.size())band.alpha(std::make_shared<const std::vector<uint8_t>>(alpha->begin()+offset,alpha->begin()+offset+count));
    for(size_t n=first;n<last;++n)paint(band,calls[n],int(top));if(band.failed()){failed=true;return;}
    std::copy_n(band.color.data(),count,surface.color.data()+offset);std::copy_n(band.depth.data(),count,surface.depth.data()+offset);std::copy_n(band.nativeZ.data(),count,surface.nativeZ.data()+offset);
   }catch(const std::bad_alloc&){failed=true;}
   });};
  size_t first=0;for(size_t n=0;n<calls.size();++n)if(calls[n].material.displace){segment(first,n);if(failed)return false;paint(surface,calls[n],0);first=n+1;}segment(first,calls.size());return !failed&&!surface.failed();
 }
};
}}

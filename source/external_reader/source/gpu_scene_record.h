#pragma once
#include "camera_raster.h"
#include "vulkan_scene.h"
#include <functional>
namespace external { namespace camera {
// A frame keeps immutable image owners, including their depth/index arrays.
// The original calls are retained too: unsupported/failed GPU work replays the
// same Surface::blit operations in this frame, without a GPU image readback.
class GpuSceneRecord {
 struct Replay {
  std::shared_ptr<const voxel::Image> image;
  float ax=0,ay=0,sx=1,sy=1,base=0,lift=1,drop=0;lighting::State light;bool smooth=false;
  int dotX=0,dotY=0;uint32_t dot=0;
 };
 struct LitSource {
  std::weak_ptr<const voxel::Image> owner;const voxel::Image*address=nullptr;
  std::array<float,3> gains{{1,1,1}};colorprecision::Mode precision=colorprecision::Mode::NativeSteps;
  std::shared_ptr<const std::vector<uint32_t>> pixels;uint64_t used=0;
 };
 vulkancompute::FrameJob frame_;std::shared_ptr<const std::vector<uint32_t>> fallbackBase_;std::function<bool(std::vector<uint32_t>&)> cpuBaseFallback_;std::vector<Replay> replay_;
 std::vector<LitSource> litSources_;size_t litPixels_=0;
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
  for(auto&cached:litSources_)if(cached.address==owner.get()&&!cached.owner.owner_before(owner)&&!owner.owner_before(cached.owner)&&cached.precision==precision_&&cached.gains==gains){cached.used=++serial_;return cached.pixels;}
  auto pixels=std::make_shared<std::vector<uint32_t>>(i.pixels.size());const bool indexed=i.indices.size()==i.pixels.size();
  for(size_t n=0;n<i.pixels.size();++n)(*pixels)[n]=light.apply(i.pixels[n],indexed?i.indices[n]:0,i.objectPalette);
  for(auto it=litSources_.begin();it!=litSources_.end();)if(it->owner.expired()){litPixels_-=it->pixels->size();it=litSources_.erase(it);}else ++it;
  while(!litSources_.empty()&&(litPixels_+pixels->size()>xbrzadapter::CachePixels||litSources_.size()>=128)){
   auto oldest=std::min_element(litSources_.begin(),litSources_.end(),[](const LitSource&a,const LitSource&b){return a.used<b.used;});litPixels_-=oldest->pixels->size();litSources_.erase(oldest);
  }
  if(pixels->size()<=xbrzadapter::CachePixels){litPixels_+=pixels->size();litSources_.push_back({owner,owner.get(),gains,precision_,pixels,++serial_});}
  return pixels;
 }
 bool flatten(const std::shared_ptr<const voxel::Image>&owner,float ax,float ay,float sx,float sy,float base,const lighting::State&state,float lift,float shadowDrop,bool smooth,unsigned nesting=0){
  if(!owner||nesting>16)return false;const auto&i=*owner;
  if(i.width<=0||i.height<=0||i.width>resolutionsampling::MaxDimension||i.height>resolutionsampling::MaxDimension||uint64_t(i.width)*i.height>resolutionsampling::MaxPixels||i.rasterScale<1||i.rasterScale>resolutionsampling::MaxRasterScale||(!i.gpuRaster&&i.pixels.size()!=size_t(i.width)*i.height)||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>8||sy>8)return false;
  auto attachment=[&](const voxel::Image&part){return std::shared_ptr<const voxel::Image>(owner,&part);};
  for(const auto&part:i.attachments)if(part.shadow&&!i.shadowsAfter&&(!part.airborneShadow||shadowDrop>0))
   if(!flatten(attachment(part),ax,ay+shadowDrop*sy,sx,sy,base,state,lift,0,smooth,nesting+1))return false;
  const float pxScale=sx/i.rasterScale,pyScale=sy/i.rasterScale,left=ax+i.x*pxScale,top=ay+i.y*pyScale;
  int x0=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),0,frame_.width),y0=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),0,frame_.height);
  int x1=std::clamp(int(std::ceil(std::clamp(left+i.width*pxScale,-1e7f,1e7f))),0,frame_.width),y1=std::clamp(int(std::ceil(std::clamp(top+i.height*pyScale,-1e7f,1e7f))),0,frame_.height);
  if(x0<x1&&y0<y1){
   vulkancompute::DrawJob draw;draw.gpuRaster=i.gpuRaster;draw.source.width=i.width;draw.source.height=i.height;
   if(!i.gpuRaster){draw.source.pixels={owner,&i.pixels};if(i.depth.size()==i.pixels.size())draw.source.depth={owner,&i.depth};if(i.indices.size()==i.pixels.size())draw.source.indices={owner,&i.indices};}
   draw.left=left;draw.top=top;draw.scaleX=pxScale;draw.scaleY=pyScale;draw.depthBase=base+i.depthBias;
   draw.originY=i.y;draw.depthBottom=i.depthBottom==std::numeric_limits<int>::min()?i.y+i.height-1:i.depthBottom;draw.rasterScale=i.rasterScale;draw.lift=lift;
   draw.shadow=i.shadow;draw.writeDepth=i.writeDepth;draw.flat=i.flat;draw.objectPalette=i.objectPalette;draw.smoothRGB=precision_==colorprecision::Mode::SmoothRGB;draw.skipZ=i.skipZ;
   Light transform(state,precision_);std::copy_n(transform.gains,3,draw.gains.begin());
   const bool reconstruct=smooth&&!i.shadow&&i.rasterScale==1&&(pxScale>1||pyScale>1);
   if(reconstruct){const int factor=xbrzadapter::factor(pxScale,pyScale);if(!factor)return false;
    draw.reconstructionSource=litSource(owner,transform);if(!draw.reconstructionSource)return false;draw.reconstructedWidth=i.width*factor;draw.reconstructedHeight=i.height*factor;
    draw.reconstructionFactor=factor;draw.sampling=vulkancompute::Sampling::Reconstructed;
   }
   else if(smooth&&(!i.shadow||i.rasterScale>1)&&(pxScale!=1||pyScale!=1))draw.sampling=i.rasterScale>1&&pxScale<=1&&pyScale<=1?vulkancompute::Sampling::Bilinear:vulkancompute::Sampling::Adaptive;
   if(frame_.draws.size()>=65536)return false;frame_.draws.push_back(std::move(draw));
  }
  for(const auto&part:i.attachments)if((!part.shadow||i.shadowsAfter)&&(!part.airborneShadow||shadowDrop>0))
   if(!flatten(attachment(part),ax,ay+(part.shadow?shadowDrop*sy:0),sx,sy,base,state,lift,0,smooth,nesting+1))return false;
  return true;
 }
public:
 void reset(){frame_={};fallbackBase_.reset();cpuBaseFallback_={};replay_.clear();valid_=false;}
 void clear(){reset();litSources_.clear();litPixels_=serial_=0;}
 void begin(int width,int height,std::shared_ptr<const std::vector<uint32_t>>base,std::shared_ptr<const std::vector<float>>depth,colorprecision::Mode precision,
            std::shared_ptr<const std::vector<vulkancompute::TileJob>>baseTiles={},std::function<bool(std::vector<uint32_t>&)>cpuBaseFallback={}){
  reset();frame_.width=width;frame_.height=height;frame_.basePixels=std::move(base);fallbackBase_=frame_.basePixels;frame_.baseDepth=std::move(depth);precision_=precision;cpuBaseFallback_=std::move(cpuBaseFallback);
  if(baseTiles&&!baseTiles->empty()){frame_.baseTiles=*baseTiles;frame_.basePixels.reset();}
  valid_=((frame_.basePixels&&frame_.basePixels->size()==size_t(width)*height)||!frame_.baseTiles.empty())&&frame_.baseDepth&&frame_.baseDepth->size()==size_t(width)*height;
 }
 bool append(const voxel::Image&i,std::shared_ptr<const voxel::Image>owner,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift,float drop,bool smooth){
  owner=retain(i,std::move(owner));replay_.push_back({owner,ax,ay,sx,sy,base,lift,drop,light,smooth});
  if(valid_)valid_=flatten(owner,ax,ay,sx,sy,base,light,lift,drop,smooth);return valid_;
 }
 void annotationDot(int x,int y,uint32_t color){
  color=0xff000000u|(color&0xffffffu);Replay replay;replay.dotX=x;replay.dotY=y;replay.dot=color;replay_.push_back(replay);
  if(!valid_)return;if(frame_.draws.size()>=65536){valid_=false;return;}vulkancompute::DrawJob draw;draw.source.width=draw.source.height=3;draw.source.pixels=std::make_shared<const std::vector<uint32_t>>(9,color);draw.left=float(x-1);draw.top=float(y-1);draw.skipZ=true;draw.writeDepth=false;frame_.draws.push_back(std::move(draw));
 }
 void clip(int left,int top,int right,int bottom){frame_.clipLeft=left;frame_.clipTop=top;frame_.clipRight=right;frame_.clipBottom=bottom;}
 void fog(std::shared_ptr<const std::vector<uint8_t>>alpha){frame_.fogAlpha=std::move(alpha);}
 void maskPlan(std::shared_ptr<const gpushroud::Plan>plan){frame_.maskPlan=std::move(plan);}
 void baseTiles(std::vector<vulkancompute::TileJob>jobs){frame_.baseTiles=std::move(jobs);if(!frame_.baseTiles.empty())frame_.basePixels.reset();}
 bool valid()const{return valid_;}
 const vulkancompute::FrameJob&frame()const{return frame_;}
 bool replay(Surface&surface)const{
  // basePixels stays captured for fallback even when GPU base tiles are used.
  if(frame_.width<1||frame_.height<1||uint64_t(frame_.width)*frame_.height>16*1024*1024||!frame_.baseDepth||frame_.baseDepth->size()!=size_t(frame_.width)*frame_.height)return false;
  const std::vector<uint32_t>*base=fallbackBase_.get();std::vector<uint32_t>generated;
  if(!base||base->size()!=size_t(frame_.width)*frame_.height){if(!cpuBaseFallback_||!cpuBaseFallback_(generated)||generated.size()!=size_t(frame_.width)*frame_.height)return false;base=&generated;}
  if(!frame_.baseDepth||!surface.reset(frame_.width,frame_.height))return false;
  surface.precision(precision_);surface.color=*base;surface.depth=*frame_.baseDepth;
  for(const auto&call:replay_){if(call.image)surface.blit(*call.image,call.ax,call.ay,call.sx,call.sy,call.base,call.light,call.lift,call.drop,call.smooth);
   else for(int y=std::max(0,call.dotY-1);y<std::min(frame_.height,call.dotY+2);++y)for(int x=std::max(0,call.dotX-1);x<std::min(frame_.width,call.dotX+2);++x)surface.color[size_t(y)*frame_.width+x]=call.dot;
  }return !surface.failed();
 }
};
}}

#pragma once
#include "native_canvas.h"
#include "gpu_scene_record.h"
#include <type_traits>
namespace external { namespace camera {
// Final device-pixel object composition for the main camera. The default
// terrain path first reconstructs one continuous, currently lit native crop
// with xBRZ, then imports its color and nearest native depth here. VXL geometry
// and SHP actors compose in this cropped physical viewport without another
// full-scene upscale or an enlarged whole-map allocation.
class ViewportCanvas {
 Surface surface_;std::shared_ptr<const std::vector<float>> baseDepth_;GpuSceneRecord gpuRecord_;uint64_t key_=0;
 int width_=0,height_=0;bool gpuRecording_=false,gpuMasked_=false;
 double x_=0,y_=0,sx_=1,sy_=1;int backingW_=0,backingH_=0;bool dirty_=false,ready_=false;
 bool terrainCached_=false;unsigned samplingMode_=0;size_t terrainCommands_=0;uint64_t terrainPixels_=0;
 static constexpr uint32_t Background=0xff080c08u;
 bool smooth(const voxel::Image&i)const{return samplingMode_!=1||i.rasterScale>1;}
 bool footprint(double left,double top,double right,double bottom,int&x0,int&y0,int&x1,int&y1)const{
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return false;
  x0=std::clamp(int(std::floor(std::clamp((left-x_)*sx_,-1e7,1e7))),0,width_);
  y0=std::clamp(int(std::floor(std::clamp((top-y_)*sy_,-1e7,1e7))),0,height_);
  x1=std::clamp(int(std::ceil(std::clamp((right-x_)*sx_,-1e7,1e7))),0,width_);
  y1=std::clamp(int(std::ceil(std::clamp((bottom-y_)*sy_,-1e7,1e7))),0,height_);return true;
 }
 bool charge(const voxel::Image&i,double ax,double ay,double drop,unsigned nesting=0){
  if(nesting>16||i.width<1||i.height<1||i.width>8192||i.height>8192||i.rasterScale<1||i.rasterScale>4||uint64_t(i.width)*i.height>16*1024*1024||i.pixels.size()!=size_t(i.width)*i.height)return false;
  int x0,y0,x1,y1;if(!footprint(ax+double(i.x)/i.rasterScale,ay+double(i.y)/i.rasterScale,ax+(double(i.x)+i.width)/i.rasterScale,ay+(double(i.y)+i.height)/i.rasterScale,x0,y0,x1,y1))return false;
  if(x0<x1&&y0<y1){if(++terrainCommands_>65536)return false;terrainPixels_+=uint64_t(x1-x0)*(y1-y0);if(terrainPixels_>128*1024*1024)return false;}
  for(const auto&part:i.attachments)if(!charge(part,ax,ay+(part.shadow?drop:0),0,nesting+1))return false;return true;
 }
 bool imageVisible(const voxel::Image&i,double ax,double ay,double drop,unsigned nesting,double sweep=0)const{
  if(nesting>16||i.rasterScale<1)return false;
  if(i.width>0&&i.height>0&&ax+double(i.x)/i.rasterScale<x_+width_/sx_&&ax+(double(i.x)+i.width)/i.rasterScale>x_&&ay+double(i.y)/i.rasterScale<y_+height_/sy_&&ay+sweep+(double(i.y)+i.height)/i.rasterScale>y_)return true;
  for(const auto&part:i.attachments)if(imageVisible(part,ax,ay+(part.shadow?drop:0),0,nesting+1,sweep))return true;return false;
 }
 void clipBacking(std::vector<uint32_t>&pixels)const{
  int w=width_,h=height_;
  // Admission follows output pixel centers, as native source sampling does:
  // nativeX=x_+(pixel+.5)/sx_. An outward edge-based ceil can paint a center
  // beyond the legal backing or wrongly erase a legal fractional first one.
  int l=std::clamp(int(std::ceil(-x_*sx_-.5)),0,w),r=std::clamp(int(std::ceil((backingW_-x_)*sx_-.5)),0,w);
  int t=std::clamp(int(std::ceil(-y_*sy_-.5)),0,h),b=std::clamp(int(std::ceil((backingH_-y_)*sy_-.5)),0,h);
  if(l==0&&r==w&&t==0&&b==h)return;
  for(int y=0;y<h;++y){auto row=pixels.begin()+size_t(y)*w;if(y<t||y>=b)std::fill_n(row,w,0);else{std::fill(row,row+l,0);std::fill(row+r,row+w,0);}}
 }
public:
 void reset(){surface_={};baseDepth_.reset();gpuRecord_.clear();width_=height_=0;key_=0;ready_=dirty_=terrainCached_=gpuRecording_=gpuMasked_=false;samplingMode_=0;}
 bool visible(double left,double top,double right,double bottom)const{return ready_&&left<x_+width_/sx_&&right>x_&&top<y_+height_/sy_&&bottom>y_;}
 bool visible(const voxel::Image&i,double ax,double ay,double drop=0)const{return ready_&&imageVisible(i,ax,ay,drop,0);}
 bool visibleShifted(const voxel::Image&i,double ax,double ay,double minY,double maxY)const{return ready_&&imageVisible(i,ax,ay+minY,0,0,maxY-minY);}
 bool prepareTerrain(const NativeCanvas&background,const std::vector<uint32_t>*cached,int w,int h,
              double x,double y,double spanX,double spanY,uint64_t key,colorprecision::Mode precision,unsigned mode,bool&rebuild){
  ready_=false;dirty_=false;gpuRecording_=gpuMasked_=false;gpuRecord_.reset();rebuild=true;
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768||w/spanX>4||h/spanY>4)return false;
  bool reuse=!surface_.failed()&&baseDepth_&&terrainCached_&&cached&&cached->size()==size_t(w)*h&&key==key_&&width_==w&&height_==h&&x==x_&&y==y_&&sx_==w/spanX&&sy_==h/spanY&&samplingMode_==mode&&surface_.precision()==precision;
  if(reuse){surface_.color=*cached;surface_.depth=*baseDepth_;rebuild=false;}
  else{terrainCached_=false;if(!surface_.reset(w,h))return false;baseDepth_.reset();}
  width_=w;height_=h;key_=key;x_=x;y_=y;sx_=w/spanX;sy_=h/spanY;samplingMode_=mode;surface_.precision(precision);backingW_=background.width();backingH_=background.height();terrainCommands_=terrainPixels_=0;ready_=true;return true;
 }
 bool underlayRect(double left,double top,double right,double bottom,uint32_t rgb){
  if(!ready_)return false;if(!visible(left,top,right,bottom))return true;
  int x0,y0,x1,y1;if(!footprint(left,top,right,bottom,x0,y0,x1,y1))return false;
  if(++terrainCommands_>65536)return false;terrainPixels_+=uint64_t(x1-x0)*(y1-y0);if(terrainPixels_>128*1024*1024)return false;
  rgb=0xff000000u|(rgb&0xffffffu);
  for(int y=y0;y<y1;++y){double nativeY=y_+(y+.5)/sy_;if(nativeY<top||nativeY>=bottom)continue;
   for(int x=x0;x<x1;++x){double nativeX=x_+(x+.5)/sx_;if(nativeX<left||nativeX>=right)continue;size_t at=size_t(y)*width_+x;
    if(surface_.color[at]==Background&&surface_.depth[at]==-std::numeric_limits<float>::infinity())surface_.color[at]=rgb;
   }}return true;
 }
 bool terrainBlit(const voxel::Image&i,float ax,float ay,float base,const lighting::State&light,float lift=1){
  if(!ready_)return false;if(!visible(i,ax,ay))return true;if(!charge(i,ax,ay,0))return false;
  surface_.blit(i,float((ax-x_)*sx_),float((ay-y_)*sy_),float(sx_),float(sy_),base,light,lift,0,smooth(i));return !surface_.failed();
 }
 bool commitTerrain(std::vector<uint32_t>&pixels){if(!ready_||surface_.failed())return false;clipBacking(surface_.color);pixels=surface_.color;baseDepth_=std::make_shared<const std::vector<float>>(surface_.depth);terrainCached_=true;dirty_=false;return true;}
 bool prepare(const NativeCanvas&background,const std::vector<uint32_t>&pixels,int w,int h,
              double x,double y,double spanX,double spanY,uint64_t key,colorprecision::Mode precision=colorprecision::Mode::NativeSteps,unsigned mode=0,
              std::shared_ptr<const std::vector<uint32_t>>basePixels={},
              std::shared_ptr<const std::vector<vulkancompute::TileJob>>baseTiles={},std::function<bool(std::vector<uint32_t>&)>cpuBaseFallback={}){
  const bool wasTerrain=terrainCached_;ready_=false;dirty_=false;terrainCached_=false;samplingMode_=mode;gpuMasked_=false;gpuRecord_.reset();
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||(pixels.size()!=size_t(w)*h&&!(pixels.empty()&&baseTiles&&!baseTiles->empty()))||
     !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  auto&gpu=xbrzadapter::gpuState();
  bool requestGpu=gpu.requested;
  if(gpu.resolve&&!gpu.resolve->diagnostics().available&&!gpu.resolve->diagnostics().reason.empty())requestGpu=false;
  const std::vector<uint32_t>*base=&pixels;std::vector<uint32_t>generatedBase;
  if(!requestGpu&&base->size()!=size_t(w)*h){if(!cpuBaseFallback||!cpuBaseFallback(generatedBase)||generatedBase.size()!=size_t(w)*h)return false;base=&generatedBase;}
  const bool depthChanged=!baseDepth_||wasTerrain||key!=key_||width_!=w||height_!=h||x!=x_||y!=y_||sx_!=w/spanX||sy_!=h/spanY;
  if(depthChanged){
   int x0=int(std::floor(x)),y0=int(std::floor(y)),nw=int(std::ceil(x+spanX))-x0,nh=int(std::ceil(y+spanY))-y0;
   std::vector<float> source;if(!background.depthRegion(x0,y0,nw,nh,source))return false;
   auto depth=std::make_shared<std::vector<float>>(size_t(w)*h);
   for(int yy=0;yy<h;++yy){int iy=std::clamp(int(std::floor(y+(yy+.5)*spanY/h))-y0,0,nh-1);
    for(int xx=0;xx<w;++xx){int ix=std::clamp(int(std::floor(x+(xx+.5)*spanX/w))-x0,0,nw-1);(*depth)[size_t(yy)*w+xx]=source[size_t(iy)*nw+ix];}
   }baseDepth_=std::move(depth);
  }
  if(!requestGpu&&(surface_.failed()||surface_.width()!=w||surface_.height()!=h||gpuRecording_))if(!surface_.reset(w,h))return false;
  if(requestGpu&&!gpuRecording_)surface_={}; // release a previous CPU frame once, not once per recorded frame
  width_=w;height_=h;key_=key;x_=x;y_=y;sx_=w/spanX;sy_=h/spanY;
  surface_.precision(precision);backingW_=background.width();backingH_=background.height();gpuRecording_=requestGpu;
  if(gpuRecording_){
   if((!baseTiles||baseTiles->empty())&&(!basePixels||basePixels.get()!=&pixels))basePixels=std::make_shared<const std::vector<uint32_t>>(pixels);
   gpuRecord_.begin(w,h,std::move(basePixels),baseDepth_,precision,std::move(baseTiles),std::move(cpuBaseFallback));
  }else{surface_.color=*base;surface_.depth=*baseDepth_;}
  ready_=true;return true;
 }
 void blit(const voxel::Image&i,float ax,float ay,float,float,float base,const lighting::State&light,float lift=1,float shadowDrop=0,
           std::shared_ptr<const voxel::Image>owner={}){
  if(!ready_)return;const float outputX=float((ax-x_)*sx_),outputY=float((ay-y_)*sy_);
  if(gpuRecording_)gpuRecord_.append(i,std::move(owner),outputX,outputY,float(sx_),float(sy_),base,light,lift,shadowDrop,smooth(i));
  else surface_.blit(i,outputX,outputY,float(sx_),float(sy_),base,light,lift,shadowDrop,smooth(i));dirty_=true;
 }
 bool gpuRecording()const{return ready_&&gpuRecording_;}
 bool gpuMasked()const{return gpuMasked_;}
 void annotationDot(int x,int y,uint32_t color){if(ready_&&gpuRecording_)gpuRecord_.annotationDot(x,y,color);}
 void gpuMaskPlan(std::shared_ptr<const gpushroud::Plan>plan){if(ready_&&gpuRecording_)gpuRecord_.maskPlan(std::move(plan));}
 void gpuBaseTiles(std::vector<vulkancompute::TileJob>jobs){if(ready_&&gpuRecording_)gpuRecord_.baseTiles(std::move(jobs));}
 bool dirty()const{return dirty_;}
 template<class Backend>static auto renderGpu(Backend&backend,const vulkancompute::FrameJob&frame,std::vector<uint32_t>&pixels,std::vector<uint8_t>*mask,int)->decltype(backend.renderFrame(frame,pixels,mask),bool()){return backend.renderFrame(frame,pixels,mask);}
 template<class Backend>static auto renderGpu(Backend&backend,const vulkancompute::FrameJob&frame,std::vector<uint32_t>&pixels,std::vector<uint8_t>*,long)->decltype(backend.renderFrame(frame,pixels),bool()){return backend.renderFrame(frame,pixels);}
 template<class Backend>static bool renderGpu(Backend&,const vulkancompute::FrameJob&,std::vector<uint32_t>&,std::vector<uint8_t>*,...){return false;}
 bool finish(std::vector<uint32_t>&pixels,bool take=false,std::shared_ptr<const std::vector<uint8_t>>fog={},
             std::shared_ptr<const gpushroud::Plan>maskPlan={},std::vector<uint8_t>*maskOut=nullptr){
  if(!ready_)return false;
  if(gpuRecording_){
   const int l=std::clamp(int(std::ceil(-x_*sx_-.5)),0,width_),r=std::clamp(int(std::ceil((backingW_-x_)*sx_-.5)),0,width_);
   const int t=std::clamp(int(std::ceil(-y_*sy_-.5)),0,height_),b=std::clamp(int(std::ceil((backingH_-y_)*sy_-.5)),0,height_);
   gpuRecord_.clip(l,t,r,b);gpuRecord_.fog(std::move(fog));if(maskPlan)gpuRecord_.maskPlan(std::move(maskPlan));
   auto&gpu=xbrzadapter::gpuState();gpu.attempted=true;
   if(!gpu.resolve)gpu.resolve=std::make_unique<typename std::decay<decltype(gpu.resolve)>::type::element_type>();
   if(gpuRecord_.valid()&&renderGpu(*gpu.resolve,gpuRecord_.frame(),pixels,maskOut,0)){
    gpu.used=true;gpuMasked_=bool(gpuRecord_.frame().fogAlpha)||bool(gpuRecord_.frame().maskPlan);if(take)ready_=false;return true;
   }
   // Reproduce original calls, including deferred VXL materialization, without
   // publishing a partial GPU frame or waiting for a second render job.
   if(!gpuRecord_.replay(surface_))return false;
  }
  if(surface_.failed())return false;if(take){pixels=std::move(surface_.color);ready_=false;}else pixels=surface_.color;
  clipBacking(pixels);return true;
 }
};
}}

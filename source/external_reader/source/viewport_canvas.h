#pragma once
#include "native_canvas.h"
namespace external { namespace camera {
// Final device-pixel composition for the main camera. The native terrain
// cache remains available; refined terrain and objects compose directly in
// the cropped physical viewport, never in an enlarged whole-map allocation.
class ViewportCanvas {
 Surface surface_;std::vector<float> baseDepth_;uint64_t key_=0;
 double x_=0,y_=0,sx_=1,sy_=1;int backingW_=0,backingH_=0;bool dirty_=false,ready_=false;
 bool terrainCached_=false;unsigned samplingMode_=0;size_t terrainCommands_=0;uint64_t terrainPixels_=0;
 static constexpr uint32_t Background=0xff080c08u;
 bool smooth(const voxel::Image&i)const{return samplingMode_!=1||i.rasterScale>1;}
 bool footprint(double left,double top,double right,double bottom,int&x0,int&y0,int&x1,int&y1)const{
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return false;
  x0=std::clamp(int(std::floor(std::clamp((left-x_)*sx_,-1e7,1e7))),0,surface_.width());
  y0=std::clamp(int(std::floor(std::clamp((top-y_)*sy_,-1e7,1e7))),0,surface_.height());
  x1=std::clamp(int(std::ceil(std::clamp((right-x_)*sx_,-1e7,1e7))),0,surface_.width());
  y1=std::clamp(int(std::ceil(std::clamp((bottom-y_)*sy_,-1e7,1e7))),0,surface_.height());return true;
 }
 bool charge(const voxel::Image&i,double ax,double ay,double drop,unsigned nesting=0){
  if(nesting>16||i.width<1||i.height<1||i.width>8192||i.height>8192||i.rasterScale<1||i.rasterScale>4||uint64_t(i.width)*i.height>16*1024*1024||i.pixels.size()!=size_t(i.width)*i.height)return false;
  int x0,y0,x1,y1;if(!footprint(ax+double(i.x)/i.rasterScale,ay+double(i.y)/i.rasterScale,ax+(double(i.x)+i.width)/i.rasterScale,ay+(double(i.y)+i.height)/i.rasterScale,x0,y0,x1,y1))return false;
  if(x0<x1&&y0<y1){if(++terrainCommands_>65536)return false;terrainPixels_+=uint64_t(x1-x0)*(y1-y0);if(terrainPixels_>128*1024*1024)return false;}
  for(const auto&part:i.attachments)if(!charge(part,ax,ay+(part.shadow?drop:0),0,nesting+1))return false;return true;
 }
 bool imageVisible(const voxel::Image&i,double ax,double ay,double drop,unsigned nesting,double sweep=0)const{
  if(nesting>16||i.rasterScale<1)return false;
  if(i.width>0&&i.height>0&&ax+double(i.x)/i.rasterScale<x_+surface_.width()/sx_&&ax+(double(i.x)+i.width)/i.rasterScale>x_&&ay+double(i.y)/i.rasterScale<y_+surface_.height()/sy_&&ay+sweep+(double(i.y)+i.height)/i.rasterScale>y_)return true;
  for(const auto&part:i.attachments)if(imageVisible(part,ax,ay+(part.shadow?drop:0),0,nesting+1,sweep))return true;return false;
 }
 void clipBacking(std::vector<uint32_t>&pixels)const{
  int w=surface_.width(),h=surface_.height();
  int l=std::clamp(int(std::ceil(-x_*sx_)),0,w),r=std::clamp(int(std::ceil((backingW_-x_)*sx_)),0,w);
  int t=std::clamp(int(std::ceil(-y_*sy_)),0,h),b=std::clamp(int(std::ceil((backingH_-y_)*sy_)),0,h);
  if(l==0&&r==w&&t==0&&b==h)return;
  for(int y=0;y<h;++y){auto row=pixels.begin()+size_t(y)*w;if(y<t||y>=b)std::fill_n(row,w,0);else{std::fill(row,row+l,0);std::fill(row+r,row+w,0);}}
 }
public:
 void reset(){surface_={};baseDepth_.clear();key_=0;ready_=dirty_=terrainCached_=false;samplingMode_=0;}
 bool visible(double left,double top,double right,double bottom)const{return ready_&&left<x_+surface_.width()/sx_&&right>x_&&top<y_+surface_.height()/sy_&&bottom>y_;}
 bool visible(const voxel::Image&i,double ax,double ay,double drop=0)const{return ready_&&imageVisible(i,ax,ay,drop,0);}
 bool visibleShifted(const voxel::Image&i,double ax,double ay,double minY,double maxY)const{return ready_&&imageVisible(i,ax,ay+minY,0,0,maxY-minY);}
 bool prepareTerrain(const NativeCanvas&background,const std::vector<uint32_t>*cached,int w,int h,
              double x,double y,double spanX,double spanY,uint64_t key,colorprecision::Mode precision,unsigned mode,bool&rebuild){
  ready_=false;dirty_=false;rebuild=true;
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768||w/spanX>4||h/spanY>4)return false;
  bool reuse=terrainCached_&&cached&&cached->size()==size_t(w)*h&&key==key_&&surface_.width()==w&&surface_.height()==h&&x==x_&&y==y_&&sx_==w/spanX&&sy_==h/spanY&&samplingMode_==mode&&surface_.precision()==precision;
  if(reuse){surface_.color=*cached;surface_.depth=baseDepth_;rebuild=false;}
  else{terrainCached_=false;if(!surface_.reset(w,h))return false;baseDepth_.clear();}
  key_=key;x_=x;y_=y;sx_=w/spanX;sy_=h/spanY;samplingMode_=mode;surface_.precision(precision);backingW_=background.width();backingH_=background.height();terrainCommands_=terrainPixels_=0;ready_=true;return true;
 }
 bool underlayRect(double left,double top,double right,double bottom,uint32_t rgb){
  if(!ready_)return false;if(!visible(left,top,right,bottom))return true;
  int x0,y0,x1,y1;if(!footprint(left,top,right,bottom,x0,y0,x1,y1))return false;
  if(++terrainCommands_>65536)return false;terrainPixels_+=uint64_t(x1-x0)*(y1-y0);if(terrainPixels_>128*1024*1024)return false;
  rgb=0xff000000u|(rgb&0xffffffu);
  for(int y=y0;y<y1;++y){double nativeY=y_+(y+.5)/sy_;if(nativeY<top||nativeY>=bottom)continue;
   for(int x=x0;x<x1;++x){double nativeX=x_+(x+.5)/sx_;if(nativeX<left||nativeX>=right)continue;size_t at=size_t(y)*surface_.width()+x;
    if(surface_.color[at]==Background&&surface_.depth[at]==-std::numeric_limits<float>::infinity())surface_.color[at]=rgb;
   }}return true;
 }
 bool terrainBlit(const voxel::Image&i,float ax,float ay,float base,const lighting::State&light,float lift=1){
  if(!ready_)return false;if(!visible(i,ax,ay))return true;if(!charge(i,ax,ay,0))return false;
  surface_.blit(i,float((ax-x_)*sx_),float((ay-y_)*sy_),float(sx_),float(sy_),base,light,lift,0,smooth(i));return true;
 }
 bool commitTerrain(std::vector<uint32_t>&pixels){if(!ready_)return false;clipBacking(surface_.color);pixels=surface_.color;baseDepth_=surface_.depth;terrainCached_=true;dirty_=false;return true;}
 bool prepare(const NativeCanvas&background,const std::vector<uint32_t>&pixels,int w,int h,
              double x,double y,double spanX,double spanY,uint64_t key,colorprecision::Mode precision=colorprecision::Mode::NativeSteps,unsigned mode=0){
  bool wasTerrain=terrainCached_;ready_=false;dirty_=false;terrainCached_=false;samplingMode_=mode;
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||pixels.size()!=size_t(w)*h||
     !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  if(wasTerrain||key!=key_||surface_.width()!=w||surface_.height()!=h||x!=x_||y!=y_||sx_!=w/spanX||sy_!=h/spanY){
   int x0=int(std::floor(x)),y0=int(std::floor(y)),nw=int(std::ceil(x+spanX))-x0,nh=int(std::ceil(y+spanY))-y0;
   std::vector<float> source;if(!background.depthRegion(x0,y0,nw,nh,source)||!surface_.reset(w,h))return false;
   baseDepth_.resize(size_t(w)*h);
   for(int yy=0;yy<h;++yy){int iy=std::clamp(int(std::floor(y+(yy+.5)*spanY/h))-y0,0,nh-1);
    for(int xx=0;xx<w;++xx){int ix=std::clamp(int(std::floor(x+(xx+.5)*spanX/w))-x0,0,nw-1);baseDepth_[size_t(yy)*w+xx]=source[size_t(iy)*nw+ix];}
   }key_=key;x_=x;y_=y;sx_=w/spanX;sy_=h/spanY;
  }
  surface_.precision(precision);
  backingW_=background.width();backingH_=background.height();surface_.color=pixels;surface_.depth=baseDepth_;ready_=true;return true;
 }
 void blit(const voxel::Image&i,float ax,float ay,float,float,float base,const lighting::State&light,float lift=1,float shadowDrop=0){
  if(!ready_)return;surface_.blit(i,float((ax-x_)*sx_),float((ay-y_)*sy_),float(sx_),float(sy_),base,light,lift,shadowDrop,smooth(i));dirty_=true;
 }
 bool dirty()const{return dirty_;}
 bool finish(std::vector<uint32_t>&pixels,bool take=false){
  if(!ready_)return false;if(take){pixels=std::move(surface_.color);ready_=false;}else pixels=surface_.color;
  // Protruding sprites may never paint past the legal native backing into a
  // transparent crop margin (the original game must remain visible there).
  clipBacking(pixels);return true;
 }
};
}}

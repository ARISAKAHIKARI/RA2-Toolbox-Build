#pragma once
#include "native_canvas.h"
namespace external { namespace camera {
// Final device-pixel composition for the main camera. Terrain retains its
// native cache; VXL/SHP, attachments, shadows and their Z tests run directly in
// the cropped physical viewport, never in an enlarged whole-map allocation.
class ViewportCanvas {
 Surface surface_;std::vector<float> baseDepth_;uint64_t key_=0;
 double x_=0,y_=0,sx_=1,sy_=1;int backingW_=0,backingH_=0;bool dirty_=false,ready_=false;
public:
 void reset(){surface_={};baseDepth_.clear();key_=0;ready_=dirty_=false;}
 bool prepare(const NativeCanvas&background,const std::vector<uint32_t>&pixels,int w,int h,
              double x,double y,double spanX,double spanY,uint64_t key){
  ready_=false;dirty_=false;
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024||pixels.size()!=size_t(w)*h||
     !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  if(key!=key_||surface_.width()!=w||surface_.height()!=h||x!=x_||y!=y_||sx_!=w/spanX||sy_!=h/spanY){
   int x0=int(std::floor(x)),y0=int(std::floor(y)),nw=int(std::ceil(x+spanX))-x0,nh=int(std::ceil(y+spanY))-y0;
   std::vector<float> source;if(!background.depthRegion(x0,y0,nw,nh,source)||!surface_.reset(w,h))return false;
   baseDepth_.resize(size_t(w)*h);
   for(int yy=0;yy<h;++yy){int iy=std::clamp(int(std::floor(y+(yy+.5)*spanY/h))-y0,0,nh-1);
    for(int xx=0;xx<w;++xx){int ix=std::clamp(int(std::floor(x+(xx+.5)*spanX/w))-x0,0,nw-1);baseDepth_[size_t(yy)*w+xx]=source[size_t(iy)*nw+ix];}
   }key_=key;x_=x;y_=y;sx_=w/spanX;sy_=h/spanY;
  }
  backingW_=background.width();backingH_=background.height();surface_.color=pixels;surface_.depth=baseDepth_;ready_=true;return true;
 }
 void blit(const voxel::Image&i,float ax,float ay,float,float,float base,const lighting::State&light,float lift=1,float shadowDrop=0){
  if(!ready_)return;surface_.blit(i,float((ax-x_)*sx_),float((ay-y_)*sy_),float(sx_),float(sy_),base,light,lift,shadowDrop,true);dirty_=true;
 }
 bool dirty()const{return dirty_;}
 bool finish(std::vector<uint32_t>&pixels,bool take=false){
  if(!ready_)return false;if(take){pixels=std::move(surface_.color);ready_=false;}else pixels=surface_.color;
  // Protruding sprites may never paint past the legal native backing into a
  // transparent crop margin (the original game must remain visible there).
  int w=surface_.width(),h=surface_.height();
  int l=std::clamp(int(std::ceil(-x_*sx_)),0,w),r=std::clamp(int(std::ceil((backingW_-x_)*sx_)),0,w);
  int t=std::clamp(int(std::ceil(-y_*sy_)),0,h),b=std::clamp(int(std::ceil((backingH_-y_)*sy_)),0,h);
  if(l==0&&r==w&&t==0&&b==h)return true;
  for(int y=0;y<h;++y){auto row=pixels.begin()+size_t(y)*w;if(y<t||y>=b)std::fill_n(row,w,0);else{std::fill(row,row+l,0);std::fill(row+r,row+w,0);}}
  return true;
 }
};
}}

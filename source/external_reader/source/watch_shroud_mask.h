#pragma once
#include "watch_view.h"
#include "radar_shroud_assets.h"
#include <array>

namespace external { namespace watch { namespace shroudmask {
// The watch camera is in tactical world pixels, while the shroud shape is
// attached to the flat Cell. Cell Level affects the TMP, never this mask.
class Raster {
 std::array<std::array<voxel::Image,47>,2> images_{};
 std::array<std::array<bool,47>,2> ready_{};
 std::vector<uint8_t> cachedAlpha_;
 std::vector<radar::ShroudFrames> cachedShade_;
 int cachedWidth_=0,cachedHeight_=0,cachedCx_=0,cachedCy_=0,cachedRadius_=0,cachedSide_=0;
 float cachedCameraX_=0,cachedCameraY_=0;
 bool cachedFog_=false,cacheValid_=false;
 size_t builds_=0,hits_=0;
 static void mask(std::vector<uint32_t>&pixels,const std::vector<uint8_t>&alpha){
  for(size_t n=0;n<pixels.size();++n){unsigned value=alpha[n];if(value>=127)continue;
   uint32_t color=pixels[n],masked=0xff000000u;
   for(int shift:{0,8,16})masked|=uint32_t((((color>>shift)&255u)*value+63)/127)<<shift;
   pixels[n]=masked;
  }
 }
public:
 void reset(){images_={};ready_={};cachedAlpha_.clear();cachedShade_.clear();cacheValid_=false;builds_=hits_=0;}
 size_t builds()const{return builds_;}
 size_t hits()const{return hits_;}
 template<class Load> bool apply(const Frame&frame,float cameraX,float cameraY,
                                 int width,int height,std::vector<uint32_t>&pixels,Load load){
  if(width<1||height<1||width>2048||height>2048||!std::isfinite(cameraX)||!std::isfinite(cameraY)||
     frame.side<1||frame.side>49||frame.radius*2+1!=frame.side||
     frame.shade.size()!=size_t(frame.side)*frame.side||pixels.size()!=size_t(width)*height)return false;
  if(cacheValid_&&width==cachedWidth_&&height==cachedHeight_&&frame.cx==cachedCx_&&frame.cy==cachedCy_&&
     frame.radius==cachedRadius_&&frame.side==cachedSide_&&cameraX==cachedCameraX_&&cameraY==cachedCameraY_&&
     frame.fogEnabled==cachedFog_&&frame.shade==cachedShade_){++hits_;mask(pixels,cachedAlpha_);return true;}
  std::vector<uint8_t> alpha(pixels.size(),2);
  std::vector<size_t> order;order.reserve(frame.shade.size());
  for(size_t n=0;n<frame.shade.size();++n)if(frame.shade[n].shroud!=127)order.push_back(n);
  std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return a%frame.side+a/frame.side<b%frame.side+b/frame.side;});
  for(int kind=0;kind<(frame.fogEnabled?2:1);++kind)for(size_t n:order){
   auto raw=kind?frame.shade[n].fog:frame.shade[n].shroud;
   int index=raw==-2?15:raw==-1?0:int(raw);
   if(index<0||index>=47)return false;
   if(!ready_[kind][index]){
    voxel::Image image;
    if(!load(kind!=0,index,image)||!shroudassets::usable(index,image))return false;
    images_[kind][index]=std::move(image);ready_[kind][index]=true;
   }
   const auto&image=images_[kind][index];
   int x=frame.cx+int(n%frame.side)-frame.radius,y=frame.cy+int(n/frame.side)-frame.radius;
   double left=width/2.+(x-y)*30.-cameraX+image.x;
   double top=height/2.+(x+y)*15.+15.-cameraY+image.y;
   int x0=std::clamp(int(std::floor(left)),0,width),x1=std::clamp(int(std::ceil(left+image.width)),0,width);
   int y0=std::clamp(int(std::floor(top)),0,height),y1=std::clamp(int(std::ceil(top+image.height)),0,height);
   for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px){
    int sx=int(std::floor(px+.5-left)),sy=int(std::floor(py+.5-top));
    if(sx<0||sy<0||sx>=image.width||sy>=image.height)continue;
    uint8_t value=image.indices[size_t(sy)*image.width+sx];if(value>127)continue;
    auto&dst=alpha[size_t(py)*width+px];
    if(!kind)dst=value;
    else dst=dst==127?value:uint8_t(std::max(0,int(dst)+int(value)-127));
   }
  }
  cachedWidth_=width;cachedHeight_=height;cachedCx_=frame.cx;cachedCy_=frame.cy;
  cachedRadius_=frame.radius;cachedSide_=frame.side;cachedCameraX_=cameraX;cachedCameraY_=cameraY;
  cachedFog_=frame.fogEnabled;cachedShade_=frame.shade;cachedAlpha_=std::move(alpha);cacheValid_=true;++builds_;
  mask(pixels,cachedAlpha_);
  return true;
 }
};
}}}

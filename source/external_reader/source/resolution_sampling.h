#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace external { namespace resolutionsampling {
constexpr int MaxDimension=16384,MaxRasterScale=4;
constexpr uint64_t MaxPixels=32*1024*1024;
struct Selection {
 bool valid=false;
 double scaleX=1,scaleY=1;
 int rasterScale=1;
};
// Source dimensions describe world/view coordinates; no source framebuffer is
// allocated here. Only the actual destination must obey pixel-allocation caps.
inline Selection choose(double sourceWidth,double sourceHeight,double targetWidth,double targetHeight){
 Selection out;
 auto bounded=[](double width,double height){return std::isfinite(width)&&std::isfinite(height)&&width>=1&&height>=1&&width<=MaxDimension&&height<=MaxDimension&&width*height<=MaxPixels;};
 if(!std::isfinite(sourceWidth)||!std::isfinite(sourceHeight)||sourceWidth<1||sourceHeight<1||sourceWidth>32768||sourceHeight>32768||!bounded(targetWidth,targetHeight))return out;
 out.scaleX=targetWidth/sourceWidth;out.scaleY=targetHeight/sourceHeight;
 double required=std::max(out.scaleX,out.scaleY);
 // Above the bounded renderer's limit the caller must retain its fallback;
 // reporting 4x as full detail would hide the unsupported physical ratio.
 if(!std::isfinite(required)||required>MaxRasterScale)return out;
 out.rasterScale=std::clamp(int(std::ceil(required)),1,MaxRasterScale);out.valid=true;
 return out;
}
}}

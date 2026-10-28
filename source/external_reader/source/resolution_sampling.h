#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace external { namespace resolutionsampling {
constexpr int MaxDimension=8192,MaxRasterScale=4;
constexpr uint64_t MaxPixels=16*1024*1024;
struct Selection {
 bool valid=false;
 double scaleX=1,scaleY=1;
 int rasterScale=1;
};
// Dimensions are the observed game viewport and the actual presentation
// client, both in pixels. This chooses geometry samples, never a display mode.
inline Selection choose(double sourceWidth,double sourceHeight,double targetWidth,double targetHeight){
 Selection out;
 auto bounded=[](double width,double height){return std::isfinite(width)&&std::isfinite(height)&&width>=1&&height>=1&&width<=MaxDimension&&height<=MaxDimension&&width*height<=MaxPixels;};
 if(!bounded(sourceWidth,sourceHeight)||!bounded(targetWidth,targetHeight))return out;
 out.scaleX=targetWidth/sourceWidth;out.scaleY=targetHeight/sourceHeight;
 double required=std::max(out.scaleX,out.scaleY);
 // Above the bounded renderer's limit the caller must retain its fallback;
 // reporting 4x as full detail would hide the unsupported physical ratio.
 if(!std::isfinite(required)||required>MaxRasterScale)return out;
 out.rasterScale=std::clamp(int(std::ceil(required)),1,MaxRasterScale);out.valid=true;
 return out;
}
}}

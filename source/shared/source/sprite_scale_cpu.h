#pragma once
#include "../../third_party/xbrz/xbrz.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
// Pure CPU material reconstruction shared by ordinary SHP rendering and small
// screen sprites. Preserve the upstream algorithm, alpha conversion, supported
// factors and pixel-center resolve in one place; no scene or GPU dependencies.
namespace spritecpu {
constexpr size_t MaxPixels=16*1024*1024;
struct ScaledImage {int width=0,height=0,factor=0;std::vector<uint32_t> pixels;};
inline uint32_t straight(uint32_t p){
 const unsigned a=p>>24;if(!a)return 0;if(a==255)return p;
 uint32_t out=a<<24;for(int s:{0,8,16})out|=std::min(255u,(((p>>s)&255)*255+a/2)/a)<<s;return out;
}
inline uint32_t premultiplied(uint32_t p){
 const unsigned a=p>>24;uint32_t out=a<<24;
 for(int s:{0,8,16})out|=((((p>>s)&255)*a+127)/255)<<s;return out;
}
inline bool scale(const std::vector<uint32_t>&src,int w,int h,int factor,ScaledImage&out){
 if(w<1||h<1||w>8192||h>8192||factor<2||factor>4||
    uint64_t(w)*factor>8192||uint64_t(h)*factor>8192||
    uint64_t(w)*h*factor*factor>MaxPixels||src.size()!=size_t(w)*h)return false;
 std::vector<uint32_t> input(src.size());std::transform(src.begin(),src.end(),input.begin(),straight);
 out.width=w*factor;out.height=h*factor;out.factor=factor;out.pixels.resize(size_t(out.width)*out.height);
 // Avoid the upstream 64 MiB distance table in the 32-bit host.
 xbrz::scale(factor,input.data(),out.pixels.data(),w,h,xbrz::ColorFormat::argbUnbuffered);
 for(auto&p:out.pixels)p=premultiplied(p);return true;
}
// Bilinear resolve of the xBRZ image to the exact output ratio. Coordinates
// refer to source pixel boundaries: (.5,.5) is the first native pixel center.
inline uint32_t sample(const ScaledImage&i,double x,double y){
 if(i.width<1||i.height<1||i.factor<2||i.pixels.size()!=size_t(i.width)*i.height||!std::isfinite(x)||!std::isfinite(y))return 0;
 const double px=std::clamp(x*i.factor-.5,0.,double(i.width-1)),py=std::clamp(y*i.factor-.5,0.,double(i.height-1));
 int ix=int(px),iy=int(py),rx=std::min(ix+1,i.width-1),by=std::min(iy+1,i.height-1);double fx=px-ix,fy=py-iy;
 const uint32_t p[4]={i.pixels[size_t(iy)*i.width+ix],i.pixels[size_t(iy)*i.width+rx],i.pixels[size_t(by)*i.width+ix],i.pixels[size_t(by)*i.width+rx]};
 uint32_t out=0;for(int s:{0,8,16,24}){double v=(1-fy)*((1-fx)*((p[0]>>s)&255)+fx*((p[1]>>s)&255))+fy*((1-fx)*((p[2]>>s)&255)+fx*((p[3]>>s)&255));out|=uint32_t(v+.5)<<s;}return out;
}
inline int factor(double sx,double sy){
 if(!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>4||sy>4||std::max(sx,sy)<=1)return 0;
 return std::clamp(int(std::ceil(std::max(sx,sy))),2,4);
}
}

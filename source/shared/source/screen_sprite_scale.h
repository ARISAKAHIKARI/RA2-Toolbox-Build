#pragma once
#include "sprite_scale_cpu.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// Small immutable screen sprites only. Geometry is supplied by the consumer;
// the reconstruction factor never multiplies its final display size. Keeping
// this adapter independent of scene/GPU code also avoids compositor cycles.
namespace screensprites {
struct Raster {int width=0,height=0;std::shared_ptr<const std::vector<uint32_t>>pixels;};
class Cache {
 static constexpr size_t Budget=4*1024*1024,MaxPixels=1024*1024;
 using Key=std::tuple<const std::vector<uint32_t>*,int,int,int,int>;
 struct Entry {std::shared_ptr<const std::vector<uint32_t>>source;Raster raster;uint64_t used=0;size_t bytes=0;};
 std::map<Key,Entry>entries_;size_t bytes_=0;uint64_t serial_=0,builds_=0,hits_=0;
public:
 void clear(){entries_.clear();bytes_=0;}
 uint64_t builds()const{return builds_;}uint64_t hits()const{return hits_;}size_t bytes()const{return bytes_;}
 Raster get(const std::shared_ptr<const std::vector<uint32_t>>&source,int w,int h,int targetW,int targetH,bool reconstruct=true){
  if(!source||w<1||h<1||uint64_t(w)*h!=source->size())return {};
  Raster native{w,h,source};
  // Native/disabled and reduction keep the original source and sampling. An
  // excessive request falls back to the existing nearest path, never a blank.
  if(!reconstruct||w>1024||h>1024||targetW<1||targetH<1||(targetW<=w&&targetH<=h)||targetW>8192||targetH>8192||uint64_t(targetW)*targetH>MaxPixels)return native;
  const int factor=spritecpu::factor(double(targetW)/w,double(targetH)/h);if(!factor)return native;
  if(uint64_t(w)*h*factor*factor>MaxPixels)return native;
  const Key key{source.get(),w,h,targetW,targetH};auto found=entries_.find(key);
  if(found!=entries_.end()){found->second.used=++serial_;++hits_;return found->second.raster;}
  spritecpu::ScaledImage scaled;if(!spritecpu::scale(*source,w,h,factor,scaled))return native;
  auto output=std::make_shared<std::vector<uint32_t>>(size_t(targetW)*targetH);
  for(int y=0;y<targetH;++y)for(int x=0;x<targetW;++x)(*output)[size_t(y)*targetW+x]=spritecpu::sample(scaled,(x+.5)*w/targetW,(y+.5)*h/targetH);
  Raster result{targetW,targetH,output};const size_t cost=(source->size()+output->size())*4;
  if(cost<=Budget){while(!entries_.empty()&&(entries_.size()>=128||bytes_+cost>Budget)){auto old=std::min_element(entries_.begin(),entries_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});bytes_-=old->second.bytes;entries_.erase(old);}entries_.emplace(key,Entry{source,result,++serial_,cost});bytes_+=cost;}++builds_;return result;
 }
};
inline Cache&cache(){static thread_local Cache images;return images;}
}

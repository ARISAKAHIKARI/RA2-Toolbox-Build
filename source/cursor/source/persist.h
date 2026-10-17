#pragma once
#include "motion.h"
#include <filesystem>
#include <fstream>
#include <string>      // std::to_string: MSVC requires the direct header
#include <utility>     // std::move
#include <cstdio>      // EOF
#include <cstring>
#include <stdexcept>
namespace persist {
namespace fs=std::filesystem;
constexpr size_t maxData=18u*1024*1024;
inline uint64_t digest(const motion::Image& a,const motion::Image& b,int w,int h){
 uint64_t v=1469598103934665603ull;
 auto take=[&](uint32_t c){for(int i=0;i<4;i++){v^=(c>>(i*8))&255;v*=1099511628211ull;}};
 take((uint32_t)w);take((uint32_t)h);
 for(auto c:a)take(c);for(auto c:b)take(c);
 return v;
}
inline fs::path path(const fs::path& cache,int a,int b){
 return cache/"timelines"/(std::to_string(a)+"-"+std::to_string(b)+".tl15");
}
inline bool load(const fs::path& file,motion::Timeline& out,int expectedW,int expectedH,uint64_t hash){
 struct Header{char magic[8];uint32_t w,h,period,step,count;uint64_t digest;};
 Header hd{};std::ifstream f(file,std::ios::binary);
 if(!f||!f.read(reinterpret_cast<char*>(&hd),sizeof(hd))||std::memcmp(hd.magic,"RA2TL15",8))return false;
 if(hd.w!=(uint32_t)expectedW||hd.h!=(uint32_t)expectedH||hd.period<3||hd.period>500||hd.count<2||hd.count>501||hd.digest!=hash)return false;
 size_t pixels=size_t(hd.w)*hd.h,bytes=size_t(hd.count)*pixels*4;
 if(!pixels||bytes>maxData)return false;
 motion::Timeline candidate;candidate.w=hd.w;candidate.h=hd.h;candidate.referenceMs=hd.period;candidate.stepMs=hd.step;candidate.images.resize(hd.count,motion::Image(pixels));
 for(auto& im:candidate.images)if(!f.read(reinterpret_cast<char*>(im.data()),pixels*4))return false;
 // Reject trailing untrusted data and partially-written cache files.
 if(f.peek()!=EOF)return false;
 out=std::move(candidate);return true;
}
inline void save(const fs::path& file,const motion::Timeline& timeline,uint64_t hash){
 if(timeline.images.size()<2||timeline.images.size()>501||timeline.bytes()>maxData)return;
 try{
  fs::create_directories(file.parent_path());
  struct Header{char magic[8];uint32_t w,h,period,step,count;uint64_t digest;};
  Header hd{};std::memcpy(hd.magic,"RA2TL15",8);hd.w=timeline.w;hd.h=timeline.h;hd.period=timeline.referenceMs;hd.step=timeline.stepMs;hd.count=(uint32_t)timeline.images.size();hd.digest=hash;
  fs::path tmp=file;tmp+=".new";
  {std::ofstream f(tmp,std::ios::binary|std::ios::trunc);if(!f)return;f.write(reinterpret_cast<const char*>(&hd),sizeof(hd));for(const auto& image:timeline.images)f.write(reinterpret_cast<const char*>(image.data()),image.size()*4);if(!f){f.close();fs::remove(tmp);return;}}
  // A reader either sees an old complete cache or no cache; never a partial stream.
  std::error_code ec;fs::remove(file,ec);fs::rename(tmp,file,ec);if(ec)fs::remove(tmp,ec);
 }catch(const std::exception&){}
}
}

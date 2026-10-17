#pragma once
#include "persist.h"
#include <fstream>
#include <filesystem>
namespace precomputed {
inline bool word(std::istream& f,uint32_t& v){unsigned char b[4];if(!f.read((char*)b,4))return false;v=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);return true;}
inline bool var(std::istream& f,uint32_t& v){v=0;for(int i=0;i<5;i++){int x=f.get();if(x==EOF)return false;if(i==4&&(x&0xf0))return false;v|=uint32_t(x&127)<<(7*i);if(!(x&128))return true;}return false;}
inline bool load(const std::filesystem::path& path,motion::Timeline& out,const motion::Image& a,const motion::Image& b,int w,int h,unsigned retained=1001){
 std::ifstream f(path,std::ios::binary);char magic[8];uint32_t fw,fh,n,lo,hi;
 if(!f||!f.read(magic,8)||std::memcmp(magic,"RA2DIS1",8)||!word(f,fw)||!word(f,fh)||!word(f,n)||!word(f,lo)||!word(f,hi))return false;
 if(fw!=unsigned(w)||fh!=unsigned(h)||n!=1001||w<1||h<1||w>128||h>128||a.size()!=size_t(w*h)||b.size()!=a.size())return false;
 if((uint64_t(hi)<<32|lo)!=persist::digest(a,b,w,h))return false;
 retained=std::clamp(retained,2u,n);
 motion::Timeline t;t.w=w;t.h=h;t.referenceMs=1000;t.stepMs=1;t.images.reserve(retained);
 unsigned nextSlot=1;
 auto keep=[&](unsigned i){return nextSlot<retained && i==(nextSlot*(n-1)+(retained-1)/2)/(retained-1);};
 motion::Image previous(size_t(w)*h);for(auto& v:previous)if(!word(f,v))return false;t.images.push_back(previous);
 for(uint32_t i=1;i<n;i++){
  uint32_t pos=0;
  while(pos<previous.size()){
   uint32_t skip,count;if(!var(f,skip)||skip>previous.size()-pos)return false;pos+=skip;
   if(!var(f,count)||count>previous.size()-pos||(!count&&pos<previous.size()))return false;
   for(uint32_t k=0;k<count;k++){uint32_t x;if(!word(f,x))return false;previous[pos++]^=x;}
  }
  if(keep(i)){t.images.push_back(previous);++nextSlot;}
 }
 if(f.peek()!=EOF||t.images.front()!=a||t.images.back()!=b)return false;
 if(t.images.size()!=retained)return false;
 out=std::move(t);return true;
}
}

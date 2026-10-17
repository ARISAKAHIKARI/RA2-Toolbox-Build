#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>
using Bytes=std::vector<uint8_t>;
inline uint16_t u16(const Bytes& b,size_t p){if(p+2>b.size())throw std::runtime_error("short data");return b[p]|(b[p+1]<<8);}
inline uint32_t u32(const Bytes& b,size_t p){if(p+4>b.size())throw std::runtime_error("short data");return b[p]|(b[p+1]<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
inline void w16(Bytes& b,size_t p,uint16_t v){b.at(p)=v;b.at(p+1)=v>>8;}
inline void w32(Bytes& b,size_t p,uint32_t v){for(int i=0;i<4;i++)b.at(p+i)=v>>(8*i);}
struct Frame {uint16_t x,y,w,h;Bytes pixels;};
struct Asset {uint16_t w=0,h=0;std::vector<Frame> frames;};
inline void validHeader(const Bytes& h){if(h.size()<8||u16(h,0)!=0||!u16(h,2)||!u16(h,4)||u16(h,2)>512||u16(h,4)>512||!u16(h,6)||u16(h,6)>4096)throw std::runtime_error("unsupported SHP header");}
inline Bytes encode(const Asset& a,bool blank){
 if(a.frames.empty()||a.frames.size()>4096)throw std::runtime_error("frame count");
 Bytes b(8+a.frames.size()*24);w16(b,2,a.w);w16(b,4,a.h);w16(b,6,(uint16_t)a.frames.size());
 for(size_t i=0;i<a.frames.size();i++){const auto& f=a.frames[i];size_t p=8+i*24;if(f.pixels.size()!=size_t(f.w)*f.h)throw std::runtime_error("pixel count");
 w16(b,p,f.x);w16(b,p+2,f.y);w16(b,p+4,f.w);w16(b,p+6,f.h);w32(b,p+8,1);
 if(f.pixels.empty())continue;w32(b,p+20,(uint32_t)b.size());if(blank)b.resize(b.size()+f.pixels.size(),0);else b.insert(b.end(),f.pixels.begin(),f.pixels.end());}
 return b;
}
inline Asset decode(const Bytes& b){validHeader(b);Asset a;a.w=u16(b,2);a.h=u16(b,4);int n=u16(b,6);if(b.size()<8+size_t(n)*24)throw std::runtime_error("truncated frames");size_t total=0;
 for(int i=0;i<n;i++){size_t p=8+i*24;Frame f{u16(b,p),u16(b,p+2),u16(b,p+4),u16(b,p+6),{}};
 if(f.w>512||f.h>512||f.x+f.w>a.w||f.y+f.h>a.h)throw std::runtime_error("frame bounds");total+=size_t(f.w)*f.h;if(total>32*1024*1024)throw std::runtime_error("SHP too large");
 uint32_t fmt=u32(b,p+8)&255,off=u32(b,p+20);f.pixels.resize(size_t(f.w)*f.h);if(off&&f.w&&f.h){size_t q=off;
 if(fmt<=1){if(q+f.pixels.size()>b.size())throw std::runtime_error("raw frame truncated");memcpy(f.pixels.data(),b.data()+q,f.pixels.size());}
 else if(fmt==2){for(int y=0;y<f.h;y++){unsigned len=u16(b,q);if(len<2+f.w||q+len>b.size())throw std::runtime_error("raw row length");memcpy(f.pixels.data()+y*f.w,b.data()+q+2,f.w);q+=len;}}
 else if(fmt==3){for(int y=0;y<f.h;y++){unsigned len=u16(b,q);if(len<2||q+len>b.size())throw std::runtime_error("RLE length");size_t end=q+len;q+=2;unsigned x=0;while(q<end){unsigned v=b[q++];if(v){if(x>=f.w)throw std::runtime_error("RLE overflow");f.pixels[y*f.w+x++]=(uint8_t)v;}else{if(q>=end)throw std::runtime_error("RLE escape");unsigned skip=b[q++];if(!skip||x+skip>f.w)throw std::runtime_error("RLE run");x+=skip;}}if(x!=f.w)throw std::runtime_error("RLE width");}}
 else throw std::runtime_error("format 2 is not supported by this preview");}
 a.frames.push_back(f);}
 return a;
}

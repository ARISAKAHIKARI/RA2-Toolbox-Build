#pragma once
#include "motion.h"
#include <memory>
#include <map>
#include <tuple>
namespace playback {
// Keep canvas geometry unchanged. Only omit exactly-zero pixels outside the
// tight bounds, so transparent anchors and partial alpha retain their meaning.
struct Frame {
 int canvasW=0,canvasH=0,x=0,y=0,w=0,h=0;
 motion::Image pixels;
 static Frame pack(const motion::Image& source,int cw,int ch){
  Frame f;f.canvasW=cw;f.canvasH=ch;
  int x0=cw,y0=ch,x1=0,y1=0;
  for(int y=0;y<ch;y++)for(int x=0;x<cw;x++)if(source[size_t(y)*cw+x]){
   x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x+1);y1=std::max(y1,y+1);
  }
  if(x1<=x0||y1<=y0)return f;
  f.x=x0;f.y=y0;f.w=x1-x0;f.h=y1-y0;f.pixels.resize(size_t(f.w)*f.h);
  for(int y=0;y<f.h;y++)std::copy_n(source.begin()+size_t(y+f.y)*cw+f.x,f.w,f.pixels.begin()+size_t(y)*f.w);
  return f;
 }
 uint32_t at(int px,int py)const{
  return px>=x&&px<x+w&&py>=y&&py<y+h?pixels[size_t(py-y)*w+px-x]:0;
 }
 bool operator==(const Frame& other)const{
  return canvasW==other.canvasW&&canvasH==other.canvasH&&x==other.x&&y==other.y&&w==other.w&&h==other.h&&pixels==other.pixels;
 }
 size_t bytes()const{return sizeof(Frame)+pixels.size()*4;}
 // Equivalent to the old nearest-neighbour full-canvas scaling. No allocation.
 void scale(uint32_t* dest,int dw,int dh)const{
  std::fill_n(dest,size_t(dw)*dh,0u);if(!w||!h)return;
  int x0=(x*dw+canvasW-1)/canvasW,x1=((x+w)*dw+canvasW-1)/canvasW;
  int y0=(y*dh+canvasH-1)/canvasH,y1=((y+h)*dh+canvasH-1)/canvasH;
  for(int dy=y0;dy<y1;dy++)for(int dx=x0;dx<x1;dx++)
   dest[size_t(dy)*dw+dx]=pixels[size_t(dy*canvasH/dh-y)*w+dx*canvasW/dw-x];
 }
};
struct Timeline {
 std::vector<std::shared_ptr<const Frame>> images;
 size_t resident=0;
 static Timeline pack(const motion::Timeline& source){
  Timeline out;out.images.reserve(source.images.size());
  for(const auto& image:source.images){
   Frame frame=Frame::pack(image,source.w,source.h);
   if(!out.images.empty()&&*out.images.back()==frame)out.images.push_back(out.images.back());
   else {out.resident+=frame.bytes();out.images.push_back(std::make_shared<Frame>(std::move(frame)));}
  }
  return out;
 }
 size_t bytes()const{return sizeof(Timeline)+resident+images.size()*sizeof(images[0]);}
};
// Cache scaled output as well as the original frames. Replaying a loop needs
// only a pixel copy; the expensive per-pixel division is done on first use.
class RasterCache {
 using Key=std::tuple<uintptr_t,int,int>;
 struct Entry {motion::Image pixels;uint64_t used;};
 std::map<Key,Entry> entries;
 size_t resident=0;uint64_t clock=0;
public:
 uint64_t hits=0,misses=0;
 static constexpr size_t Budget=64u*1024*1024;
 static constexpr size_t MaxSlots=4096;
 void clear(){entries.clear();resident=0;clock=hits=misses=0;}
 size_t bytes()const{return resident;}
 const motion::Image& get(const Frame& frame,int w,int h){
  Key key{reinterpret_cast<uintptr_t>(&frame),w,h};++clock;
  auto found=entries.find(key);
  if(found!=entries.end()){++hits;found->second.used=clock;return found->second.pixels;}
  ++misses;size_t bytes=size_t(w)*h*4;
  while(!entries.empty()&&(resident+bytes>Budget||entries.size()>=MaxSlots)){
   auto oldest=entries.begin();for(auto it=entries.begin();it!=entries.end();++it)if(it->second.used<oldest->second.used)oldest=it;
   resident-=oldest->second.pixels.size()*4;entries.erase(oldest);
  }
  Entry entry{motion::Image(size_t(w)*h),clock};frame.scale(entry.pixels.data(),w,h);
  auto stored=entries.emplace(key,std::move(entry)).first;resident+=bytes;
  return stored->second.pixels;
 }
};
}

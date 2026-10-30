#pragma once
#include "native_feedback.h"
namespace external { namespace nativefeedback {
// Bounds belong to the sprite that was actually presented. Selection and
// health are read afresh; no second decoder or guessed mouse gestures.
struct Object {uint32_t pointer=0,id=0,type=0,owner=0;Rect bounds;int kind=-1;};
struct Selected {Rect bounds;int health=0,strength=0;};
template<class Read>bool selected(Read read,const std::vector<Object>&objects,std::vector<Selected>&out){
 out.clear();std::array<uint32_t,6> head{},after{};
 if(!read(0xA8ECB8,head.data(),sizeof head)||head[2]>8192||head[4]>head[2]||head[4]>8192||
    (head[4]&&!range(head[1],size_t(head[4])*4)))return false;
 std::vector<uint32_t> pointers(head[4]);
 if(!pointers.empty()&&!read(head[1],pointers.data(),pointers.size()*4))return false;
 auto sorted=pointers;std::sort(sorted.begin(),sorted.end());
 if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())return false;
 std::vector<Selected> pending;
 for(const auto&o:objects){
  if(!std::binary_search(sorted.begin(),sorted.end(),o.pointer))continue;
  if(!range(o.pointer,0x220)||!range(o.type,0xA4)||!o.id||!o.bounds.valid())return false;
  uint32_t id=0,owner=0,id2=0,owner2=0;int health=0,strength=0;uint8_t state[4]{},state2[4]{},alive=0;
  if(!read(o.pointer+0x10,&id,4)||id!=o.id||!read(o.pointer+0x21C,&owner,4)||owner!=o.owner||
     !read(o.pointer+0x80,state,sizeof state)||state[1]||state[3]!=1||
     !read(o.pointer+0x90,&alive,1)||alive!=1||!read(o.pointer+0x6C,&health,4)||health<=0||
     !read(o.type+0xA0,&strength,4)||strength<=0||strength>100000000||
     !read(o.pointer+0x80,state2,sizeof state2)||std::memcmp(state,state2,sizeof state)||
     !read(o.pointer+0x10,&id2,4)||id2!=id||!read(o.pointer+0x21C,&owner2,4)||owner2!=owner)return false;
  pending.push_back({o.bounds,health,strength});
 }
 if(!read(0xA8ECB8,after.data(),sizeof after)||head!=after)return false;
 if(!pointers.empty()){std::vector<uint32_t> verify(pointers.size());if(!read(head[1],verify.data(),verify.size()*4)||verify!=pointers)return false;}
 out=std::move(pending);return true;
}
struct Primitive {Rect rect;uint32_t color=0;bool operator==(const Primitive&o)const{return rect.x==o.rect.x&&rect.y==o.rect.y&&rect.w==o.rect.w&&rect.h==o.rect.h&&color==o.color;}};
inline std::vector<Primitive> selectionPrimitives(const std::vector<Selected>&items,const Band&band,Rect viewport,double sx,double sy,double yellow=.5,double red=.25){
 std::vector<Primitive> out;
 if(!viewport.valid()||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>16||sy>16)return out;
 if(!std::isfinite(yellow)||!std::isfinite(red)||red<0||yellow>1||red>yellow){yellow=.5;red=.25;}
 const int thick=std::clamp(int(std::lround(std::min(sx,sy))),1,8);
 auto emit=[&](Rect r,uint32_t c){r=intersect(r,viewport);if(r.valid())out.push_back({r,c});};
 for(const auto&i:items){Rect r=i.bounds;
  if(!r.valid()||r.w>16384||r.h>16384||std::abs(int64_t(r.x))>1000000||std::abs(int64_t(r.y))>1000000||i.strength<=0||i.health<=0)continue;
  int k=std::clamp(std::min(r.w,r.h)/4,3*thick,10*thick);
  for(int x:{r.x,r.x+r.w-thick})for(int y:{r.y,r.y+r.h-thick}){
   emit({x==r.x?x:x-k+thick,y,k,thick},0xff60ff60);
   emit({x,y==r.y?y:y-k+thick,thick,k},0xff60ff60);
  }
  int w=std::clamp(r.w,12*thick,80*thick),h=3*thick,x=r.x+(r.w-w)/2,y=r.y-5*thick;
  emit({x-thick,y-thick,w+2*thick,h+2*thick},0xff080808);
  double fraction=std::clamp(double(i.health)/i.strength,0.,1.);
  uint32_t color=fraction<=red?0xffff4040:fraction<=yellow?0xffffd840:0xff40e850;
  int filled=std::clamp(int(std::ceil(w*fraction)),0,w);emit({x,y,filled,h},color);
 }
 for(auto r:bandStrips(band,viewport,sx,sy))emit(r,0xffffffff);
 return out;
}
inline Rect envelope(const std::vector<Primitive>&p){
 if(p.empty())return {};int l=p[0].rect.x,t=p[0].rect.y,r=l+p[0].rect.w,b=t+p[0].rect.h;
 for(const auto&q:p){l=std::min(l,q.rect.x);t=std::min(t,q.rect.y);r=std::max(r,q.rect.x+q.rect.w);b=std::max(b,q.rect.y+q.rect.h);}return {l,t,r-l,b-t};
}
inline bool paint(const std::vector<Primitive>&p,Canvas&out,Rect&bounds){
 bounds=envelope(p);if(!bounds.valid()||!out.reset(bounds.w,bounds.h))return false;
 for(const auto&q:p)out.fill({q.rect.x-bounds.x,q.rect.y-bounds.y,q.rect.w,q.rect.h},q.color);return true;
}
// Attenuate premultiplied RGB AND alpha; unknown cells remain hidden.
inline bool applyVisibility(Canvas&c,const std::vector<uint8_t>*mask){
 if(!mask)return true;if(mask->size()!=c.pixels.size())return false;
 for(size_t i=0;i<c.pixels.size();++i){unsigned a=(*mask)[i];if(a>127)return false;if(a<=2){c.pixels[i]=0;continue;}
  uint32_t src=c.pixels[i],out=0;for(int s:{0,8,16,24})out|=(((((src>>s)&255)*a)+63)/127)<<s;c.pixels[i]=out;
 }return true;
}
// Crop and hotspot reference the full native SHA canvas, not its centre.
inline Rect cursorRect(int px,int py,int cropX,int cropY,int width,int height,int hotX,int hotY,double sx,double sy){
 if(width<1||height<1||width>512||height>512||std::abs(int64_t(px))>1000000||std::abs(int64_t(py))>1000000||
    std::abs(int64_t(cropX))>4096||std::abs(int64_t(cropY))>4096||hotX<0||hotY<0||hotX>512||hotY>512||
    !std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>16||sy>16)return {};
 int l=px+int(std::floor((cropX-hotX)*sx)),t=py+int(std::floor((cropY-hotY)*sy));
 int r=px+int(std::ceil((cropX-hotX+width)*sx)),b=py+int(std::ceil((cropY-hotY+height)*sy));return {l,t,r-l,b-t};
}
inline bool cursorVisible(bool scene,bool externalCursor,bool systemCursor,const Mouse&m){return scene&&!externalCursor&&!systemCursor&&m.refCount>=0;}
}}

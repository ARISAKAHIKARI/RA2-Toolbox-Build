#pragma once
#include "native_surface.h"
#include <list>
#include <unordered_map>
namespace external { namespace nativesurface {
// YRpp d979554: select one intensity remapper before walking a sprite, then
// index it by A. Keep the established Palette::apply integer rules exactly.
// Immutable math is shared within one renderer thread; current native palette
// bytes and opaque/translucent selection are bound separately on every blit.
struct PaletteMathKey {
 unsigned rows=0,level=0;std::array<uint32_t,3>scale{};bool special=false,scalar=false;
 bool operator==(const PaletteMathKey&o)const{return rows==o.rows&&level==o.level&&scale==o.scale&&special==o.special&&scalar==o.scalar;}
};
struct PaletteMathHash {size_t operator()(const PaletteMathKey&k)const{
 size_t h=k.rows*257u+k.level;for(auto s:k.scale)h=(h^s)*16777619u;return (h*4+k.special*2+k.scalar);
}};
struct PaletteMath {
 std::array<uint16_t,256>offset{};
 std::array<std::array<uint32_t,3>,256>gain{};
 std::array<uint32_t,256>emissive{};
 explicit PaletteMath(const PaletteMathKey&k){
  const unsigned n=k.rows,d=std::min(n*30/200?n*30/200-1:0,(n-1)/2);
  for(unsigned a=0;a<256;++a){unsigned row=std::min(a*k.level*(n-1)/32258u,n-1);offset[a]=uint16_t(row*256);
   for(unsigned c=0;c<3;++c){uint32_t g=k.scale[c];if(n>1)g=g*(2*row)/(n-1);gain[a][c]=k.scalar?g:g>>4;}
   uint32_t g=65536;if(n>1&&row<=d)g=d?row*65536/d:0;emissive[a]=k.scalar?g:g>>4;
  }
 }
};
inline std::shared_ptr<const PaletteMath> paletteMath(const Palette&p){
 struct Cache {
  using Recent=std::list<PaletteMathKey>;
  struct Entry {std::shared_ptr<const PaletteMath>value;Recent::iterator at;};
  Recent recent;std::unordered_map<PaletteMathKey,Entry,PaletteMathHash>entries;
 };
 static thread_local Cache cache;
 PaletteMathKey key{std::clamp(p.rows,1u,256u),std::min((unsigned(std::clamp(p.brightness,0,2000))*261u)>>11,254u),p.scale,p.special,p.scalar};
 auto found=cache.entries.find(key);if(found!=cache.entries.end()){cache.recent.splice(cache.recent.end(),cache.recent,found->second.at);return found->second.value;}
 auto value=std::make_shared<const PaletteMath>(key);
 // About 18 MiB per renderer at the ceiling. Active calls own their math even
 // if recursive drawing evicts the entry; palette pixels are never cached here.
 if(cache.entries.size()>=4096){cache.entries.erase(cache.recent.front());cache.recent.pop_front();}
 cache.recent.push_back(key);cache.entries.emplace(key,Cache::Entry{value,std::prev(cache.recent.end())});return value;
}
inline const uint32_t*packedRGB(bool rgb555){
 if(rgb555){static const auto table=[](){std::array<uint32_t,65536>a{};for(unsigned n=0;n<a.size();++n)a[n]=nativepacked::color(n,0,true);return a;}();return table.data();}
 static const auto table=[](){std::array<uint32_t,65536>a{};for(unsigned n=0;n<a.size();++n)a[n]=nativepacked::color(n,0,false);return a;}();return table.data();
}
class PaletteLookup {
 std::shared_ptr<const PaletteMath>math_;std::shared_ptr<const std::vector<uint32_t>>tableOwner_;
 const uint32_t*table_=nullptr,*rgb_=nullptr;uint16_t colour_=0;bool special_=false,scalar_=false,rgb555_=false;
public:
 void prepare(const Palette&p,bool opaque){math_=paletteMath(p);tableOwner_=p.table&&p.table->size()==size_t(std::clamp(p.rows,1u,256u))*256?p.table:nullptr;table_=tableOwner_?tableOwner_->data():nullptr;
  rgb_=packedRGB(p.rgb555);colour_=opaque?p.colour:0;special_=p.special;scalar_=p.scalar;rgb555_=p.rgb555;}
 bool table()const{return table_!=nullptr;}
 template<bool Table>uint32_t shade(uint32_t pixel,uint8_t index,uint8_t a)const{
  if constexpr(Table)return (pixel&0xff000000u)|rgb_[uint16_t(table_[math_->offset[a]+index]|colour_)];
  const bool emissive=special_&&index>=240&&index<=254;const auto&gain=math_->gain[a];uint32_t out=0;const unsigned shift=scalar_?16:12;
  for(unsigned c=0;c<3;++c){unsigned s=16-c*8,v=(pixel>>s)&255u,g=emissive?math_->emissive[a]:gain[c];out|=std::min((v*g)>>shift,255u)<<s;}
  return (pixel&0xff000000u)|rgb_[uint16_t(nativepacked::word(out,rgb555_)|colour_)];
 }
 uint32_t apply(uint32_t pixel,uint8_t index,uint8_t a)const{return table_?shade<true>(pixel,index,a):shade<false>(pixel,index,a);}
};
}}

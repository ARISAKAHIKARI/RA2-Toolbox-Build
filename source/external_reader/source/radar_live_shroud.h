#pragma once
#include "radar_shroud_mask.h"
#include "radar_publication.h"
#include <map>

namespace external { namespace radar { namespace shroudmask {
struct Snapshot {
 Geometry geo;std::vector<Cell> cells;int frame=-1;bool fog=false;
 bool sameMask(const Snapshot&o)const{return geo.same(o.geo)&&fog==o.fog&&cells==o.cells;}
};
// Sample only the CURRENT destination crop and its selector neighbors. Terrain
// banks may take many bounded passes to refresh; exploration must not inherit
// that whole-map delay. Production Read::fresh bypasses the page/art cache.
class Live {
 struct Bits {uint32_t epoch=0;bool valid=false;shroudselector::Bits value;};
 std::vector<Bits> bits_;
 std::vector<uint32_t> pointers_;uint32_t epoch_=0;
 struct Page {std::array<uint8_t,4096>bytes;uint32_t epoch=0;bool valid=false;};
 std::map<uint32_t,Page>pages_;
 size_t calls_=0,bytes_=0,touchedPages_=0;
 template<class Read>static auto immediate(Read read,uint32_t p,void*out,size_t n,int)->decltype(read.fresh(p,out,n)){return read.fresh(p,out,n);}
 template<class Read>static bool immediate(Read read,uint32_t p,void*out,size_t n,long){return read(p,out,n);}
public:
 struct Stats {size_t calls,bytes,pages;};
 Stats stats()const{return {calls_,bytes_,touchedPages_};}
 void reset(){pointers_.clear();pages_.clear();for(auto&b:bits_)b.epoch=0;epoch_=0;}
 template<class Read>bool capture(Read read,const Geometry&g,double x,double y,double w,double h,Snapshot&out){
  out={};calls_=bytes_=touchedPages_=0;if(!g.valid()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||!std::isfinite(h)||
    w<=0||h<=0||w>32768||h>32768||std::abs(x)>1e7||std::abs(y)>1e7)return false;
  auto fresh=[&](uint32_t p,void*data,size_t n){++calls_;bytes_+=n;return immediate(read,p,data,n,0);};
  auto matches=[&](const PublicationContext&c){return c.active&&c.house==g.house&&c.cells==g.cells&&c.bounds==g.bounds&&
    c.rawWidth==g.rawWidth&&c.rawHeight==g.rawHeight;};
  const auto before=publicationContext(fresh);if(!matches(before))return false;
  int offsets[2]{},count=0;if(!fresh(Map+0x1490,&offsets[0],4)||!fresh(Map+0x1498,&offsets[1],4)||
    !fresh(Map+0x140,&count,4)||count!=g.count||offsets[0]!=g.offsetX||offsets[1]!=g.offsetY)return false;
  pointers_.resize(size_t(g.count));if(!fresh(g.cells,pointers_.data(),pointers_.size()*4))return false;
  std::array<int8_t,256>lookup;if(!shroudselector::readLookup(fresh,lookup))return false;
  if(bits_.empty())bits_.resize(512*512);
  if(++epoch_==0){for(auto&b:bits_)b.epoch=0;pages_.clear();++epoch_;}
  // This cache belongs to ONE visibility capture epoch. It never reuses a
  // page sampled earlier by the art/terrain renderer, and each next capture
  // refreshes every touched page. Adjacent Cell allocations share one RPM.
  auto fields=[&](uint32_t p,void*out,size_t n){
   size_t done=0;while(done<n){uint32_t address=p+uint32_t(done),page=address&~4095u;
    if(pages_.size()>=8192&&!pages_.count(page))pages_.clear();
    auto&cached=pages_[page];if(cached.epoch!=epoch_){++touchedPages_;cached.epoch=epoch_;cached.valid=fresh(page,cached.bytes.data(),cached.bytes.size());}
    size_t take=std::min(n-done,size_t(4096-(address&4095)));
    if(cached.valid)std::memcpy(static_cast<uint8_t*>(out)+done,cached.bytes.data()+(address&4095),take);
    else if(!fresh(address,static_cast<uint8_t*>(out)+done,take))return false;
    done+=take;
   }return true;
  };
  bool complete=true;
  auto fetch=[&](int cx,int cy,shroudselector::Bits&value){
   if(!g.legal(cx,cy))return false;size_t index=size_t(cy)*512+cx;
   if(index>=pointers_.size())return false;
   auto&cached=bits_[index];if(cached.epoch!=epoch_){
    cached.epoch=epoch_;cached.valid=false;uint32_t cell=pointers_[index];
    if(cell>=0x10000&&cell<=0x7ffffe00u){std::array<uint8_t,0x11D>field{};
     if(fields(cell+0x24,field.data(),field.size())){int16_t coord[2]{};std::memcpy(coord,field.data(),sizeof coord);
      if(coord[0]==cx&&coord[1]==cy){cached.value={field[0x108],field[0x11C]};cached.valid=true;}
      else complete=false;
     }else complete=false;
    }else if(cell)complete=false;
   }
   if(!cached.valid)return false;value=cached.value;return true;
  };
  // The mask's own 60x30 rectangle, not the radar's 2x1 RGB rectangle.
  const double d0=(x-30)/30.-g.offsetX,d1=(x+w+30)/30.-g.offsetX;
  const double s0=(y-30)/15.+g.offsetY,s1=(y+h)/15.+g.offsetY;
  const int firstY=std::clamp(int(std::floor((s0-d1)/2))+1,1,511);
  const int lastY=std::clamp(int(std::ceil((s1-d0)/2))-1,1,511);
  Snapshot candidate;candidate.geo=g;
  uint32_t scenario=0;uint8_t option=0,houseFog=0;
  if(!fresh(0xA8B230,&scenario,4)||scenario<0x10000||!fresh(scenario+1,&option,1)||!fresh(g.house+0x1F5,&houseFog,1))return false;
  candidate.fog=(option&0x10)&&houseFog==0;
  for(int cy=firstY;cy<=lastY;++cy){
   int lo=std::clamp(int(std::floor(std::max(d0+cy,s0-cy)))+1,1,511);
   int hi=std::clamp(int(std::ceil(std::min(d1+cy,s1-cy)))-1,1,511);
   for(int cx=lo;cx<=hi;++cx){if(!g.legal(cx,cy))continue;
    mapbounds::TileOrigin at;if(!g.shroudOrigin(cx,cy,at)||at.x>=x+w||at.x+60<=x||at.y>=y+h||at.y+30<=y)continue;
    if(candidate.cells.size()>=65536)return false;
    ShroudFrames shade{shroudselector::frame(cx,cy,false,lookup,fetch),shroudselector::frame(cx,cy,true,lookup,fetch)};
    candidate.cells.push_back({uint32_t(cy*512+cx),shade});
   }
  }
  // A failed read must not become an opaque selector neighbor in a published
  // mask. The caller keeps its previous completed scene and retries later.
  if(!complete)return false;
  const auto after=publicationContext(fresh);uint32_t afterScenario=0;
  if(!matches(after)||after.frame<before.frame||int64_t(after.frame)-before.frame>4||
    !fresh(0xA8B230,&afterScenario,4)||afterScenario!=scenario||!fresh(Map+0x140,&count,4)||count!=g.count||
    !fresh(Map+0x1490,&offsets[0],4)||!fresh(Map+0x1498,&offsets[1],4)||offsets[0]!=g.offsetX||offsets[1]!=g.offsetY)return false;
  candidate.frame=after.frame;out=std::move(candidate);return true;
 }
};
}}}

#pragma once
#include "formation.h"
#include <unordered_map>

namespace assist { namespace formation { namespace quick {
// The existing six configurable shapes retain their wire layout. This value
// identifies a transient quick-placement preview, not an extra key binding.
constexpr int Shape=6, NodeLimit=256;
template<class Keys>inline bool claimsMiddle(bool enabled,const Keys&keys){if(!enabled)return false;for(unsigned vk=8;vk<256;++vk)if(keys[vk])return false;return true;}
struct Path {
 Point nodes[NodeLimit]{};int count=0;
 void push(int x,int y,bool tail=false){
  Point p{double(x),double(y)};
  if(!count){nodes[count++]=p;return;}
  const auto last=nodes[count-1];
  if(last.x==p.x&&last.y==p.y)return;
  if(!tail&&std::max(std::abs(p.x-last.x),std::abs(p.y-last.y))<2)return;
  if(count<NodeLimit)nodes[count++]=p;else nodes[NodeLimit-1]=p;
 }
 void measure(double*lengths)const{
  lengths[0]=0;for(int n=1;n<count;++n)lengths[n]=lengths[n-1]+std::hypot(nodes[n].x-nodes[n-1].x,nodes[n].y-nodes[n-1].y);
 }
 Point at(double fraction,const double*lengths)const{
  if(!count)return {};if(count==1)return nodes[0];
  const double total=lengths[count-1];
  if(total<.001)return nodes[0];
  double target=std::clamp(fraction,0.,1.)*total;int n=1;
  while(n<count-1&&lengths[n]<target)++n;
  double distance=lengths[n]-lengths[n-1],t=distance>.001?(target-lengths[n-1])/distance:0;
  return {nodes[n-1].x+(nodes[n].x-nodes[n-1].x)*t,nodes[n-1].y+(nodes[n].y-nodes[n-1].y)*t};
 }
 Point at(double fraction)const{double lengths[NodeLimit]{};measure(lengths);return at(fraction,lengths);}
};
inline int category(const Unit&u,bool air){return air?2:u.kind==1?1:0;}
inline int quota(int category){return category==1?3:1;}
// Infantry, vehicles and air each have independent occupancy quotas. The
// actual simulation remains responsible for pathfinding and collision.
inline bool room(const Unit*units,const bool*air,const Slot*slots,int before,int current,int x,int y){
 int used=0,c=category(units[current],air[current]);
 for(int n=0;n<before;++n)if(slots[n].valid&&slots[n].x/256==x&&slots[n].y/256==y&&category(units[n],air[n])==c)++used;
 return used<quota(c);
}
template<class Valid>inline void layout(const Unit*units,int count,const bool*air,const Path&path,int x,int y,Slot*out,Valid valid){
 count=std::clamp(count,0,Maximum);for(int n=0;n<count;++n)out[n]={};
 int groupCount[2]{},groupAt[2]{};for(int n=0;n<count;++n)++groupCount[air[n]?1:0];
 const bool line=path.count>1;
 double lengths[NodeLimit]{};if(line)path.measure(lengths);
 // Per-solve cache: a cell is queried once per air/ground layer. Occupancy
 // counts retain the independent infantry/vehicle/air quotas without scanning
 // every previously placed unit for every spiral candidate. Never reuse this
 // cache across previews; terrain/occupancy may have changed by then.
 struct Cell {uint8_t known=0,passable=0,used[3]{},exhausted=0;};
 std::unordered_map<uint64_t,Cell> cells;cells.reserve(size_t(count)*4);
 for(int n=0;n<count;++n){
  const int group=air[n]?1:0;
  auto point=line?path.at((groupAt[group]+.5)/groupCount[group],lengths):Point{double(x),double(y)};++groupAt[group];
  int cx=int(std::lround(point.x)),cy=int(std::lround(point.y));
  const int limit=line?5:40;
  const uint64_t center=(uint64_t(uint32_t(cx))<<32)|uint32_t(cy);
  const uint8_t exhausted=uint8_t(1u<<category(units[n],air[n]));
  if(cells[center].exhausted&exhausted)continue;
  auto take=[&](int dx,int dy){int tx=cx+dx,ty=cy+dy;
   auto&cell=cells[(uint64_t(uint32_t(tx))<<32)|uint32_t(ty)];const uint8_t layer=uint8_t(1u<<group);
   if(!(cell.known&layer)){cell.known|=layer;if(valid(tx,ty,air[n]))cell.passable|=layer;}
   const int c=category(units[n],air[n]);if(!(cell.passable&layer)||cell.used[c]>=quota(c))return false;
   ++cell.used[c];out[n]={tx*256+128,ty*256+128,0,1};return true;};
  if(take(0,0))continue;
  bool done=false;
  for(int r=1;r<=limit&&!done;++r){
   for(int dx=-r;dx<=r&&!done;++dx)done=take(dx,-r);
   for(int dy=-r+1;dy<=r&&!done;++dy)done=take(r,dy);
   for(int dx=r-1;dx>=-r&&!done;--dx)done=take(dx,r);
   for(int dy=r-1;dy>-r&&!done;--dy)done=take(-r,dy);
  }
  // Capacity only decreases within this solve. Once this class has no slot
  // around an anchor, identical later members cannot discover a new one.
  if(!done)cells[center].exhausted|=exhausted;
 }
 // Reduce crossings within the same occupancy class. Never swap an air or
 // infantry slot into another class and invalidate the placement quotas.
 Slot assigned[Maximum]{};bool used[Maximum]{};
 for(int n=0;n<count;++n){int best=-1;double distance=1e100;
  for(int j=0;j<count;++j)if(!used[j]&&out[j].valid&&category(units[n],air[n])==category(units[j],air[j])){
   double dx=double(units[n].x)-out[j].x,dy=double(units[n].y)-out[j].y,d=dx*dx+dy*dy;if(d<distance){distance=d;best=j;}
  }
  if(best>=0){used[best]=true;assigned[n]=out[best];}
 }
 for(int n=0;n<count;++n)out[n]=assigned[n];
}
}}}

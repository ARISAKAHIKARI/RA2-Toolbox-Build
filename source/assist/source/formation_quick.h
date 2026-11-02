#pragma once
#include "formation.h"

namespace assist { namespace formation { namespace quick {
// The existing six configurable shapes retain their wire layout. This value
// identifies a transient quick-placement preview, not an extra key binding.
constexpr int Shape=6, NodeLimit=64;
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
 Point at(double fraction)const{
  if(!count)return {};if(count==1)return nodes[0];
  double lengths[NodeLimit]{},total=0;
  for(int n=1;n<count;++n){total+=std::hypot(nodes[n].x-nodes[n-1].x,nodes[n].y-nodes[n-1].y);lengths[n]=total;}
  if(total<.001)return nodes[0];
  double target=std::clamp(fraction,0.,1.)*total;int n=1;
  while(n<count-1&&lengths[n]<target)++n;
  double distance=lengths[n]-lengths[n-1],t=distance>.001?(target-lengths[n-1])/distance:0;
  return {nodes[n-1].x+(nodes[n].x-nodes[n-1].x)*t,nodes[n-1].y+(nodes[n].y-nodes[n-1].y)*t};
 }
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
 for(int n=0;n<count;++n){
  const int group=air[n]?1:0;
  auto point=line?path.at((groupAt[group]+.5)/groupCount[group]):Point{double(x),double(y)};++groupAt[group];
  int cx=int(std::lround(point.x)),cy=int(std::lround(point.y));
  const int limit=line?5:40;
  auto take=[&](int dx,int dy){int tx=cx+dx,ty=cy+dy;if(!valid(tx,ty,air[n])||!room(units,air,out,n,n,tx,ty))return false;out[n]={tx*256+128,ty*256+128,0,1};return true;};
  if(take(0,0))continue;
  bool done=false;
  for(int r=1;r<=limit&&!done;++r){
   for(int dx=-r;dx<=r&&!done;++dx)done=take(dx,-r);
   for(int dy=-r+1;dy<=r&&!done;++dy)done=take(r,dy);
   for(int dx=r-1;dx>=-r&&!done;--dx)done=take(dx,r);
   for(int dy=r-1;dy>-r&&!done;--dy)done=take(-r,dy);
  }
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

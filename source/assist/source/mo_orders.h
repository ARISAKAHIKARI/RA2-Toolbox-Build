#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
// Behaviour reference: MagicShiba/MO_ExperienceEnhancements, commit
// 33ed558bd38f36b5012eddcc9c0fb109cc68e38a. Independent implementation;
// this header is pure geometry and preview data, with no engine calls.
namespace assist { namespace moorders {
constexpr uint32_t Fuzzy=1,Area=2,Patrol=4,QuickFormation=8;
template<class Keys>inline uint32_t claimsMiddle(uint32_t features,const Keys&keys){
 if(keys[18])return 0;const bool ctrl=keys[17]!=0,z=keys['Z']!=0,shift=keys[16]!=0;
 const uint32_t mode=ctrl&&!z?Fuzzy:!ctrl&&z?Patrol:!ctrl&&shift?Area:0;return features&mode?mode:0;
}
constexpr int Actors=256,Targets=256,Nodes=64,Waypoints=16,Lines=128;
struct Point {int x=0,y=0,z=0;}; // World leptons in previews, map cells in geometry.
struct Line {Point from,to;};
struct Preview {
 uint32_t match=0,tick=0,active=0;
 int mode=0,count=0,nodeCount=0;
 double zScale=0.0866025403784;
 Line lines[Lines]{};Point nodes[Nodes]{};
};
inline int distance(Point a,Point b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));}
inline void sample(Point*nodes,int&n,Point p,bool force=false){
 if(n&&nodes[n-1].x==p.x&&nodes[n-1].y==p.y)return;
 if(n&&!force&&distance(nodes[n-1],p)<2)return;
 if(n<Nodes)nodes[n++]=p;else nodes[Nodes-1]=p;
}
// Small clicks form a square. An open stroke makes a narrow rectangle; a
// closed freehand stroke preserves its entire shape, resampled to native's
// sixteen-node limit rather than truncating the start of a longer stroke.
inline int patrol(const Point*nodes,int n,Point*out){
 if(!nodes||!out||n<=0)return 0;n=std::min(n,Nodes);
 Point a=nodes[0],b=nodes[n-1];
 if(n>=4&&distance(a,b)<3){
  double lengths[Nodes]{};for(int i=1;i<n;++i)lengths[i]=lengths[i-1]+std::hypot(double(nodes[i].x-nodes[i-1].x),double(nodes[i].y-nodes[i-1].y));
  double closing=std::hypot(double(a.x-b.x),double(a.y-b.y));double total=lengths[n-1]+closing;
  if(total>=8){int count=std::clamp(int(std::ceil(total/3.)),4,Waypoints),used=0,seg=0;
   for(int i=0;i<count;++i){double at=total*i/count;while(seg+1<n&&lengths[seg+1]<at)++seg;Point left=nodes[seg],right=seg+1<n?nodes[seg+1]:a;double len=seg+1<n?lengths[seg+1]-lengths[seg]:closing;double ratio=len>0?(at-lengths[seg])/len:0;Point p{int(std::lround(left.x+(right.x-left.x)*ratio)),int(std::lround(left.y+(right.y-left.y)*ratio)),0};
    if(!used||distance(out[used-1],p))out[used++]=p;
   }if(used>=4)return used;
  }
 }
 if(distance(a,b)<2){out[0]={b.x-2,b.y-2};out[1]={b.x+2,b.y-2};out[2]={b.x+2,b.y+2};out[3]={b.x-2,b.y+2};return 4;}
 double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);dx/=len;dy/=len;
 const double sideX=-dy*2,sideY=dx*2;
 out[0]={int(std::lround(a.x-dx*2+sideX)),int(std::lround(a.y-dy*2+sideY))};
 out[1]={int(std::lround(b.x+dx*2+sideX)),int(std::lround(b.y+dy*2+sideY))};
 out[2]={int(std::lround(b.x+dx*2-sideX)),int(std::lround(b.y+dy*2-sideY))};
 out[3]={int(std::lround(a.x-dx*2-sideX)),int(std::lround(a.y-dy*2-sideY))};return 4;
}
// Fuzzy attack keeps groups large enough to focus fire. The remainder is
// balanced over the selected targets; no actor is duplicated or omitted.
inline int groups(int actors,int targets,int squad=4){return actors>0&&targets>0?std::clamp(actors/std::max(1,squad),1,targets):0;}
inline int groupTarget(int actor,int actors,int groups){
 if(actor<0||actor>=actors||groups<=0)return -1;int base=actors/groups,extra=actors%groups,wide=(base+1)*extra;
 return actor<wide?actor/(base+1):extra+(actor-wide)/base;
}
}} // assist::moorders

#pragma once
#include "../../external_reader/source/enemy_markers.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <vector>
namespace assist { namespace enemyclusters {
constexpr int Radius=3*256;
struct Group {std::vector<int> members;int left=0,top=0,right=0,bottom=0;};
inline int64_t square(int64_t n){return n*n;}
inline bool coordinate(const external::enemies::Marker&m){return m.worldX>=0&&m.worldY>=0&&m.worldX<512*256&&m.worldY<512*256;}
inline std::vector<Group> group(const external::enemies::Frame&f){
 std::vector<Group> groups;int count=std::clamp(f.count,0,external::enemies::Maximum);
 std::vector<int> order(count);std::iota(order.begin(),order.end(),0);
 std::sort(order.begin(),order.end(),[&](int a,int b){return std::tie(f.items[a].id,f.items[a].object)<std::tie(f.items[b].id,f.items[b].object);});
 using Key=std::tuple<uint32_t,uint32_t,int,int,int>;
 std::map<Key,std::vector<int>> buckets;
 constexpr int BucketWidth=Radius/2,Neighbours=(Radius+BucketWidth-1)/BucketWidth;
 std::vector<int> parents(count);std::iota(parents.begin(),parents.end(),0);
 auto root=[&](int i){while(parents[i]!=i){parents[i]=parents[parents[i]];i=parents[i];}return i;};
 // Components, not a maximum group diameter: every pair within three cells
 // supplies an edge, including a later unit bridging two existing groups.
 for(int i:order){const auto&m=f.items[i];
  bool mobile=m.kind>=0&&m.kind<3&&coordinate(m);int bx=mobile?m.worldX/BucketWidth:0,by=mobile?m.worldY/BucketWidth:0;
  if(mobile)for(int y=by-Neighbours;y<=by+Neighbours;++y)for(int x=bx-Neighbours;x<=bx+Neighbours;++x){
   auto found=buckets.find({m.owner,m.type,m.kind,x,y});if(found==buckets.end())continue;
   // A half-radius square is entirely connected. One cross-bucket edge
   // joins its whole component; skip it once joined, avoiding quadratic
   // pair checks for a crowded army or many units sharing one cell.
   int a=root(i),b=root(found->second.front());if(a==b)continue;
   if(x==bx&&y==by){parents[a]=b;continue;}
   for(int j:found->second){const auto&p=f.items[j];if(square(int64_t(m.worldX)-p.worldX)+square(int64_t(m.worldY)-p.worldY)<=square(Radius)){parents[a]=b;break;}}
  }
  if(mobile)buckets[{m.owner,m.type,m.kind,bx,by}].push_back(i);
 }
 std::vector<int> component(count,-1);
 for(int i:order){const auto&m=f.items[i];int r=root(i),chosen=component[r];
  if(chosen<0){component[r]=chosen=int(groups.size());groups.push_back({{},m.worldX,m.worldY,m.worldX,m.worldY});}
  auto&g=groups[chosen];g.members.push_back(i);g.left=std::min(g.left,m.worldX);g.right=std::max(g.right,m.worldX);g.top=std::min(g.top,m.worldY);g.bottom=std::max(g.bottom,m.worldY);
 }
 return groups;
}
// Aggregate already-smoothed individual anchors. Merge/split never feeds a
// group centroid back into Motion, so there is no invented velocity or lag.
template<class View>int render(const external::enemies::Frame&raw,const View*positions,int n,
 const std::vector<Group>&groups,external::enemies::Frame&out,View*views){
 out.match=raw.match;out.tick=raw.tick;out.house=raw.house;out.frame=raw.frame;out.omitted=raw.omitted;out.width=raw.width;out.height=raw.height;out.count=0;
 std::unordered_map<uint32_t,int> byId;
 for(int i=0;i<std::clamp(n,0,external::enemies::Maximum);++i)byId.emplace(positions[i].id,i);
 for(const auto&g:groups){double x=0,y=0;int count=0,first=-1,position=-1;
  for(int i:g.members){if(i<0||i>=raw.count)continue;auto found=byId.find(raw.items[i].id);if(found==byId.end())continue;
   if(first<0){first=i;position=found->second;}x+=positions[found->second].x;y+=positions[found->second].y;++count;
  }
  if(!count||out.count==external::enemies::Maximum)continue;
  auto&m=out.items[out.count];m=raw.items[first];m.quantity=count;m.x=int(std::lround(x/count));m.y=int(std::lround(y/count));
  auto&v=views[out.count++];v=positions[position];v.x=float(x/count);v.y=float(y/count);
 }
 return out.count;
}
}}

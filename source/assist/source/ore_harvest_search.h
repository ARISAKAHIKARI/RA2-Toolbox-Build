#pragma once
#include <cstdint>
#include <climits>
namespace assist { namespace oreharvest {
// Visits each square ring once. A completed ring can prove that no unvisited
// cell is nearer in Euclidean distance; ties use the native encoded cell ID.
// Reader: -1 uncertain (retry this cell), 0 unsuitable, 1 harvestable resource.
struct Search {
 int x=0,y=0,radius=0,at=0;uint32_t best=0;int64_t distance=INT64_MAX;bool done=false;
 void begin(int originX,int originY){*this={};x=originX;y=originY;}
 template<class Cell>void advance(int&budget,Cell cell){
  while(budget>0&&!done){
   if(radius>511||(best&&int64_t(radius)*radius>distance)){done=true;break;}
   int cx=x,cy=y;
   if(radius){const int edge=2*radius;
    if(at<edge){cx=x-radius+at;cy=y-radius;}
    else if(at<2*edge){cx=x+radius;cy=y-radius+at-edge;}
    else if(at<3*edge){cx=x+radius-(at-2*edge);cy=y+radius;}
    else{cx=x-radius;cy=y+radius-(at-3*edge);}
   }
   --budget;const int result=cx>0&&cx<512&&cy>0&&cy<512?cell(cx,cy):0;
   if(result<0)return;
   if(result>0){const uint32_t id=uint32_t(cy*1000+cx);const int64_t dx=cx-x,dy=cy-y,d=dx*dx+dy*dy;
    if(d<distance||(d==distance&&(!best||id<best))){distance=d;best=id;}
   }
   if(++at>=(radius?8*radius:1)){++radius;at=0;}
  }
 }
};
}} // assist::oreharvest

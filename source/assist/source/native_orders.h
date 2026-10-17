#pragma once
#include <stdint.h>
namespace nativeorders {
constexpr int MaxOrders=2048, MaxPassengers=2048, WorkPerFrame=64, EventBudget=8;
inline double value(int cost,float veterancy,int veteranPercent=200,int elitePercent=300){
 if(cost<0)cost=0;
 int weight=veterancy>=2?elitePercent:veterancy>=1?veteranPercent:100;
 if(weight<100)weight=100;if(weight>10000)weight=10000;
 return double(cost)*weight/100.0;
}
inline bool fireEligible(int error){return error==0||error==2||error==3||error==4||error==7||error==9||error==10||error==11;}
inline int specialMission(bool engineer,bool infiltrator,bool c4,int action){
 if(engineer&&action==9)return 8;
 if(infiltrator&&action==3)return 7;
 if(c4&&action==16)return 17;
 if(c4&&action==5)return 1;
 return -1;
}
struct Progress {
 int x{},y{},since{},issued{};bool seenMove{},hold{};
 void begin(int px,int py,int frame){x=px;y=py;since=issued=frame;seenMove=false;}
 void combat(int px,int py,int frame){x=px;y=py;since=frame;}
 void update(int px,int py,int frame,int mission,int gx,int gy,int timeout){
  if(hold)return;
  int64_t dx=int64_t(px)-gx,dy=int64_t(py)-gy;
  if(dx*dx+dy*dy<=128*128){hold=true;return;}
  dx=int64_t(px)-x;dy=int64_t(py)-y;
  if(dx*dx+dy*dy>=16*16){x=px;y=py;since=frame;}
  if(mission==2)seenMove=true;
  // Combat is handled before this method; a combat stop cannot expire the path.
  if((seenMove&&(mission==0||mission==5||mission==11)&&frame-issued>=12)||frame-since>=timeout)hold=true;
 }
};
}

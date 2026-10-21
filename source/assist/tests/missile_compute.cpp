#include "../source/missile_compute.h"
#include "../source/planning_budget.h"
#include <cassert>
#include <cstdio>
using namespace assist;
int main(){
 missilecompute::Input i;i.match=1;i.frame=10;i.count=3;i.targetCount=2;
 i.targets[0]={20,200,50,1};i.targets[1]={21,210,50,1};
 for(int s=0;s<3;++s){i.sources[s].id=s+1;for(int t=0;t<2;++t)i.edges[s][t]={50,uint32_t(t+1)};}
 auto r=missilecompute::calculate(i);assert(r.count==2&&r.orders[0].target!=r.orders[1].target);
 i.sources[0].current=20;i.sources[1].current=20;r=missilecompute::calculate(i);assert(r.count==1&&r.orders[0].target==1);
 i.sources[0].pending=20;i.sources[1].pending=21;r=missilecompute::calculate(i);assert(!r.count);
 i.sources[0].pending=i.sources[1].pending=i.sources[0].current=i.sources[1].current=0;
 i.edges[0][0].damage=i.edges[1][0].damage=i.edges[2][0].damage=0;
 r=missilecompute::calculate(i);assert(r.count==1&&r.orders[0].target==1);
 i.count=-1;assert(!missilecompute::calculate(i).count);i.count=65;assert(!missilecompute::calculate(i).count);
 // Full capacity stays bounded to eight events, and neither writer receives
 // an invalid source or target index.
 i={};i.frame=1;i.match=2;i.count=i.targetCount=64;
 for(int s=0;s<64;++s){i.sources[s].id=s+1;i.targets[s]={uint32_t(100+s),uint32_t(1000+s),100,1};for(int t=0;t<64;++t)i.edges[s][t]={100,uint32_t(s+t)};}
 r=missilecompute::calculate(i);assert(r.count==8);for(int n=0;n<r.count;++n)assert(r.orders[n].source<64&&r.orders[n].target<64);
 planningbudget::Budget budget;budget.begin(1,1,12);
 for(int c=0;c<3;++c){assert(budget.roomMask(c,7)==4);for(int n=0;n<4;++n)budget.consume(c);assert(!budget.roomMask(c,7));}
 assert(!budget.roomMask(0,1)&&!budget.roomMask(1,2)&&!budget.roomMask(2,4));
 budget.begin(1,2,24);assert(budget.roomMask(2,4)==24);assert(budget.room(0,true)==12);
 puts("PASS missile allocation, pending coverage, bounds and shared three-family frame budget");
}

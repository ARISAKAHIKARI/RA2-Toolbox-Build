#include "../source/missile_compute.h"
#include <cassert>
#include <cstdio>
using namespace assist::missilecompute;
int main(){
 Input i;i.frame=1;i.match=1;i.count=6;i.targetCount=2;
 for(int s=0;s<6;++s){i.sources[s].id=s+1;for(int t=0;t<2;++t)i.edges[s][t]={40,uint32_t(100+t*100),0,1};}
 i.targets[0]={20,0,40,1};i.targets[1]={21,0,120,1};
 auto counts=[&](Result r,int*attack,int&hold){for(int n=0;n<r.count;++n){auto&o=r.orders[n];if(o.action==Attack)++attack[o.target];else if(o.action==Hold)++hold;}};
 int a[2]{},h=0;counts(calculate(i),a,h);assert(a[0]==2&&a[1]==4&&h==0);
 i.mode=Even;a[0]=a[1]=h=0;counts(calculate(i),a,h);assert(a[0]==3&&a[1]==3&&h==0);
 // Staggered entry: assign half to a trailing target, but send Sleep instead
 // of an out-of-range Attack. These guns cannot spend their shot on the leader.
 for(int s=0;s<6;++s)i.edges[s][1]={40,400,10,0};
 a[0]=a[1]=h=0;counts(calculate(i),a,h);assert(a[0]==3&&a[1]==0&&h==3);
 // Existing automatic locks all point at the leader. Excess locks still hold.
 for(auto&s:i.sources)s.current=20;
 auto r=calculate(i);h=0;for(int n=0;n<r.count;++n)h+=r.orders[n].action==Hold;assert(h==3);
 // Cooldown sources do not steal the immediately available slot.
 i={};i.frame=1;i.count=2;i.targetCount=1;i.targets[0]={20,0,40,0};
 i.sources[0].id=1;i.sources[0].cooldown=90;i.sources[1].id=2;
 i.edges[0][0]=i.edges[1][0]={40,100,0,1};r=calculate(i);assert(r.count==2);for(int n=0;n<r.count;++n)assert(r.orders[n].action==Attack);
 // A pending attack counts once; pending Sleep reserves no damage.
 i.sources[1].pending=20;i.sources[1].pendingMission=Attack;r=calculate(i);assert(r.count==1&&r.orders[0].action==Attack);
 // A tracked hold wakes when its target disappears, regardless of ammunition.
 i.targetCount=0;i.sources[0].held=1;r=calculate(i);assert(r.count==1&&r.orders[0].action==Release);
 // Safe reserve: one 120% shot covers the leader, a trailing target is
 // about to enter. Timeout, loss of trailing target or urgent danger wakes.
 for(int reason=0;reason<5;++reason){Input q;q.frame=1;q.count=3;q.targetCount=2;q.targets[0]={20,0,100,0};q.targets[1]={21,0,100,0};
  for(int n=0;n<3;++n){q.sources[n].id=n+1;q.edges[n][0]={120,100,0,1};q.edges[n][1]={120,200,10,0};}
  auto first=calculate(q);int held=0;for(int n=0;n<first.count;++n)held+=first.orders[n].action==Hold;assert(held==2);
  for(int n=0;n<3;++n){q.sources[n].held=1;if(reason==0)q.sources[n].holdFrames=30;if(reason==1)q.sources[n].releaseFrames=20;if(reason==4)q.sources[n].cooldown=20;}
  if(reason==2)q.targetCount=1;if(reason==3)q.targets[0].threatening=2;
  auto next=calculate(q);assert(next.count==3);for(int n=0;n<next.count;++n)assert(next.orders[n].action==Attack);
 }
 i.count=-1;assert(!calculate(i).count);i.count=65;assert(!calculate(i).count);
 i={};i.frame=1;i.count=i.targetCount=64;i.mode=Even;
 for(int s=0;s<64;++s){i.sources[s].id=s+1;i.targets[s]={uint32_t(100+s),0,100,0};for(int t=0;t<64;++t)i.edges[s][t]={40,uint32_t(s+t),0,1};}
 r=calculate(i);assert(r.count==64);bool used[64]{};for(int n=0;n<r.count;++n){assert(!used[r.orders[n].source]);used[r.orders[n].source]=true;}
 puts("PASS smart/even quotas, staggered entry, explicit reserve, excess locks, cooldown, pending, release and capacity");
}

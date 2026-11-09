// Differential regression for the 1.0.46.4 greedy allocator.
// Frozen reference source: f452d1cb95c57f465356b55a3d7b83fb7403a70a.
// The independent scan reference intentionally retains its original ordering.
#include "../source/missile_compute.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
namespace reference147 {
using namespace assist::missilecompute;
inline Result calculate(const Input&i){
 Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;
 int assigned[MaxSources],number[MaxTargets]{},options[MaxSources]{};int64_t damage[MaxTargets]{};
 bool busy[MaxSources]{};
 std::fill(assigned,assigned+MaxSources,-1);
 if(i.mode&ReserveSleep)for(int s=0;s<i.count;++s)if(i.sources[s].committedDamage>0)
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].id==i.sources[s].committedTarget&&i.targets[t].hp>0)damage[t]+=i.sources[s].committedDamage;
 for(int s=0;s<i.count;++s){
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].hp>0&&i.edges[s][t].damage>0){
   if(std::max(i.edges[s][t].eta,i.sources[s].cooldown)<=LookAhead)++options[s];
  }
  // Pending events reserve their shooters, including attacks still approaching.
  busy[s]=i.sources[s].pendingMission!=-1;
  if(busy[s]&&(i.sources[s].pendingMission==Attack||i.sources[s].pendingMission==0))for(int t=0;t<i.targetCount;++t)
   if(i.sources[s].pending==i.targets[t].id&&i.edges[s][t].damage>0){assigned[s]=t;++number[t];if(!(i.mode&ReserveSleep)||i.sources[s].committedTarget!=i.targets[t].id)damage[t]+=i.edges[s][t].damage;break;}
 }
 for(int pass=0;pass<i.count;++pass){int bs=-1,bt=-1,br=INT_MAX;
  for(int s=0;s<i.count;++s)if(!busy[s]&&assigned[s]<0)for(int t=0;t<i.targetCount;++t){
   const auto&e=i.edges[s][t];const auto&target=i.targets[t];int ready=std::max(e.eta,i.sources[s].cooldown);
   if(e.damage<=0||target.hp<=0||ready>LookAhead||((i.mode&1)==Smart&&damage[t]*100>=int64_t(target.hp)*CoveragePercent))continue;
   bool better=bt<0;
   if(!better){
    const int64_t left=(i.mode&1)==Even?number[t]:damage[t]*i.targets[bt].hp;
    const int64_t right=(i.mode&1)==Even?number[bt]:damage[bt]*target.hp;
    if(left!=right)better=left<right;
    else if(target.threatening!=i.targets[bt].threatening)better=target.threatening>i.targets[bt].threatening;
    else if(ready!=br)better=ready<br;
    else if(options[s]!=options[bs])better=options[s]<options[bs];
    else if((i.sources[s].current==target.id)!=(i.sources[bs].current==i.targets[bt].id))better=i.sources[s].current==target.id;
    else if(e.distance!=i.edges[bs][bt].distance)better=e.distance<i.edges[bs][bt].distance;
    else if(target.id!=i.targets[bt].id)better=target.id<i.targets[bt].id;
    else better=i.sources[s].id<i.sources[bs].id;
   }
   if(better){bs=s;bt=t;br=ready;}
  }
  if(bs<0)break;assigned[bs]=bt;++number[bt];damage[bt]+=i.edges[bs][bt].damage;
 }
 // Only immediately usable shots justify sending a ready gun ahead. Future
 // assignments/cooldowns are planning reservations, not guaranteed damage.
 int64_t covering[MaxTargets]{};
 if(i.mode&ReserveSleep)for(int s=0;s<i.count;++s)if(i.sources[s].committedDamage>0)
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].id==i.sources[s].committedTarget)covering[t]+=i.sources[s].committedDamage;
 for(int s=0;s<i.count;++s){int t=assigned[s];if(t>=0&&i.edges[s][t].close&&i.sources[s].cooldown<=3&&i.sources[s].pendingMission!=0)covering[t]+=i.edges[s][t].damage;}
 int actions[MaxSources]{};
 for(int s=0;s<i.count;++s){if(busy[s])continue;int t=assigned[s],fallback=-1,future=-1;
  for(int n=0;n<i.targetCount;++n){const auto&e=i.edges[s][n];if(e.damage<=0||i.targets[n].hp<=0)continue;
   if(!e.close&&e.eta<=LookAhead){if(future<0||e.eta<i.edges[s][future].eta)future=n;continue;}
   if(!e.close)continue;
   if(fallback<0||i.targets[n].threatening>i.targets[fallback].threatening||
      (i.targets[n].threatening==i.targets[fallback].threatening&&covering[n]*i.targets[fallback].hp<covering[fallback]*i.targets[n].hp))fallback=n;
  }
  int action=Release;
  if(i.mode&ReserveSleep){
   if(t>=0)action=i.edges[s][t].close&&i.sources[s].cooldown<=3?Attack:Hold;
   else if(fallback>=0){
    t=fallback;bool covered=covering[t]*100>=int64_t(i.targets[t].hp)*CoveragePercent;
    action=i.sources[s].cooldown>3||covered&&i.targets[t].threatening<2?Hold:Attack;
    if(action==Attack)covering[t]+=i.edges[s][t].damage;
   }else if(future>=0){t=future;action=Hold;}
   actions[s]=action;assigned[s]=t;continue;
  }
  if(t>=0&&i.edges[s][t].close)action=Attack;
  else if(fallback>=0){
   bool covered=covering[fallback]*100>=int64_t(i.targets[fallback].hp)*CoveragePercent;
   bool ahead=future>=0&&covered&&i.targets[fallback].threatening<2&&!i.sources[s].releaseFrames&&i.sources[s].cooldown<=LookAhead;
   if(ahead){action=Advance;if(t<0)t=future;}
   else{action=Attack;t=fallback;if(i.sources[s].cooldown<=3)covering[t]+=i.edges[s][t].damage;}
  }else if(future>=0&&!i.sources[s].releaseFrames&&i.sources[s].cooldown<=LookAhead){action=Advance;if(t<0)t=future;}
  // No eligible target: release only an automatic order we still own.
  actions[s]=action;assigned[s]=t;
 }
 // Covering attacks/wakes precede advances if the gateway runs out of budget.
 for(int phase=0;phase<2;++phase)for(int s=0;s<i.count;++s){if(busy[s])continue;
  int t=assigned[s],action=actions[s];if((phase==1)!=(action==Advance||action==Hold))continue;
  if(action==Release&&!i.sources[s].held&&!i.sources[s].managed)continue;
  // held records ownership from enqueue, not proof the game entered Sleep.
  // Runtime deduplicates only an observed Sleep with an empty target, so a
  // timed-out or rejected hold remains eligible for another native event.
  if(action!=Release&&i.sources[s].current==i.targets[t].id&&
     ((action==Attack&&!i.sources[s].held)||(action==Advance&&i.sources[s].managed&&i.sources[s].advanceFrames>=0)))continue;
  r.orders[r.count++]={s,t,action};
 }
 return r;
}

}
using namespace assist::missilecompute;
static unsigned state=0x147a46f4u;
static unsigned randomValue(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static bool same(const Result&a,const Result&b){
 if(a.match!=b.match||a.tick!=b.tick||a.frame!=b.frame||a.count!=b.count)return false;
 for(int n=0;n<a.count;++n)if(a.orders[n].source!=b.orders[n].source||a.orders[n].target!=b.orders[n].target||a.orders[n].action!=b.orders[n].action)return false;
 return true;
}
static int scenarios=0;
static void check(const Input&i,Workspace&w){
 auto expected=reference147::calculate(i),actual=calculateWithWorkspace(i,w);
 if(!same(expected,actual)){std::fprintf(stderr,"mismatch case=%d mode=%d sources=%d targets=%d\n",scenarios,i.mode,i.count,i.targetCount);std::abort();}
 ++scenarios;
}
static void randomized(Input&i,int size){
 reset(i);i.match=randomValue();i.tick=randomValue();i.frame=int(randomValue()%100000);
 i.count=int(randomValue()%unsigned(size+1));i.targetCount=int(randomValue()%unsigned(size+1));
 for(int t=0;t<i.targetCount;++t){auto&v=i.targets[t];v.id=randomValue()%12;v.hp=int(randomValue()%4001)-3;v.threatening=randomValue()%4;}
 for(int s=0;s<i.count;++s){auto&v=i.sources[s];v.id=randomValue()%12;v.current=randomValue()%12;v.pending=randomValue()%12;
  v.pendingMission=int(randomValue()%8)-2;v.cooldown=int(randomValue()%100)-5;v.held=randomValue()%2;v.managed=randomValue()%2;
  v.advanceFrames=int(randomValue()%20)-2;v.releaseFrames=randomValue()%31;
  v.committedTarget=randomValue()%12;v.committedDamage=randomValue()%3?int(randomValue()%500):0;
  for(int t=0;t<i.targetCount;++t){auto&e=i.edges[s][t];e.damage=int(randomValue()%400)-4;e.distance=randomValue()%8;e.eta=int(randomValue()%50)-5;e.close=randomValue()%2;}
 }
}
static void dense(Input&i,int n,int mode){
 reset(i);i.frame=100;i.match=4;i.tick=12345;i.count=i.targetCount=n;i.mode=mode;
 for(int t=0;t<n;++t)i.targets[t]={uint32_t(t+1000),0,1000,0};
 for(int s=0;s<n;++s){i.sources[s].id=s+1;for(int t=0;t<n;++t)i.edges[s][t]={1,uint32_t((s*17+t*31)%257),0,1};}
}
int main(int argc,char**argv){
 auto input=std::make_unique<Input>();auto workspace=std::unique_ptr<Workspace>(new Workspace);
 auto&i=*input;auto&w=*workspace;
 // Reuse the same dirty workspace across shrinking, empty and invalid inputs.
 for(int n:{0,1,2,3,16,32,64,128,256})for(int mode=0;mode<4;++mode){dense(i,n,mode);check(i,w);}
 for(int trial=0;trial<2000;++trial){randomized(i,32);for(int mode=0;mode<4;++mode){i.mode=mode;check(i,w);}}
 // Exact ties with duplicate IDs exercise the original source-major tie order.
 for(int n:{1,2,8,256})for(int mode=0;mode<4;++mode){dense(i,n,mode);
  for(int t=0;t<n;++t)i.targets[t].id=0;
  for(int s=0;s<n;++s){i.sources[s].id=0;for(int t=0;t<n;++t)i.edges[s][t].distance=0;}
  check(i,w);
 }
 for(int mode=0;mode<4;++mode){dense(i,256,mode);
  for(int s=0;s<256;++s){i.sources[s].pendingMission=s%3?Attack:TransientBusy;i.sources[s].pending=1000+s;}
  check(i,w);
 }
 for(int field=0;field<7;++field){dense(i,1,0);switch(field){case 0:i.frame=-1;break;case 1:i.count=-1;break;case 2:i.count=257;break;case 3:i.targetCount=-1;break;case 4:i.targetCount=257;break;case 5:i.mode=-1;break;default:i.mode=4;}check(i,w);assert(same(calculate(i),reference147::calculate(i)));}
 dense(i,12,3);assert(same(calculate(i),calculateWithWorkspace(i,w)));
 std::printf("{\"equivalent_scenarios\":%d,\"workspace_bytes\":%zu,\"same_workspace_reused\":true}\n",scenarios,sizeof(Workspace));
 if(argc>1&&std::strcmp(argv[1],"--no-benchmark")==0)return 0;
 for(int n:{32,64,128,256})for(int mode=0;mode<4;++mode)for(int aligned=0;aligned<(n==256?2:1);++aligned){dense(i,n,mode);std::vector<double>before,after;
  if(aligned)for(int s=0;s<n;++s)for(int t=0;t<n;++t)i.edges[s][t].distance=0;
  for(int pass=0;pass<9;++pass){
   auto a=std::chrono::steady_clock::now();auto x=reference147::calculate(i);auto b=std::chrono::steady_clock::now();
   auto y=calculateWithWorkspace(i,w);auto c=std::chrono::steady_clock::now();assert(same(x,y));
   before.push_back(std::chrono::duration<double,std::milli>(b-a).count());after.push_back(std::chrono::duration<double,std::milli>(c-b).count());
  }
  std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
  std::printf("{\"sources\":%d,\"targets\":%d,\"mode\":%d,\"pattern\":\"%s\",\"baseline_median_ms\":%.6f,\"optimized_median_ms\":%.6f}\n",n,n,mode,aligned?"same_distance":"varied_distance",before[4],after[4]);
 }
}

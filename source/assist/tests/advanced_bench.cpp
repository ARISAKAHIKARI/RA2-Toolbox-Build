// Compare the current planner with a separately extracted previous source tree:
// g++ -O2 -std=c++17 advanced_bench.cpp -o advanced_bench
// g++ -O2 -std=c++17 -DBASELINE_HEADER='"/absolute/old/assist/source/external_planner.h"' advanced_bench.cpp -o old_bench
// Synthetic CPU work and native-order counts, not real game/network latency.
#ifdef BASELINE_HEADER
#include BASELINE_HEADER
#else
#include "../source/external_planner.h"
#endif
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
int main(){
 auto world=std::make_unique<World>();
 for(int total:{1000,8192})for(int scene=0;scene<3;++scene){
  auto&w=*world;w={};w.match=total*10+scene+1;w.count=total;w.intentCount=64;Planner planner;
  for(int i=0;i<total;++i){auto&a=w.actors[i];a.id=i+1;a.kind=2;a.flags=i<64?Own:Enemy;a.cost=100;
   a.x=100000+(i%100)*512;a.y=100000+(i/100)*512;
   if(i<64){a.x=50000+i*2;a.y=50000;a.range=2048;a.weapons[0]={2048,0,GroundWeapon,0x7ff};a.mission=2;w.intents[i]={a.id,30030,0};}
  }
  for(int i=64;i<66;++i){w.actors[i].x=50500+(i-64)*500;w.actors[i].y=50000;w.actors[i].cost=500;}
  std::vector<double> timings;uint64_t checks=0,orders=0;
  for(int frame=0;frame<700;++frame){w.frame=frame;
   if(scene==1){w.actors[64].x=frame%4<2?50400:51000;w.actors[65].x=frame%4<2?51000:50400;}
   if(scene==2){w.actors[64].cost=frame%60<30?900:500;w.actors[65].cost=frame%60<30?500:900;}
   for(int i=0;i<64;++i){w.actors[i].rearmStart=scene==0?0:frame/10*10;w.actors[i].rearmDuration=scene==0?2000:1;}
   auto start=std::chrono::steady_clock::now();auto out=planner.step(w,AdvancedInput,200,300,90);auto stop=std::chrono::steady_clock::now();
   // Model an accepted event becoming visible in the next snapshot, rather
   // than leaving every target permanently absent as in a failed-shot fixture.
   for(const auto&o:out){planner.accepted(o,frame);auto&a=w.actors[o.unit-1];a.target=o.target;a.mission=o.mission;}
   if(frame>=100){timings.push_back(std::chrono::duration<double,std::micro>(stop-start).count());checks+=planner.targetChecks;orders+=out.size();}
  }
  std::sort(timings.begin(),timings.end());double sum=0;for(double us:timings)sum+=us;
  std::cout<<"actors="<<total<<" scene="<<(scene==0?"cooldown":scene==1?"equal_value_motion":"priority_change")
   <<" mean_us="<<std::fixed<<std::setprecision(2)<<sum/timings.size()<<" p95_us="<<timings[timings.size()*95/100]
   <<" checks_per_step="<<double(checks)/timings.size()<<" orders_600_steps="<<orders<<"\n";
 }
}

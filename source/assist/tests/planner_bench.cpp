#include "../source/external_planner.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
using namespace assist;using namespace assist::planner;
int main(){
 auto world=std::make_unique<World>();auto planner=std::make_unique<Planner>();std::mt19937 rng(314159);
 for(int total:{1000,8192}){
  auto&w=*world;w={};w.match=uint32_t(total);w.count=total;w.intentCount=64;
  for(int i=0;i<total;++i){auto&a=w.actors[i];a.id=i+1;a.kind=2;a.x=int(rng()%128000);a.y=int(rng()%128000);a.cost=100+int(rng()%4000);a.veterancy=float(rng()%3);a.flags=i<64?Own:Enemy;
   if(i<64){a.x=62000+i*80;a.y=62000;a.range=2048;a.weapons[0]={2048,0,GroundWeapon,0x7FF};a.mission=2;w.intents[i]={a.id,30030,0};}
  }
  std::vector<double> timings;uint64_t checks=0;
  for(int frame=0;frame<700;++frame){w.frame=frame;auto start=std::chrono::steady_clock::now();auto orders=planner->step(w,AdvancedInput,200,300,90);auto end=std::chrono::steady_clock::now();
   for(auto&o:orders)planner->accepted(o,frame);
   if(frame>=100){timings.push_back(std::chrono::duration<double,std::micro>(end-start).count());
checks+=planner->targetChecks;
   }
  }
  std::sort(timings.begin(),timings.end());double sum=0;for(auto t:timings)sum+=t;
  std::cout<<total<<" actors, 64 orders: mean_us="<<std::fixed<<std::setprecision(2)<<sum/timings.size()<<", p95_us="<<timings[size_t(timings.size()*.95)]<<", mean_target_checks="<<checks/timings.size()<<"\n";
 }
}

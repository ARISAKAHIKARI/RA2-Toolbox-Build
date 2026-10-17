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
 auto data=std::make_unique<World>();auto planner=std::make_unique<Planner>();auto&w=*data;
 for(int miners:{32,128,256})for(int mines:{1,16}){
  w={};w.match=1;w.count=miners+mines;w.refineryCount=mines;
  for(int i=0;i<miners;++i){auto&a=w.actors[i];a.id=i+1;a.kind=2;a.flags=Own|Harvester;a.x=(20+(i%mines)*25)*256-128;a.y=40*256-128;a.mission=10;a.storageLimit=i%2?20:40;a.storage[0]=float(a.storageLimit);a.dockTypes[0]=900;}
  for(int i=0;i<mines;++i){auto&r=w.actors[miners+i];r.id=1000+i;r.type=900;r.kind=3;r.flags=Own;r.x=(20+i*25)*256;r.y=40*256;r.radioIDs[0]=9999;
   auto&area=w.refineries[i];area.id=r.id;area.count=40;for(int n=0;n<40;++n)area.waiting[n]={uint32_t((43+n/8)*1000+(17+i*25+n%8)),0,0};}
  for(bool cold:{true,false}){
   std::vector<double> times;int ordersCount=0;planner->reset();
   for(int frame=0;frame<500;++frame){if(cold)planner->reset();w.frame=frame;auto start=std::chrono::steady_clock::now();auto orders=planner->step(w,AutoOreReturn,200,300,90);auto end=std::chrono::steady_clock::now();
    for(auto&o:orders){planner->accepted(o,frame);if(!cold&&o.mission==2){auto&a=w.actors[o.unit-1];a.x=int(o.destination%1000)*256+128;a.y=int(o.destination/1000)*256+128;}}
    if(frame>=50){times.push_back(std::chrono::duration<double,std::micro>(end-start).count());ordersCount+=int(orders.size());}
   }
   std::sort(times.begin(),times.end());double sum=0;for(auto t:times)sum+=t;
   std::cout<<miners<<" miners / "<<mines<<" refineries / "<<(cold?"queue fill":"parked")<<": mean_us="<<std::fixed<<std::setprecision(2)<<sum/times.size()<<", p95_us="<<times[size_t(times.size()*.95)]<<", mean_orders="<<double(ordersCount)/times.size()<<"\n";
  }
 }
}

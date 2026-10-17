// CPU-only microbenchmarks: no Windows capture/RPM/network/input-to-photon.
#include "../source/external_planner.h"
#include "../source/cooperative_plan.h"
#include "../source/autoload.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <cstring>
using namespace assist;
static volatile uint64_t sink;
static bool first=true;
template<class F>void measure(const char*name,int size,F task){
 std::vector<double> samples;for(int n=0;n<210;++n){auto a=std::chrono::steady_clock::now();sink+=task(n);auto b=std::chrono::steady_clock::now();if(n>=10)samples.push_back(std::chrono::duration<double,std::micro>(b-a).count()/1000);}
 std::sort(samples.begin(),samples.end());double sum=std::accumulate(samples.begin(),samples.end(),0.0);
 if(!first)std::cout<<",\n";first=false;std::cout<<"{\"name\":\""<<name<<"\",\"size\":"<<size<<",\"samples\":200,\"mean_ms\":"<<sum/200<<",\"p50_ms\":"<<samples[99]<<",\"p95_ms\":"<<samples[189]<<",\"p99_ms\":"<<samples[197]<<",\"max_ms\":"<<samples[199]<<"}"<<std::flush;
}
int main(){static assist::loading::Workspace workspace;std::cout<<std::fixed<<std::setprecision(6)<<"{\"scope\":\"Linux native optimized C++ production algorithms; CPU time only; excludes game reads, IPC, OS scheduling, event execution and rendering\",\"results\":[\n";
 for(int count:{64,256,512})for(int mode:{1,2}){
  cooperative::Input input;input.mode=mode;
  for(int i=0;i<count;++i)input.units.push_back({{uint32_t(i+1),0},1,i*16,0,1});
  for(int j=0;j<512;++j)input.targets.push_back({uint32_t(1000+j),2,j*16,256,4,0,{1,2,4},0});input.compatible.assign(count*512,1);
  measure(mode==1?"cooperative_enter_512_targets":"cooperative_air_512_targets",count,[&](int){return cooperative::allocate(input).size();});
 }
 for(int count:{64,512})for(int mode:{0,1}){
  auto composition=std::make_unique<loading::Composition>();loading::Infantry people[512]{};int out[512]{};
  for(int i=0;i<count;++i)people[i]={uint32_t(0x10000+i*16),uint32_t(i+1),(i%32)*128,(i/32)*128,1,uint32_t(i%8+1),1};
  measure(mode==0?"autoload_nearest_128_carriers":"autoload_balanced_128_carriers",count,[&](int){loading::Vehicle vehicles[128]{};for(int j=0;j<128;++j)vehicles[j]={uint32_t(0x30000+j*16),uint32_t(1000+j),(j%16)*256,(j/16)*256,6,0,0,3.,0};composition->clear();return loading::assign(people,count,vehicles,128,mode,30,*composition,workspace,out,[](const auto&,const auto&){return true;});});
 }
 for(int count:{1024,8192}){
  auto w=std::make_unique<planner::World>();w->match=1;w->count=count;w->intentCount=512;planner::Planner plan;
  for(int i=0;i<count;++i){auto&a=w->actors[i];a.id=i+1;a.kind=2;a.flags=i<512?planner::Own:planner::Enemy;a.cost=100+i%10;a.x=30000+(i%100)*128;a.y=30000+(i/100)*128;a.mission=2;a.weapons[0]={2048,0,planner::GroundWeapon,0x7ff};if(i<512)w->intents[i]={a.id,30030,0};}
  measure("advanced_512_intents",count,[&](int n){w->frame=n;auto orders=plan.step(*w,AdvancedInput,200,300,90);for(auto&o:orders){plan.accepted(o,n);w->actors[o.unit-1].target=o.target;}return orders.size();});
  auto copy=std::make_unique<planner::World>();measure("world_memory_copy_no_RPM",count,[&](int n){std::memcpy(copy.get(),w.get(),sizeof(planner::World));return copy->actors[n%count].id;});
 }
 std::cout<<"]}\n";
}

#include "../../external_reader/source/immutable_atlas_plan.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
using namespace external::immutableatlas;
// Frozen Hotfix9 algorithm, used to compare allocation results and cost.
bool reference(const std::vector<Source>&input,const std::vector<Resident>&resident,Plan&out,uint64_t maximum){
 out={};std::vector<Resident>active;std::vector<size_t>indices;uint64_t total=0;
 for(const auto&s:input){if(!s.owner||(!s.data&&!s.generated)||!s.bytes||s.bytes>maximum)return false;size_t n=0;for(;n<active.size();++n)if(identical(active[n],s))break;
  if(n==active.size()){active.push_back({s.owner,s.owner.get(),0,uint32_t((s.bytes+3)/4),s.signature});total+=active.back().words;}indices.push_back(n);}
 if(total>maximum/4)return false;out.retained=resident;out.retained.erase(std::remove_if(out.retained.begin(),out.retained.end(),[](const Resident&r){return r.owner.expired();}),out.retained.end());
 bool repack=false;std::vector<bool>dirty(active.size(),false);
 for(size_t n=0;n<active.size();++n){const auto&original=input[std::find(indices.begin(),indices.end(),n)-indices.begin()];auto found=std::find_if(out.retained.begin(),out.retained.end(),[&](const Resident&r){return identical(r,original);});
  if(found!=out.retained.end()){active[n].offset=found->offset;continue;}
  std::sort(out.retained.begin(),out.retained.end(),[](const Resident&a,const Resident&b){return a.offset<b.offset;});uint64_t offset=0;
  for(const auto&r:out.retained){if(offset+active[n].words<=r.offset)break;offset=uint64_t(r.offset)+r.words;}
  if(offset+active[n].words>maximum/4){repack=true;break;}active[n].offset=uint32_t(offset);out.retained.push_back(active[n]);dirty[n]=true;
 }
 if(repack){out.retained=active;uint32_t offset=0;for(size_t n=0;n<active.size();++n){active[n].offset=out.retained[n].offset=offset;offset+=active[n].words;dirty[n]=true;}}
 std::vector<bool>seen(active.size(),false);for(size_t i=0;i<input.size();++i){const size_t n=indices[i];out.offsets.push_back(active[n].offset);if(dirty[n]&&!seen[n]){seen[n]=true;out.dirty.push_back(i);if(!input[i].generated)out.bytes+=uint64_t(active[n].words)*4;}}return true;
}
int main(){
 std::mt19937 random(390710);std::vector<std::shared_ptr<std::vector<uint32_t>>>owners;
 for(size_t n=0;n<2048;++n)owners.push_back(std::make_shared<std::vector<uint32_t>>(16+n%16,0xff00ff00u));
 std::vector<Resident>oldResident,newResident;unsigned passed=0;
 for(unsigned epoch=0;epoch<300;++epoch){std::vector<Source>input;
  for(unsigned i=0;i<1000;++i){unsigned n=random()%2048;auto owner=owners[n];input.push_back({owner,owner->data(),owner->size()*4,(n%4)==0,uint64_t(epoch%3)});}
  Plan oldPlan,newPlan;bool a=reference(input,oldResident,oldPlan,256*1024),b=build(input,newResident,newPlan,256*1024);assert(a==b&&a);
  assert(oldPlan.offsets==newPlan.offsets&&oldPlan.dirty==newPlan.dirty&&oldPlan.bytes==newPlan.bytes);
  oldResident=std::move(oldPlan.retained);newResident=std::move(newPlan.retained);++passed;
 }
 // Reusing a raw address with a different owner or generation cannot reuse
 // old bytes. Aliasing pointers with the SAME owner still deduplicate.
 auto p=owners[0];auto different=std::shared_ptr<const void>(p.get(),[](const void*){});
 std::vector<Source>identity{{p,p->data(),64,false,1},{p,p->data(),64,false,2},{different,p->data(),64,false,1},{p,p->data(),64,false,1}};
 Plan id;assert(build(identity,{},id,1024)&&id.dirty.size()==3&&id.offsets[0]==id.offsets[3]&&id.offsets[0]!=id.offsets[2]);
 auto invalid=identity;invalid[0].bytes=2048;assert(!build(invalid,{},id,1024));
 // Crowded warm source list: CPU planner benchmark, never GPU timing/FPS.
 std::vector<Source>busy;for(auto&owner:owners)for(int duplicate=0;duplicate<2;++duplicate)busy.push_back({owner,owner->data(),owner->size()*4,false,0});
 Plan warm;assert(build(busy,{},warm,4*1024*1024));
 auto bench=[&](auto fn){std::vector<double> times;for(unsigned n=0;n<15;++n){Plan plan;auto start=std::chrono::steady_clock::now();assert(fn(busy,warm.retained,plan,4*1024*1024));assert(plan.dirty.empty());times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}std::sort(times.begin(),times.end());return times[times.size()/2];};
 double before=bench(reference),after=bench(build);
 std::cout<<"{\"status\":\"PASS\",\"randomized_epochs\":"<<passed<<",\"sources\":"<<busy.size()<<",\"baseline_median_ms\":"<<before<<",\"optimized_median_ms\":"<<after<<",\"speedup\":"<<before/after<<",\"scope\":\"CPU atlas planner only\"}\n";
}

#include "../source/cooperative_plan.h"
#include "../source/planning_budget.h"
#include "../source/unit_task_arbiter.h"
#include <cassert>
#include <climits>
#include <map>
#include <random>
#include <set>
#include <iostream>
using namespace assist;
int main(){
 std::mt19937 rng(0x740032);unsigned ordersChecked=0;
 for(int run=0;run<30000;++run){cooperative::Input w;w.mode=1+rng()%3;int ns=rng()%65,nt=1+rng()%25;
  for(int i=0;i<ns;++i)w.units.push_back({{uint32_t(i+1),uint32_t(rng())},1,int(rng()%4096),int(rng()%4096),int(1+rng()%4)});
  for(int j=0;j<nt;++j)w.targets.push_back({uint32_t(1000+j),2,int(rng()%4096),0,int(rng()%50),0,{int(rng()%15),int(rng()%15),int(rng()%20)},int(rng()%25)});
  w.compatible.resize(ns*nt);for(auto&v:w.compatible)v=uint8_t(rng()%2);
  auto out=cooperative::allocate(w);std::set<uint32_t> seen;std::map<uint32_t,int> counts,slots;
  for(auto&o:out){assert(seen.insert(o.id).second);auto&unit=w.units[o.id-1];auto&target=w.targets[o.target-1000];assert(o.token==unit.source.token&&w.compatible[(o.id-1)*nt+o.target-1000]);
   ++counts[o.target];slots[o.target]+=unit.slots;
   if(w.mode==cooperative::Enter)assert(slots[o.target]<=target.free);
   else assert(!target.quota.maximum||counts[o.target]+target.prior<=target.quota.maximum);
   ++ordersChecked;
  }
  // A new plan may keep only its unprocessed suffix. Repeated chunks must
  // never return an identity which was in a processed prefix.
  cooperative::Request request{};request.tick=77;request.serial=9;request.match=4;
  int processed=int(out.size()?rng()%(out.size()+1):0);
  cooperative::retryRemaining(request,out.data(),int(out.size()),processed);
  assert(request.count==int(out.size())-processed&&request.tick==77&&request.match==4&&request.serial==9);
  for(int i=0;i<request.count;++i){assert(request.sources[i].id==out[i+processed].id);for(int k=0;k<processed;++k)assert(request.sources[i].id!=out[k].id);}
  if(run%5==0){w.targets[0].prior=-1;assert(cooperative::allocate(w).empty());w.targets[0].prior=INT_MAX;assert(cooperative::allocate(w).empty());}
 }
 planningbudget::Budget budget;int sends[2]{};
 for(int frame=0;frame<10000;++frame){int limit=frame%2?12:24;budget.begin(1,frame,limit);int used=0;
  for(int wake=0;wake<10;++wake){budget.begin(1,frame,24);for(int c:{int(rng()%2),int(rng()%2)}){
   while(budget.room(c,true)){budget.consume(c);++sends[c];++used;}
  }}
  assert(used==limit&&budget.used[0]==limit/2&&budget.used[1]==limit/2);
  assert(!budget.room(0,false)&&!budget.room(1,false));
 }
 using namespace taskarbiter;int pairs=0;
 for(int a=0;a<=10;++a)for(int b=0;b<=10;++b){LeaseArbiter<2> arb;Owner first=Owner(a),second=Owner(b);assert(arb.record(1,100,7,0x10000,2,first,31));
  bool allowed=first==second||priority(second)>priority(first);assert(arb.canIssue(1,100,7,0x10000,2,second)==allowed);
  assert(arb.canIssue(1,101,7,0x10000,3,second)); // new manual command version
  assert(arb.canIssue(2,100,7,0x10000,2,second)); // another round
  assert(arb.canIssue(1,146,7,0x10000,2,second)); // expired lease
  if(allowed){assert(arb.record(1,100,7,0x10000,2,second,32));arb.release(1,7,31);assert(arb.find(7)>=0);}
  ++pairs;
 }
 std::cout<<"{\"allocation_scenarios\":30000,\"orders_checked\":"<<ordersChecked<<",\"shared_budget_frames\":10000,\"budget_wakes\":100000,\"owner_pairs\":"<<pairs<<",\"seed\":7602226,\"status\":\"PASS\"}\n";
}

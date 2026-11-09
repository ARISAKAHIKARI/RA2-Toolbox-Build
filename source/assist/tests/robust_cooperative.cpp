#include "../source/cooperative_plan.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <string>
using namespace assist::cooperative;
Input base(){Input w;w.units={{{1,7},1,0,0,1},{{2,8},1,0,0,1}};w.targets={{100,2,0,0,2,0,{}}};w.compatible={1,1};return w;}
void check(const Input&w){auto a=allocate(w);std::set<uint32_t> seen;std::map<uint32_t,int> load,count;
 for(auto&o:a){assert(seen.insert(o.id).second);size_t i=0,j=0;while(i<w.units.size()&&w.units[i].source.id!=o.id)++i;while(j<w.targets.size()&&w.targets[j].id!=o.target)++j;
  assert(i<w.units.size()&&j<w.targets.size());const auto&u=w.units[i];const auto&t=w.targets[j];assert(w.compatible[i*w.targets.size()+j]&&o.token==u.source.token&&o.type==t.type);
  if(w.mode==Enter){load[o.target]+=u.slots;assert(load[o.target]<=t.free);}else{++count[o.target];auto q=bounded(t.quota);assert(!q.maximum||count[o.target]<=q.maximum);}}
 auto b=allocate(w);assert(a.size()==b.size());for(size_t i=0;i<a.size();++i)assert(a[i].id==b[i].id&&a[i].target==b[i].target);
}
int main(int argc,char**argv){std::string mode=argc>1?argv[1]:"all";
 if(mode=="all"||mode=="duplicate_source"){auto w=base();w.units[1].source=w.units[0].source;assert(allocate(w).empty());puts("PASS duplicate_source rejected");}
 if(mode=="all"||mode=="duplicate_target"){auto w=base();w.targets[0].free=1;w.targets.push_back(w.targets[0]);w.compatible.assign(4,1);assert(allocate(w).empty());puts("PASS duplicate_target rejected");}
 if(mode=="all"||mode=="overflow"){auto w=base();w.targets[0].used=INT_MAX;auto a=allocate(w);assert(a.size()==2);w.mode=AirFirst;w.units[0].slots=INT_MAX;w.units[1].slots=INT_MAX;assert(allocate(w).size()==2);puts("PASS capacity accounting avoids signed overflow");}
 if(mode=="all"||mode=="random"){std::mt19937 rng(0x6214);for(int run=0;run<10000;++run){Input w;w.mode=1+rng()%3;int ns=rng()%65,nt=rng()%25;
  for(int i=0;i<ns;++i)w.units.push_back({{uint32_t(i+1),uint32_t(rng())},1,int(rng()%2049)-1024,int(rng()%2049)-1024,int(rng()%132)-2});
  for(int j=0;j<nt;++j)w.targets.push_back({uint32_t(1000+j),2,int(rng()%2049)-1024,0,int(rng()%260)-2,run%2?INT_MAX:0,{int(rng()%20)-2,int(rng()%20)-2,int(rng()%20)-2}});
  for(int i=0;i<ns*nt;++i)w.compatible.push_back(rng()%2);check(w);
 }auto w=base();w.units.resize(MaxSources+1);w.compatible.resize(MaxSources+1);assert(allocate(w).empty());w=base();w.compatible.clear();assert(allocate(w).empty());
 w=base();w.units[0].x=INT_MIN;w.units[1].y=INT_MAX;assert(allocate(w).empty());
 w={};for(int i=0;i<MaxSources;++i){w.units.push_back({{uint32_t(i+1),1},1,i,0,128});w.targets.push_back({uint32_t(1000+i),2,i,0,128,INT_MAX,{}});}w.compatible.assign(size_t(MaxSources)*MaxTargets,1);assert(allocate(w).size()==MaxSources);check(w);
 puts("PASS 10000 seeded allocation cases plus current source/target capacity boundary: capacity, identity, compatibility, quotas, determinism and limits");}
}

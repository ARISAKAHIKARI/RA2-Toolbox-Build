#include "../source/autoload.h"
#include <memory>
#include <vector>
#include <random>
#include <cassert>
#include <iostream>
using namespace assist::loading;
// Exhaustive greedy specification, independent of ranked cursor optimization.
std::vector<int> reference(const std::vector<Infantry>&p,std::vector<Vehicle> v,int mode,Composition c){std::vector<int> out(p.size(),-1),type(p.size());for(size_t i=0;i<p.size();++i)type[i]=c.index(p[i].type);bool changed=true;
 while(changed){changed=false;for(int t=0;t<(mode?c.count:1);++t){int bi=-1,bj=-1,bn=0,bp=0;int64_t bd=INT64_MAX;
  for(size_t i=0;i<p.size();++i)for(size_t j=0;j<v.size();++j){if(out[i]>=0||(mode&&type[i]!=t)||p[i].slots<=0||freeSlots(v[j])<p[i].slots||v[j].sizeLimit<p[i].slots||!coordinate(p[i].x)||!coordinate(v[j].x)||(p[i].id^v[j].id)%7==0)continue;
   int64_t dx=int64_t(p[i].x)-v[j].x,dy=int64_t(p[i].y)-v[j].y,d=dx*dx+dy*dy;if(d>int64_t(7680)*7680)continue;int n=mode?c.numbers[j][t]:0,pass=mode?v[j].passengers:0;
   if(bi<0||n<bn||(n==bn&&(pass<bp||(pass==bp&&(d<bd||(d==bd&&(p[i].id<p[bi].id||(p[i].id==p[bi].id&&v[j].id<v[bj].id)))))))){bi=i;bj=j;bn=n;bp=pass;bd=d;}
  }if(bi>=0){out[bi]=bj;v[bj].reserved+=p[bi].slots;++v[bj].passengers;c.add(bj,p[bi].type);changed=true;}
 }}return out;}
int main(){static assist::loading::Workspace workspace;std::mt19937 rng(20261014);auto c=std::make_unique<Composition>();for(int trial=0;trial<500;++trial){int ns=rng()%65,nv=rng()%16+1;std::vector<Infantry> p(ns);std::vector<Vehicle> v(nv);c->clear();
 for(int j=0;j<nv;++j){v[j]={uint32_t(0x50000+j*16),uint32_t(1000+j),int(rng()%16000),int(rng()%16000),int(rng()%8+1),int(rng()%2),int(rng()%2),double(rng()%4+1),int(rng()%3)};for(int t=1;t<=5;++t)if(rng()%3==0)c->add(j,t);}
 for(int i=0;i<ns;++i)p[i]={uint32_t(0x10000+i*16),uint32_t(i+1),int(rng()%16000),int(rng()%16000),int(rng()%4+1),uint32_t(rng()%7+1),1};if(ns&&trial%11==0)p[0].x=INT32_MAX;
 for(int mode:{0,1}){auto carriers=v;auto cc=std::make_unique<Composition>(*c);std::vector<int> actual(ns);int calls=0;assign(p.data(),ns,carriers.data(),nv,mode,30,*cc,workspace,actual.data(),[&](const auto&a,const auto&b){++calls;return (a.id^b.id)%7!=0;});assert(actual==reference(p,v,mode,*c)&&calls<=ns*nv);}}
 std::cout<<"PASS: 500 seeded mixed size/capacity/composition worlds match exhaustive assignment in both modes; each compatibility pair queried at most once\n";
}

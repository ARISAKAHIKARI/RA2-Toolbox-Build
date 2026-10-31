#include "../source/sequence.h"
#include "../source/catalog.h"
#include <set>
#include <cassert>
#include <cstdio>
int main(){
 using namespace hooktest;Sequence s;s.start(0,100);assert(!s.tick(1000000));
 s.installed(1000000);assert(!s.tick(1029999));assert(s.tick(1030000));
 assert(!s.tick(1090000)&&s.index==0);assert(s.normal());
 assert(!s.restored(false,true,1090000)&&s.index==0);assert(s.restored(true,true,1090000)&&s.index==1);
 s.installed(1100000);s.stop(true);assert(!s.normal()&&!s.restored(true,true,1200000)&&s.index==1);
 s.start(3,0);s.installed(0);assert(s.tick(30000));assert(s.normal());assert(s.restored(true,false,30001));assert(s.phase==Phase::Complete);
 s.start(0,100000);s.installed(100000);assert(!s.tick(90000));
 s.pause(20000);assert(!s.tick(130000));assert(s.tick(150000));
 auto all=allSites();assert(all.size()==51);unsigned active=0,dormantCount=0,denied=0;
 for(auto a:all){assert(covered(a)!=dormant(a));if(covered(a))++active;else ++dormantCount;}
 for(const auto&stage:steps()){std::set<uint32_t>unique;for(auto a:stage.sites){assert(experimentalpatch::size(a)&&!dormant(a));assert(unique.insert(a).second);}}
 for(uint32_t a=0x400000;a<0x800000;++a)if(assist::hookregistry::ownerAt(a)!=assist::hookregistry::Owner::Unknown&&!experimentalpatch::size(a))++denied;
 assert(active==48&&dormantCount==3&&denied==14&&steps().size()==21);
 puts("PASS: all 51 native sites accounted for: 48 scheduled (camera conditional), 3 product-disabled; 14 denied sites never scheduled; 21 stages");
 puts("PASS: timing starts after verified installation; answer required; failed restoration and OOS cannot advance; final stage and clock regression");
}

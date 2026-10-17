#include "../source/planner_index.h"
#include <cassert>
#include <memory>
#include <random>
#include <set>
#include <iostream>
using namespace assist::planner;
int main(){
 auto world=std::make_unique<World>();auto lookup=std::make_unique<ActorLookup>();auto grid=std::make_unique<EnemyGrid>();
 std::mt19937 random(20261014);
 world->count=MaxActors;
 for(int i=0;i<world->count;++i){auto&a=world->actors[i];a.id=uint32_t(1+i*16384);a.x=int(random()%300000)-150000;a.y=int(random()%300000)-150000;lookup->add(a);grid->add(i,a);}
 for(int i=0;i<world->count;++i)assert(lookup->find(world->actors[i].id)==&world->actors[i]);
 assert(!lookup->find(2));lookup->clear();assert(!lookup->find(1));
 for(int query=0;query<80;++query){int x=int(random()%240000)-120000,y=int(random()%240000)-120000,range=query==79?500000:1+int(random()%12000);
  std::set<int> candidates,expected,actual;
  grid->nearby(x,y,range,[&](int i){assert(candidates.insert(i).second);auto&a=world->actors[i];int64_t dx=int64_t(x)-a.x,dy=int64_t(y)-a.y;if(dx*dx+dy*dy<=int64_t(range)*range)actual.insert(i);});
  for(int i=0;i<world->count;++i){auto&a=world->actors[i];int64_t dx=int64_t(x)-a.x,dy=int64_t(y)-a.y;if(dx*dx+dy*dy<=int64_t(range)*range)expected.insert(i);}
  assert(actual==expected);
 }
 grid->clear();int visits=0;grid->nearby(0,0,500000,[&](int){++visits;});assert(!visits);
 std::cout<<"PASS: actor collisions/clear; spatial lookup matches complete distance oracle including negative coordinates, bucket boundaries and long-range fallback\n";
}

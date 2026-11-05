#include "../source/single_player_fps.h"
#include "../../shared/source/hardware_slot_budget.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
using namespace assist;
int main(){
 assert(singlefps::valid(0)&&singlefps::valid(10)&&singlefps::valid(240));
 assert(!singlefps::valid(1)&&!singlefps::valid(9)&&!singlefps::valid(241));
 for(int s=-1;s<10;++s)assert(singlefps::session(s)==(s==0||s==5));
 for(unsigned rate:{10u,30u,40u,45u,50u,60u,144u,240u}){
  singlefps::Clock c;const uint64_t hz=10000003,start=1234567;
  uint64_t due=start;
  for(unsigned n=0;n<rate*600;++n){due=c.deadline(due,rate,hz);assert(due==start+uint64_t(n)*hz/rate);}
  const auto late=due+hz*10;assert(c.deadline(late,rate,hz)==late);
  assert(c.deadline(late,rate,hz)==late+hz/rate);
  assert(c.deadline(late,rate==40?45:40,hz)==late);
  assert(c.deadline(late,0,hz)==late&&c.rate==0);
 }
 const auto b=slotbudget::pack(1,2,1,slotbudget::Full|singlefps::Tag);
 assert(slotbudget::known(b)&&slotbudget::tags(b)==(slotbudget::Full|singlefps::Tag));
 assert(slotbudget::allows(b,slotbudget::Full|singlefps::Tag|slotbudget::Map));
 assert(!slotbudget::allows(b,slotbudget::Full|singlefps::Tag|slotbudget::Map|slotbudget::Control));
 puts("PASS: 10–240 FPS rational deadlines, late/pause/reset, SP-only session gate, shared slot accounting");
}

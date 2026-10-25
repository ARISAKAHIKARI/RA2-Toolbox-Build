#include "../source/frame_clock.h"
#include <cassert>
#include <iostream>
int main(){
 for(int rate:{5,60,144,165,240,500}){
  umframe::Schedule clock;uint64_t now=0;int frames=0;
  while(now<10000000){auto next=clock.step(now,true,rate);frames+=next.frame;assert(next.wait>0);now+=next.wait;}
  assert(frames==rate||frames==rate+1);
 }
 umframe::Schedule clock;
 auto next=clock.step(0,false,60);assert(!next.frame);
 next=clock.step(100,true,5);assert(next.frame&&next.wait==2000000);
 next=clock.step(200,true,144);assert(next.frame&&next.wait==10000000/144);
 next=clock.step(201,true,144);assert(!next.frame);
 // A slow UI consumes one pending frame, not a queue of missed presentations.
 umframe::Pending pending;assert(pending.claim());
 for(int i=0;i<1000;++i)assert(!pending.claim());
 pending.complete();assert(pending.claim());pending.complete();
 next=clock.step(20000000,true,240);assert(next.frame&&next.wait==10000000/240);
 next=clock.step(20000001,true,240);assert(!next.frame);
 next=clock.step(21000000,false,240);assert(!next.frame);
 next=clock.step(22000000,true,60);assert(next.frame&&next.wait==10000000/60);
 next=clock.step(50000000000000ull,true,500);assert(next.frame&&next.wait==20000);
 std::cout<<"PASS statistics presentation: 5/60/144/165/240/500 Hz, immediate rate/focus changes, one pending frame through slow presentation, no catch-up burst\n";
}

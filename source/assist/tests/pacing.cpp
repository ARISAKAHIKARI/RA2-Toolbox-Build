#include "../source/pacing.h"
#include <cassert>
#include <iostream>
using assist::pacing::Clock;
int main(){
 for(int fps:{5,30,60,120,500}){Clock clock;int controls=0,frames=0;uint64_t now=0;
  while(now<10000000){auto next=clock.step(now,true,fps);controls+=next.control;frames+=next.frame;assert(next.wait>0);now+=next.wait;}
  assert(controls==63);assert(frames==fps||frames==fps+1);
 }
 Clock clock;auto next=clock.step(0,false,5);assert(next.control&&!next.frame&&next.wait==160000);
 next=clock.step(20000,true,5);assert(next.frame&&!next.control);
 next=clock.step(160000,true,5);assert(next.control&&!next.frame);
 next=clock.step(170000,true,500);assert(next.frame&&!next.control&&next.wait==20000);
 // A 2-second delay coalesces work; it never schedules 1000 stale paints.
 next=clock.step(20000000,true,500);assert(next.control&&next.frame&&next.wait==20000);
 next=clock.step(20000001,true,500);assert(!next.control&&!next.frame);
 Clock longRunning;next=longRunning.step(50000000000000ull,true,30);assert(next.control&&next.frame);
 next=longRunning.step(50000000160000ull,true,30);assert(next.control&&!next.frame);
 std::cout<<"PASS: 5/30/60/120/500 FPS keep independent 16ms controls, immediate rate changes, no catch-up burst and long-running monotonic time\n";
}

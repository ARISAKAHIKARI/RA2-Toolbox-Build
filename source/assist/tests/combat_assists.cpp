#include "../source/burst_compute.h"
#include "../source/dodge_compute.h"
#include "../source/planning_budget.h"
#include <cassert>
#include <iostream>
using namespace assist;
int main(){
 burstcompute::Source b;b.frame=100;b.target=b.currentTarget=10;b.mission=1;b.burst=2;b.index=1;b.inRange=b.eligible=1;b.fireStamp=99;
 assert(burstcompute::decide(b).mission==13);b.burst=1;assert(burstcompute::decide(b).mission<0);b.burst=4;b.index=3;assert(burstcompute::decide(b).mission==13);
 b.index=4;assert(burstcompute::decide(b).mission<0);b.index=0;assert(burstcompute::decide(b).mission<0);b.index=1;b.moving=1;assert(burstcompute::decide(b).done);b.moving=0;
 b.phase=1;b.sentFrame=100;assert(burstcompute::decide(b).mission<0);b.frame=101;b.currentTarget=0;b.mission=13;b.eligible=0;assert(burstcompute::decide(b).mission==1); // finish existing cycle on key release
 b.inRange=0;assert(burstcompute::decide(b).done);b.inRange=1;b.currentTarget=99;assert(burstcompute::decide(b).done);
 b.phase=2;b.sentFrame=101;b.frame=102;b.currentTarget=b.target;b.mission=1;b.previousFire=b.fireStamp;assert(!burstcompute::decide(b).done);++b.fireStamp;assert(burstcompute::decide(b).done);
 dodgecompute::Source d;d.frame=10;d.count=1;d.freshThreat=1;
 assert(dodgecompute::decide(d).scatter);d.busy=1;assert(!dodgecompute::decide(d).scatter);d.busy=0;
 d.active=1;d.sentFrame=10;
 for(int f=10;f<310;++f){d.frame=f;auto out=dodgecompute::decide(d);assert(!out.scatter&&!out.release);} // every frame can contain a fresh projectile
 d.frame=310;assert(dodgecompute::decide(d).release);d.busy=1;assert(!dodgecompute::decide(d).release);
 d.busy=0;d.observed=1;d.quietFrame=309;assert(!dodgecompute::decide(d).release);d.frame=311;assert(dodgecompute::decide(d).release);
 d.active=0;d.freshThreat=0;assert(!dodgecompute::decide(d).scatter);d.freshThreat=1;d.count=0;assert(!dodgecompute::decide(d).scatter);
 planningbudget::Budget budget;budget.begin(1,100,24);for(int n=0;n<5;++n){assert(budget.roomMask(n,31)==4);for(int k=0;k<4;++k)budget.consume(n);assert(budget.roomMask(n,31)==0);}assert(budget.total()==20);budget.begin(1,101,12);assert(budget.total()==0&&budget.roomMask(4,31)==2);
 std::cout<<"PASS burst phases, ordinary/elite parameters, hold release, stale target, native scatter, delayed execution, turning/movement exclusion and settled release and five-family budget\n";
}

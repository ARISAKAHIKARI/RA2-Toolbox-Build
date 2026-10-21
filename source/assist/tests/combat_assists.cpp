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
 dodgecompute::Source d;d.x=d.y=1000;d.frame=10;d.count=1;d.threats[0]={10,0,1100,1000,1};d.cells=3;d.options[0]={1001,750,1000};d.options[1]={1002,1250,1000};d.options[2]={1003,750,1000};
 auto out=dodgecompute::decide(d);assert(out.mission==2&&out.destination==1001);d.count=3;d.threats[1]={11,0,750,1000,1};d.threats[2]={12,0,1250,1000,1};assert(dodgecompute::decide(d).mission<0); // cannot move toward the second threat
 d.job=1;d.sentFrame=10;d.count=1;d.frame=11;assert(dodgecompute::decide(d).mission<0);d.frame=22;assert(dodgecompute::decide(d).mission==2);
 d.count=0;d.restoreMission=1;d.restoreTarget=55;out=dodgecompute::decide(d);assert(out.done&&out.mission==1&&out.target==55);
 d.count=1;d.moves=3;assert(dodgecompute::decide(d).mission<0);d.frame=71;assert(dodgecompute::decide(d).done);
 planningbudget::Budget budget;budget.begin(1,100,24);for(int n=0;n<5;++n){assert(budget.roomMask(n,31)==4);for(int k=0;k<4;++k)budget.consume(n);assert(budget.roomMask(n,31)==0);}assert(budget.total()==20);budget.begin(1,101,12);assert(budget.total()==0&&budget.roomMask(4,31)==2);
 std::cout<<"PASS burst phases, ordinary/elite parameters, hold release, stale target, evasion direction, conflicting threats, bounded retries and five-family budget\n";
}

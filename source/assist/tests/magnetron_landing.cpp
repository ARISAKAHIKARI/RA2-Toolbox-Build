#include "../source/magnetron_drop.h"
#include <cassert>
#include <iostream>
int main(){using namespace assist::magnetdrop;int cases=0;
 for(int speed=0;speed<8;++speed)for(int land:{0,1,2,3,6,7}){
  Ground g;g.lifted=g.enemy=true;g.height=256;g.speed=speed;g.zone=0;g.land=land;
  g.mobilityKnown=true;g.passable=land!=3;
  bool expect=speed==4?false:land==2?speed!=3&&speed!=5&&speed!=6&&speed!=7:
   speed==5?true:speed==7?land!=6:land==3;
  assert(opportunity(g)==expect);++cases;
  g.bridge=true;assert(!opportunity(g));g.bridge=false;g.height=0;assert(!opportunity(g));
 }
 for(int zone:{3,4,5})for(int land:{0,2,7}){Ground g;g.lifted=g.enemy=true;g.height=256;g.speed=1;g.zone=zone;g.land=land;g.mobilityKnown=g.passable=true;
  assert(!opportunity(g));g.terrain=true;assert(opportunity(g));g.terrain=false;g.groundVehicle=true;assert(opportunity(g));++cases;}
 Ground unknown;unknown.lifted=unknown.enemy=true;unknown.height=256;unknown.land=3;assert(!opportunity(unknown));
 std::cout<<"PASS "<<cases<<" water/land/cliff/beach/amphibious combinations; bridge, altitude and unknown-type exclusions\n";
}

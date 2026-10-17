#include "../source/native_orders.h"
#include <cassert>
using namespace nativeorders;
int main(){
 assert(value(1000,0)==1000&&value(1000,1)==2000&&value(1000,2)==3000);
 assert(value(1000,2)>value(2500,0)&&value(1000,1,400,800)==4000);
 assert(specialMission(true,false,false,9)==8&&specialMission(false,true,false,3)==7);
 assert(specialMission(false,false,true,16)==17&&specialMission(false,false,true,5)==1);
 assert(specialMission(true,false,false,3)==-1&&specialMission(false,true,false,9)==-1);
 assert(!fireEligible(5)&&!fireEligible(8)&&fireEligible(3));
 Progress p;p.begin(0,0,100);p.update(0,0,101,2,5000,5000,90);assert(!p.hold);
 for(int f=102;f<1000;++f)p.combat(0,0,f);p.update(0,0,1000,2,5000,5000,90);assert(!p.hold);
 p.update(0,0,1089,2,5000,5000,90);assert(p.hold);
 p={};p.begin(0,0,0);p.update(32,0,1,2,5000,5000,90);p.update(32,0,12,5,5000,5000,90);assert(p.hold);
 p={};p.begin(0,0,0);p.update(5000,5000,2,2,5000,5000,90);assert(p.hold);
 p.combat(5000,5000,100);p.begin(5000,5000,101);assert(p.hold);
}

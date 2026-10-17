#include "../source/autoload.h"
#include <cassert>
#include <limits>
using namespace assist::loading;
int main(){
 assert(slots(1)==1&&slots(2.1)==3&&slots(0)==0&&slots(-1)==0);
 assert(slots(std::numeric_limits<double>::quiet_NaN())==0);
 assert(slots(std::numeric_limits<double>::infinity())==0);
 Vehicle v[]={{100,10,0,0,1,0,0},{200,20,20,0,3,1,0},{300,30,30,0,5,0,0}};
 auto allow=[](const Infantry&,const Vehicle&){return true;};
 Infantry p{1,1,1,0,1};assert(nearest(p,v,3,allow)==0);++v[0].reserved;
 assert(nearest(p,v,3,allow)==1);v[1].reserved+=2;
 assert(nearest(p,v,3,allow)==2);
 v[2].used=5;assert(nearest(p,v,3,allow)==-1);
 v[0].reserved=0;v[1].reserved=0;p.slots=2;assert(nearest(p,v,3,allow)==1);
 p.slots=1;assert(nearest(p,v,3,[](const Infantry&,const Vehicle&v){return v.id!=10;})==1);
 v[0].x=-10;v[1].x=10;p.x=0;assert(nearest(p,v,2,allow)==0);
 v[0].id=30;assert(nearest(p,v,2,allow)==1);
 p.x=std::numeric_limits<int>::max();assert(nearest(p,v,3,allow)==-1);
 Ledger<2> l;assert(l.add({1,7,2,8,1,10}));assert(l.contains(7));
 assert(!l.add({1,7,3,9,1,11}));assert(l.add({2,9,3,10,1,10}));
 assert(!l.add({3,11,4,12,1,10}));l.erase(0);assert(!l.contains(7)&&l.contains(9));
 l.clear();assert(l.count==0);
}

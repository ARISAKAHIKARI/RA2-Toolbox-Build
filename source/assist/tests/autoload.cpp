#include "../source/autoload.h"
#include <cassert>
#include <limits>
using namespace assist::loading;
static void pickupAssignment(){
 static Workspace workspace;
 const Infantry people[]={{1,1,20*256,10*256,1,100},{2,2,21*256,10*256,1,100},
                          {3,3,22*256,10*256,2,200},{4,4,23*256,10*256,1,200}};
 const Vehicle selected{10,10,10*256,10*256,3,1,0,1},nearby{11,11,22*256,10*256,3};
 const auto allowed=[](const Infantry&,const Vehicle&){return true;};
 for(int mode:{0,1}){
  Vehicle fleet[]={selected,nearby};Composition composition{};int assignment[4]{};
  assert(assign(people,4,fleet,2,mode,30,composition,workspace,assignment,allowed,1)==4);
  assert(assignment[0]==0&&assignment[2]==1&&fleet[0].reserved==2&&fleet[1].reserved==3);
  assert(assignment[1]!=assignment[3]); // the other single seat goes to a selected carrier
  fleet[0]=selected;fleet[1]=nearby;composition.clear();
  assert(assign(people,4,fleet,2,mode,7,composition,workspace,assignment,allowed,1)==2);
  assert(fleet[0].reserved==0&&fleet[1].reserved==3);
 }
}
static void rendezvous(){
 auto passable=[](int,int){return true;};
 int troopersX[]={20*256,21*256,50*256},troopersY[]={10*256,10*256,10*256};
 assert(meetingCell(10*256,10*256,troopersX,troopersY,3,false,passable)==10017);
 assert(meetingCell(10*256,10*256,troopersX,troopersY,3,false,[](int x,int y){return x==17&&y==9;})==9017);
 assert(!meetingCell(10*256,10*256,troopersX,troopersY,3,false,[](int,int){return false;}));
 int splitX[]={4*256,35*256},splitY[]={10*256,10*256};
 assert(!meetingCell(10*256,10*256,splitX,splitY,2,false,passable));
 assert(meetingCell(10*256,10*256,splitX,splitY,2,true,passable));
 int farX[]={150*256},farY[]={10*256};
 assert(meetingCell(10*256,10*256,farX,farY,1,false,passable)==10022);
 assert(!meetingCell(10*256,10*256,farX,farY,1,false,[](int x,int){return x>22;}));
 Reservation job{1,1,2,2,1,10};job.pickup=true;job.vehicleManual=55;job.pickupAt=0xfffffff0;
 assert(!pickupCancelled(job,55,0x20)&&pickupCancelled(job,56,0x20));
 assert(!pickupCancelled(job,55,job.pickupAt+PickupWindowMs-1));
 assert(pickupCancelled(job,55,job.pickupAt+PickupWindowMs));
 job.approachAddress=0x10000;
 assert(!takenOver(job,{2,-1,0,job.approachAddress},0));
 assert(takenOver(job,{2,-1,0,0x10004},0));
 job.lastSent=10;
 assert(!pickupCancelled(job,55,job.pickupAt+PickupWindowMs));
}
int main(){
 pickupAssignment();rendezvous();
 assert(!retryDue(1132,true,0,1000,1133,false));
 assert(retryDue(1133,true,0,1000,1133,false));
 assert(!retryDue(1499,false,1000,1000,1000,false));
 assert(retryDue(1500,false,1000,1000,1000,false));
 assert(!retryDue(2999,false,1000,1000,1000,true));
 assert(retryDue(3000,false,1000,1000,1000,true));
 assert(!retryDue(1099,false,1000,1000,1100,false));
 assert(retryDue(0x20,false,0xfffffe28u,0xfffffe28u,0xfffffff0u,false));

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

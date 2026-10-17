#include "../source/order_policy.h"
#include <cassert>
using namespace assist::orderpolicy;
int main(){
 assert(mission(true,true,true,7,false,true,true,true)==Reject::None);
 assert(mission(false,true,true,7,false,true,true,true)==Reject::NoActor);
 assert(mission(true,false,true,7,false,true,true,true)==Reject::ForeignActor);
 assert(mission(true,true,false,7,false,true,true,true)==Reject::DeadActor);
 assert(mission(true,true,true,1,false,true,false,true)==Reject::BadTarget);
 assert(mission(true,true,true,1,true,false,false,true)==Reject::BadTarget);
 assert(mission(true,true,true,7,false,true,true,false)==Reject::BadDestination);
 assert(objectEvent(0x15,true,true,true,true,true)==Reject::None);
 assert(objectEvent(0x15,true,true,false,true,false)==Reject::UnauthorizedOwner);
 assert(objectEvent(0x15,true,true,true,false,false)==Reject::UnauthorizedOwner);
 assert(objectEvent(0x16,true,true,false,true,true)==Reject::UnauthorizedOwner);
 assert(objectEvent(0x16,true,true,true,false,true)==Reject::UnauthorizedOwner);
 assert(objectEvent(0x16,true,true,false,true,false)==Reject::None); // offline experiment only
 assert(objectEvent(0x55,true,true,true,true,false)==Reject::InvalidEvent);
 assert(missionWithTargetTag(true,true,true,1,11,128128,false,true,false,false)==Reject::None);
 assert(missionWithTargetTag(true,true,true,1,11,128128,false,false,false,false)==Reject::BadTarget);
 assert(productionEvent(true,false,false)==Reject::None);
 assert(productionEvent(true,true,false)==Reject::UnauthorizedOwner);
 assert(superWeaponEvent(true,true,true,0)==Reject::None);
}

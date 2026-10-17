#include "../source/session_epoch.h"
#include <cassert>
using namespace assist::sessionepoch;
int main(){
 Tracker t;assert(t.update(false,0,0,0)==Transition::None);
 assert(t.update(true,0x4000,3,50)==Transition::Start&&t.epoch==1);
 assert(t.update(true,0x4000,3,50)==Transition::None);
 assert(t.update(true,0x4000,3,51)==Transition::None);
 assert(t.update(true,0x4000,4,52)==Transition::ModeChanged&&t.epoch==2);
 assert(t.update(true,0x5000,4,53)==Transition::HouseChanged&&t.epoch==3);
 assert(t.update(true,0x5000,4,1)==Transition::FrameRestart&&t.epoch==4);
 assert(t.update(false,0,0,0)==Transition::Stop);
 assert(t.update(true,0x5000,4,1)==Transition::Start&&t.epoch==5);
}

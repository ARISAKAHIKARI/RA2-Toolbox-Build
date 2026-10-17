#include <cassert>
#include "../source/object_event_backend.h"
using namespace assist::objectqueue;
int main(){
 Capability c{};
 assert(admit(Backend::ConstructAndEnqueue,c)==Result::DefaultEventClass);
 assert(admit(Backend::ClickedEvent,c)==Result::UnsupportedSession);
 c.multiplayer=true;
 assert(admit(Backend::ClickedEvent,c)==Result::InvalidEvent);
 c.validEvent=true;
 assert(admit(Backend::ClickedEvent,c)==Result::InvalidActor);
 c.actorOwnedAlive=true;
 assert(admit(Backend::ClickedEvent,c)==Result::SignatureMismatch);
 c.signature=c.vtableValid=true;
 assert(admit(Backend::ClickedEvent,c)==Result::HookConflict);
 c.unhooked=true;
 assert(admit(Backend::ClickedEvent,c)==Result::WrongThread);
 c.sameGameThread=true;
 assert(admit(Backend::ClickedEvent,c)==Result::NotReady);
 c.ready=true;
 assert(admit(Backend::ClickedEvent,c)==Result::QueueFull);
 c.hasRoom=true;
 assert(admit(Backend::ClickedEvent,c)==Result::Appended);
 assert(ClickedEventVtableOffset==0x374 && ClickedEventVanilla==0x6FFE00);
 assert(appended(4,127,5,0));
 assert(appended(10,30,11,31));
 assert(!appended(127,127,127,127));
 assert(!appended(4,8,4,8)); // returned true, but dropped
 assert(!appended(4,8,5,8)); // count changed without tail move
 assert(!appended(128,8,129,9));
 return 0;
}

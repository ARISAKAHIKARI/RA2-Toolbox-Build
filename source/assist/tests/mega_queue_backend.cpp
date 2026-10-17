#include <cassert>
#include <cstdint>
#include "../source/mega_queue_backend.h"
using namespace assist::megaqueue;
int main(){
 Capability cap{};
 assert(admit(Backend::ConstructAndEnqueue,cap,0,0)==Result::DefaultEventClass);
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::UnsupportedSession);
 cap.isSingleplayer=true;
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::UnsupportedSession);
 cap.isSingleplayer=false;cap.isMultiplayer=true;
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::SignatureMismatch);
 cap.engineSignature=true;
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::ConflictingHook);
 cap.knownOriginalOrOwnedHook=true;
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::NotReady);
 cap.timingCallbackExecutable=true;
 assert(admit(Backend::GameQueueMegaMission,cap,17,0)==Result::UnsupportedArguments);
 assert(admit(Backend::GameQueueMegaMission,cap,0,0x34)==Result::UnsupportedArguments);
 assert(admit(Backend::GameQueueMegaMission,cap,0,0)==Result::Queued);
 static_assert(sizeof(Target)==8);
 Target a{123,0x34,{0,0,0}};
 Target b{321,11,{0,0,0}};
 assert(a.value==123&&a.tag==0x34&&b.value==321&&b.tag==11);
 assert(appended(5,127,6,0));
 assert(!appended(5,127,5,127));
 assert(!appended(127,127,127,127));
 return 0;
}

#pragma once
#include <stdint.h>
#include "scheduler_policy.h"

// Only selects WHICH existing callback may run the single game-thread
// GuardedAssistTick(). It never builds or queues game EventClass messages.
// Business producers submit requests to the existing Shared request channels;
// the selected tick consumes them through the existing CommandGateway.
namespace dispatchrouter {
enum class Trigger : uint8_t { Message, GameFrame };
struct State {
 bool bridgeAttached;
 scheduler::Mode requested;
 scheduler::Status status;
 bool frameInstalled;
 bool frameValidated;
 bool matchingGameThread;
 uint32_t now;
 uint32_t frameCallbackAt;
};

// Strict fail closed for the experimental hook. Message continues to own
// scheduling until a live frame callback has been verified on the game thread.
inline Trigger owner(const State& state){
 const bool verifiedFrame=state.bridgeAttached &&
   state.requested==scheduler::GameFrame &&
   state.frameInstalled && state.frameValidated &&
   state.matchingGameThread &&
   state.status==scheduler::FrameActive &&
   uint32_t(state.now-state.frameCallbackAt)<=250u;
 return verifiedFrame?Trigger::GameFrame:Trigger::Message;
}
inline bool accepts(Trigger invoked,const State& state){return owner(state)==invoked;}
} // namespace dispatchrouter

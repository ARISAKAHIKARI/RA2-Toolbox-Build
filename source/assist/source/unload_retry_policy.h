#pragma once
#include <cstdint>
namespace assist { namespace unloadretry {
// A scheduled Ctrl+D is NOT an engine event. Do not consume its one-shot
// Deploy entitlement merely because admission was attempted. Only a successful
// native enqueue (2) or a complete native-first enrolment (1) changes the
// durable carrier state. Back-pressure (-6) must stay retryable.
enum class Outcome:uint8_t {Deferred,QueuedDeploy,Enrolled,Finished};
inline Outcome classify(int result){
 return result==2?Outcome::QueuedDeploy:
        result==1?Outcome::Enrolled:
        result==0?Outcome::Finished:Outcome::Deferred;
}
inline bool committed(Outcome o){return o==Outcome::QueuedDeploy||o==Outcome::Enrolled;}
inline bool retain(Outcome o){return o==Outcome::Deferred||o==Outcome::QueuedDeploy;}
inline bool maySendInitialDeploy(bool acknowledged){return !acknowledged;}
}} // namespace assist::unloadretry

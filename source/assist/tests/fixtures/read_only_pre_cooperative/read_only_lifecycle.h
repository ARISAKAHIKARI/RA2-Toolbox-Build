#pragma once
#include <cstdint>

namespace assist { namespace readonly {
// The Host publishes a durable retirement request before it can resume
// commands. A quick off/on cannot erase the request before Runtime observes it.
template<class Shared,class Barrier>void request(Shared&s,Barrier barrier){
 if(s.readOnly)return;
 s.readOnly=1;barrier();
 uint32_t next=s.readOnlyRequest+1;if(!next)++next;
 s.readOnlyRequest=next;barrier();
}
template<class Shared>bool settled(const Shared&s){
 return s.readOnlyRequest==s.readOnlyAck;
}
template<class Shared>bool commands(const Shared*s){
 return s&&!s->readOnly&&settled(*s);
}

enum class Admission { Commands, Retiring, Observe };
struct RuntimeState {
 bool active=false,pending=false;uint32_t serial=0;
 template<class Shared,class Cancel,class Retire,class Barrier>
 Admission service(Shared&s,Cancel cancel,Retire retire,Barrier barrier){
  const uint32_t requested=s.readOnlyRequest;
  if((s.readOnly||requested!=s.readOnlyAck)&&(!active||requested!=serial)){
   active=true;pending=true;serial=requested;
  }
  if(!active)return Admission::Commands;
  // Late or malformed submissions during observation are also terminal.
  // Cancellation occurs before and after retirement, which can own delayed ACKs.
  cancel();
  if(pending){pending=!retire();cancel();}
  if(pending)return Admission::Retiring;
  barrier();s.readOnlyAck=serial;barrier();
  if(s.readOnly||s.readOnlyRequest!=serial)return Admission::Observe;
  active=false;return Admission::Commands;
 }
};

// Runtime owns all acknowledgements. Host code never fabricates completion
// while pausing. No command payload or preview is replayable after this fence.
template<class Shared,class Barrier>void cancelMailboxes(Shared&s,Barrier barrier){
 if(s.request!=s.ack){s.result=-7;s.count=0;barrier();s.ack=s.request;}
 if(s.selectRequest!=s.selectAck){s.selectResult=-7;s.selectCount=0;barrier();s.selectAck=s.selectRequest;}
 if(s.planRequest!=s.planAck){s.planResult=-7;s.planAccepted=0;s.planAcceptedMask=s.planRejectedMask=0;barrier();s.planAck=s.planRequest;}
 if(s.cooperativePlan!=s.cooperativeAck){
  s.cooperativeResult=-7;s.cooperativeAccepted=s.cooperativeProcessed=0;
  s.cooperativeRejected=s.cooperativeCount>0?s.cooperativeCount:0;
  for(auto&bits:s.cooperativeAcceptedBits)bits=0;
  barrier();s.cooperativeAck=s.cooperativePlan;
 }
 if(s.pointer.request!=s.pointer.ack){
  s.pointer.result=-7;s.pointer.accepted=0;s.pointer.rejected=s.pointer.intent.count;
  barrier();s.pointer.ack=s.pointer.request;
 }
 barrier();
 s.productionAck=s.productionRequest;s.uiSoundAck=s.uiSoundRequest;
 s.formationKeyAck=s.formationKey;s.formationConsumedUntil=0;
 s.reserveConsumed=s.reserveInput;
 const auto camera=s.cameraSeq;if(!(camera&1))s.cameraAck=uint32_t(camera);
 barrier();
}
}} // assist::readonly

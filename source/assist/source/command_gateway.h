#pragma once
#include <stdint.h>
#include "unit_task_arbiter.h"

// Common contract for all game-changing orders. Features choose targets; only
// the game-thread EventClass sender decides if a vanilla event is queued.
// Never interpret Queued as "the game executed the order".
namespace assist { namespace gateway {
enum class Phase : uint8_t { Idle, Planned, Queued, Observed, Completed, Cancelled, Failed, Expired };
enum class Failure : uint8_t { None, WrongMatch, Stale, ChatActive, InvalidObject, InvalidOrder, QueueFull, TimingUnavailable, Superseded };
struct Context {uint32_t match=0,now=0;bool active=false,chat=false;};
struct Intent {uint32_t match=0,created=0,unit=0,target=0,manualVersion=0;};
struct Receipt {uint32_t serial=0,match=0,unit=0,target=0,queuedAt=0,observedAt=0;Phase phase=Phase::Idle;Failure failure=Failure::None;taskarbiter::Owner owner=taskarbiter::Owner::Generic;};
inline Failure validate(Context ctx,Intent intent,uint32_t maximumAge){
 if(!ctx.active)return Failure::WrongMatch;
 if(ctx.chat)return Failure::ChatActive;
 if(intent.match!=ctx.match)return Failure::WrongMatch;
 if(uint32_t(ctx.now-intent.created)>maximumAge)return Failure::Stale;
 if(!intent.unit)return Failure::InvalidObject;
 return Failure::None;
}
inline void planned(Receipt& r,uint32_t serial,const Intent& intent){
 r={};r.serial=serial;r.match=intent.match;r.unit=intent.unit;r.target=intent.target;r.phase=Phase::Planned;
}
inline void submitted(Receipt& r,uint32_t now){
 if(r.phase==Phase::Planned){r.phase=Phase::Queued;r.queuedAt=now;}
}
inline void observed(Receipt& r,uint32_t now){
 if(r.phase==Phase::Queued){r.phase=Phase::Observed;r.observedAt=now;}
}
inline void finished(Receipt& r){if(r.phase==Phase::Observed)r.phase=Phase::Completed;}
inline void cancelled(Receipt& r,Failure why){
 if(r.phase==Phase::Planned||r.phase==Phase::Queued||r.phase==Phase::Observed){r.phase=Phase::Cancelled;r.failure=why;}
}
inline void expired(Receipt& r,uint32_t now,uint32_t maxAge){
 if((r.phase==Phase::Queued||r.phase==Phase::Observed)&&uint32_t(now-r.queuedAt)>maxAge){r.phase=Phase::Expired;r.failure=Failure::Stale;}
}
inline bool confirmed(const Receipt& r){return r.phase==Phase::Observed||r.phase==Phase::Completed;}
// Helper to allocate a non-zero ticket serial and build a Queued receipt for
// immediate non-MegaMission orders (Object Repair/Sell, Production, SuperWeapon).
inline Receipt makeQueuedReceipt(uint32_t& serialCounter,const Intent& intent,uint32_t now,taskarbiter::Owner owner){
 uint32_t serial=++serialCounter;if(!serial)serial=++serialCounter;
 Receipt r{};planned(r,serial,intent);submitted(r,now);r.owner=owner;
 return r;
}
// Bounded observer for normal MegaMission orders. The DLL never alters the
// simulation to obtain a receipt; it compares subsequent read-only snapshots.
// If an order matched the pre-existing task, matching again is not evidence.
struct MissionState {bool valid=false;int mission=-1,queued=-1;uint32_t carrier=0;};
struct MissionTicket {
 Receipt receipt{};uint32_t target=0;int requested=-1,frame=0;MissionState before{};
};
template<int Capacity=128> struct MissionObserver {
 MissionTicket tickets[Capacity]{};int count=0,head=0;uint32_t nextSerial=0;
 void clear(){count=head=0;nextSerial=0;}
 bool pending()const{return count>0;}
 uint32_t allocateSerial(){
  uint32_t serial=++nextSerial;if(!serial)serial=++nextSerial;
  return serial;
 }
 uint32_t record(uint32_t match,uint32_t id,uint32_t target,int mission,int frame,
             uint32_t now,MissionState before,taskarbiter::Owner owner=taskarbiter::Owner::Generic){
  if(!id||!before.valid)return 0; // never invent evidence without a baseline
  // If saturated, drop telemetry only, NEVER the already queued game event.
  if(count>=Capacity)return 0;
  auto&t=tickets[count++];t={};Intent intent{match,now,id,target,0};
  uint32_t serial=allocateSerial();
  planned(t.receipt,serial,intent);submitted(t.receipt,now);t.receipt.owner=owner;
  t.target=target;t.requested=mission;t.frame=frame;t.before=before;
  return serial;
 }
 template<class Sample,class OnTerminal>
 int pollWithReceipts(uint32_t match,int frame,uint32_t now,Sample sample,OnTerminal report,int budget=32){
  int confirmations=0;
  if(budget<0)budget=0;
  // Round-robin: an unchanging early task must not starve later orders.
  for(int checked=0;checked<budget&&count>0;++checked){
   if(head>=count)head=0;
   int i=head++;auto&t=tickets[i];bool remove=false;
   if(t.receipt.match!=match||frame<t.frame){cancelled(t.receipt,Failure::WrongMatch);remove=true;}
   else if(frame==t.frame)continue;
   else if(frame-t.frame>300){t.receipt.phase=Phase::Expired;t.receipt.failure=Failure::Stale;remove=true;}
   else {
    MissionState seen=sample(t.receipt.unit);
    if(seen.valid){
     bool changed=(seen.mission==t.requested&&t.before.mission!=t.requested)||
                  (seen.queued==t.requested&&t.before.queued!=t.requested);
     if(t.requested==7&&t.target&&seen.carrier==t.target&&t.before.carrier!=t.target)changed=true;
     if(changed){observed(t.receipt,now);++confirmations;remove=true;}
    }
   }
   if(remove){report(t.receipt);tickets[i]=tickets[--count];head=i;}
  }
  if(count&&head>=count)head=0;
  return confirmations;
 }
 template<class OnTerminal> bool cancelTicket(uint32_t match,uint32_t serial,Failure why,OnTerminal report){
  if(!serial)return false;
  for(int i=0;i<count;++i)if(tickets[i].receipt.match==match&&tickets[i].receipt.serial==serial){
   cancelled(tickets[i].receipt,why);report(tickets[i].receipt);
   tickets[i]=tickets[--count];if(head>i) --head;if(head>=count)head=0;
   return true;
  }
  return false;
 }
 bool dismissTicket(uint32_t match,uint32_t serial){
  if(!serial)return false;
  for(int i=0;i<count;++i)if(tickets[i].receipt.match==match&&tickets[i].receipt.serial==serial){
   tickets[i]=tickets[--count];if(head>i)--head;if(head>=count)head=0;
   return true;
  }
  return false;
 }
 template<class Sample> int poll(uint32_t match,int frame,uint32_t now,Sample sample,int budget=32){
  return pollWithReceipts(match,frame,now,sample,[](const Receipt&){},budget);
 }
};
// Host and injected-side statuses represent separate subsystems, NOT a shared
// "connected" bit. Bridge commands must be checked against the bridge state.
struct Channels {int read=0,bridge=0,command=0,render=0;};
inline int chooseVisibleStatus(Channels ch){return ch.bridge<0?ch.bridge:(ch.bridge==1||ch.command==1)?1:ch.read;}
inline bool canControl(Channels ch){return ch.bridge==1&&ch.command==1;}
}} // namespace assist::gateway

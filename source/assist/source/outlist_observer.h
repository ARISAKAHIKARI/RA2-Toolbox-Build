#pragma once
#include <stdint.h>
#include <string.h>

// Diagnostic snapshots only. A visible suffix is NOT proof of a manual order,
// command execution, or complete event history. Never use this class to mutate
// game queues, cancel tasks, or replace the native successful-append callback.
namespace assist { namespace outlistobserve {
constexpr unsigned Capacity=128, EventBytes=111;
struct Ring {int32_t count=0,head=0,tail=0;};
inline bool valid(const Ring&r){
 return r.count>=0&&r.count<=int(Capacity)&&r.head>=0&&r.head<int(Capacity)&&
  r.tail>=0&&r.tail<int(Capacity)&&r.tail==((r.head+r.count)&127);
}
inline unsigned slot(const Ring&r,unsigned n){return (unsigned(r.head)+n)&127;}
inline bool sameRing(const Ring&a,const Ring&b){return a.count==b.count&&a.head==b.head&&a.tail==b.tail;}
struct Identity {
 uint32_t match=0,scenario=0,house=0,houseID=0;
 int32_t session=-1,houseIndex=-1;
};
inline bool valid(const Identity&i){return i.match&&i.scenario&&i.house&&i.houseID&&
 (i.session==3||i.session==4)&&i.houseIndex>=0&&i.houseIndex<32;}
inline bool sameIdentity(const Identity&a,const Identity&b){return a.match==b.match&&
 a.scenario==b.scenario&&a.house==b.house&&a.houseID==b.houseID&&a.session==b.session&&a.houseIndex==b.houseIndex;}
struct Snapshot {
 Ring ring{};Identity identity{};int32_t frame=-1;uint32_t toolSerial=0;bool complete=false;
 uint8_t events[Capacity][EventBytes]{};uint32_t timings[Capacity]{};
};
enum class Outcome:uint8_t {Baseline,Invalid,ContextChanged,OwnTraffic,Ambiguous,NoVisibleAppend,VisibleSuffix};
struct Delta {Outcome outcome=Outcome::Invalid;unsigned first=0,count=0;};
inline uint32_t word(const uint8_t*p){uint32_t v=0;memcpy(&v,p,sizeof v);return v;}
struct Candidate {uint32_t actor=0,frame=0;uint8_t mission=0;};
inline bool localMega(const Snapshot&s,unsigned logical,Candidate&out){
 out={};if(!s.complete||!valid(s.ring)||!valid(s.identity)||s.frame<0||logical>=unsigned(s.ring.count))return false;
 const auto*p=s.events[slot(s.ring,logical)];
 // Only ordinary, unexecuted MegaMission; planning/F/other events remain opaque.
 if(p[0]!=4||(p[1]&1)||p[2]!=uint8_t(s.identity.houseIndex)||p[11]!=0x34||p[12]>=32||p[29]!=0)return false;
 const uint32_t actor=word(p+7),frame=word(p+3);
 if(!actor||frame>uint32_t(s.frame))return false;
 out={actor,frame,p[12]};return true;
}
class Observer {
 Snapshot previous{};bool have=false;
public:
 void clear(){have=false;}
 Delta observe(const Snapshot&s){
  if(!s.complete||!valid(s.ring)||!valid(s.identity)||s.frame<0){clear();return {Outcome::Invalid,0,0};}
  Delta d{Outcome::Baseline,0,0};
  if(have){
   if(!sameIdentity(previous.identity,s.identity)||s.frame<previous.frame)d.outcome=Outcome::ContextChanged;
   else if(s.toolSerial!=previous.toolSerial)d.outcome=Outcome::OwnTraffic;
   // A drain, reset or changed prefix cannot be reconstructed from polling.
   // Even unchanged snapshots may hide a complete consume/refill (ABA) cycle.
   else if(int64_t(s.frame)-previous.frame>8||s.ring.head!=previous.ring.head||s.ring.count<previous.ring.count)d.outcome=Outcome::Ambiguous;
   else {
    bool prefix=true;
    for(unsigned n=0;n<unsigned(previous.ring.count);++n){const unsigned i=slot(previous.ring,n);
     if(s.timings[i]!=previous.timings[i]||memcmp(s.events[i],previous.events[i],EventBytes)){prefix=false;break;}
    }
    if(!prefix)d.outcome=Outcome::Ambiguous;
    else if(s.ring.count==previous.ring.count)d.outcome=Outcome::NoVisibleAppend;
    else d={Outcome::VisibleSuffix,unsigned(previous.ring.count),unsigned(s.ring.count-previous.ring.count)};
   }
  }
  previous=s;have=true;return d;
 }
};
}}

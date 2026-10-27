#include "../source/outlist_observer.h"
#include <cstdio>
#include <cstdlib>
using namespace assist::outlistobserve;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static Snapshot empty(int head=0){Snapshot s{};s.complete=true;s.identity={1,0x10000,0x20000,7,3,0};s.frame=100;s.ring={0,head,head};return s;}
static void append(Snapshot&s,uint32_t actor=1,uint8_t mission=2){
 if(!valid(s.ring)||s.ring.count==128)std::abort();
 auto*p=s.events[s.ring.tail];memset(p,0,EventBytes);p[0]=4;p[2]=uint8_t(s.identity.houseIndex);
 const uint32_t f=uint32_t(s.frame);memcpy(p+3,&f,4);memcpy(p+7,&actor,4);p[11]=0x34;p[12]=mission;
 s.timings[s.ring.tail]=uint32_t(s.frame)+actor;s.ring.tail=(s.ring.tail+1)&127;++s.ring.count;
}
int main(){
 Observer o;auto s=empty();CHECK(o.observe(s).outcome==Outcome::Baseline);
 CHECK(o.observe(s).outcome==Outcome::NoVisibleAppend);
 append(s,42);auto d=o.observe(s);CHECK(d.outcome==Outcome::VisibleSuffix&&d.first==0&&d.count==1);
 Candidate c{};CHECK(localMega(s,0,c)&&c.actor==42&&c.mission==2);
 CHECK(o.observe(s).outcome==Outcome::NoVisibleAppend); // no duplicate report
 // Full queue and every possible physical wrap point.
 for(int h=0;h<128;++h){Observer q;auto a=empty(h);CHECK(q.observe(a).outcome==Outcome::Baseline);
  for(int n=0;n<128;++n){append(a,uint32_t(n+1));auto x=q.observe(a);
   CHECK(x.outcome==Outcome::VisibleSuffix&&x.first==unsigned(n)&&x.count==1);
   CHECK(localMega(a,x.first,c)&&c.actor==unsigned(n+1));}
  CHECK(valid(a.ring)&&a.ring.count==128&&a.ring.head==a.ring.tail);
  CHECK(q.observe(a).outcome==Outcome::NoVisibleAppend);
 }
 s=empty();o.clear();o.observe(s);append(s);append(s,2);d=o.observe(s);
 CHECK(d.outcome==Outcome::VisibleSuffix&&d.count==2);
 // Draining and reset are unknown, never replay the remaining queue.
 s.ring.head=(s.ring.head+1)&127;--s.ring.count;CHECK(o.observe(s).outcome==Outcome::Ambiguous);
 s.ring={0,0,0};CHECK(o.observe(s).outcome==Outcome::Ambiguous);
 s=empty();append(s);o.clear();o.observe(s);s.events[0][20]^=1;CHECK(o.observe(s).outcome==Outcome::Ambiguous);
 s.timings[0]++;CHECK(o.observe(s).outcome==Outcome::Ambiguous);
 append(s,2);s.toolSerial++;CHECK(o.observe(s).outcome==Outcome::OwnTraffic);
 append(s,3);s.toolSerial=0;CHECK(o.observe(s).outcome==Outcome::OwnTraffic); // wrap/reset
 ++s.identity.match;CHECK(o.observe(s).outcome==Outcome::ContextChanged);
 ++s.identity.houseID;CHECK(o.observe(s).outcome==Outcome::ContextChanged);
 ++s.identity.scenario;CHECK(o.observe(s).outcome==Outcome::ContextChanged);
 --s.frame;CHECK(o.observe(s).outcome==Outcome::ContextChanged);
 s.frame+=9;CHECK(o.observe(s).outcome==Outcome::Ambiguous);
 s.complete=false;CHECK(o.observe(s).outcome==Outcome::Invalid);
 s.complete=true;CHECK(o.observe(s).outcome==Outcome::Baseline);
 s.ring.tail=128;CHECK(o.observe(s).outcome==Outcome::Invalid);
 s=empty();s.identity.houseIndex=-1;CHECK(o.observe(s).outcome==Outcome::Invalid);
 s=empty();s.identity.session=5;CHECK(o.observe(s).outcome==Outcome::Invalid);
 // Invalid/foreign/planning events cannot be labelled local ordinary commands.
 s=empty();append(s,9);CHECK(localMega(s,0,c));
 s.events[0][2]=1;CHECK(!localMega(s,0,c));s.events[0][2]=0;
 s.events[0][29]=1;CHECK(!localMega(s,0,c));s.events[0][29]=0;
 s.events[0][0]=5;CHECK(!localMega(s,0,c));s.events[0][0]=4;
 s.events[0][1]=1;CHECK(!localMega(s,0,c));s.events[0][1]=0;
 s.events[0][11]=11;CHECK(!localMega(s,0,c));s.events[0][11]=0x34;
 s.events[0][12]=255;CHECK(!localMega(s,0,c));s.events[0][12]=2;
 uint32_t future=101;memcpy(s.events[0]+3,&future,4);CHECK(!localMega(s,0,c));
 CHECK(!localMega(s,128,c));
 // Two histories can produce identical snapshots: never infer zero actual events.
 s=empty();o.clear();o.observe(s);CHECK(o.observe(s).outcome==Outcome::NoVisibleAppend);
 // Inputs are immutable; observer owns only its local history.
 Snapshot unchanged=s;o.observe(s);CHECK(!memcmp(&s,&unchanged,sizeof s));
 std::printf("PASS: %u checks; wrap/full/drain/reset/ownership/own traffic/opaque events/ABA limits\n",checks);
}

// No-inline branch: bounded READ-ONLY OutList telemetry, not task arbitration.
// The legacy option/IPC fields are retained for ABI/configuration compatibility.
// Accepted counts visible local ordinary-MegaMission candidates, NOT execution
// or proven human input. Skipped counts ambiguous/invalid sampling intervals.
#include "outlist_observer.h"
static bool nativeMegaGatewayActive;
static assist::outlistobserve::Observer nativeMegaObserver;
static assist::outlistobserve::SampleCadence nativeMegaCadence;

// Retain the guarded action ABI, but there is no game-code entry to install.
static int nativeMegaGatewayAction(unsigned,uint32_t,uint32_t){return 0;}
enum class OutListSample {Complete,Deferred,Failed};

static OutListSample sampleNativeOutList(assist::outlistobserve::Snapshot&s){
 using namespace assist::outlistobserve;
 s.complete=false;
 if(!sh||!match||!lastHouse)return OutListSample::Failed;
 s.identity={match,0,lastHouse,0,-1,-1};
 s.toolSerial=snap.queueSent;
 uint32_t local=0,scenario=0,houseID=0;int frame=-1,session=-1,houseIndex=-1;
 if(!read(0xA8B230,&s.identity.scenario,4)||!read(0xA83D4C,&local,4)||local!=lastHouse||
    !read(lastHouse+0x10,&s.identity.houseID,4)||!read(lastHouse+0x30,&s.identity.houseIndex,4)||
    !read(0xA8B238,&s.identity.session,4)||!read(0xA8ED84,&s.frame,4)||
    !valid(s.identity)||s.frame<0)return OutListSample::Failed;
 // Check cheap identity metadata before bulk queue reads. Never slice one
 // queue snapshot over several frames: that would combine unrelated states.
 if(!nativeMegaCadence.admit(s.identity,s.frame))return OutListSample::Deferred;
 Ring after{};
 if(!read(0xA802C8,&s.ring,sizeof s.ring)||!valid(s.ring)||
    !read(0xA802D4,s.events,sizeof s.events)||!read(0xA83A54,s.timings,sizeof s.timings)||
    !read(0xA802C8,&after,sizeof after)||!sameRing(s.ring,after)||
    !read(0xA83D4C,&local,4)||local!=s.identity.house||
    !read(0xA8B230,&scenario,4)||scenario!=s.identity.scenario||
    !read(lastHouse+0x10,&houseID,4)||houseID!=s.identity.houseID||
    !read(lastHouse+0x30,&houseIndex,4)||houseIndex!=s.identity.houseIndex||
    !read(0xA8ED84,&frame,4)||frame!=s.frame||
    !read(0xA8B238,&session,4)||session!=s.identity.session||
    snap.queueSent!=s.toolSerial)return OutListSample::Failed;
 s.complete=true;return OutListSample::Complete;
}
static void restoreNativeMegaGateway(){
 nativeMegaGatewayActive=false;nativeMegaObserver.clear();nativeMegaCadence.clear();
 if(sh)sh->nativeMegaGatewayStatus=0;
}
static void nativeMegaGatewayTick(){
 if(!sh||!sh->nativeMegaGateway){restoreNativeMegaGateway();return;}
 if(!riskReady()||snap.status!=1||snap.commandStatus!=1){restoreNativeMegaGateway();return;}
 // This first diagnostic is MP OutList only. SP delivery remains untouched.
 if(!eventdelivery::isMultiplayer(snap.session)){
  restoreNativeMegaGateway();sh->nativeMegaGatewayStatus=-4;return;
 }
 static assist::outlistobserve::Snapshot sample;
 const auto sampled=sampleNativeOutList(sample);
 if(sampled==OutListSample::Deferred)return; // Keep status/counters, no fake observation.
 if(sampled==OutListSample::Failed){
  nativeMegaObserver.clear();nativeMegaGatewayActive=false;
  ++sh->nativeMegaGatewaySkipped;sh->nativeMegaGatewayStatus=-3;return;
 }
 nativeMegaGatewayActive=true;sh->nativeMegaGatewayStatus=1;
 const auto delta=nativeMegaObserver.observe(sample);
 using assist::outlistobserve::Outcome;
 if(delta.outcome==Outcome::VisibleSuffix){
  for(unsigned n=0;n<delta.count;++n){assist::outlistobserve::Candidate candidate{};
   if(assist::outlistobserve::localMega(sample,delta.first+n,candidate))++sh->nativeMegaGatewayAccepted;
  }
 }else if(delta.outcome==Outcome::Invalid||delta.outcome==Outcome::OwnTraffic||
          delta.outcome==Outcome::Ambiguous||delta.outcome==Outcome::ContextChanged){
  ++sh->nativeMegaGatewaySkipped;
 }
 // Deliberately NO cancelNative, lease/revision changes, game calls, events,
 // queue writes, instruction patching, or assumptions about missed commands.
}

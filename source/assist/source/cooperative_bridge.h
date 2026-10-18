// Local mouse/UI entry points only. Received network events, simulation AI,
// weapon selectors, ownership, rules and missions are never patched.
#include "../../external_reader/source/game_view.h"
#include "business_effects.h"
struct LocalRawRead {bool operator()(uint32_t a,void*out,size_t n)const{if(a<0x10000||uint64_t(a)+n>=0x80000000ull)return false;memcpy(out,(const void*)(uintptr_t)a,n);return true;}};
static external::GameView<LocalRawRead> clickView(LocalRawRead{});
static uint32_t clickSerial,cooperativePlanSeen;static int cooperativeAt;
struct EnterReservation {uint32_t id,target,type,token;int frame,slots;};
static EnterReservation entryReservations[cooperative::MaxSources];static int entryReservationCount;
struct CooperativeEffect {uint32_t actor=0,target=0,ticket=0,match=0,actorPointer=0,targetPointer=0,beforeTarget=0,queuedAt=0;int frame=0,mission=0,rearmStart=-1;bool acquired=false;};
static CooperativeEffect cooperativeEffects[cooperative::MaxSources];static int cooperativeEffectCount;
static void observeCooperativeEffects(uint32_t house,int frame){
 for(int i=0;i<cooperativeEffectCount;){const auto e=cooperativeEffects[i];
  uint32_t actor=worldByID(e.actor),target=worldByID(e.target);
  bool identity=e.match==match&&frame>=e.frame&&actor&&actor==e.actorPointer&&get<uint32_t>(actor+0x21C)==house;
  bool active=identity&&live(actor); // aircraft can temporarily leave the map
  bool targetSame=target&&target==e.targetPointer; // no target-ID reuse false positive
  bool boarded=identity&&e.mission==7&&targetSame&&clickView.entered(actor,target);
  bool targeting=active&&e.mission==1&&frame>e.frame&&targetSame&&get<uint32_t>(actor+0x2B4)==target;
  // An already-selected target is not fresh acquisition, but a NEW rearm can
  // still demonstrate that the repeated Attack caused another firing cycle.
  if(targeting&&e.beforeTarget!=target)cooperativeEffects[i].acquired=true;
  int rearm=active?get<int>(actor+0x2EC):-1; // verified raw combat field, read-only
  auto air=effects::aircraft(active,targeting,e.rearmStart,rearm,e.frame,frame);
  bool shot=e.mission==1&&air==effects::Aircraft::ShotObserved;
  // Temporarily absent aircraft are not immediate failure. A reused pointer is.
  bool expired=e.match!=match||(actor&&actor!=e.actorPointer)||(target&&target!=e.targetPointer)||frame<e.frame||frame-e.frame>180;
  if(boarded||shot||expired){
   if(e.ticket){gateway::Receipt receipt{};gateway::Intent intent{e.match,e.queuedAt,e.actor,e.target,0};
    gateway::planned(receipt,e.ticket,intent);receipt.owner=taskarbiter::Owner::Cooperative;
    receipt.queuedAt=e.queuedAt;
    // Shot evidence = changed rearm after target acquisition, not proven damage.
    receipt.phase=(boarded||shot)?gateway::Phase::Completed:
       cooperativeEffects[i].acquired?gateway::Phase::Observed:gateway::Phase::Expired;
    receipt.failure=(expired&&!boarded&&!shot&&!cooperativeEffects[i].acquired)?gateway::Failure::Stale:gateway::Failure::None;
    receipt.observedAt=receipt.phase!=gateway::Phase::Expired?sampleClock(sh):0;
    missionObserver.dismissTicket(e.match,e.ticket);recordReceipt(receipt);
    actorLeaseArbiter.release(e.match,e.actor,e.ticket);
   }
   cooperativeEffects[i]=cooperativeEffects[--cooperativeEffectCount];
  }else ++i;
 }
}
// Only cancel older watch records AFTER the replacement event was enqueued.
// Never cancel on a failed queue attempt; that would lose a valid task.
static void supersedeCooperativeEffect(uint32_t actor,uint32_t session){
 for(int i=0;i<cooperativeEffectCount;){auto e=cooperativeEffects[i];
  if(e.actor==actor&&e.match==session){
   gateway::Receipt rec{};gateway::Intent intent{e.match,e.queuedAt,e.actor,e.target,0};
   gateway::planned(rec,e.ticket,intent);rec.owner=taskarbiter::Owner::Cooperative;
   rec.queuedAt=e.queuedAt;gateway::cancelled(rec,gateway::Failure::Superseded);
   missionObserver.dismissTicket(e.match,e.ticket);recordReceipt(rec);++snap.receiptCancelled;
   actorLeaseArbiter.release(e.match,e.actor,e.ticket);
   cooperativeEffects[i]=cooperativeEffects[--cooperativeEffectCount];
  }else ++i;
 }
}
static bool airUnit(uint32_t p){int k=kind(p);return featureObjectAllowed(featuretypes::Aircraft,p)&&(k==0||(k==2&&(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D))));}
static bool vehicleSellCell(uint32_t p){
 uint32_t cell=readMapCell(get<int>(p+0x9C)/256,get<int>(p+0xA0)/256);if(!cell)return false;
 for(uint32_t q=get<uint32_t>(cell+0xE4),n=0;q>=0x10000&&n<256;++n){if(!worldObjects.contains(q))return false;if(kind(q)==3)return true;uint32_t next=get<uint32_t>(q+0x30);if(next==q)return false;q=next;}return false;
}
static bool sellAllowed(uint32_t p){
 if(!nativeReady()||!p||!live(p))return false;
 int k=kind(p);uint32_t owner=get<uint32_t>(p+0x21C);bool drained=get<uint32_t>(p+0x1D0)!=0;
 if(owner!=lastHouse)return false;
 if(drained&&!(sh->flags&SellDrained))return false;
 if(k==2)return featureObjectAllowed(featuretypes::SellVehicle,p)&&owner>=0x10000&&(sh->flags&SellVehicles)&&vehicleSellCell(p);
 if(k!=3)return false;
 if(owner!=lastHouse)return (sh->flags&SellEnemy)&&nativeEnemy(p,lastHouse);
 return drained&&(sh->flags&SellDrained);
}
static bool newCooperative(uint32_t p,uint32_t target,int mode){
 auto&r=sh->cooperativeRequest;int frame=get<int>(0xA8ED84);DWORD now=sampleClock(sh);uint32_t id=get<uint32_t>(p+0x10),targetID=get<uint32_t>(target+0x10);
 if(!id||!targetID)return false;
 if(get<int>(0xA8ECB8+16)>cooperative::MaxSources){
  if(r.count<=cooperative::MaxSources||r.frame!=frame||r.target!=targetID||r.mode!=mode){InterlockedIncrement(&sh->cooperativeSeq);memset(&r,0,sizeof r);r.serial=++clickSerial;if(!r.serial)r.serial=++clickSerial;r.match=match;r.target=targetID;r.mode=mode;r.frame=frame;r.tick=now;r.count=cooperative::MaxSources+1;MemoryBarrier();InterlockedIncrement(&sh->cooperativeSeq);notifyHost();}return false;
 }
 bool same=r.serial&&r.match==match&&r.target==targetID&&r.mode==mode&&r.frame==frame&&r.count>=0&&r.count<=cooperative::MaxSources&&DWORD(now-r.tick)<100;
 if(same)for(int i=0;i<r.count;++i)if(r.sources[i].id==id){same=false;break;}
 if(same&&r.count>=cooperative::MaxSources)return false;
 if(!manualVersions.trackable(id))return false;
 cancelNative(p);InterlockedIncrement(&sh->cooperativeSeq);
 if(!same){memset(&r,0,sizeof r);r.serial=++clickSerial;if(!r.serial)r.serial=++clickSerial;r.match=match;r.target=targetID;r.type=typeOf(target,kind(target));r.mode=mode;r.frame=frame;
  uint32_t tactical=get<uint32_t>(0x887324);if(tactical>=0x10000){r.cameraX=get<int>(tactical+0xB0);r.cameraY=get<int>(tactical+0xB4);}
  int width=get<int>(0xB0CE30),height=get<int>(0xB0CE34);r.viewWidth=width>0&&width<=32768?width:get<int>(0xA8EB84);r.viewHeight=height>0&&height<=32768?height:get<int>(0xA8EB88);
 }
 r.tick=now;r.sources[r.count++]={id,manualVersions.get(id)};MemoryBarrier();InterlockedIncrement(&sh->cooperativeSeq);notifyHost();return true;
}
static int cooperativeInput(uint32_t p,uint32_t target,uint32_t bindings){
 if(!nativeReady()||!p||!target||!own(p,lastHouse)||!get<bool>(p+0x83)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C))return 0;
 if(((bindings&pointerinput::Shift)&&!(bindings&(pointerinput::Enter|pointerinput::AirFirst|pointerinput::AirSecond)))||get<bool>(0x87F7E8+0x11B3)||get<bool>(0x87F7E8+0x11B1))return 0;
 indexWorld(get<int>(0xA8ED84));if(!worldObjects.contains(p)||!worldObjects.contains(target)||!live(target))return 0;
 if((sh->flags&AdvancedEnter)&&(bindings&pointerinput::Enter)&&featureObjectAllowed(featuretypes::EnterSource,p)&&featureObjectAllowed(featuretypes::EnterTarget,target)&&clickView.entryCompatible(p,target,lastHouse))return newCooperative(p,target,cooperative::Enter)?1:0;

 if((sh->flags&DiskPrimary)&&kind(p)==2&&kind(target)==3&&(bindings&pointerinput::Control)){
  uint32_t t=typeOf(p,2),secondary=t?get<uint32_t>(t+0x898+28):0;
  if(t&&featureTypeAllowed(featuretypes::Disk,t)&&(sameid((const char*)(t+0x24),"DISK")||(secondary>=0x10000&&get<bool>(secondary+0x142)))){
   int x=get<int>(target+0x9C)/256,y=get<int>(target+0xA0)/256;if(!readMapCell(x,y)||!loadInterfaces())return 0;
   uint32_t diskID=get<uint32_t>(p+0x10);if(!diskID)return 0;
   // Explicit input cancellation already released previous tool ownership.
   return nativeMission(lastHouse,diskID,1,uint32_t(y*1000+x),0,0,0,0,
                        taskarbiter::Owner::Cooperative,nullptr,11)?1:-1;
  }
 } if((sh->flags&AircraftSpread)&&airUnit(p)&&nativeEnemy(target,lastHouse)){
  int mode=(bindings&pointerinput::AirSecond)?cooperative::AirSecond:(bindings&pointerinput::AirFirst)?cooperative::AirFirst:0;
  if(mode)return newCooperative(p,target,mode)?1:0;
 }
 return 0;
}
static void cancelCooperativePlan(int result){
 if(!sh||sh->cooperativePlan==sh->cooperativeAck)return;
 // Counters belong to planSeen, never to a newer unconsumed mailbox serial.
 if(sh->cooperativePlan!=cooperativePlanSeen){sh->cooperativeAccepted=0;sh->cooperativeProcessed=0;memset(sh->cooperativeAcceptedBits,0,sizeof sh->cooperativeAcceptedBits);}
 int count=sh->cooperativeCount;if(count<0)count=0;if(count>cooperative::MaxSources)count=cooperative::MaxSources;
 sh->cooperativeRejected=count-sh->cooperativeAccepted;if(sh->cooperativeRejected<0)sh->cooperativeRejected=0;
 sh->cooperativeResult=result;MemoryBarrier();sh->cooperativeAck=sh->cooperativePlan;
}
static void resetCooperativeState(){entryReservationCount=cooperativeEffectCount=0;cooperativeAt=0;cooperativePlanSeen=0;}
static void restoreClickHooks(){}
static void clickHooksTick(uint32_t flags){sh->clickHookStatus=(flags&SpecialClickFlags)?1:0;}
static int entryReserved(uint32_t target,uint32_t ignore,uint32_t house){
 int reserved=0;bool facility=clickView.capacity(target).singleVehicle;Vec all=vec(0xA8EC78);for(int i=0;i<all.count;++i){uint32_t p=item(all,i);int k=p?kind(p):-1;
  if((k!=1&&k!=2)||!own(p,house)||get<uint32_t>(p+0x10)==ignore||clickView.entryDestination(p)!=target)continue;
  if(facility&&clickView.radioLinked(target,p))continue;
  int slots=clickView.entrySlots(p,target);if(!slots)return 1024;reserved+=slots;if(reserved>=1024)return 1024;
 }
 for(int i=0;i<entryReservationCount;++i){auto&r=entryReservations[i];uint32_t p=worldByID(r.id);if(r.target==get<uint32_t>(target+0x10)&&r.id!=ignore&&p&&clickView.entryDestination(p)!=target&&!(facility&&clickView.radioLinked(target,p)))reserved+=r.slots;}
 return reserved;
}
static void cooperativeTick(uint32_t house,int frame,DWORD now){
 observeCooperativeEffects(house,frame);
 for(int i=0;i<entryReservationCount;){auto&r=entryReservations[i];uint32_t p=worldByID(r.id),t=worldByID(r.target);
  if(!p||!t||!own(p,house)||manualVersions.get(r.id)!=r.token||get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2E4)||!live(p)||frame-r.frame>180)entryReservations[i]=entryReservations[--entryReservationCount];else ++i;
 }
 uint32_t plan=sh->cooperativePlan;if(plan==sh->cooperativeAck)return;MemoryBarrier();
 if(plan!=cooperativePlanSeen){cooperativePlanSeen=plan;cooperativeAt=0;sh->cooperativeAccepted=sh->cooperativeRejected=0;sh->cooperativeProcessed=0;sh->cooperativeResult=1;memset(sh->cooperativeAcceptedBits,0,sizeof sh->cooperativeAcceptedBits);}
 if(sh->cooperativeCount<0||sh->cooperativeCount>cooperative::MaxSources){sh->cooperativeRejected=sh->cooperativeCount>0?cooperative::MaxSources:0;sh->cooperativeResult=-4;MemoryBarrier();sh->cooperativeAck=plan;notifyHost();return;}
 bool enter=sh->cooperativeMode==cooperative::Enter;
 auto fresh=[&](){return freshness::valid(sh->cooperativeMatch,match,sh->cooperativeTick,sh->cooperativeFrame,sampleClock(sh),get<int>(0xA8ED84),freshness::Cooperative);};
 auto expire=[&](){sh->cooperativeResult=-7;sh->cooperativeRejected+=sh->cooperativeCount-cooperativeAt;cooperativeAt=sh->cooperativeCount;};
 if(!nativeReady()||sh->cooperativeSerial!=sh->cooperativeRequest.serial||sh->cooperativeMode!=sh->cooperativeRequest.mode||sh->cooperativeMode<cooperative::Enter||sh->cooperativeMode>cooperative::AirSecond||sh->cooperativeRequest.match!=match||!freshness::intentAlive(sh->cooperativeRequest.tick,now)||sh->cooperativeCount<0||sh->cooperativeCount>cooperative::MaxSources||!(sh->flags&(enter?AdvancedEnter:AircraftSpread))){sh->cooperativeResult=-4;sh->cooperativeRejected+=sh->cooperativeCount-cooperativeAt;cooperativeAt=sh->cooperativeCount;}
 if(cooperativeAt<sh->cooperativeCount&&!fresh())expire();
 while(cooperativeAt<sh->cooperativeCount&&planningRoom(1)>0&&outgoingRoom()>0){if(!fresh()){expire();break;}const auto&o=sh->cooperativeOrders[cooperativeAt];uint32_t p=worldByID(o.id),t=worldByID(o.target);bool valid=p&&t&&own(p,house)&&live(t)&&typeOf(t,kind(t))==o.type&&manualVersions.get(o.id)==o.token&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C);
  int slots=0,mission=enter?7:1;
  if(valid&&enter){valid=featureObjectAllowed(featuretypes::EnterSource,p)&&featureObjectAllowed(featuretypes::EnterTarget,t)&&clickView.entryCompatible(p,t,house);auto c=clickView.capacity(t);slots=clickView.entrySlots(p,t);int reserved=entryReserved(t,o.id,house);
   valid=valid&&entryReservationCount<cooperative::MaxSources&&slots>0&&int64_t(c.used)+reserved+slots<=c.maximum;
   if(valid){int action=call<int>(get<uint32_t>(get<uint32_t>(p)+0x74),p,t,true);valid=action==3||action==11;if(action==11)mission=9;}
  }else if(valid){valid=airUnit(p)&&nativeEnemy(t,house);if(valid){planner::Actor source{},target{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),source,true);rawCombatInfo(t,o.type,kind(t),target,false);valid=planner::compatibleTarget(source,target,false);}}
  if(!valid){++sh->cooperativeRejected;++cooperativeAt;sh->cooperativeProcessed=cooperativeAt;continue;}
  if(!fresh()){expire();break;}
  uint32_t ticket=0;uint32_t beforeTarget=get<uint32_t>(p+0x2B4);
  int beforeRearm=get<int>(p+0x2EC);
  if(!nativeMission(house,o.id,mission,enter?0:o.target,enter?o.target:0,enter?0x34:0,0,0,taskarbiter::Owner::Cooperative,&ticket))break;
  // Preserve raw native event encoding, add only read-only effect observations.
  supersedeCooperativeEffect(o.id,match);
  if(ticket&&cooperativeEffectCount<cooperative::MaxSources&&(mission==1||mission==7)){
   // One observer owns this ticket: avoid generic timeout/observed duplicates.
   missionObserver.dismissTicket(match,ticket);
   cooperativeEffects[cooperativeEffectCount++]={o.id,o.target,ticket,match,p,t,beforeTarget,now,frame,mission,beforeRearm,false};
  }
  if(enter)entryReservations[entryReservationCount++]={o.id,o.target,o.type,o.token,frame,slots};
  sh->cooperativeAcceptedBits[cooperativeAt/32]|=uint32_t(1)<<(cooperativeAt%32);++sh->cooperativeAccepted;++cooperativeAt;sh->cooperativeProcessed=cooperativeAt;consumePlanning(1);
 }
 if(cooperativeAt>=sh->cooperativeCount){MemoryBarrier();sh->cooperativeAck=plan;notifyHost();}
}

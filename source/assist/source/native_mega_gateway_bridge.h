// Observe the ordinary native QueueMegaMission only AFTER its original
// EventClass, OutList copy, timestamp and queue-index updates succeeded.
// This bridge never gates native input, queues a second event, rewrites its
// payload, or invokes a simulation mission/target setter.
// Coverage: only calls reaching 646E90's successful OutList append. Planning
// direct DoList writers, QueueMegaMissionF and receiver QueueMission are not
// intercepted. A planning node participates only if it later uses this path.
static bool nativeMegaGatewayActive;

static int nativeMegaGatewayAction(unsigned op,uint32_t packetAddress,uint32_t expectedActor){
 if(!sh)return 0;
 auto skip=[](){++sh->nativeMegaGatewaySkipped;return 0;};
 if(op!=11||!nativeMegaGatewayActive||!sh->nativeMegaGateway||nativeMegaQueueCallDepth)return skip();
 // Capture lease and riskReady(true) were checked by AssistRiskAction.
 // Match the still-live constructor local to the slot just appended. Keep
 // vanilla's opaque/unused bytes intact (including None-tagged nonzero IDs).
 uint8_t local[111]{},queued[111]{};
 const int count=get<int>(0xA802C8),tail=get<int>(0xA802D0);
 if(count<=0||count>128||tail<0||tail>=128||
    !read(packetAddress,local,sizeof local)||
    !read(0xA802D4+uint32_t((tail+127)&127)*111,queued,sizeof queued)||
    memcmp(local,queued,sizeof local))return skip();
 uint32_t id=0,eventFrame=0,target=0,destination=0;
 memcpy(&id,local+7,4);memcpy(&eventFrame,local+3,4);
 memcpy(&target,local+0xE,4);memcpy(&destination,local+0x13,4);
 const int frame=get<int>(0xA8ED84),mission=local[0xC];
 if(local[0]!=4||local[0xB]!=0x34||!id||id!=expectedActor||
    eventFrame!=uint32_t(frame)||mission>=32)return skip();
 indexWorld(frame);
 const uint32_t object=worldByID(id);
 if(!object||!live(object)||!manualVersions.trackable(id))return skip();
 const uint32_t owner=get<uint32_t>(object+0x21C);const int houseIndex=owner>=0x10000?get<int>(owner+0x30):-1;
 if(houseIndex<0||houseIndex>=32||local[2]!=uint8_t(houseIndex)||
    (owner!=lastHouse&&!controlCommandOwnerReady(owner)))return skip();
 gateway::MissionState before{};
 before.valid=true;before.mission=get<int>(object+0xAC);
 before.queued=get<int>(object+0xB4);before.carrier=get<uint32_t>(object+0x11C);

 // false is essential: releaseSticky/ForceMission must not run here. This
 // cancellation updates tool versions, leases, reservations and follow-ups;
 // the native event remains the only operation that can change simulation.
 cancelNative(object,false);
 const uint32_t revision=manualVersions.get(id);
 if(mission==10||mission==12)manualVersions.resumeOre(id);
 else if(manualVersions.watchingOre(id)){
  manualVersions.expectOreMission(id,mission);
  manualVersions.observeOre(id,before.mission,before.queued);
 }
 // Manual input has already been accepted by vanilla. Replace any earlier
 // automatic claim in this frame, without consulting automation's budget or
 // rejection rules. A full table can drop metadata only, never the event.
 actorFrameArbiter.begin(match,frame);
 bool replaced=false;
 for(int n=0;n<actorFrameArbiter.count;++n)if(actorFrameArbiter.actors[n].actor==id){
  actorFrameArbiter.actors[n]={id,revision,taskarbiter::Owner::Generic};replaced=true;break;
 }
 if(!replaced)actorFrameArbiter.record(id,revision,taskarbiter::Owner::Generic);
 const uint32_t now=sampleClock(sh);
 // Only object targets are useful to the read-only mission observer. A cell
 // value must never be interpreted as an object ID for Enter confirmation.
 const uint32_t observedTarget=local[0x17]==0x34?destination:local[0x12]==0x34?target:0;
 const uint32_t ticket=missionObserver.record(match,id,observedTarget,mission,frame,now,before,taskarbiter::Owner::Generic);
 actorLeaseArbiter.record(match,frame,id,object,revision,taskarbiter::Owner::Generic,ticket);
 if(!ticket)++snap.receiptDropped;
 else{
  gateway::Receipt receipt{};gateway::Intent intent{match,now,id,observedTarget,revision};
  gateway::planned(receipt,ticket,intent);gateway::submitted(receipt,now);
  receipt.owner=taskarbiter::Owner::Generic;recordReceipt(receipt);
 }
 ++sh->nativeMegaGatewayAccepted;return 1;
}

// Success epilogue at 646F4E. At entry S=ESP: local EventClass=S+8 and
// original stack actorID=S+7C. After PUSHFD/PUSHAD, EBX=S-36, hence +44/+160.
// Save every register/flag and x87/SSE state before the guarded C++ callback.
// Replay the displaced native epilogue, including its AL=1 and ADD flags.
static __declspec(naked) void nativeMegaGatewayEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+160]
 lea eax,[ebx+44]
 push eax
 push 11
 call GuardedRiskAction
 add esp,12
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,1
 pop edi
 pop esi
 add esp,70h
 push 646F55h
 ret
}}

static RiskSite nativeMegaGatewaySite{0x646F4E,7,{0xB0,0x01,0x5F,0x5E,0x83,0xC4,0x70},{}};
static void prepareNativeMegaGateway(){
 if(nativeMegaGatewaySite.after[0])return;
 auto&s=nativeMegaGatewaySite;memset(s.after,0x90,s.size);s.after[0]=0xE9;
 const uint32_t offset=uint32_t(uintptr_t(&nativeMegaGatewayEntry))-s.address-5;
 memcpy(s.after+1,&offset,4);
}
static void restoreNativeMegaGateway(){
 nativeMegaGatewayActive=false;
 const bool restored=restoreRiskSite(nativeMegaGatewaySite);
 if(sh)sh->nativeMegaGatewayStatus=restored?0:-2;
}
static void nativeMegaGatewayTick(){
 if(!sh||!sh->nativeMegaGateway){restoreNativeMegaGateway();return;}
 if(!riskReady()||!controlCommandOwnerMegaCompatible()){restoreNativeMegaGateway();sh->nativeMegaGatewayStatus=-2;return;}
 prepareNativeMegaGateway();nativeMegaGatewayActive=installRiskSite(nativeMegaGatewaySite);
 sh->nativeMegaGatewayStatus=nativeMegaGatewayActive?1:-2;
}

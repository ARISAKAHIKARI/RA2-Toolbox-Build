static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,
                          uint32_t dest,int destTag,uint32_t nearest=0,int nearestTag=0,
                          taskarbiter::Owner owner=taskarbiter::Owner::Generic,uint32_t* receiptID=nullptr,
                          int targetTag=-1,OrderAuthority authority=OrderAuthority::Local,bool appendWaypoint=false,bool cooperativeTakeover=false){
 const bool foreign=authority==OrderAuthority::PlayerTest&&sh&&controlmode::proxyOrders(sh->controlAll)&&playableHouse(house);
 if(!id||!house||(house!=lastHouse&&!foreign)||!eventdelivery::mayQueue(get<int>(0xA8B238))||!loadInterfaces())return false;
 const uint8_t resolvedTargetTag=targetTag>=0?uint8_t(targetTag):(target?0x34:0);
 const uint32_t submittedAt=sampleClock(sh);
 const gateway::Context ctx{match,submittedAt,sh&&(!sh->stop||missileCleanup)&&!fault&&snap.status==1,
                            get<bool>(0xA8BC60+25)};
 const gateway::Intent intent{match,submittedAt,id,target,manualVersions.get(id)};
 if(gateway::validate(ctx,intent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 // The command gateway, not just the planner, owns the final game-thread
 // actor/ownership preflight. Never serialize a vanished/foreign actor ID.
 uint32_t object=worldByID(id);
 if(stickyOwns(object))return false; // Explicit held units belong to the player.
 uint32_t targetObject=(target&&resolvedTargetTag==0x34)?worldByID(target):0;
 const bool targetCellValid=(resolvedTargetTag==11)&&vanilla::validTargetRef(int(target),vanilla::TargetTag::Cell);
 uint32_t destObject=(destTag==0x34&&dest)?worldByID(dest):0;
 auto reject=orderpolicy::missionWithTargetTag(object!=0,object&&own(object,house),object&&live(object),
  mission,resolvedTargetTag,target,targetObject!=0,targetCellValid,destTag==0x34,destObject!=0);
 if(reject!=orderpolicy::Reject::None){++snap.commandRejected;return false;}
 const int frame=get<int>(0xA8ED84);
 actorFrameArbiter.begin(match,frame);
 // An explicitly captured limited aircraft click must leave failed/unused
 // actors alone. Replace old autonomous ownership only AFTER the new event
 // physically queues; early cancelNative would change every candidate.
 const bool takeover=cooperativeTakeover&&owner==taskarbiter::Owner::Cooperative&&authority==OrderAuthority::Local&&house==lastHouse&&mission==1&&!appendWaypoint;
 if(!actorFrameArbiter.canIssue(id,manualVersions.get(id))||
    (!actorLeaseArbiter.canIssue(match,frame,id,object,manualVersions.get(id),owner)&&!(takeover&&actorLeaseArbiter.find(id)>=0))){
  ++snap.commandRejected;return false;
 }
 gateway::MissionState before{};
 before.valid=true;before.mission=get<int>(object+0xAC);
 before.queued=get<int>(object+0xB4);before.carrier=get<uint32_t>(object+0x11C);
 if(foreign||appendWaypoint){auto order=intent::mega(get<int>(house+0x30),id,mission,target,resolvedTargetTag,dest,uint8_t(destTag),nearest,uint8_t(nearestTag));order.appendWaypoint=appendWaypoint;if(!submitConstructed(order,authority))return false;}
 else if(!submitMegaMission(house,id,mission,target,resolvedTargetTag,
                       dest,uint8_t(destTag),nearest,uint8_t(nearestTag)))return false;
 showOrderLines(object);
 actorFrameArbiter.record(id,manualVersions.get(id),owner);
 // A queued EventClass is not an executed mission. This read-only tracker
 // records the later state transition; unsupported events stay unconfirmed.
 uint32_t ticket=missionObserver.record(match,id,destTag==0x34?dest:target,mission,
                             frame,submittedAt,before,owner);
 taskarbiter::Lease superseded{};
 if(takeover){int old=actorLeaseArbiter.find(id);if(old>=0){superseded=actorLeaseArbiter.leases[old];actorLeaseArbiter.remove(old);}
  actorLeaseArbiter.record(match,frame,id,object,manualVersions.get(id),owner,ticket);
 }else actorLeaseArbiter.record(match,frame,id,object,manualVersions.get(id),owner,ticket,&superseded);
 if(superseded.serial)cancelReceiptTicket(superseded.match,superseded.serial);
 if(receiptID)*receiptID=ticket;
 if(!ticket)++snap.receiptDropped;
 else {gateway::Receipt queued{};
  gateway::Intent ticketIntent{match,submittedAt,id,destTag==0x34?dest:target,manualVersions.get(id)};
  gateway::planned(queued,ticket,ticketIntent);gateway::submitted(queued,submittedAt);
  queued.owner=owner;recordReceipt(queued);
 }
 return true;
}
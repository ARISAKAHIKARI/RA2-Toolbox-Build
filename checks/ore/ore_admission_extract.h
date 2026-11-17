static bool externalOrderValid(const planner::Order&o,uint32_t house){
 uint32_t unit=worldByID(o.unit);if(!unit||!own(unit,house)||manualVersions.get(o.unit)!=o.token||(stickyOwns(unit)&&o.purpose!=planner::MagnetronDropOrder))return false;
 if(get<int>(unit+0x6C)<=0||!get<bool>(unit+0x74)||get<bool>(unit+0x81)||!get<bool>(unit+0x90)||get<bool>(unit+0x1C8)||get<uint32_t>(unit+0x11C))return false;
 if(burstOwns(o.unit)||dodgeOwns(o.unit)||minerDefenseOwns(o.unit))return false;
 if(o.purpose==planner::OreHarvestOrder)return oreHarvestOrderValid(o,house);
 if(o.purpose==planner::MagnetronDropOrder)return o.mission==13&&!o.target&&!o.destination&&!o.destinationTag&&!o.pursuit&&o.refinery&&rawMagnetronDrop(unit,worldByID(o.refinery),house);
 int globalIndex=orderIndex(unit);if(globalIndex>=0&&nativeOrders[globalIndex].global&&(!sh->magnetronGlobal||(o.mission==2&&!o.pursuit)))return false;
 bool defense=o.purpose==planner::DefenseOrder,combat=o.purpose==1||o.purpose==3,mag=o.purpose==3;
 if(defense){if(!(sh->flags&AntiTheft)||!antiTheftBuilding(unit)||o.mission!=1||!o.target||o.destination||o.destinationTag||!antiTheftThreat(worldByID(o.target),house)||antiTheftCurrentTargetUsable(unit,house))return false;}
 if(combat){if(mag?!rawMagnetron(unit):!featureObjectAllowed(featuretypes::AdvancedAttack,unit))return false;int index=orderIndex(unit);if(!(sh->flags&(mag?MagnetronInput:AdvancedInput))||snap.nativeHookStatus!=1||index<0||nativeOrders[index].token!=o.token||nativeOrders[index].mode!=(mag?planner::MagnetronMove:planner::OrdinaryMove))return false;}
 else if(o.purpose==0||o.purpose==2||o.purpose==planner::OreYield){
  if(manualVersions.orePaused(o.unit))return false;
  if(((o.purpose==0||o.purpose==planner::OreYield)&&!(sh->flags&AutoOreReturn))||(o.purpose==2&&o.mission!=7))return false;
  uint32_t refinery=worldByID(o.refinery);
  if(!refinery||!own(refinery,house)||!rawOreCompatible(unit,refinery))return false;
  if(get<bool>(unit+0x6D1))return false;
  if(o.purpose==planner::OreYield){
   // Only the explicit managed yield may replace an en-route radio reservation.
   // Recheck distance at execution; never interrupt an arrival or unloading.
   uint32_t rt=typeOf(refinery,3);
   if(o.mission!=2||o.target||o.destinationTag!=11||rawDockBusy(refinery,unit)||
      ore::dockDistance(get<int>(unit+0x9C),get<int>(unit+0xA0),get<int>(refinery+0x9C),get<int>(refinery+0xA0),get<bool>(rt+0x16E5))<=ore::FarDockCells)return false;
  }else if(o.mission==2&&rawDockLinked(refinery,unit))return false;
  float storage[4];memcpy(storage,(void*)(unit+0x33C),sizeof storage);if(ore::storageAmount(storage)<=0)return false;
  // The EXE chooses the dock turn/nearest/cells. Recheck volatile raw preconditions here
  // to reject a dock occupied or foundation placed since its snapshot.
  if(o.mission==7&&(o.destination!=o.refinery||(o.purpose==0&&rawDockBusy(refinery,unit))))return false;
  if(o.mission==2){uint32_t cell=readMapCell(int(o.destination%1000),int(o.destination/1000));if(!cell||get<uint32_t>(cell+0x124)||get<int>(cell+0xEC)==2||get<int>(cell+0xEC)==7)return false;}
 }else if(!defense)return false;
 // Reject stale pre.6 Sticky proposals at the shared admission boundary.
 if(o.mission!=1&&o.mission!=2&&o.mission!=5&&o.mission!=7)return false;
 if(o.mission==5&&(!combat||o.target||o.destination||o.destinationTag||o.pursuit))return false;
 if(o.mission==1&&!o.target)return false;
 if(o.target){uint32_t target=worldByID(o.target);if((!combat&&!defense)||o.mission!=1||!target||!nativeEnemy(target,house)||get<int>(target+0x6C)<=0||!get<bool>(target+0x74)||get<bool>(target+0x81)||!get<bool>(target+0x90)||get<uint32_t>(target+0x11C)||get<int>(target+0x220)!=0)return false;
  planner::Actor source{},victim{};int sk=kind(unit),tk=kind(target);uint32_t st=typeOf(unit,sk),tt=typeOf(target,tk);if(!st||!tt)return false;
  victim.kind=tk;rawCombatInfo(unit,st,sk,source,true);rawCombatInfo(target,tt,tk,victim,false);
  int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
  if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)return false;
  if(combat&&!mag){auto probe=probeCombatPair(unit,target,source,victim,0);if(!(probe.flags&(planner::InWeaponRange|planner::NeedsReposition)))return false;}
  else if(mag?!planner::magneticAcquisition(source,victim,dx*dx+dy*dy,int(std::min(uint32_t(sh->advancedRangeCells),30u))*256):!planner::canTarget(source,victim,dx*dx+dy*dy,false,0))return false;
 }
 if(o.pursuit){
  if((o.purpose!=planner::CombatOrder&&o.purpose!=planner::MagnetronOrder)||o.mission!=2||o.target||o.destinationTag!=11)return false;
  uint32_t target=worldByID(o.pursuit);if(!target||!nativeEnemy(target,house))return false;
  planner::Actor source{},victim{};rawCombatInfo(unit,typeOf(unit,kind(unit)),kind(unit),source,true);rawCombatInfo(target,typeOf(target,kind(target)),kind(target),victim,false);
  int padding=int(std::min(uint32_t(sh->advancedRangeCells),30u))*256;
  if(mag){int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
   if(!planner::magneticAcquisition(source,victim,dx*dx+dy*dy,padding)||planner::canTarget(source,victim,dx*dx+dy*dy,true))return false;
  }else{auto probe=probeCombatPair(unit,target,source,victim,padding);
   if(!(probe.flags&planner::InAcquisitionRange)||(probe.flags&planner::InWeaponRange))return false;
  }
 }
 if(o.destinationTag==0x34){uint32_t destination=worldByID(o.destination);if(combat||o.mission!=7||!destination||!own(destination,house)||kind(destination)!=3)return false;}
 else if(o.destinationTag==11){if(o.mission!=2||!readMapCell(int(o.destination%1000),int(o.destination/1000)))return false;}
 else if(o.destinationTag||o.destination)return false;
 return true;
}
static int admitPlannerOrder(const planner::Order&o,uint32_t house,uint32_t sourceMatch,uint32_t sourceTick){
 // A fresh in-range target takes precedence over any delayed movement proposal.
 // Reject an obsolete movement proposal immediately so an external planner can
 // receive its receipt and use the next world; do not block its whole batch
 // until the original movement deadline expires.
 if(o.purpose==planner::CombatOrder&&o.mission==2&&advancedTargetInRange(o.unit,house))return -1;
 if(!externalOrderValid(o,house))return -1;
 // Explicit and global magnets may use different calculation locations.
 // Reserve queued magnetic targets at their common admission boundary too.
 if(o.purpose==planner::MagnetronOrder&&o.mission==1){int frame=get<int>(0xA8ED84);
  for(int n=0;n<nativeCount;++n){const auto&r=nativeOrders[n];if(r.id!=o.unit&&r.mode==planner::MagnetronMove&&r.attacking&&r.target==o.target&&frame>=r.sentFrame&&frame-r.sentFrame<12&&manualVersions.get(r.id)==r.token)return -1;}
 }
 actorFrameArbiter.begin(match,get<int>(0xA8ED84));
 if(!actorFrameArbiter.canIssue(o.unit,o.token)){++snap.planDeferred;return 0;}
 if(!planningRoom(0)||!outgoingRoom()){++snap.planDeferred;return 0;}
 // Identity provenance comes from the original captured World, not a
 // possibly recycled unit rebound after the Host finishes computing.
 const uint32_t destinationID=o.destinationTag==0x34?o.destination:0;
 const executionproof::Ticket proof{sourceMatch,house,sourceTick,
   {o.unit,o.unitPointer,o.token},
   {o.target,o.targetPointer},
   {destinationID,o.destinationPointer}};
 if(o.purpose==planner::MagnetronDropOrder){
  // Drop uses an Object Event rather than a MegaMission, so apply the same
  // original-identity guard explicitly before its native enqueue path.
  if(executionproof::verify(proof,{match,house,sampleClock(sh)},
      [](uint32_t uid){return worldByID(uid);},
      [](uint32_t uid){return manualVersions.get(uid);},
      [](uint32_t p,uint32_t h){return own(p,h);})!=executionproof::Failure::None)return -1;
  if(!nativeObjectEvent(get<int>(house+0x30),vanilla::EventType::Idle,o.unit,
                        OrderAuthority::Local,nullptr,true))return 0;
  int index=orderIndex(worldByID(o.unit));
  if(index>=0)nativeOrders[index].attacking=false;
  consumePlanning(0);return 1;
 }
 // Native Harvest click uses (10,nullptr,clickedCell,nullptr), not Move's
 // nearest-accessible override. Preserve that exact target packing.
 const bool nearest=o.purpose!=planner::OreHarvestOrder&&o.destinationTag==11&&(!sh||sh->megaQueueMode==0);
 if(!submitProvenMission(house,o.unit,o.mission,o.target,o.destination,o.destinationTag,
    o.purpose==3?taskarbiter::Owner::Magnetron:
      (o.purpose==1||o.purpose==planner::DefenseOrder)?taskarbiter::Owner::Combat:taskarbiter::Owner::Ore,
    proof,nullptr,nearest?o.destination:0,nearest?11:0))return -1;
 consumePlanning(0);minerDefenseRememberOre(o,house);
 if(o.purpose==1||o.purpose==3){int index=orderIndex(worldByID(o.unit));if(index>=0){auto&r=nativeOrders[index];r.stopped=o.mission==5;r.attacking=o.mission==1;r.target=o.target;r.sentFrame=get<int>(0xA8ED84);}}
 return 1;
}
static int admitPlannerOrder(const planner::Order&o,uint32_t house){
 return o.purpose==planner::OreHarvestOrder?admitPlannerOrder(o,house,match,0):-1;
}

// Raw observations are the default. Engine selectors/range calls require the
// explicit experimental option; both planners consume the resulting values.
struct CombatProbeTargetSample {
 uint32_t pointer,type,epoch;int kind,armor,x,y,z;uint32_t flags;
};
struct CombatProbeBatch {
 CombatProbeTargetSample*targets;uint32_t epoch=1,sourceEpoch=0,sourcePointer=0,sourceType=0;int sourceKind=-1;
 planner::Actor source{};
 void invalidate(){++epoch;sourceEpoch=0;}
};
static int sampledCombatWeaponQuery(CombatProbeBatch&batch,int targetIndex,uint32_t p,uint32_t q,uint32_t*weapon,int*selected,int*close,int*airborne){
 const int k=kind(p),tk=kind(q);const uint32_t type=typeOf(p,k),targetType=typeOf(q,tk);
 if(k<0||tk<0||!type||!targetType||get<bool>(p+0x1C8))return 0;
 // The raw collector calls no game routine and writes no game fields between
 // pairs. Reuse only this invocation's fresh selector samples. The published
 // Actor weapons use a 30-frame metadata cache and cannot replace these reads.
 if(batch.sourceEpoch!=batch.epoch||batch.sourcePointer!=p||batch.sourceType!=type||batch.sourceKind!=k){
  batch.source={};rawCombatInfo(p,type,k,batch.source,true);
  batch.source.x=get<int>(p+0x9C);batch.source.y=get<int>(p+0xA0);
  batch.sourcePointer=p;batch.sourceType=type;batch.sourceKind=k;batch.sourceEpoch=batch.epoch;
 }
 auto&sample=batch.targets[targetIndex];
 if(sample.epoch!=batch.epoch||sample.pointer!=q||sample.type!=targetType||sample.kind!=tk){
  planner::Actor target{};rawCombatInfo(q,targetType,tk,target,false);
  sample={q,targetType,batch.epoch,target.kind,target.armor,get<int>(q+0x9C),get<int>(q+0xA0),target.z,target.flags};
 }
 // weaponIndex/choose consume exactly kind, armor, XYZ and Airborne on the
 // target side. Compatibility below still consumes the original world Actor.
 planner::Actor target{};target.kind=sample.kind;target.armor=sample.armor;target.x=sample.x;target.y=sample.y;target.z=sample.z;target.flags=sample.flags;
 const auto choice=combatquery::choose(batch.source,target);if(choice.index<0)return 0;
 const auto ptr=rawWeapon(type,get<float>(p+0x150),choice.slot);if(ptr<0x10000)return 0;
 *weapon=ptr;*selected=choice.slot;*close=choice.close;*airborne=(target.flags&planner::Airborne)!=0;return 1;
}
static planner::CombatProbe probeCombatPair(uint32_t p,uint32_t q,const planner::Actor& actor,const planner::Actor& victim,int padding,CombatProbeBatch*batch=nullptr,int targetIndex=-1,bool rawOnly=false){
 planner::CombatProbe result{};result.target=get<uint32_t>(q+0x10);
 uint32_t weapon=0;int slot=0,close=0,airborne=0;
 int status;
 if(batch&&targetIndex>=0&&(rawOnly||!(sh&&sh->nativeCombatQueries)))status=sampledCombatWeaponQuery(*batch,targetIndex,p,q,&weapon,&slot,&close,&airborne);
 else {if(batch)batch->invalidate();status=combatWeaponQuery(p,q,&weapon,&slot,&close,&airborne);}
 if(status<0){result.flags=4;return result;}if(!status)return result;
 planner::Actor source=actor,target=victim;
 source.flags|=planner::FixedWeapon;source.weapons[0]=rawWeaponInfo(weapon,true);
 if(airborne)target.flags|=planner::Airborne;else target.flags&=~planner::Airborne;
 if(!planner::compatibleTarget(source,target,false))return result;
 result.slot=slot;result.effectiveness=source.weapons[0].effectiveness[target.armor];
 if(close){result.flags=planner::InWeaponRange|planner::InAcquisitionRange;return result;}
 int64_t dx=int64_t(get<int>(p+0x9C))-get<int>(q+0x9C),dy=int64_t(get<int>(p+0xA0))-get<int>(q+0xA0);
 if(dx>=-1048576&&dx<=1048576&&dy>=-1048576&&dy<=1048576){
  const auto&weapon=source.weapons[0];int64_t dz=(source.flags&planner::Airborne)?0:int64_t(source.z)-target.z;
  int64_t distance=dx*dx+dy*dy;if(std::abs(dz)<=1048576&&weapon.minimum>0&&distance+dz*dz<int64_t(weapon.minimum)*weapon.minimum)
   result.flags=planner::InAcquisitionRange|planner::NeedsReposition;
  else if(padding>0&&planner::canTarget(source,target,distance,false,padding))result.flags=planner::InAcquisitionRange;
 }
 return result;
}
static bool combatCandidate(const planner::Actor&a){return (a.flags&planner::Enemy)&&!(a.flags&(planner::Protected|planner::Cloaked|planner::Limbo|planner::OffMap|planner::Transported));}
static void collectCombatProbes(planner::World&w,bool rawOnly=false){
 static uint32_t probeMatch=0;static int cursor=0;if(probeMatch!=w.match){probeMatch=w.match;cursor=0;}
 // Scratch storage avoids a large game-thread stack allocation. Validity is
 // reset for every publication; no object sample survives into a later batch.
 static CombatProbeTargetSample targetSamples[planner::MaxActors];
 for(int n=0;n<w.count;++n)targetSamples[n].epoch=0;
 CombatProbeBatch batch{targetSamples};
 int enemies=0;for(int n=0;n<w.count;++n)enemies+=combatCandidate(w.actors[n]);
 int visited=0,queries=0,sources=0;
 // Finish a source atomically within this fresh world. At least one source
 // fits even at MaxActors; never publish half a candidate set as "no target".
 for(;visited<w.count&&sources<128;++visited){int index=(cursor+visited)%w.count;auto&a=w.actors[index];
  if(!(a.flags&planner::Own)||a.flags&(planner::Disabled|planner::Transported|planner::Limbo|planner::OffMap|planner::Manipulated))continue;
  uint32_t p=worldByID(a.id);int order=p?orderIndex(p):-1;
  if(order<0||nativeOrders[order].mode!=planner::OrdinaryMove||burstOwns(a.id)||dodgeOwns(a.id))continue;
  if(queries+enemies>planner::MaxCombatProbes||w.combatCount+enemies>planner::MaxCombatProbes)break;
  ++sources;const int first=w.combatCount;bool faulted=false;
  for(int n=0;n<w.count;++n){const auto&t=w.actors[n];if(!combatCandidate(t))continue;++queries;uint32_t q=worldByID(t.id);if(!q)continue;
   auto probe=probeCombatPair(p,q,a,t,int(std::min(uint32_t(sh->advancedRangeCells),30u))*256,&batch,n,rawOnly);
   if(probe.flags&4){faulted=true;break;}
   if(probe.flags)w.combat[w.combatCount++]=probe;
  }
  if(faulted){w.combatCount=first;continue;}
  a.combatFirst=first;a.combatCount=w.combatCount-first;
 }
 if(w.count)cursor=(cursor+visited)%w.count;
}
static bool advancedTargetInRange(uint32_t id,uint32_t house){
 uint32_t p=worldByID(id);if(!p||!own(p,house))return false;
 planner::Actor source{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),source,true);
 Vec all=vec(0xA8EC78);
 for(int n=0;n<all.count;++n){uint32_t q=item(all,n);if(!q||!nativeEnemy(q,house)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))continue;
  int k=kind(q);if(k<0)continue;planner::Actor target{};rawCombatInfo(q,typeOf(q,k),k,target,false);
  auto probe=probeCombatPair(p,q,source,target,0);if(probe.flags&(planner::InWeaponRange|4))return true;
 }
 return false;
}

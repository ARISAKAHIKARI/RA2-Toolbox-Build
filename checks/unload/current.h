static bool live(uint32_t p){return p>=0x10000&&get<int>(p+0x6c)>0&&get<bool>(p+0x74)&&!get<bool>(p+0x81)&&get<bool>(p+0x90);}
static int kind(uint32_t p){uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}
static bool own(uint32_t p,uint32_t house){return live(p)&&get<uint32_t>(p+0x21c)==house;}
static bool unloadPassengerEligible(uint32_t p,uint32_t house){
 return p&&worldObjects.contains(p)&&kind(p)==1&&get<uint32_t>(p+0x21C)==house&&
  get<int>(p+0x6C)>0&&
  // Loaded infantry is often Limbo/inactive until the engine ejects each one.
  // Enrollment must not be restricted to the first currently active exit.
  (get<uint32_t>(p+0x11C)||get<bool>(p+0x74)&&live(p));
}
static int unloadPassengerKnowledge(uint32_t p,uint32_t house,uint32_t&id,uint32_t&carrier){
 id=carrier=0;if(!p||!worldObjects.contains(p))return -1;
 uint32_t vt=0,owner=0;int hp=0;uint8_t onMap=0,alive=0,limbo=0;
 if(!read(p,&vt,sizeof vt)||!read(p+0x10,&id,sizeof id)||
    !read(p+0x11C,&carrier,sizeof carrier)||!read(p+0x21C,&owner,sizeof owner)||
    !read(p+0x6C,&hp,sizeof hp)||!read(p+0x74,&onMap,sizeof onMap)||
    !read(p+0x90,&alive,sizeof alive)||!read(p+0x81,&limbo,sizeof limbo))return 0;
 if(vt!=0x7EB058u||!id||owner!=house||hp<=0)return -1;
 if(!carrier&&(!onMap||!alive||limbo))return -1;
 return 1;
}
struct UnloadRosterEntry {uint32_t carrier,pointer,id,token;int history=-1;};
static UnloadRosterEntry unloadRoster[nativeorders::MaxPassengers*2];static int unloadRosterCount;
static void buildUnloadAdmissionRoster(uint32_t house){
 unloadRosterCount=0;Vec inf=vec(0xA83DE8);
 for(int n=0;n<inf.count&&unloadRosterCount<nativeorders::MaxPassengers;++n){uint32_t p=item(inf,n),carrier=p?get<uint32_t>(p+0x11C):0;
  if(!carrier||!worldObjects.contains(carrier)||queuedUnload.find(carrier,get<uint32_t>(carrier+0x10))<0||!unloadPassengerEligible(p,house)||!featureObjectAllowed(featuretypes::UnloadPassenger,p))continue;
  unloadRoster[unloadRosterCount++]={carrier,p,get<uint32_t>(p+0x10),manualVersions.get(get<uint32_t>(p+0x10)),-1};
 }
 for(int n=0;n<recentPassengerCount&&unloadRosterCount<nativeorders::MaxPassengers*2;++n){const auto&r=recentPassengers[n];
  if(queuedUnload.find(r.carrier,r.carrierID)>=0)unloadRoster[unloadRosterCount++]={r.carrier,r.p,r.id,r.token,n};
 }
 std::sort(unloadRoster,unloadRoster+unloadRosterCount,[](const auto&a,const auto&b){if(a.carrier!=b.carrier)return a.carrier<b.carrier;if(a.pointer!=b.pointer)return a.pointer<b.pointer;return a.history<b.history;});
}
// A Ctrl+D command may arrive after the game's native D began deploying.
// Reject stale/foreign carriers, not the transport's own unload-in-progress
// state; otherwise the queued command is acknowledged then discarded next tick.
// Three-state admission for an already-accepted Ctrl+D request.
//  1: native fields prove an eligible carrier
//  0: unreadable or query unavailable; do NOT retire the player's intent
// -1: complete roster/fields prove replacement, death or manual takeover
static int unloadCarrierKnowledge(const unloadadmission::Carrier&task,uint32_t house){
 const uint32_t p=task.pointer;
 if(!p||!worldObjects.contains(p))return -1;
 uint32_t id=0,vt=0,owner=0,carried=0;int hp=0,cargo=0;
 uint8_t map=0,alive=0,limbo=1;
 if(!read(p+0x10,&id,sizeof id)||!read(p,&vt,sizeof vt)||
    !read(p+0x21C,&owner,sizeof owner)||!read(p+0x6C,&hp,sizeof hp)||
    !read(p+0x114,&cargo,sizeof cargo)||!read(p+0x11C,&carried,sizeof carried)||
    !read(p+0x74,&map,sizeof map)||!read(p+0x90,&alive,sizeof alive)||
    !read(p+0x81,&limbo,sizeof limbo))return 0;
 if(id!=task.id||vt!=0x7F5C70u||owner!=house||hp<=0||!map||!alive||limbo||carried)return -1;
 if(!unloadadmission::retainAfterCarrierMove(task.token,manualVersions.get(task.id),cargo))return -1;
 // A failed optional TypeID/feature-query observation cannot establish death
 // or cancellation. The original 3.5s / 12s admission horizon still applies.
 if(!featureObjectAllowed(featuretypes::UnloadTransport,p))return 0;
 return 1;
}
static void observeUnloadRoster(uint32_t house,int frame,DWORD now){
 if(!(sh->flags&SmartUnloadOrders)||snap.chat){recentPassengerCount=0;return;}
 if(indexedFrame!=frame)return; // incomplete identity sample: retain history until retry
 for(int n=0;n<recentPassengerCount;){auto&r=recentPassengers[n];
  const bool expired=DWORD(now-r.tick)>1500||frame<r.frame||frame-r.frame>90;
  const bool present=worldObjects.contains(r.p);
  uint32_t uid=0;const bool known=present&&read(r.p+0x10,&uid,sizeof uid);
  if(expired||!present||manualVersions.get(r.id)!=r.token||(known&&uid!=r.id))
   r=recentPassengers[--recentPassengerCount];
  else ++n; // Unreadable UID is not proof the prior Ctrl+D passenger vanished.
 }
 static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> history;history.clear();
 for(int n=0;n<recentPassengerCount;++n)history.add(recentPassengers[n].p,n);
 Vec inf=vec(0xA83DE8);
 for(int n=0;n<inf.count;++n){uint32_t p=item(inf,n);if(!p)continue;uint32_t carrier=get<uint32_t>(p+0x11C);
  // Normal loaded passengers must be remembered too. 0x83 is an
  // exceptional vehicle state used by EjectAssist; requiring it here
  // left ordinary Ctrl+D with no pre-ejection passenger history.
  if(!carrier||!worldObjects.contains(carrier)||!own(carrier,house)||kind(carrier)!=2||
     !featureObjectAllowed(featuretypes::UnloadTransport,carrier)||get<uint32_t>(p+0x21C)!=house||
     !featureObjectAllowed(featuretypes::UnloadPassenger,p))continue;
  uint32_t uid=0,carrierUID=0;
  if(!read(p+0x10,&uid,sizeof uid)||!read(carrier+0x10,&carrierUID,sizeof carrierUID)||!uid||!carrierUID)continue;
  int at=history.find(p);
  if(at<0){if(recentPassengerCount==nativeorders::MaxPassengers)continue;at=recentPassengerCount++;history.add(p,at);}
  recentPassengers[at]={p,uid,carrier,carrierUID,typeOf(carrier,2),manualVersions.get(uid),now,frame};
 }
}
static void cancelNative(uint32_t p,bool manual=true){
 // If the original Ctrl+D has already unloaded the transporter, its
 // passenger roster may still await the next admission tick. Retain that
 // pending handoff while moving this now-empty transport. An interruption
 // with passengers still aboard cancels only the unstarted unload.
 const uint32_t carrierID=get<uint32_t>(p+0x10);
 if(queuedUnload.find(p,carrierID)<0||get<int>(p+0x114)>0)
  queuedUnload.cancel(p,carrierID);
 if(manual)releaseSticky(p);
 pauseGlobalMagnetron(p);
 cancelBurst(get<uint32_t>(p+0x10));cancelDodge(get<uint32_t>(p+0x10));cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);for(i=0;i<unloadCount;)
 // Transport commands must not cancel the passengers' independent post-ejection jobs.
 if(unloadPassengers[i].p==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}
 else ++i;}
static bool unloadCarrierEligible(uint32_t p,uint32_t house){return p&&worldObjects.contains(p)&&own(p,house)&&live(p)&&kind(p)==2&&featureObjectAllowed(featuretypes::UnloadTransport,p)&&!get<uint32_t>(p+0x11C);}
static int smartUnloadNow(uint32_t house,uint32_t p,bool firstAttempt=true){
 if(!unloadCarrierEligible(p,house))return 0;
 const int frame=get<int>(0xA8ED84);const DWORD now=sampleClock(sh);
 int cargoCount=0;uint32_t nativeCarrierID=0;
 if(!read(p+0x114,&cargoCount,sizeof cargoCount)||!read(p+0x10,&nativeCarrierID,sizeof nativeCarrierID))return -6;
 if(cargoCount<0||cargoCount>256||!nativeCarrierID)return -6;
 static uint32_t roster[nativeorders::MaxPassengers];int count=0;uint32_t previous=0;
 auto*begin=std::lower_bound(unloadRoster,unloadRoster+unloadRosterCount,p,[](const UnloadRosterEntry&r,uint32_t carrier){return r.carrier<carrier;});
 for(auto*at=begin;at<unloadRoster+unloadRosterCount&&at->carrier==p;++at){uint32_t q=at->pointer;
  if(q==previous)continue;
  uint32_t uid=0,carrier=0;const int known=unloadPassengerKnowledge(q,house,uid,carrier);
  if(!known)return -6; // Unknown later passenger must not be dropped.
  if(known<0||uid!=at->id||manualVersions.get(at->id)!=at->token||
     !featureObjectAllowed(featuretypes::UnloadPassenger,q))continue;
  if(at->history>=0){const auto&old=recentPassengers[at->history];
   if(old.carrierID!=nativeCarrierID||old.carrierType!=typeOf(p,2)||DWORD(now-old.tick)>1500||frame<old.frame||frame-old.frame>90||carrier)continue;
  }else if(carrier!=p)continue;
  if(count==nativeorders::MaxPassengers)return -6;
  previous=q;roster[count++]=q;
 }
  // Native cargo chain contains the entire load even while later occupants
 // remain inactive/limbo. Supplement the infantry inventory once per carrier
 // admission; do not turn a partial active-unit sample into a one-person job.
 if(cargoCount>0){
  uint32_t q=0;if(!read(p+0x118,&q,sizeof q))return -6;
  // A transport can contain vehicles only, or have no infantry-chain head
  // while vanilla Deploy is still pending. Issue at most one native Deploy,
  // but retain the Ctrl+D queue until a complete passenger handoff exists.
  const bool awaitingNativeRoster=!q&&!count;
  uint32_t seen[256]{};int scanned=0;
  while(!awaitingNativeRoster&&q&&scanned<cargoCount){
   uint32_t id=0,carrier=0,vt=0;
   if(!read(q,&vt,sizeof vt))return -6;
   int known=0;
   if(vt==0x7EB058u)known=unloadPassengerKnowledge(q,house,id,carrier);
   else {
    // The native cargo chain contains vehicles as well as infantry. Visit
    // every link, but create post-ejection work only for actual infantry.
    if(!read(q+0x10,&id,sizeof id)||!read(q+0x11C,&carrier,sizeof carrier))return -6;
    known=id? -1:0;
   }
   if(!known)return -6;
   if(carrier!=p)break;
   if(known<0&&vt==0x7EB058u)break; // Proven-invalid infantry, not vehicle cargo.
   bool duplicate=false;for(int k=0;k<scanned;++k)if(seen[k]==q){duplicate=true;break;}
   if(duplicate)break;
   seen[scanned++]=q;
   if(known>0&&featureObjectAllowed(featuretypes::UnloadPassenger,q)){
    const uint32_t token=manualVersions.get(id);
    for(auto*at=begin;at<unloadRoster+unloadRosterCount&&at->carrier==p;++at)
     if(at->pointer==q&&(at->id!=id||at->token!=token))return -6;
    bool enrolled=false;for(int k=0;k<count;++k)if(roster[k]==q){enrolled=true;break;}
    if(!enrolled){if(count==nativeorders::MaxPassengers)return -6;roster[count++]=q;}
   }
   uint32_t next=0;if(!read(q+0x30,&next,sizeof next))return -6;
   q=next;
  }
  // Validate every native link, including the final null tail.
  if(!awaitingNativeRoster&&(scanned!=cargoCount||q))return -6;
 }
 const bool loaded=cargoCount>0;
  // Missing infantry is not proof of an empty transport: it may carry tanks,
 // or the infantry list may lag native D. Issue at most one ordinary Deploy
 // on initial admission, but retain this Ctrl+D for later passenger enrolment.
 // Return 2 means "native event issued, passenger admission still pending".
 if(!count){
  if(!loaded||!firstAttempt||get<bool>(p+0x1C8)||get<int>(p+0xAC)==16||get<int>(p+0xB4)==16)return -6;
  return nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload)?2:-6;
 }
 int replace=0;for(int j=0;j<unloadCount;++j)if(unloadPassengers[j].p==p||unloadPassengers[j].carrier==p)++replace;
 if(count>nativeorders::MaxPassengers-unloadCount+replace)return -6;
 // Queue acceptance is separate from this native Deploy. Existing active
 // unload and empty historical carriers never receive a second Deploy.
 // Original D can already have initiated unloading. Never queue a second
 // native Deploy while its in-progress state is present.
 if(loaded&&!get<bool>(p+0x1C8)&&get<int>(p+0xAC)!=16&&get<int>(p+0xB4)!=16&&
    !nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload))return -6;
 // A NEW Ctrl+D supersedes this carrier's prior unload episode. Moving
 // the carrier does not; the passenger's own manual order does.
 for(int j=0;j<unloadCount;)if(unloadPassengers[j].carrier==p){
  finishUnloadFollowup(unloadPassengers[j],frame);
  unloadPassengers[j]=unloadPassengers[--unloadCount];
 }else ++j;
 cancelNative(p);
 for(int j=0;j<count;++j){uint32_t q=roster[j];auto&r=unloadPassengers[unloadCount++];r={};r.job=++unloadJobSerial;r.p=q;r.id=get<uint32_t>(q+0x10);r.carrier=p;r.carrierID=get<uint32_t>(p+0x10);r.carrierType=typeOf(p,2);r.frame=frame;r.manualToken=manualVersions.get(r.id);}
 return 1;
}
static int smartUnload(uint32_t house){
 if(!(sh->flags&SmartUnloadOrders)||!loadInterfaces())return -12;
 if(!simulationReady()||get<bool>(0xA8BC60+25))return -11;
 const DWORD now=sampleClock(sh);queuedUnload.begin(match,house,now);
 static unloadadmission::Carrier candidates[unloadadmission::Capacity];static unloadpolicy::RosterLookup<unloadadmission::Capacity> selected;selected.clear();
 static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> recent;recent.clear();
 const int frame=get<int>(0xA8ED84);
 for(int n=0;n<recentPassengerCount;++n){const auto&r=recentPassengers[n];
  if(DWORD(now-r.tick)<=1500&&frame>=r.frame&&frame-r.frame<=90)recent.add(r.carrier,n);
 }
 int count=0,added=0;Vec v=vec(0xA8ECB8);
 for(int n=0;n<v.count;++n){uint32_t p=item(v,n);if(!unloadCarrierEligible(p,house)||selected.find(p)>=0)continue;
  uint32_t id=get<uint32_t>(p+0x10);int old=recent.find(p);
  if(get<int>(p+0x114)<=0&&(old<0||recentPassengers[old].carrierID!=id||recentPassengers[old].carrierType!=typeOf(p,2)))continue;
  if(count==unloadadmission::Capacity)return -42;
  selected.add(p,count);candidates[count++]={p,id,manualVersions.get(id)};
  if(queuedUnload.find(p,id)<0)++added;
 }
 // Preflight the whole request; never report a silently truncated prefix.
 if(added>unloadadmission::Capacity-queuedUnload.count)return -42;
 for(int n=0;n<count;++n)queuedUnload.add(candidates[n]);
 // Latch the passenger identities at the command boundary, not one frame
 // later: native D may unlink their carrier before admissionTick runs.
 // This only remembers members of the explicitly selected own transports;
 // all UID/manual-command/owner/filter checks still run at admission.
 if(count){
  static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> tracked;tracked.clear();
  for(int n=0;n<recentPassengerCount;++n)tracked.add(recentPassengers[n].p,n);
  Vec inf=vec(0xA83DE8);
  for(int n=0;n<inf.count;++n){uint32_t q=item(inf,n);if(!q)continue;
   uint32_t carrier=get<uint32_t>(q+0x11C);
   if(selected.find(carrier)<0||!unloadPassengerEligible(q,house)||!featureObjectAllowed(featuretypes::UnloadPassenger,q))continue;
   const uint32_t id=get<uint32_t>(q+0x10),carrierID=get<uint32_t>(carrier+0x10);
   int at=tracked.find(q);
   if(at<0){if(recentPassengerCount==nativeorders::MaxPassengers)continue;at=recentPassengerCount++;tracked.add(q,at);}
   recentPassengers[at]={q,id,carrier,carrierID,typeOf(carrier,2),manualVersions.get(id),now,frame};
  }
 }
 return count; // acknowledgement means arranged, not physically unloaded
}

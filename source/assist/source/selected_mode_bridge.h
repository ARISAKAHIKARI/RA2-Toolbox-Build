// Explicit selected-ground-unit modes. All simulation changes are native
// MegaMission events, through the same SP DoList / MP OutList gateway.
static int selectedModeCommand(uint32_t house,int mission){
 if(mission!=0&&mission!=5&&mission!=23)return -4;
 if(!eventdelivery::mayQueue(get<int>(0xA8B238)))return -40;
 // Explicit mailbox commands may originate from the tool's own buttons.
 // Automatic pointer features need nativeReady's foreground gate; these
 // deliberate requests still require the full simulation/session boundary.
 if(!simulationReady()||get<bool>(0xA8BC60+25))return -11;
 // Use the same bounded, frozen-cohort command continuation as own-ground.
 // Queue pressure must not silently discard the unsubmitted selection tail.
 auto&job=ownground::job;
 if(job.pending)return -6;
 Vec selected=vec(0xA8ECB8);if(selected.count<0||selected.count>ownground::Capacity)return -42;
 job.count=job.next=job.sent=0;job.mission=mission;job.request=sh->request;
 job.epoch=match;job.started=sampleClock(sh);job.frame=-1;job.voiced=false;
 int sticky=0;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);int k=kind(p);
  if(!worldObjects.contains(p)||!own(p,house)||!live(p)||(k!=1&&k!=2)||
     get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2B0)||get<bool>(p+0x1C8))continue;
  // A normal event cannot promise to release Sticky. In particular, never
  // let cancellation invoke the SP-only local ForceMission exit here.
  if(stickyOwns(p)||get<int>(p+0xAC)==6||get<int>(p+0xB4)==6){++sticky;continue;}
  const uint32_t id=get<uint32_t>(p+0x10);
  job.sources[job.count++]={p,id,manualVersions.get(id),0,false};
 }
 if(!job.count)return sticky?-41:0;
 job.pending=true;ownGroundTick(house,get<int>(0xA8ED84),job.started);
 return job.pending?-30:sh->result>0?int(sh->count):int(sh->result);
}

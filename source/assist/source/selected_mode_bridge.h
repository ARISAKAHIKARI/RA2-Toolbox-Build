// Explicit selected-ground-unit modes. All simulation changes are native
// MegaMission events, through the same SP DoList / MP OutList gateway.
static int selectedModeCommand(uint32_t house,int mission){
 if(mission!=0&&mission!=5&&mission!=23)return -4;
 if(!eventdelivery::mayQueue(get<int>(0xA8B238)))return -40;
 // Explicit mailbox commands may originate from the tool's own buttons.
 // Automatic pointer features need nativeReady's foreground gate; these
 // deliberate requests still require the full simulation/session boundary.
 if(!simulationReady()||get<bool>(0xA8BC60+25))return -11;
 Vec selected=vec(0xA8ECB8);int sent=0,sticky=0;uint32_t voice=0;bool failed=false;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);int k=kind(p);
  if(!worldObjects.contains(p)||!own(p,house)||!live(p)||(k!=1&&k!=2)||
     get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2B0)||get<bool>(p+0x1C8))continue;
  // A normal event cannot promise to release Sticky. In particular, never
  // let cancellation invoke the SP-only local ForceMission exit here.
  if(stickyOwns(p)||get<int>(p+0xAC)==6||get<int>(p+0xB4)==6){++sticky;continue;}
  if(!outgoingRoom()){failed=true;break;}
  // Same explicit-input hand-back as pointer commands: invalidate stale
  // automation tokens/leases before recording the new native order receipt.
  cancelNative(p,false);
  if(!nativeMission(house,get<uint32_t>(p+0x10),mission,0,0,0)){failed=true;break;}
  ++sent;if(!voice)voice=p;
 }
 if(voice)inputFeedback(voice,0,false);
 return sent?sent:failed?-6:sticky?-41:0;
}

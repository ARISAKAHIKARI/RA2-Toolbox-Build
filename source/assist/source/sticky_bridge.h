// Explicit player-only Sticky. Network entry uses the original synchronized
// mission event; the local ForceMission/NextMission exit is single-player only.
struct StickyHold {uint32_t pointer,id,house,epoch;int queuedFrame=0;bool network=false,observed=false;};
static StickyHold stickyHolds[1024];
static bool stickySession(){int s=get<int>(0xA8B238);return s==0||s==5;}
static bool stickyABI(){
 const uint8_t force[]={0x8B,0x81,0xAC,0,0,0,0x83,0xEC,0x0C,0x83};
 const uint8_t next[]={0x8B,0x81,0xB4,0,0,0,0x83,0xEC,0x0C,0x83};uint8_t bytes[10];
 return read(0x5B2FD0,bytes,10)&&!memcmp(bytes,force,10)&&read(0x5B3570,bytes,10)&&!memcmp(bytes,next,10);
}
static int stickyIndex(uint32_t p){for(int i=0;i<stickyCount;++i)if(stickyHolds[i].pointer==p&&stickyHolds[i].id==get<uint32_t>(p+0x10)&&stickyHolds[i].epoch==match)return i;return -1;}
static bool stickyOwns(uint32_t p){return stickyIndex(p)>=0;}
static bool releaseSticky(uint32_t p){int i=stickyIndex(p);if(i<0)return true;const auto h=stickyHolds[i];
 if(h.network||!stickySession()||!gameentry::active(gameentry::Client::Assist)||!stickyABI())return false;
 if(own(p,h.house)&&get<uint32_t>(0xA83D4C)==h.house){int current=get<int>(p+0xAC),queued=get<int>(p+0xB4);
  if(current==6){if(queued>=0&&queued!=6)call<void>(0x5B3570,p);else call<void>(0x5B2FD0,p,5);}
  else if(queued==6)call<void>(0x5B2FD0,p,current);
 }
 stickyHolds[i]=stickyHolds[--stickyCount];return true;
}
static void stickyTick(bool releaseAll=false){
 if(!stickyCount)return;int frame=get<int>(0xA8ED84);indexWorld(frame);
 for(int i=0;i<stickyCount;){auto h=stickyHolds[i];
  if(h.epoch!=match||!worldObjects.contains(h.pointer)||get<uint32_t>(h.pointer+0x10)!=h.id||!own(h.pointer,h.house)){stickyHolds[i]=stickyHolds[--stickyCount];continue;}
  int current=get<int>(h.pointer+0xAC),queued=get<int>(h.pointer+0xB4);
  if(h.network){
   // Observe delivery without modifying the peer's mission. A failed/unseen
   // request expires locally; it is never re-sent by this maintenance pass.
   if(current==6||queued==6)stickyHolds[i].observed=true;
   if(releaseAll||(stickyHolds[i].observed&&current!=6&&queued!=6)||(!stickyHolds[i].observed&&frame>=h.queuedFrame&&frame-h.queuedFrame>900)){stickyHolds[i]=stickyHolds[--stickyCount];continue;}
   ++i;continue;
  }
  if(releaseAll||current!=6||(queued>=0&&queued!=6)){if(releaseSticky(h.pointer))continue;}++i;
 }
}
static int stickyCommand(uint32_t house){
 bool network=eventdelivery::isMultiplayer(get<int>(0xA8B238));
 if(!network&&!stickySession())return -40;if(!nativeReady()||(!network&&!stickyABI()))return -11;
 Vec selected=vec(0xA8ECB8);int count=0;uint32_t voice=0;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);int k=kind(p);
  if(!worldObjects.contains(p)||!own(p,house)||(k!=1&&k!=2)||get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2B0)||get<bool>(p+0x1C8))continue;
  if(stickyOwns(p)){if(!network&&releaseSticky(p))++count;continue;}
  if(network&&(get<int>(p+0xAC)==6||get<int>(p+0xB4)==6))continue;
  if(stickyCount>=1024)break;cancelNative(p);
  if(network){
   if(!nativeMission(house,get<uint32_t>(p+0x10),6,0,0,0))continue;
   stickyHolds[stickyCount++]={p,get<uint32_t>(p+0x10),house,match,get<int>(0xA8ED84),true,false};++count;if(!voice)voice=p;continue;
  }
  if(!nativeObjectEvent(get<int>(house+0x30),vanilla::EventType::Idle,get<uint32_t>(p+0x10)))continue;
  call<void>(0x5B2FD0,p,6);
  if(get<int>(p+0xAC)!=6)continue;
  stickyHolds[stickyCount++]={p,get<uint32_t>(p+0x10),house,match};++count;if(!voice)voice=p;
 }
 if(voice)inputFeedback(voice,0,false);return count;
}

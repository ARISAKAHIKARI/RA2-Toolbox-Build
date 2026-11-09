// Button and hotkey share this host-side selection intent. No game call here.
static uint32_t lastIdleAircraft=0,lastAircraftMatch=0,lastAircraftPID=0;
static void selectNextIdleAircraft(){
 if(!enabled||!foregroundGame()||!commandFresh(GetTickCount())||snap.renderStatus!=1||snap.chat||!externalData||
    !externalSampler.idleAircraftTick||uint32_t(GetTickCount()-externalSampler.idleAircraftTick)>500||
    externalSampler.idleAircraftCount<=0||externalSampler.idleAircraftCount>8192)return;
 if(lastAircraftMatch!=commandMatch()||lastAircraftPID!=pid){lastAircraftMatch=commandMatch();lastAircraftPID=pid;lastIdleAircraft=0;}
 auto first=externalSampler.idleAircraftIDs;auto end=first+externalSampler.idleAircraftCount;
 auto next=std::upper_bound(first,end,lastIdleAircraft);if(next==end)next=first;lastIdleAircraft=*next;
 sendSelection(-4,false,nullptr,*next);
}

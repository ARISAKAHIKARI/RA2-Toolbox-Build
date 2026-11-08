static controllink::View commandLink(){return {shared!=nullptr,lastData!=0,shared&&shared->stop!=0,
 incoming.status,incoming.commandStatus,incoming.match};}
static uint32_t commandMatch(){return controllink::identity(commandLink());}
static bool commandChannelReady(){return controllink::ready(commandLink());}
static bool commandFresh(DWORD now){return commandChannelReady()&&DWORD(now-lastData)<1500;}

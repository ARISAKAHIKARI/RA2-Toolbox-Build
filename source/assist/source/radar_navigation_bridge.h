#include "../../shared/source/debug_entry_checks.h"
// A latest-only host mailbox. Camera movement is local UI and uses the existing
// game-thread dispatcher, never the unit order/event queue.
static void radarNavigationTick(DWORD now){
 LONG seq=sh->cameraSeq;if((seq&1)||uint32_t(seq)==sh->cameraAck)return;
 MemoryBarrier();uint32_t epoch=sh->cameraMatch,tick=sh->cameraTick;smallcamera::Point p{sh->cameraTargetX,sh->cameraTargetY};MemoryBarrier();if(seq!=sh->cameraSeq)return;
 sh->cameraAck=uint32_t(seq);
 if(!sh->foreground||snap.chat||epoch!=match||DWORD(now-tick)>200||p.x<-100000||p.x>100000||p.y<-100000||p.y>100000)return;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return;
 static const uint8_t signature[]={0x83,0xEC,8,0x56,0x57,0x8B,0x7C,0x24,0x14};
 uint32_t tactical=get<uint32_t>(0x887324);if(tactical<0x10000||!debugentries::pattern(debugentries::Kind::Call,0x6D6000,signature,sizeof signature,read))return;
 smallcamera::clamp(cameraGeometry(),p);call<void>(0x6D6000,tactical,&p);
}

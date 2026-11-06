#include "../../shared/source/debug_entry_checks.h"
// Manual social commands run under the existing GameRuntime request lease.
// Cheer constructs the original AllCheer event through submitConstructed.
// Taunts are chat/network UI commands, NOT simulation EventClass packets.
static int socialCommand(uint32_t house,int command,int number,DWORD now){
 if(!gameentry::active(gameentry::Client::Assist)||!simulationReady()||get<uint32_t>(0xA83D4C)!=house)return -3;
 if(get<uint32_t>(0xA8B538))return -4; // original modal input exclusion
 if(command==Cheer){
  int frame=get<int>(0xA8ED84),previous=get<int>(0x84555C);
  uint32_t rules=get<uint32_t>(0x8871E0);if(rules<0x10000)return -3;
  int interval=std::clamp(get<int>(rules+0x580),0,36000);
  if(frame>=previous&&int64_t(previous)+interval>=frame)return -22;
  static const uint8_t ctor[]={0x53,0x8B,0x5C,0x24,0x0C,0x8B,0xC3,0x56};
  if(!debugentries::pattern(debugentries::Kind::Call,0x4C66C0,ctor,sizeof ctor,read))return -12;
  intent::EventOrder event{};event.domain=vanilla::Domain::NoOptions;
  event.eventKind=vanilla::EventType::AllCheer;event.player=get<int>(house+0x30);
  if(!submitConstructed(event))return -6;
  put<int>(0x84555C,frame); // same local key cooldown as AllToCheerCommand
  return 1;
 }
 if(command!=Taunt||number<1||number>8)return -4;
 int session=get<int>(0xA8B238);if(session!=3&&session!=4)return -13;
 if(!get<uint8_t>(session==3?0xA8D110:0xA8D111))return -23;
 DWORD previous=get<DWORD>(0x825C88);if(previous!=UINT32_MAX&&DWORD(now-previous)<=5000)return -22;
 // CommandClass::Array and Execute layout are from YRpp. Resolve the actual
 // registered object rather than fabricate a taunt packet or call a mid-body.
 Vec commands=vec(0x87F658,4096);
 for(int i=0;i<commands.count;++i){uint32_t object=item(commands,i);if(object<0x10000)continue;
  uint32_t table=get<uint32_t>(object);if(table<0x10000)continue;
  if(get<uint32_t>(table+4)!=0x5362B0||get<int>(object+4)!=number)continue;
  uint32_t execute=get<uint32_t>(table+0x20);
  if(execute!=0x536350)return -12;
  static const uint8_t signature[]={0xA1,0x38,0xB2,0xA8,0,0x81,0xEC,0xD0,1,0,0};
  if(!debugentries::pattern(debugentries::Kind::Call,0x536350,signature,sizeof signature,read))return -12;
  call<void>(execute,object,0u);return 1;
 }
 return -12;
}

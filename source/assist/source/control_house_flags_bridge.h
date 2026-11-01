// A data-only opt-in. This never changes IsHumanPlayer, CurrentPlayer, object
// ownership, code, events, or the native shared control predicate.
#include "control_house_flags.h"
static controlflags::Model controlHouseFlags;
static bool controlHouseFlagFailed=false;
static bool controlCodeRetired();
static bool riskReady(bool);
static bool controlHouseFlagWrite(uint32_t at,const void*value,size_t size){
 if(size!=1||!gameentry::active(gameentry::Client::Assist))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery((void*)(uintptr_t)at,&info,sizeof info)||info.State!=MEM_COMMIT||
    (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))||
    uintptr_t(at)+size>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
 const uint8_t byte=*(const uint8_t*)value;put<uint8_t>(at,byte);
 uint8_t after=0;return read(at,&after,1)&&after==byte;
}
static bool controlHouseFlagsPending(){return controlHouseFlags.pending();}
static bool restoreControlHouseFlags(){
 const bool ok=controlHouseFlags.restoreAll(sessionTracker.epoch,read,controlHouseFlagWrite);
 if(!ok&&sh)sh->playerControlStatus=-2;
 return ok;
}
static void controlHouseFlagMaintenance(){
 // Run before native adapters so a mode change cannot overlap old data with
 // a new patch group. Focus loss is not a reason to retire this ownership.
 if(!sh||!controlmode::data(sh->controlAll)){
  if(restoreControlHouseFlags())controlHouseFlagFailed=false;
 }else if(controlHouseFlags.epoch&&(controlHouseFlags.epoch!=sessionTracker.epoch||controlHouseFlags.world(sessionTracker.epoch,read)!=controlflags::State::Current)){
  if(restoreControlHouseFlags())controlHouseFlagFailed=false;
 }
}
static void controlHouseFlagTick(){
 if(!sh||!controlmode::data(sh->controlAll))return;
 if(!riskReady(false)){sh->playerControlStatus=-2;return;}
 if(!controlCodeRetired()){sh->playerControlStatus=-2;return;}
 if(controlHouseFlagFailed){restoreControlHouseFlags();sh->playerControlStatus=-2;return;}
 if(!controlHouseFlags.epoch&&!controlHouseFlags.bind(sessionTracker.epoch,read)){sh->playerControlStatus=-2;return;}
 if(controlHouseFlags.epoch!=sessionTracker.epoch||controlHouseFlags.world(sessionTracker.epoch,read)!=controlflags::State::Current){sh->playerControlStatus=-2;return;}
 Vec houses=vec(0xA80228,32);bool ok=houses.count>0;
 for(int i=0;i<houses.count;++i){
  auto&r=controlHouseFlags.records[i];controlflags::Identity identity{};
  if(!controlflags::capture(unsigned(i),identity,read)){ok=false;continue;}
  if(!playableHouse(identity.pointer)){if(!controlflags::restore(r,read,controlHouseFlagWrite))ok=false;continue;}
  if(!controlflags::apply(r,identity,read,controlHouseFlagWrite))ok=false;
 }
 if(!ok){controlHouseFlagFailed=true;restoreControlHouseFlags();sh->playerControlStatus=-2;return;}
 controlHouseFlags.frame=get<int>(0xA8ED84);
 sh->playerControlStatus=2; // Applied data, NOT proof of native control/network effect.
}

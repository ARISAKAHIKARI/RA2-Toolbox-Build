// Retired House-flag ownership recovery only; never enables a feature.
#include "control_house_flags.h"
static controlflags::Model controlHouseFlags;
static bool controlHouseFlagFailed=false;
static bool controlCompanionsRetired();
static bool controlTransitionPending();
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
// ABI 86 retires the data experiment. No activation route remains: this
// helper only preserves restoration responsibility for an already-held record.
static void controlHouseFlagMaintenance(){
 if(restoreControlHouseFlags())controlHouseFlagFailed=false;
}

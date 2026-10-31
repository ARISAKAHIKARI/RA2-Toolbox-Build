#include "tech_enhancement.h"
static techenhancement::Record enhancedTypes[techenhancement::Count];
static uint32_t enhancementEpoch=0,enhancementHouse=0;
static int enhancementFrame=-1,enhancementSession=-1;
static DWORD enhancementScan=0;
static bool enhancementRequested=false,enhancementConflict=false,enhancementRestoring=false;
static bool enhancementWrite(uint32_t at,const void*p,size_t n){
 if(n!=4||!gameentry::active(gameentry::Client::Assist))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery((void*)(uintptr_t)at,&info,sizeof info)||info.State!=MEM_COMMIT||
    (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY))||
    uintptr_t(at)+n>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
 int value=0;memcpy(&value,p,4);put<int>(at,value);int after=0;return read(at,&after,4)&&after==value;
}
static techenhancement::IdentityState enhancementWorld(){
 using State=techenhancement::IdentityState;
 if(!enhancementEpoch||enhancementEpoch!=sessionTracker.epoch)return State::Retired;
 uint32_t house=0;int session=-1,frame=-1;
 if(!read(0xA83D4C,&house,4)||!read(0xA8B238,&session,4)||!read(0xA8ED84,&frame,4))return State::Unavailable;
 return house==enhancementHouse&&session==enhancementSession&&frame>=enhancementFrame?State::Current:State::Retired;
}
static bool restoreTechEnhancement(){
 const auto result=techenhancement::restoreRecords(enhancedTypes,enhancementWorld(),read,enhancementWrite);
 enhancementRequested=false;enhancementRestoring=!result.complete;enhancementConflict=!result.ok;
 if(result.complete){enhancementEpoch=enhancementHouse=0;enhancementFrame=-1;enhancementSession=-1;}
 if(sh){sh->techEnhanceStatus=result.ok?techenhancement::Off:techenhancement::Incompatible;sh->techEnhanceCount=result.retained;}
 if(result.changed){enhancementChangedCatalogue=true;catalogueDirty=true;catalogueReady=false;catalogueAt=0;}
 return result.complete;
}
static void techEnhancementTick(DWORD now){
 if(!sh)return;
 // Restoration owns the original world/values until it has really finished.
 if(enhancementRestoring&&!restoreTechEnhancement())return;
 if(!sh->techEnhance){if(enhancementRequested)restoreTechEnhancement();return;}
 // TechLevel is a simulation rule, not a synchronized EventClass operation.
 if(!techenhancement::mayApply(get<int>(0xA8B238))){
  if(enhancementRequested&&!restoreTechEnhancement())return;
  sh->techEnhanceStatus=techenhancement::SingleplayerOnly;sh->techEnhanceCount=0;return;
 }
 if(enhancementRequested&&enhancementWorld()!=techenhancement::IdentityState::Current&&!restoreTechEnhancement())return;
 if(!enhancementRequested){
  enhancementRequested=true;enhancementConflict=false;enhancementEpoch=sessionTracker.epoch;
  enhancementHouse=get<uint32_t>(0xA83D4C);enhancementFrame=get<int>(0xA8ED84);enhancementSession=get<int>(0xA8B238);enhancementScan=now-1000;
 }
 if(DWORD(now-enhancementScan)<500)return;enhancementScan=now;
 unsigned count=0;bool changed=false;
 for(unsigned i=0;i<techenhancement::Count;++i){const auto&rule=techenhancement::Rules[i];auto&r=enhancedTypes[i];
  const auto identity=r.pointer?techenhancement::inspect(rule,r,read):techenhancement::IdentityState::Retired;
  if(identity==techenhancement::IdentityState::Unavailable){enhancementConflict=true;continue;}
  if(r.pointer&&identity==techenhancement::IdentityState::Retired){
   // Replaced/freed catalogues are never restored through old pointers.
   if(r.owned)enhancementConflict=true;r={};
  }
  if(!r.pointer){Vec types=vec(rule.array,4096);for(int j=0;j<types.count;++j){uint32_t p=item(types,j);char id[24]{};
   if(p>=0x10000&&read(p+0x24,id,sizeof id)&&memchr(id,0,sizeof id)&&!strcmp(id,rule.id)){r.pointer=p;r.items=types.items;r.index=j;break;}
  }}
  if(!r.pointer)continue;
  const bool owned=r.owned;
  if(!techenhancement::apply(rule,r,read,enhancementWrite))enhancementConflict=true;
  changed|=!owned&&r.owned;if(r.owned&&!r.conflict)++count;
 }
 sh->techEnhanceCount=count;sh->techEnhanceStatus=enhancementConflict?-2:1;
 if(changed){
  enhancementChangedCatalogue=true;catalogueDirty=true;catalogueReady=false;catalogueAt=0;scanTypes();
  if(snap.techOpen)requestTechRefresh();
 }
}

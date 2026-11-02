#include "tech_data_policy.h"
#include "technology_mode_policy.h"
static techdata::Record techDataRecords[MaxTypes];
static uint32_t techDataScenario=0,techDataHouse=0,techDataEpoch=0;
static int techDataSession=-1,techDataFrame=-1;
static bool techDataRestoring=false;
static bool techDataWrite(uint32_t at,const void*p,size_t n){
 if((n!=1&&n!=4)||!gameentry::active(gameentry::Client::Assist))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery((void*)(uintptr_t)at,&info,sizeof info)||info.State!=MEM_COMMIT||
    (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY))||
    uintptr_t(at)+n>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
 memcpy((void*)(uintptr_t)at,p,n);uint32_t after=0,wanted=0;memcpy(&wanted,p,n);
 return read(at,&after,n)&&after==wanted;
}
static techdata::State techDataWorld(){
 if(!techDataScenario)return techdata::State::Retired;
 uint32_t scenario=0;int session=-1,frame=-1;
 if(!read(0xA8B230,&scenario,4)||!read(0xA8B238,&session,4)||!read(0xA8ED84,&frame,4))return techdata::State::Unavailable;
 return scenario==techDataScenario&&session==techDataSession&&frame>=techDataFrame?techdata::State::Current:techdata::State::Retired;
}
static bool techDataPending(){for(const auto&r:techDataRecords)if(r.owned)return true;return false;}
static bool techDataOwns(uint32_t type){for(const auto&r:techDataRecords)if(r.owned&&r.identity.pointer==type)return true;return false;}
static bool restoreTechData(){
 const auto world=techDataWorld();bool ok=true,changed=techDataPending();
 for(auto&r:techDataRecords)if(!techdata::restore(r,world,read,techDataWrite))ok=false;
 techDataRestoring=techDataPending();
 if(!techDataRestoring){techDataScenario=techDataHouse=techDataEpoch=0;techDataSession=techDataFrame=-1;}
 if(changed){enhancementChangedCatalogue=true;catalogueDirty=true;catalogueReady=false;catalogueAt=0;}
 return ok&&!techDataRestoring;
}
static bool techDataBlocksEnhancement(){
 if(techmode::parameters(techImplementation)&&sh&&(sh->flags&UnsafeTech))return true;
 if(techImplementation==3&&sh&&(sh->flags&UnsafeTech)){
  // High-water tracking is per game tick, not the once/second sidebar refresh.
  int frame=-1;const auto state=techDataWorld();
  if(techDataScenario&&(state!=techdata::State::Current||techDataHouse!=lastHouse||techDataEpoch!=sessionTracker.epoch))restoreTechData();
  else if(techDataScenario&&read(0xA8ED84,&frame,4))techDataFrame=frame;
  return true;
 }
 if(techDataPending()||techDataRestoring)restoreTechData();
 return techDataPending()||techDataRestoring;
}
static bool applyTechData(uint32_t house){
 // Keep the two independent TechLevel writers from capturing each other's
 // output as an original value. The saved TechEnhance preference is retained.
 if(!restoreTechEnhancement())return false;
 uint32_t scenario=0;int session=-1,frame=-1;
 if(!sessionTracker.epoch||!read(0xA8B230,&scenario,4)||scenario<0x10000||!read(0xA8B238,&session,4)||
    !read(0xA8ED84,&frame,4)||frame<0||frame>=INT32_MAX||!house)return false;
 if(techDataScenario&&(techDataRestoring||techDataScenario!=scenario||techDataSession!=session||frame<techDataFrame||
    techDataHouse!=house||techDataEpoch!=sessionTracker.epoch)&&!restoreTechData())return false;
 // An allow-list change first restores no-longer-requested records. Failure
 // retains their journal and blocks the entire refreshed presentation.
 for(auto&r:techDataRecords)if(r.captured){bool keep=false;
  for(int i=0;i<snap.typeCount;++i)if(techAllowed[i]&&typePointers[i]==r.identity.pointer){keep=true;break;}
  if(!keep&&!techdata::restore(r,techDataWorld(),read,techDataWrite)){techDataRestoring=true;return false;}
 }
 techDataScenario=scenario;techDataSession=session;techDataFrame=frame;techDataHouse=house;techDataEpoch=sessionTracker.epoch;
 for(int i=0;i<snap.typeCount;++i)if(techAllowed[i]){
  techdata::Record*record=nullptr;for(auto&r:techDataRecords)if(r.captured&&r.identity.pointer==typePointers[i]){record=&r;break;}
  if(!record)for(auto&r:techDataRecords)if(!r.captured){record=&r;break;}
  if(!record||(!record->captured&&!techdata::capture(*record,typePointers[i],snap.types[i].kind,snap.types[i].index,read))||
     !techdata::apply(*record,read,techDataWrite)){
   techDataRestoring=true;restoreTechData();return false;
  }
 }
 return true;
}

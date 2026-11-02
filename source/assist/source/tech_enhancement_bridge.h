#include "tech_enhancement.h"
static techenhancement::Record enhancedTypes[techenhancement::Count];
static uint32_t enhancementEpoch=0,enhancementHouse=0;
static int enhancementFrame=-1,enhancementSession=-1;
static DWORD enhancementScan=0;
static bool enhancementRequested=false,enhancementConflict=false;
static bool enhancementWrite(uint32_t at,const void*p,size_t n){
 if(n!=4||!gameentry::active(gameentry::Client::Assist))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery((void*)(uintptr_t)at,&info,sizeof info)||info.State!=MEM_COMMIT||
    (info.Protect&(PAGE_GUARD|PAGE_NOACCESS|PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY))||
    uintptr_t(at)+n>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
 int value=0;memcpy(&value,p,4);put<int>(at,value);int after=0;return read(at,&after,4)&&after==value;
}
static bool enhancementSameWorld(){
 return enhancementEpoch&&enhancementEpoch==sessionTracker.epoch&&enhancementHouse==get<uint32_t>(0xA83D4C)&&
  enhancementSession==get<int>(0xA8B238)&&get<int>(0xA8ED84)>=enhancementFrame;
}
static void restoreTechEnhancement(){
 bool ok=true,changed=false;const bool same=enhancementSameWorld();
 for(unsigned i=0;i<techenhancement::Count;++i){auto&r=enhancedTypes[i];
  if(same&&r.owned){changed=true;if(!techenhancement::restore(techenhancement::Rules[i],r,read,enhancementWrite))ok=false;}
  r={};
 }
 enhancementEpoch=enhancementHouse=0;enhancementFrame=-1;enhancementSession=-1;
 enhancementRequested=false;enhancementConflict=!ok;
 if(sh){sh->techEnhanceStatus=ok?0:-2;sh->techEnhanceCount=0;}
 if(changed){enhancementChangedCatalogue=true;catalogueDirty=true;catalogueReady=false;catalogueAt=0;}
}
static void techEnhancementTick(DWORD now){
 if(!sh)return;
 if(!sh->techEnhance){if(enhancementRequested)restoreTechEnhancement();return;}
 if(enhancementRequested&&!enhancementSameWorld())restoreTechEnhancement();
 if(!enhancementRequested){
  enhancementRequested=true;enhancementConflict=false;enhancementEpoch=sessionTracker.epoch;
  enhancementHouse=get<uint32_t>(0xA83D4C);enhancementFrame=get<int>(0xA8ED84);enhancementSession=get<int>(0xA8B238);enhancementScan=now-1000;
 }
 if(DWORD(now-enhancementScan)<500)return;enhancementScan=now;
 unsigned count=0;bool changed=false;
 for(unsigned i=0;i<techenhancement::Count;++i){const auto&rule=techenhancement::Rules[i];auto&r=enhancedTypes[i];
  if(r.pointer&&!techenhancement::identity(rule,r,read)){
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

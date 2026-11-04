#pragma once
#include "crc_shadow_policy.h"
#include <stddef.h>
namespace assist {namespace crcshadow {
enum class Observation { Current, Retired, Unavailable };
struct World {uint32_t epoch,scenario,items,local,id;int mode;};
template<class Read> constexpr Observation observeWorld(const World&w,uint32_t epoch,Read read){
 if(!w.epoch||w.epoch!=epoch)return Observation::Retired;
 uint32_t scenario=0,items=0,local=0,id=0;int mode=-1;
 if(!read(0xA8B230,&scenario,4)||!read(0xA8022C,&items,4)||!read(0xA83D4C,&local,4)||!read(0xA8B238,&mode,4))return Observation::Unavailable;
 if(scenario!=w.scenario||items!=w.items||local!=w.local||mode!=w.mode)return Observation::Retired;
 if(!read(local+0x10,&id,4))return Observation::Unavailable;
 return id==w.id?Observation::Current:Observation::Retired;
}
template<class Read> constexpr Observation observeIdentity(const Identity&i,uint32_t expectedItems,Read read){
 if(!i.valid()||i.pointer<0x10000)return Observation::Retired;
 uint32_t items=0,p=0,id=0;int count=0,index=-1;
 if(!read(0xA8022C,&items,4)||!read(0xA80238,&count,4))return Observation::Unavailable;
 if(items<0x10000||count<0||count>32)return Observation::Unavailable;
 if(items!=expectedItems||i.index>=unsigned(count))return Observation::Retired;
 if(!read(items+4*i.index,&p,4))return Observation::Unavailable;
 if(p!=i.pointer)return Observation::Retired;
 if(!read(p+0x10,&id,4)||!read(p+0x30,&index,4))return Observation::Unavailable;
 return id==i.id&&index==int(i.index)?Observation::Current:Observation::Retired;
}
struct Restoration {bool complete=true,conflict=false;unsigned retained=0;};
// Restore only the already-owned MapIsClear byte. No new CRC substitution,
// event, native call, cell/Visionary rollback or RNG operation is introduced.
template<class Read,class Write> constexpr Restoration restoreData(Model&model,const World&w,uint32_t epoch,int frame,int lastFrame,Read read,Write write){
 Restoration result{};const auto world=observeWorld(w,epoch,read);
 if(world==Observation::Retired||(frame>=0&&frame<lastFrame))return result;
 for(const auto&e:model.entries())if(e.active&&e.known&&!e.blocked&&e.original!=e.display){
  const auto identity=world==Observation::Unavailable||frame<0?Observation::Unavailable:observeIdentity(e.identity,w.items,read);
  if(identity==Observation::Retired)continue;
  uint8_t live=0;
  if(identity==Observation::Unavailable||!read(e.identity.pointer+0x241,&live,1)){
   result.complete=false;++result.retained;continue;
  }
  // A previous restoration may have written successfully but failed read-back.
  if(live==e.original){model.naturalWrite(e.identity,e.original);continue;}
  if(live!=e.display){result.complete=false;result.conflict=true;++result.retained;continue;}
  if(!write(e.identity.pointer+0x241,&e.original,1)){result.complete=false;++result.retained;continue;}
  model.naturalWrite(e.identity,e.original);
 }
 return result;
}
}}

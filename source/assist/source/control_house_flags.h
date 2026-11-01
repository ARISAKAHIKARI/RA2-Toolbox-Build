#pragma once
#include <stdint.h>
#include <stddef.h>
namespace assist { namespace controlflags {
constexpr uint32_t FlagOffset=0x1ED; // IsInPlayerControl, NOT IsHumanPlayer (+1EC).
constexpr unsigned Capacity=32;
enum class State { Current, Retired, Unavailable };
struct Identity {uint32_t pointer=0,id=0,type=0,vtable=0;int index=-1;};
struct Record {Identity identity{};uint8_t original=0;bool owned=false,conflict=false;};
template<class Read> bool capture(unsigned slot,Identity&out,Read read){
 uint32_t items=0,p=0;int count=0;
 if(!read(0xA8022C,&items,4)||!read(0xA80238,&count,4)||items<0x10000||count<0||count>int(Capacity)||slot>=unsigned(count)||
    !read(items+4*slot,&p,4)||p<0x10000)return false;
 Identity v{};v.pointer=p;
 if(!read(p,&v.vtable,4)||!read(p+0x10,&v.id,4)||!read(p+0x34,&v.type,4)||!read(p+0x30,&v.index,4)||
    v.type<0x10000||v.vtable<0x10000||v.index!=int(slot))return false;
 out=v;return true;
}
template<class Read> State inspect(const Identity&v,Read read){
 uint32_t items=0,p=0;int count=0;
 if(!read(0xA8022C,&items,4)||!read(0xA80238,&count,4)||items<0x10000||count<0||count>int(Capacity))return State::Unavailable;
 if(v.index<0||v.index>=count)return State::Retired;
 if(!read(items+4*v.index,&p,4))return State::Unavailable;
 if(p!=v.pointer)return State::Retired;
 Identity now{};now.pointer=p;
 if(!read(p,&now.vtable,4)||!read(p+0x10,&now.id,4)||!read(p+0x34,&now.type,4)||!read(p+0x30,&now.index,4))return State::Unavailable;
 return now.id==v.id&&now.type==v.type&&now.vtable==v.vtable&&now.index==v.index?State::Current:State::Retired;
}
template<class Read,class Write> bool apply(Record&r,const Identity&identity,Read read,Write write){
 if(r.owned){
  if(inspect(r.identity,read)!=State::Current)return false;
  uint8_t value=0;if(!read(r.identity.pointer+FlagOffset,&value,1))return false;
  if(value!=1)r.conflict=true;
  return !r.conflict&&value==1;
 }
 if(inspect(identity,read)!=State::Current)return false;
 uint8_t value=0;if(!read(identity.pointer+FlagOffset,&value,1)||value>1)return false;
 if(value==1)return true; // Already set by the game/another owner: never claim it.
 r.identity=identity;r.original=value;r.conflict=false;r.owned=true;
 const uint8_t one=1;
 // Register responsibility BEFORE writing: failure can mean write succeeded
 // but the read-back failed. Only a verified restore may release this record.
 return write(identity.pointer+FlagOffset,&one,1);
}
template<class Read,class Write> bool restore(Record&r,Read read,Write write){
 if(!r.owned)return true;
 const State state=inspect(r.identity,read);
 if(state==State::Unavailable)return false;
 if(state==State::Retired){r={};return true;}
 uint8_t value=0;if(!read(r.identity.pointer+FlagOffset,&value,1))return false;
 if(value==r.original){r={};return true;}
 if(r.conflict||value!=1){r.conflict=true;return false;}
 if(!write(r.identity.pointer+FlagOffset,&r.original,1))return false;
 r={};return true;
}
struct Model {
 Record records[Capacity]{};
 uint32_t epoch=0,scenario=0;int session=-1,frame=-1;
 bool pending()const{for(const auto&r:records)if(r.owned)return true;return false;}
 template<class Read> State world(uint32_t currentEpoch,Read read)const{
  // A local-player change also increments the command epoch. It does not
  // prove the old Houses died: restore their validated records in that case.
  // Scenario/session changes or a frame rewind DO retire the saved world.
  (void)currentEpoch;if(!epoch)return State::Retired;
  uint32_t nowScenario=0;int nowSession=-1,nowFrame=-1;
  if(!read(0xA8B230,&nowScenario,4)||!read(0xA8B238,&nowSession,4)||!read(0xA8ED84,&nowFrame,4))return State::Unavailable;
  return nowScenario==scenario&&nowSession==session&&nowFrame>=frame?State::Current:State::Retired;
 }
 template<class Read> bool bind(uint32_t currentEpoch,Read read){
  if(pending()||!currentEpoch)return false;
  uint32_t s=0;int mode=-1,f=-1;
  if(!read(0xA8B230,&s,4)||!read(0xA8B238,&mode,4)||!read(0xA8ED84,&f,4)||s<0x10000||f<0)return false;
  epoch=currentEpoch;scenario=s;session=mode;frame=f;return true;
 }
 template<class Read,class Write> bool restoreAll(uint32_t currentEpoch,Read read,Write write){
  const State state=world(currentEpoch,read);bool ok=true;
  for(auto&r:records){
   if(state==State::Retired){r={};continue;}
   if(r.owned&&(state==State::Unavailable||!restore(r,read,write)))ok=false;
  }
  if(!pending()){epoch=scenario=0;session=frame=-1;}
  return ok&&!pending();
 }
};
}}

#include "../source/tech_enhancement.h"
#include <stddef.h>
using namespace techenhancement;
// These execute the production templates during C++17 constant evaluation.
// No private engine, Windows API or imitation of the mutation policy is used.
struct Memory {
 enum Write { Success, FailBefore, FailAfter } mode=Success;
 uint32_t pointer=0x20000,items=0x30000,table=Rules[0].vtable;
 int count=1,level=-1,writes=0,attempts=0;
 uint32_t unreadable=0;bool foreignID=false,terminated=true;
 constexpr bool read(uint32_t at,uint32_t*out,size_t n){
  if(at==unreadable||n!=4)return false;
  if(at==Rules[0].array+4)*out=items;
  else if(at==items)*out=pointer;
  else if(at==0x20000)*out=table;
  else return false;return true;
 }
 constexpr bool read(uint32_t at,int*out,size_t n){
  if(at==unreadable||n!=4)return false;
  if(at==Rules[0].array+16)*out=count;
  else if(at==0x20000+LevelOffset)*out=level;
  else return false;return true;
 }
 constexpr bool read(uint32_t at,char*out,size_t n){
  if(at==unreadable||at!=0x20024||n!=24)return false;
  for(unsigned i=0;i<24;++i)out[i]=i<7?Rules[0].id[i]:terminated?0:'X';
  if(foreignID)out[0]='?';return true;
 }
 constexpr bool write(uint32_t at,const int*value,size_t n){
  if(at!=0x20000+LevelOffset||n!=4)return false;
  ++attempts;if(mode==FailBefore)return false;
  level=*value;++writes;return mode!=FailAfter;
 }
};
struct Fixture {
 Memory memory{};Record records[Count]{};
 constexpr Fixture(){records[0].pointer=memory.pointer;records[0].items=memory.items;}
 constexpr bool apply(){return techenhancement::apply(Rules[0],records[0],
  [this](uint32_t at,auto*out,size_t n){return memory.read(at,out,n);},
  [this](uint32_t at,const int*value,size_t n){return memory.write(at,value,n);});}
 constexpr Restoration restore(IdentityState world=IdentityState::Current){return restoreRecords(records,world,
  [this](uint32_t at,auto*out,size_t n){return memory.read(at,out,n);},
  [this](uint32_t at,const int*value,size_t n){return memory.write(at,value,n);});}
};
constexpr bool fullWord(){Fixture f;if(!f.apply()||!f.records[0].owned||f.records[0].original!=-1||f.memory.level!=1)return false;
 auto r=f.restore();return r.complete&&r.ok&&f.memory.level==-1&&!f.records[0].pointer&&f.memory.writes==2;}
constexpr bool failedApply(bool after){Fixture f;f.memory.mode=after?Memory::FailAfter:Memory::FailBefore;
 if(f.apply()||!f.records[0].owned||f.records[0].original!=-1)return false;
 f.memory.mode=Memory::Success;auto r=f.restore();return r.complete&&r.ok&&f.memory.level==-1&&!f.records[0].owned&&f.memory.writes==(after?2:0);}
constexpr bool failedRestore(bool after){Fixture f;if(!f.apply())return false;f.memory.mode=after?Memory::FailAfter:Memory::FailBefore;
 auto first=f.restore();if(first.complete||first.ok||first.retained!=1||!f.records[0].owned||f.records[0].original!=-1)return false;
 f.memory.mode=Memory::Success;auto next=f.restore();return next.complete&&next.ok&&f.memory.level==-1&&!f.records[0].pointer&&f.memory.writes==2;}
constexpr bool readFailure(uint32_t at){Fixture f;if(!f.apply())return false;f.memory.unreadable=at;
 auto first=f.restore();if(first.complete||first.ok||first.retained!=1||!f.records[0].owned||f.memory.writes!=1)return false;
 f.memory.unreadable=0;auto next=f.restore();return next.complete&&next.ok&&f.memory.level==-1;}
constexpr bool retired(unsigned which){Fixture f;if(!f.apply())return false;
 if(which==0)f.memory.pointer+=0x1000;else if(which==1)f.memory.items+=0x1000;
 else if(which==2)f.memory.table+=4;else if(which==3)f.memory.foreignID=true;
 else if(which==4)f.memory.count=0;else f.memory.terminated=false;
 auto r=f.restore();return r.complete&&r.ok&&f.memory.writes==1&&!f.records[0].pointer;}
constexpr bool foreignField(){Fixture f;if(!f.apply())return false;f.memory.level=7;
 auto r=f.restore();if(r.complete||r.ok||r.retained!=1||!f.records[0].owned||!f.records[0].conflict||f.memory.writes!=1||f.memory.level!=7)return false;
 f.memory.level=-1;r=f.restore();return r.complete&&r.ok&&f.memory.writes==1;}
constexpr bool worldUnavailable(){Fixture f;if(!f.apply())return false;auto r=f.restore(IdentityState::Unavailable);
 if(r.complete||r.ok||r.retained!=1||!f.records[0].owned||f.memory.writes!=1)return false;
 r=f.restore();return r.complete&&r.ok&&f.memory.level==-1;}
constexpr bool worldRetired(){Fixture f;if(!f.apply())return false;auto r=f.restore(IdentityState::Retired);
 return r.complete&&r.ok&&!r.changed&&f.memory.writes==1&&!f.records[0].pointer;}
constexpr bool invalidVector(){Fixture f;if(!f.apply())return false;f.memory.count=-1;auto r=f.restore();
 return !r.complete&&!r.ok&&r.retained==1&&f.records[0].owned&&f.memory.writes==1;}
constexpr bool ineligible(){Fixture f;f.memory.level=6;return f.apply()&&!f.records[0].owned&&!f.memory.writes&&f.memory.level==6;}
static_assert(fullWord(),"32-bit value and successful rollback");
static_assert(failedApply(false),"ownership before a write that fails before mutation");
static_assert(failedApply(true),"ownership before failed read-back after mutation");
static_assert(failedRestore(false),"keep original and retry failed restoration");
static_assert(failedRestore(true),"idempotent retry after restore modified memory but reported failure");
static_assert(readFailure(Rules[0].array+4),"catalogue read failure is not retirement");
static_assert(readFailure(0x30000),"slot read failure is not retirement");
static_assert(readFailure(0x20000),"vtable read failure is not retirement");
static_assert(readFailure(0x20024),"ID read failure is not retirement");
static_assert(readFailure(0x20000+LevelOffset),"field read failure retains ownership");
static_assert(retired(0),"replaced pointer is never restored");
static_assert(retired(1),"replaced catalogue is never restored");
static_assert(retired(2),"different native type is never restored");
static_assert(retired(3),"different ID is never restored");
static_assert(retired(4),"removed entry is never restored");
static_assert(retired(5),"unterminated ID is rejected");
static_assert(foreignField(),"foreign value is not overwritten; already-original state may retire");
static_assert(worldUnavailable(),"unknown world retains journal");
static_assert(worldRetired(),"retired world cannot be mutated");
static_assert(invalidVector(),"corrupt/incomplete vector is not evidence of retirement");
static_assert(ineligible(),"ordinary non-hidden type stays unchanged");
static_assert(eligible(Rules[9],10)&&!eligible(Rules[9],101)&&!eligible(Rules[0],10),"LUNR exception is bounded");
int main(){}

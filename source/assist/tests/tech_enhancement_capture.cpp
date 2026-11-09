// Actual cold-capture block; admission is already passed. A separate oracle
// checks stable identities, per-rule first matches and failed-array recovery.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "tech_enhancement.h"
#include "tech_parameter_lookup.h"
using DWORD=uint32_t;using LONG=int32_t;using namespace assist;
namespace techparameters {
struct TypeIdentity {uint32_t pointer=0,id=0,table=0;};
struct EnhancedIdentity {TypeIdentity type;uint32_t items=0;int index=-1,level=0;};
struct Policy {volatile LONG enhanced=0;EnhancedIdentity enhancement[techenhancement::Count]{};techlookup::PointerIndex<64>enhancementIndex;}policy;
static DWORD enhancementParameterScan=0;
static bool enhancedRequested(){return true;}
}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
struct Type {uint32_t pointer,table,uid;int level;char id[24]{};};
struct Array {uint32_t address,items;std::vector<uint32_t>members;int count=-1,capacity=-1;};
static std::array<Array,3>arrays;
static std::vector<Type>types;
static uint32_t blocked=0;static uint64_t safeReads=0,directReads=0,visits=0,checks=0;
static void check(bool condition){++checks;assert(condition);}
static void reset(){
 arrays={Array{0xA8E348,0x100000},Array{0xA83CE0,0x110000},Array{0xA8B218,0x120000}};types.clear();blocked=0;safeReads=directReads=visits=0;
 techparameters::policy={};techparameters::enhancementParameterScan=0;
}
static Type&add(unsigned array,const char*id,int level=-1,uint32_t uid=1){
 const uint32_t tables[]={0x7EB610,0x7F6218,0x7E2868};const uint32_t pointer=0x10000000+uint32_t(types.size())*0x1000;
 Type type{pointer,tables[array],uid,level};std::strncpy(type.id,id,24);types.push_back(type);arrays[array].members.push_back(pointer);return types.back();
}
static const Type*find(uint32_t pointer){if(pointer<0x10000000)return nullptr;const size_t i=(pointer-0x10000000)/0x1000;return i<types.size()?&types[i]:nullptr;}
static bool bytes(uint32_t p,void*out,size_t n){
 for(const auto&a:arrays){
  if(p==a.address&&n==sizeof(Vec)){const int count=a.count<0?int(a.members.size()):a.count,capacity=a.capacity<0?int(a.members.size()):a.capacity;const Vec v{0,a.items,capacity,0,count,1};std::memcpy(out,&v,n);return true;}
  if(p>=a.items&&uint64_t(p)+n<=uint64_t(a.items)+a.members.size()*4&&n==4){++visits;std::memcpy(out,&a.members[(p-a.items)/4],4);return true;}
 }
 const auto*t=find(p);if(!t)return false;const uint32_t offset=p-t->pointer;
 if(offset==0&&n==4)std::memcpy(out,&t->table,4);else if(offset==0x10&&n==4)std::memcpy(out,&t->uid,4);
 else if(offset==0x634&&n==4)std::memcpy(out,&t->level,4);else if(offset==0x24&&n==24)std::memcpy(out,t->id,24);else return false;return true;
}
static bool read(uint32_t p,void*out,size_t n){++safeReads;return !(blocked>=p&&uint64_t(blocked)<uint64_t(p)+n)&&bytes(p,out,n);}
template<class T>static T get(uint32_t p){++directReads;T result{};check(bytes(p,&result,sizeof result));return result;}
static LONG InterlockedExchange(volatile LONG*p,LONG v){auto prior=*p;*p=v;return prior;}static void MemoryBarrier(){}
#include "tech_capture_subject.inc"
static uint64_t identity(){uint64_t hash=1469598103934665603ull;for(const auto&t:techparameters::policy.enhancement)for(uint32_t n:{t.type.pointer,t.type.id,t.type.table,t.items,uint32_t(t.index),uint32_t(t.level)})hash=(hash^n)*1099511628211ull;return hash;}
static void oracle(){
 using namespace techparameters;check(policy.enhancementIndex.ready());
 for(unsigned rule=0;rule<techenhancement::Count;++rule){const auto&r=techenhancement::Rules[rule];EnhancedIdentity expected;
  for(const auto&a:arrays)if(a.address==r.array){
   const int count=a.count<0?int(a.members.size()):a.count,capacity=a.capacity<0?int(a.members.size()):a.capacity;
   if(count<0||count>4096||count>capacity||a.items<0x10000||(blocked>=a.address&&blocked<a.address+sizeof(Vec)))continue;
   for(int n=0;n<count;++n){const auto*t=find(a.members[n]);if(!t||!t->uid||t->table!=r.vtable||!std::memchr(t->id,0,24)||std::strcmp(t->id,r.id)||!techenhancement::eligible(r,t->level))continue;
    if((blocked>=t->pointer&&blocked<t->pointer+4)||(blocked>=t->pointer+0x24&&blocked<t->pointer+0x3C))continue;
    expected={{t->pointer,t->uid,t->table},a.items,n,t->level};break;
   }
  }
  const auto&actual=policy.enhancement[rule];check(!std::memcmp(&actual,&expected,sizeof actual));
  if(actual.type.pointer){const unsigned found=policy.enhancementIndex.find(policy.enhancement,actual.type.pointer,[](const EnhancedIdentity&t){return t.type.pointer;});check(found<=rule&&policy.enhancement[found].type.pointer==actual.type.pointer);}
 }
}
static void scan(const char*name,DWORD now){executeOriginalScanBlock(now);oracle();techparameters::policy.enhanced=1;
 std::printf("{\"case\":\"%s\",\"identity\":%llu,\"safe_reads\":%llu,\"direct_reads\":%llu,\"visits\":%llu}\n",name,(unsigned long long)identity(),(unsigned long long)safeReads,(unsigned long long)directReads,(unsigned long long)visits);
}
int main(){
 for(unsigned count:{128u,512u,4096u}){reset();for(unsigned a=0;a<3;++a)for(unsigned n=0;n<count;++n)add(a,"NON_TARGET");const auto label="no_targets_"+std::to_string(count);scan(label.c_str(),1000);
  const auto first=safeReads,firstDirect=directReads,firstVisits=visits;executeOriginalScanBlock(1001);check(safeReads==first&&directReads==firstDirect&&visits==firstVisits);executeOriginalScanBlock(1500);check(safeReads==first*2&&directReads==firstDirect*2&&visits==firstVisits*2);
 }
 reset();for(unsigned a=0;a<3;++a)add(a,"NON_TARGET");for(unsigned i=0;i<techenhancement::Count;++i)add(i<10?0:i<18?1:2,techenhancement::Rules[i].id,i==9?2:-1,i+1);scan("all_named_rules",1000);
 for(unsigned a=0;a<3;++a)for(unsigned n=0;n<512;++n)add(a,"NON_TARGET");safeReads=directReads=visits=0;scan("all_rules_before_long_tail",1500);check(visits<=118);
 reset();add(1,"BUS",-1,0);auto first=add(1,"BUS",-1,2).pointer;add(1,"BUS",-1,3);arrays[1].members.push_back(first);add(0,"LUNR",1);add(0,"LUNR",100,5);add(0,"LUNR",101,6);scan("duplicate_names_uid_zero_and_level_bounds",1000);check(techparameters::policy.enhancement[10].type.pointer==first);
 reset();auto&wrong=add(1,"BUS");wrong.table=0x7EB610;auto&unterminated=add(1,"BUS");std::memset(unterminated.id,'A',24);add(1,"BUS",0);add(1,"BUS",-2);add(1,"BUS",101);add(1,"BUS",-1,7);scan("invalid_candidates",1000);
 reset();add(0,"PRES");add(1,"BUS");add(2,"V3ROCKET");blocked=arrays[1].address+4;scan("array_unreadable",1000);blocked=0;scan("array_recovered",1500);
 reset();add(0,"PRES");add(1,"BUS");add(2,"V3ROCKET");arrays[0].count=4097;arrays[1].capacity=0;scan("invalid_array_bounds",1000);arrays[0].count=-1;arrays[1].capacity=-1;scan("array_bounds_recovered",1500);
 reset();auto blockedType=add(1,"BUS").pointer;add(0,"PRES");blocked=blockedType+0x24;scan("id_unreadable",1000);blocked=0;scan("id_recovered",1500);
 blocked=blockedType;scan("vtable_unreadable",2000);blocked=0;techparameters::policy.enhancementIndex.reset();scan("unready_index_recaptures",2001);
 std::strcpy(types[0].id,"REMOVED");scan("old_candidate_removed",2501);check(!techparameters::policy.enhancement[10].type.pointer);
 reset();add(1,"BUS");scan("clock_before_wrap",0xFFFFFF00u);const auto count=safeReads;executeOriginalScanBlock(0x10u);check(safeReads==count);scan("clock_after_wrap_due",0x100u);
 std::printf("{\"case\":\"summary\",\"checks\":%llu,\"passed\":true}\n",(unsigned long long)checks);
}

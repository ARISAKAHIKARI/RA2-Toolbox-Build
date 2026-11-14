// Candidate selection is local; authorization must still use current memory.
// The runner extracts both current and pinned pre-change callback branches.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <sys/mman.h>
#include "tech_parameter_lookup.h"
#include "tech_enhancement.h"
#include "tech_enhancement_parameter_policy.h"
#include "technology_mode_policy.h"
#include "tech_ui_breakpoint_hot.h"
using namespace assist;
using LONG=int32_t;
static unsigned checks=0,callbackCases=0,flagCases=0;
static void check(bool ok,const char*what){++checks;if(!ok){std::fprintf(stderr,"FAIL %s (check %u)\n",what,checks);std::exit(1);}}
static constexpr uint32_t House=0x10000000,Type=0x10020000,Unknown=0x10021000,Items=0x10030000,Stack=0x10040000,Scenario=0x10050000;
static bool mapped(uint32_t p,size_t n){return (p>=0x400000&&uint64_t(p)+n<=0xB00000)||(p>=0x10000000&&uint64_t(p)+n<=0x10100000);}
static bool inWorld=false;static unsigned worldCalls=0,worldReads=0,totalReads=0,nativeChecks=0,enhancementChecks=0;
static uint32_t unreadable=0;
static bool read(uint32_t p,void*out,size_t n){++totalReads;if(inWorld)++worldReads;if(!mapped(p,n)||(unreadable>=p&&uint64_t(unreadable)<uint64_t(p)+n))return false;std::memcpy(out,reinterpret_cast<void*>(uintptr_t(p)),n);return true;}
template<class T>static T get(uint32_t p){T v{};read(p,&v,sizeof v);return v;}
template<class T>static void put(uint32_t p,T v){assert(mapped(p,sizeof v));std::memcpy(reinterpret_cast<void*>(uintptr_t(p)),&v,sizeof v);}
static LONG InterlockedExchange(volatile LONG*p,LONG v){const LONG before=*p;*p=v;return before;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){const LONG before=*p;if(before==expected)*p=v;return before;}
namespace subject {
static void AssistTechEnhancementProbeCall(){}
struct Context {uint32_t Eip=0x4F7951,Esp=Stack,Eax=99,Ebx=10,Ecx=Type,Edx=98,Ebp=House,Esi=97,Edi=Type,EFlags=0x1F7202;};
constexpr unsigned UnsafeTech=4096;
struct Settings {unsigned techProductionCompatibility=0,techProductionFullPasses=0,techProductionEnhancePasses=0;unsigned flags=UnsafeTech,techEnhance=1,techEnhanceImplementation=1,techEnhanceCompareLevel=1,stop=0;} storage,*sh=&storage;
static bool fault=false,controlEligible=false;static unsigned techImplementation=techmode::Parameters;
static uint32_t lastHouse=House,worldCaptures=0,fullStack[4]{};
static unsigned probeDepth=0;static uint32_t probeToken=0,techEnhanceProbeReturn=0x700014;
namespace gameentry {static volatile LONG activeToken=0;static uint32_t currentToken(){return uint32_t(activeToken);}}
namespace debugentries {static uint32_t policyRevision=77;}
namespace eventdelivery {static bool mayQueue(int session){return session==3;}}
static techbp::SignatureSnapshot<20> nativeDraw;static techbp::SignatureSnapshot<15> nativePrune;static techbp::SignatureSnapshot<16> nativeEntry;
static techbp::SignatureSnapshot<13> enhancementLimit;static techbp::SignatureSnapshot<16> enhancementCompare,enhancementEntry;
static uint32_t nativePolicyRevision=77,enhancementPolicyRevision=77;
#include "subject.inc"
}
#include "flags-oracle.inc"
static void mapMemory(){for(auto region:{std::pair<uint32_t,size_t>{0x400000,0x700000},{0x10000000,0x100000}}){void*p=mmap(reinterpret_cast<void*>(uintptr_t(region.first)),region.second,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);check(p==reinterpret_cast<void*>(uintptr_t(region.first)),"private fixture mmap");}}
template<size_t N>static void signature(techbp::SignatureSnapshot<N>&snapshot,uint32_t address){snapshot.valid=true;for(unsigned i=0;i<N;++i){snapshot.bytes[i]=uint8_t(address+i*13);put<uint8_t>(address+i,snapshot.bytes[i]);}}
static void counters(){worldCalls=worldReads=totalReads=nativeChecks=enhancementChecks=0;}
static void rebuild(){using namespace subject;policy.enhancementIndex.rebuild(policy.enhancement,[](const EnhancedIdentity&t){return t.type.pointer;});}
static void fullRecord(uint32_t pointer,uint32_t id,uint32_t table){using namespace subject;unsigned n=techlookup::bucket(pointer,1023);while(policy.types[n].pointer&&policy.types[n].pointer!=pointer)n=(n+1)&1023;policy.types[n]={pointer,id,table};}
static void seed(unsigned rule=10){
 using namespace subject;std::memset(reinterpret_cast<void*>(uintptr_t(0x10000000)),0,0x100000);policy=Policy{};storage={};sh=&storage;fault=false;unreadable=0;techImplementation=techmode::Parameters;lastHouse=House;probeDepth=probeToken=0;techEnhanceProbeReturn=0x700014;gameentry::activeToken=0;
 policy.enabled=policy.enhanced=policy.compareLevel=1;policy.house=House;policy.houseID=77;policy.scenario=Scenario;policy.session=3;policy.frame=100;
 put<uint32_t>(0xA83D4C,House);put<uint32_t>(House+0x10,77);put<uint32_t>(0xA8B230,Scenario);put<int>(0xA8B238,3);put<int>(0xA8ED84,101);put<bool>(0xA8E9A0,true);put<uint32_t>(0xAC1198,0);
 put<uint32_t>(Type,techenhancement::Rules[rule].vtable);put<uint32_t>(Type+0x10,100);put<int>(Type+0x634,-1);
 put<uint32_t>(techenhancement::Rules[rule].array+4,Items);put<int>(techenhancement::Rules[rule].array+16,1);put<uint32_t>(Items,Type);
 fullRecord(Type,100,techenhancement::Rules[rule].vtable);policy.enhancement[rule]={{Type,100,techenhancement::Rules[rule].vtable},Items,0,-1};rebuild();
 fullStack[0]=0x6A97D7;fullStack[1]=Type;fullStack[2]=fullStack[3]=0;
 put<uint32_t>(Stack+0x10,House);put<uint32_t>(Stack+0x4C,0x6A97D7);put<uint32_t>(Stack+0x50,Type);put<uint32_t>(Stack+0x54,0);put<uint32_t>(Stack+0x58,0);
 signature(nativeDraw,0x6A97C3);signature(nativePrune,0x6AA777);signature(nativeEntry,0x4F7870);signature(enhancementLimit,0x4F794A);signature(enhancementCompare,0x4F7B5C);signature(enhancementEntry,0x4F7870);
 nativePolicyRevision=enhancementPolicyRevision=debugentries::policyRevision;worldCaptures=0;counters();
}
static int dispatch(subject::Context&context,unsigned tag){++callbackCases;return tag?subject::enhancementDispatch(context,tag):subject::fullDispatch(context);}
static subject::Context context(unsigned tag){subject::Context c;c.Eip=tag==0?0x4F7870:tag==2?0x4F7951:0x4F7B66;if(!tag)c.Ecx=House;return c;}
static void rejects(unsigned tag,const char*what){auto c=context(tag);const auto before=c;uint32_t stack[4];std::memcpy(stack,subject::fullStack,sizeof stack);check(dispatch(c,tag)==0,what);check(!std::memcmp(&c,&before,sizeof c),"rejected callback preserves every context field");check(!std::memcmp(stack,subject::fullStack,sizeof stack),"rejected callback preserves Full stack");}
static void succeeds(unsigned tag,const char*what){auto c=context(tag);auto before=c;check(dispatch(c,tag)==1,what);if(tag){before.EFlags=originalCompareFlags(1,tag==2?UINT32_MAX:c.Ebx,before.EFlags);check(!std::memcmp(&c,&before,sizeof c),"enhancement changes only expected flags");}else{check(subject::fullStack[2]==1,"Full changes buildLimitOnly");check(!std::memcmp(&c,&before,sizeof c),"Full preserves context");subject::fullStack[2]=0;}}
static void liveGuards(){using namespace subject;
 for(unsigned tag:{0u,2u,3u}){
  seed();succeeds(tag,"initial candidate accepted");
  for(unsigned kind=0;kind<16;++kind){seed();succeeds(tag,"permission is not cached after initial success");
   switch(kind){case 0:storage.stop=1;break;case 1:fault=true;break;case 2:sh=nullptr;break;case 3:put<uint32_t>(0xA83D4C,House+0x1000);break;case 4:put<uint32_t>(House+0x10,78);break;case 5:put<uint32_t>(0xA8B230,Scenario+0x1000);break;case 6:put<int>(0xA8B238,4);break;case 7:put<int>(0xA8ED84,99);break;case 8:put<bool>(0xA8E9A0,false);break;case 9:put<uint32_t>(0xAC1198,House);break;case 10:put<bool>(House+0x1F5,true);break;case 11:put<bool>(House+0x1F6,true);break;case 12:put<bool>(House+0x1F8,true);break;case 13:put<uint32_t>(Type,0x7EB610);break;case 14:put<uint32_t>(Type+0x10,101);break;case 15:unreadable=0xA83D4C;break;}
   rejects(tag,"live world/type/stop mutation rejects after prior success");
  }
 }
 for(unsigned kind=0;kind<8;++kind){seed();switch(kind){case 0:policy.enabled=0;break;case 1:storage.flags=0;break;case 2:techImplementation=3;break;case 3:fullStack[0]=0x650000;break;case 4:fullStack[2]=1;break;case 5:fullStack[3]=1;break;case 6:fullStack[1]=0xFF00;break;case 7:nativePolicyRevision=0;break;}rejects(0,"Full permission and caller guards");}
 for(unsigned tag:{2u,3u})for(unsigned kind=0;kind<12;++kind){seed();switch(kind){case 0:policy.enhanced=0;break;case 1:storage.techEnhance=0;break;case 2:put<int>(Type+0x634,1);break;case 3:put<uint32_t>(techenhancement::Rules[10].array+4,Items+4);break;case 4:policy.enhancement[10].index=-1;break;case 5:put<int>(techenhancement::Rules[10].array+16,0);break;case 6:put<uint32_t>(Items,Unknown);break;case 7:put<uint32_t>(Stack+0x10,House+4);break;case 8:put<uint32_t>(Stack+0x54,1);break;case 9:put<uint32_t>(Stack+0x58,1);break;case 10:put<uint32_t>(Stack+0x4C,0x650000);break;case 11:enhancementPolicyRevision=0;break;}rejects(tag,"enhancement live provenance and caller guards");}
 for(unsigned tag:{0u,2u,3u}){seed();auto c=context(tag);if(!tag)c.Ecx=House+4;else c.Ebp=House+4;const auto before=c;check(!dispatch(c,tag),"register House mismatch rejects");check(!std::memcmp(&before,&c,sizeof c),"House mismatch leaves context intact");}
 for(unsigned tag:{2u,3u}){seed();auto c=context(tag);if(tag==2)c.Edi=Unknown;else c.Ecx=Unknown;const auto before=c;check(!dispatch(c,tag),"type register mismatch rejects");check(!std::memcmp(&before,&c,sizeof c),"type register mismatch leaves context intact");}
 seed();policy.compareLevel=0;rejects(3,"compare permission is immediate");succeeds(2,"limit permission remains independent");
 for(unsigned tag:{2u,3u}){seed();storage.techEnhanceImplementation=techenhanceparams::Rules;storage.techEnhanceCompareLevel=0;succeeds(tag,"published policy survives requested mode/compare handoff");}
 for(unsigned tag:{2u,3u}){seed();put<uint32_t>(Stack+0x4C,techEnhanceProbeReturn);put<uint32_t>(Stack+0x58,1);probeDepth=1;probeToken=99;gameentry::activeToken=99;succeeds(tag,"active enhancement probe caller accepted");gameentry::activeToken=98;rejects(tag,"stale probe token rejects");gameentry::activeToken=99;probeDepth=0;rejects(tag,"inactive probe depth rejects");}
 for(unsigned rule:{0u,10u,20u})for(unsigned tag:{0u,2u,3u}){seed(rule);fullStack[0]=0x6AA786;fullStack[3]=1;put<uint32_t>(Stack+0x4C,0x6AA786);put<uint32_t>(Stack+0x58,1);succeeds(tag,"all array categories and prune caller accepted");}
 // First matching record controls rejection, even if a later duplicate would pass.
 seed(0);policy.enhancement[1]=policy.enhancement[0];++policy.enhancement[0].type.id;rebuild();rejects(2,"duplicate pointer preserves first rule failure");
}
static void signatureGuards(){using namespace subject;
 for(unsigned tag:{0u,2u,3u}){
  const uint32_t addresses[3]={tag?0x4F794Au:0x6A97C3u,tag?0x4F7B5Cu:0x6AA777u,0x4F7870};const unsigned sizes[3]={tag?13u:20u,tag?16u:15u,16};
  for(unsigned s=0;s<3;++s)for(unsigned byte=0;byte<sizes[s];++byte){seed();put<uint8_t>(addresses[s]+byte,uint8_t(get<uint8_t>(addresses[s]+byte)^1));rejects(tag,"every admitted signature byte remains live");if(!tag)check(!policy.enabled,"Full signature failure revokes permission");}
 }
 seed();policy.compareLevel=0;put<uint8_t>(0x4F7B5C,uint8_t(get<uint8_t>(0x4F7B5C)^1));succeeds(2,"unrequested compare snapshot remains unnecessary");
}
static unsigned absentFullReads=0,absentEnhancementReads=0,absentEnhancementSignatures=0;
static void earlyReject(){using namespace subject;
 seed();fullStack[1]=Unknown;rejects(0,"uncaptured Full type rejected");absentFullReads=worldReads;check(worldCalls==(CURRENT_PHASE?0u:1u)&&worldReads==(CURRENT_PHASE?0u:10u),"unknown Full avoids precisely the world guard");check(!nativeChecks,"unknown Full never validates signature");
 seed();put<uint32_t>(Stack+0x50,Unknown);auto c=context(2);c.Edi=Unknown;const auto before=c;check(!dispatch(c,2),"uncaptured enhancement type rejected");check(!std::memcmp(&before,&c,sizeof c),"unknown enhancement leaves context intact");absentEnhancementReads=worldReads;absentEnhancementSignatures=enhancementChecks;check(worldCalls==(CURRENT_PHASE?0u:1u)&&worldReads==(CURRENT_PHASE?0u:10u),"unknown enhancement avoids precisely the world guard");check(enhancementChecks==(CURRENT_PHASE?0u:1u),"unknown enhancement avoids complete signature comparison");
 seed();counters();check(fullType(Type),"cold Full helper still validates identity");check(totalReads==2&&!worldCalls,"cold Full helper retains two live identity reads");put<uint32_t>(Type+0x10,101);check(!fullType(Type),"cold Full helper detects immediate UID reuse");
}
static void indexChecks(){using namespace subject;
 static_assert(sizeof(techlookup::PointerIndex<64>)<=80,"fixed tiny index");static_assert(std::is_trivially_destructible<techlookup::PointerIndex<64>>::value,"no destruction runtime");
 EnhancedIdentity records[techenhancement::Count]{};techlookup::PointerIndex<64> index;const auto key=[](const EnhancedIdentity&t){return t.type.pointer;};
 check(index.find(records,Type,key)==techenhancement::Count,"zero initialized index refuses lookup");
 uint32_t first=0x20000;while(techlookup::bucket(first,63)!=63)first+=16;
 for(unsigned i=0;i<techenhancement::Count;++i)records[i].type.pointer=first+i*1024;
 index.rebuild(records,key);check(index.ready(),"complete rebuild publishes readiness");
 for(unsigned i=0;i<techenhancement::Count;++i)check(index.find(records,records[i].type.pointer,key)==i,"21 colliding pointers wrap correctly");
 check(index.find(records,first+techenhancement::Count*1024,key)==techenhancement::Count,"collision-chain miss terminates");check(index.find(records,0,key)==techenhancement::Count,"zero key is absent");
 records[1].type.pointer=records[0].type.pointer;index.rebuild(records,key);check(index.find(records,first,key)==0,"first duplicate owns candidate");
 unsigned reentries=0;index.rebuild(records,[&](const EnhancedIdentity&t){++reentries;check(!index.ready()&&index.find(records,first,key)==techenhancement::Count,"reentrant lookup cannot observe partial rebuild");return t.type.pointer;});check(reentries>=techenhancement::Count,"reentrant rebuild probes exercised");
 for(auto&r:records)r={};index.rebuild(records,key);check(index.ready()&&index.find(records,first,key)==techenhancement::Count,"empty rebuild erases old candidates");index.reset();check(!index.ready(),"explicit invalidation clears readiness");
 TypeIdentity full[1024]{};for(unsigned i=0;i<512;++i){const uint32_t pointer=0x40000+i*16384;unsigned n=techlookup::bucket(pointer,1023);while(full[n].pointer)n=(n+1)&1023;full[n]={pointer,i+1,0x7F6218};}
 for(unsigned i=0;i<512;++i){const auto*r=techlookup::findIdentity(full,0x40000+i*16384);check(r&&r->id==i+1,"Full half-load worst collision membership");}
 check(!techlookup::findIdentity(full,0x40000+512*16384)&&!techlookup::findIdentity(full,0),"Full missing and zero candidates terminate");
#if CURRENT_PHASE
 seed();policy.enhancementIndex.reset();rejects(2,"unpublished index never grants permission");rebuild();succeeds(2,"explicit completed rebuild restores candidate selection");
 seed();put<uint32_t>(0xA8B230,Scenario+0x1000);check(captureWorld(House),"changed world captures");check(!policy.enabled&&!policy.enhanced&&!policy.enhancementIndex.ready(),"world generation change revokes and invalidates index");
 seed();check(captureWorld(House)&&policy.enhancementIndex.ready(),"unchanged world preserves captured index");
#endif
}
static void flags(){
 for(uint32_t low=0;low<65536;++low){const uint32_t previous=low|((low*2654435761u)&0xFFFF0000u);check(techenhanceparams::limitFlags(previous)==originalCompareFlags(1,UINT32_MAX,previous),"fixed compare matches original full flags word");++flagCases;}
 for(unsigned bit=0;bit<32;++bit)for(uint32_t previous:{uint32_t(1u<<bit),uint32_t(~(1u<<bit))}){check(techenhanceparams::limitFlags(previous)==originalCompareFlags(1,UINT32_MAX,previous),"every original non-arithmetic bit preserved");++flagCases;}
 uint32_t random=1;for(unsigned i=0;i<65536;++i){random=random*1664525u+1013904223u;const uint32_t previous=~random;check(techenhanceparams::compareFlags(1,random,previous)==originalCompareFlags(1,random,previous),"dynamic EBX comparison unchanged");++flagCases;}
 for(unsigned tag:{2u,3u})for(uint32_t rhs:{0u,1u,2u,15u,16u,255u,256u,0x7FFFFFFFu,0x80000000u,UINT32_MAX}){seed();auto c=context(tag);c.Ebx=rhs;const uint32_t expected=originalCompareFlags(1,tag==2?UINT32_MAX:rhs,c.EFlags);check(dispatch(c,tag)==1&&c.EFlags==expected,"extracted callback uses correct fixed/dynamic flags");}
}
int main(){mapMemory();liveGuards();signatureGuards();earlyReject();indexChecks();flags();std::printf("{\"pass\":true,\"checks\":%u,\"callback_cases\":%u,\"flags_differential_cases\":%u,\"absent_full_world_reads\":%u,\"absent_enhancement_world_reads\":%u,\"absent_enhancement_signature_calls\":%u}\n",checks,callbackCases,flagCases,absentFullReads,absentEnhancementReads,absentEnhancementSignatures);}

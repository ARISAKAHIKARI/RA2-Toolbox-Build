// Actual production admission and Full callback body on private synthetic
// memory. The native callability result is modeled; no game/DR/VEH executes.
#ifdef NDEBUG
#error This fixture requires assertions
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <sys/mman.h>
#include "tech_ui_breakpoint_hot.h"
#include "technology_mode_policy.h"
using namespace assist;
using LONG=int32_t;
struct CONTEXT {uint32_t Eip=0,Ecx=0,Esp=0,EFlags=0;};
static constexpr uint32_t Entry=0x4F7870,House=0x10002000,Type=0x10001000,Stack=0x10000000,UnsafeTech=4096;
struct Settings {uint32_t flags=UnsafeTech;} settings;
static Settings*sh=&settings;
static unsigned techImplementation=4,checks=0,validations=0;
static bool callable=true,ares=false,mutateDuringValidation=false,worldValid=true,typeAllowed=true,lastRefresh=false;
static uint32_t unreadable=0;
static bool mapped(uint32_t at,size_t n){return (at>=0x4F7000&&uint64_t(at)+n<=0x4F9000)||(at>=0x6A9000&&uint64_t(at)+n<=0x6AB000)||(at>=Stack&&uint64_t(at)+n<=Stack+0x10000);}
static bool read(uint32_t at,void*out,size_t n){if(!mapped(at,n)||(unreadable>=at&&uint64_t(unreadable)<uint64_t(at)+n))return false;std::memcpy(out,reinterpret_cast<void*>(uintptr_t(at)),n);return true;}
static LONG InterlockedExchange(volatile LONG*p,LONG next){LONG old=*p;*p=next;return old;}
static bool nativeEntryCallable(uint32_t at,bool refresh=false){assert(at==Entry);lastRefresh=refresh;++validations;if(mutateDuringValidation)*reinterpret_cast<uint8_t*>(uintptr_t(Entry+15))^=1;return callable;}
static bool nativeAresAvailable(){return ares;}
namespace techparameters {
struct Policy {volatile LONG enabled=1;uint32_t house=House;} policy;
static bool world(){return worldValid;}
static bool fullType(uint32_t p){return typeAllowed&&p==Type;}
}
#include "tech_ares_entry_subject.inc"
static void check(bool good,const char*what){++checks;if(!good){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
static constexpr uint8_t Original[]={0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53,0x55,0x8B,0xE9,0x56,0x57,0x84,0xC0,0x89};
// Exact observed Runtime(10).log probe at 004F7870, including its E9 target.
static constexpr uint8_t Runtime10[]={0xE9,0x8B,0x87,0xDE,0x04,0x90,0x90,0x53,0x55,0x8B,0xE9,0x56,0x57,0x84,0xC0,0x89};
static constexpr uint8_t Draw[]={0x8B,0x0D,0x4C,0x3D,0xA8,0x00,0x6A,0x00,0x6A,0x00,0x50,0x89,0x44,0x24,0x38,0xE8,0x99,0xE0,0xE4,0xFF};
static constexpr uint8_t Prune[]={0x8B,0x88,0x1C,0x02,0x00,0x00,0x6A,0x01,0x57,0x53,0xE8,0xEA,0xD0,0xE4,0xFF};
static void seed(const uint8_t*entry){std::memcpy(reinterpret_cast<void*>(uintptr_t(Entry)),entry,16);std::memcpy(reinterpret_cast<void*>(uintptr_t(0x6A97C3)),Draw,sizeof Draw);std::memcpy(reinterpret_cast<void*>(uintptr_t(0x6AA777)),Prune,sizeof Prune);techparameters::policy.enabled=1;}
static void invocation(uint32_t caller,unsigned include,bool accepted){
 uint32_t args[4]={caller,Type,0,include};std::memcpy(reinterpret_cast<void*>(uintptr_t(Stack)),args,sizeof args);
 CONTEXT context{Entry,House,Stack,0x246};const auto before=context;uint8_t entry[16]{};assert(read(Entry,entry,sizeof entry));const unsigned beforeValidations=validations;
 const int result=applyFull(context);auto*after=reinterpret_cast<uint32_t*>(uintptr_t(Stack));
 check(result==int(accepted)&&after[2]==unsigned(accepted),"production callback modifies only the accepted buildLimitOnly argument");
 check(after[0]==args[0]&&after[1]==args[1]&&after[3]==args[3],"return address, type and includeProduction remain unchanged");
 check(!std::memcmp(&context,&before,sizeof context)&&context.Eip==Entry,"execution resumes at the original native entry, including the Ares jump");
 check(!std::memcmp(reinterpret_cast<void*>(uintptr_t(Entry)),entry,sizeof entry),"callback never replaces the Ares entry or instruction bytes");
 check(validations==beforeValidations,"hot callback performs no chain/query revalidation");
}
int main(){
 for(auto region:{std::pair<uint32_t,size_t>{0x4F7000,0x2000},{0x6A9000,0x2000},{Stack,0x10000}}){void*p=mmap(reinterpret_cast<void*>(uintptr_t(region.first)),region.second,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(p==reinterpret_cast<void*>(uintptr_t(region.first)));}
 seed(Original);check(techparameters::nativeCompatible(),"vanilla exact entry remains compatible");invocation(0x6A97D7,0,true);invocation(0x6AA786,1,true);
 seed(Runtime10);ares=true;check(techparameters::nativeCompatible()&&lastRefresh,"Runtime10 Ares E9 entry is admitted after fresh native-callability validation");
 check(techparameters::nativeCompatibleHot(),"verified Ares entry matches its hot snapshot");invocation(0x6A97D7,0,true);invocation(0x6AA786,1,true);
 invocation(0x12345678,0,false);invocation(0x6A97D7,1,false);invocation(0x6AA786,0,false);
 typeAllowed=false;invocation(0x6A97D7,0,false);typeAllowed=true;worldValid=false;invocation(0x6A97D7,0,false);worldValid=true;
 for(auto region:{std::pair<uint32_t,size_t>{0x6A97C3,sizeof Draw},{0x6AA777,sizeof Prune},{Entry,sizeof Runtime10}}){
  for(size_t i=0;i<region.second;++i){seed(Runtime10);check(techparameters::nativeCompatible(),"valid baseline refresh succeeds");*reinterpret_cast<uint8_t*>(uintptr_t(region.first+i))^=0x80;check(!techparameters::nativeCompatibleHot(),"every changed caller/entry byte is rejected immediately");invocation(0x6A97D7,0,false);check(!techparameters::policy.enabled,"changed-site callback disables only its Full permission");}
 }
 seed(Runtime10);callable=false;check(!techparameters::nativeCompatible()&&!techparameters::nativeEntry.valid,"an uncallable detour never produces an accepted hot image");callable=true;
 unreadable=Entry;check(!techparameters::nativeCompatible()&&!techparameters::nativeEntry.valid,"unreadable entry cannot authorize Full mode");unreadable=0;
 seed(Runtime10);mutateDuringValidation=true;check(!techparameters::nativeCompatible()&&!techparameters::nativeEntry.valid,"entry replacement during cold validation is rejected");mutateDuringValidation=false;
 seed(Runtime10);ares=true;check(techparameters::enhancementBodyBypassed()&&!lastRefresh,"Ares diagnostic uses the bounded cache without granting comparison-site permission");
 ares=false;check(!techparameters::enhancementBodyBypassed(),"unidentified replacement is not mislabeled as Ares");ares=true;callable=false;check(!techparameters::enhancementBodyBypassed(),"uncallable code is not reported as a supported Ares replacement");callable=true;
 seed(Original);check(!techparameters::enhancementBodyBypassed(),"an unchanged original body remains eligible for Exact comparisons");
 std::printf("{\"pass\":true,\"checks\":%u,\"production_callback_extracted\":true,\"native_entry_callability_modeled\":true,\"game_tested\":false,\"windows_executed\":false}\n",checks);
}

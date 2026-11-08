#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include "../source/control_parameter_policy.h"
#include "../source/control_mode_policy.h"
#include "../source/tech_enhancement_parameter_policy.h"
#include "../source/tech_ui_breakpoint_hot.h"
#include "../../shared/source/debug_entry_policy.h"
#include "../../shared/source/hardware_slot_budget.h"
using namespace assist;
static unsigned checks=0;
static void check(bool value){++checks;if(!value){std::fprintf(stderr,"check %u failed\n",checks);std::abort();}}
static std::map<uint32_t,uint8_t> memory;
static bool read(uint32_t at,void*out,size_t size){for(size_t i=0;i<size;++i){auto p=memory.find(at+uint32_t(i));if(p==memory.end())return false;static_cast<uint8_t*>(out)[i]=p->second;}return true;}
template<class T>static void put(uint32_t at,T v){const auto*p=reinterpret_cast<const uint8_t*>(&v);for(unsigned i=0;i<sizeof v;++i)memory[at+i]=p[i];}
namespace controlparams_fixture {
struct Context {uint32_t Eip=controlparams::Entry,Esp=0x40000,Eax=99,Ebx=98,Ecx=0x20000,Edx=97,Ebp=96,Esi=95,Edi=94,EFlags=0x202;};
constexpr unsigned UnsafeTech=4096;
static unsigned techImplementation=4;static bool compatible=true;static uint32_t stack[4]{};
static uint32_t controlPolicyRevision=debugentries::policyRevision;
namespace techmode{static bool parameters(unsigned mode){return mode==4;}}
struct Shared {uint32_t controlAll=controlmode::Parameters,stop=0,flags=UnsafeTech;}storage,*sh=&storage;
struct Policy {int controlling=1,enabled=1;uint32_t house=0x10000,houseID=10,scenario=0x60000;int session=3,frame=100;}policy;
struct TypeIdentity {uint32_t pointer,id,table;};
static const TypeIdentity fullCandidate{0x60000,60,0x7F6218};
static bool fault=false;static const TypeIdentity*fullIdentity(uint32_t p){return p==fullCandidate.pointer?&fullCandidate:nullptr;}
template<class T>static T get(uint32_t p){T value{};read(p,&value,sizeof value);return value;}
static bool nativeCompatibleHot(){return compatible;}
static void InterlockedExchange(int*p,int value){*p=value;}
static int controlExceptions=0;static void InterlockedIncrement(int*p){++*p;}
namespace controlparams {
using assist::controlparams::destination;
static bool guardedRead(uint32_t a,void*out,size_t n){return read(a,out,n);}
template<class Read>bool world(uint32_t h,uint32_t id,uint32_t s,int m,int f,Read r){return assist::controlparams::world(h,id,s,m,f,r);}
template<class Read>bool house(uint32_t h,Read r){return assist::controlparams::house(h,r);}
}
static bool world(){return !storage.stop&&!fault&&assist::controlparams::world(policy.house,policy.houseID,policy.scenario,policy.session,policy.frame,read);}
// This contains the ACTUAL tag-4 branch extracted from the production callback.
static int dispatch(Context&context,unsigned tag){
#include CONTROL_CALLBACK_BRANCH
 return 0;
}
static int fullDispatch(Context&context){const unsigned tag=0;
#include FULL_CALLBACK_BRANCH
 return 0;
}
}
static void seed(){
 memory.clear();using namespace controlparams_fixture;storage={};policy={};fault=false;controlExceptions=0;techImplementation=4;compatible=true;
 put<uint32_t>(0xA8022C,0x50000);put<int>(0xA80238,2);put<uint32_t>(0x50000,0x10000);put<uint32_t>(0x50004,0x20000);
 for(unsigned i=0;i<2;++i){uint32_t p=0x10000+i*0x10000;put<uint32_t>(p,0x7E0000);put<uint32_t>(p+0x10,10+i);put<int>(p+0x30,i);put<uint8_t>(p+0x1F5,0);put<uint8_t>(p+0x1F6,0);put<uint8_t>(p+0x1F8,0);put<uint8_t>(p+0x1EC,1);put<uint8_t>(p+0x1ED,0);put<uint32_t>(p+0x21C,0x12340000+i);}
 put<uint32_t>(0xA83D4C,0x10000);put<uint32_t>(0xA8B230,0x60000);put<int>(0xA8B238,3);put<int>(0xA8ED84,101);put<uint8_t>(0xA8E9A0,1);put<uint32_t>(0xAC1198,0);put<uint32_t>(0x40000,0x650005);
 put<uint32_t>(fullCandidate.pointer,fullCandidate.table);put<uint32_t>(fullCandidate.pointer+0x10,fullCandidate.id);
}
int main(){
 using namespace controlparams_fixture;
 check(!slotbudget::allows(0,16));check(!slotbudget::pack(4,1,0,16));check(!slotbudget::pack(0,4,0,31));
 for(uint32_t owned=0;owned<32;++owned){const unsigned occupied=slotbudget::count(owned);if(occupied>4)continue;
  for(unsigned foreign=0;foreign<=4-occupied;++foreign){const unsigned free=4-occupied-foreign;const auto budget=slotbudget::pack(free,occupied,foreign,owned);
   check(slotbudget::known(budget)&&slotbudget::owned(budget)==occupied&&slotbudget::foreign(budget)==foreign&&slotbudget::free(budget)==free&&slotbudget::tags(budget)==owned);
   for(uint32_t wanted=0;wanted<32;++wanted)check(slotbudget::allows(budget,wanted)==(slotbudget::count(wanted&~owned)<=free));
  }
 }
 check(techenhanceparams::breakpointTags(false)==4&&__builtin_popcount(techenhanceparams::breakpointTags(false))==1);
 check(techenhanceparams::breakpointTags(true)==12&&__builtin_popcount(techenhanceparams::breakpointTags(true))==2);
 for(uint32_t caller:{0x650005u,assist::controlparams::OwnerReturn,assist::controlparams::PlanningReturn}){
  seed();put<uint32_t>(0x40000,caller);Context c,before;const auto data=memory;check(dispatch(c,4)==1);
  check(c.Eip==(caller==0x650005u?assist::controlparams::Allowed:assist::controlparams::Strict));before.Eip=c.Eip;check(!std::memcmp(&c,&before,sizeof c));check(memory==data);check(controlExceptions==1);
 }
 for(unsigned kind=0;kind<15;++kind){seed();Context c,before;
  switch(kind){case 0:policy.controlling=0;break;case 1:storage.controlAll=controlmode::Master;break;case 2:storage.stop=1;break;case 3:fault=true;break;case 4:put<uint32_t>(0xA83D4C,0x20000);break;case 5:put<uint32_t>(0x10010,42);break;case 6:put<uint32_t>(0xA8B230,0x61000);break;case 7:put<int>(0xA8B238,4);break;case 8:put<int>(0xA8ED84,99);break;case 9:put<uint8_t>(0xA8E9A0,0);break;case 10:put<uint8_t>(0x101F5,1);break;case 11:put<uint32_t>(0xAC1198,0x10000);break;case 12:put<int>(0x20030,4);break;case 13:memory.erase(0x40000);break;case 14:put<uint32_t>(0x40000,0);break;}
  const auto data=memory;check(dispatch(c,4)==0);check(!std::memcmp(&c,&before,sizeof c));check(memory==data);
 }
 for(unsigned caller=0;caller<2;++caller){seed();Context c;c.Ecx=policy.house;const Context before=c;stack[0]=caller?0x6AA786:0x6A97D7;stack[1]=0x60000;stack[2]=0;stack[3]=caller;const auto data=memory;
  check(fullDispatch(c)==1);check(stack[0]==(caller?0x6AA786u:0x6A97D7u)&&stack[1]==0x60000&&stack[2]==1&&stack[3]==caller);check(!memcmp(&c,&before,sizeof c));check(data==memory);
 }
 for(unsigned kind=0;kind<10;++kind){seed();Context c;c.Ecx=policy.house;stack[0]=0x6A97D7;stack[1]=0x60000;stack[2]=0;stack[3]=0;
  switch(kind){case 0:stack[0]=0x630000;break;case 1:stack[1]=0x61000;break;case 2:stack[2]=1;break;case 3:stack[3]=1;break;case 4:c.Ecx=0x20000;break;case 5:policy.enabled=0;break;case 6:storage.flags=0;break;case 7:techImplementation=0;break;case 8:storage.stop=1;break;case 9:compatible=false;break;}
  const Context before=c;uint32_t saved[4];memcpy(saved,stack,sizeof stack);check(fullDispatch(c)==0);check(!memcmp(&c,&before,sizeof c));check(!memcmp(saved,stack,sizeof stack));
 }
 // The core accepts only original instructions, including known forward
 // variants. It rejects an already-patched predicate without claiming it.
 static const uint8_t tail[]={0xB8,1,0,0,0,0xC3,0x8B,0x15,0x4C,0x3D,0xA8,0,0x33,0xC0,0x3B,0xCA,0x0F,0x94,0xC0,0xC3};
 for(unsigned variant=0;variant<3;++variant){seed();for(const auto&p:patchcontext::Probes)for(unsigned i=0;i<p.size;++i)memory[p.address+i]=p.bytes[i];for(const auto&s:mastercontrol::Sites)for(unsigned i=0;i<5;++i)memory[s.address+i]=s.before[i];for(unsigned i=0;i<sizeof tail;++i)memory[0x50B710+i]=tail[i];
  const uint8_t entries[][9]={{0xA1,0x38,0xB2,0xA8,0,0x85,0xC0,0x75,0x1D},{0xE9,0xFB,0x2A,0x67,0,0xCC,0xCC,0xCC,0xCC},{0xE9,0x8B,0x2C,0x67,0,0xCC,0xCC,0xCC,0xCC}};
  for(unsigned i=0;i<9;++i)memory[0x50B6F0+i]=entries[variant][i];check(assist::controlparams::compatible(read));
  memory[0x6339BA]^=1;check(!assist::controlparams::compatible(read));memory[0x6339BA]^=1;
  for(unsigned i=0;i<5;++i)memory[0x50B6F0+i]=mastercontrol::Sites[2].after[i];check(!assist::controlparams::compatible(read));
 }
 std::printf("PASS: %u production control callback, strict caller, original byte, no-data-write and fail-closed checks\n",checks);
}


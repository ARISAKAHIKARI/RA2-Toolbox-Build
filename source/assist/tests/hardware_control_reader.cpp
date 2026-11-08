#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>
#include "../source/control_parameter_policy.h"
#include "../source/control_mode_policy.h"
#include "../source/tech_ui_breakpoint_hot.h"

using Trace=std::vector<std::pair<uint32_t,size_t>>;
static std::map<uint32_t,uint8_t> memory;
static Trace trace;
static unsigned failRead=0;
static bool currentRead(uint32_t at,void*out,size_t n){
 trace.emplace_back(at,n);
 if((failRead&&trace.size()==failRead)||at<0x10000||uint64_t(at)+n>=0x80000000ull||(n!=1&&n!=4))return false;
 for(size_t i=0;i<n;++i){auto found=memory.find(at+uint32_t(i));if(found==memory.end())return false;static_cast<uint8_t*>(out)[i]=found->second;}
 return true;
}
template<class T>static void put(uint32_t at,T value){const auto*p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)memory[at+uint32_t(i)]=p[i];}
struct Context {uint32_t Eip=assist::controlparams::Entry,Esp=0x40000,Eax=99,Ebx=98,Ecx=0x20000,Edx=97,Ebp=96,Esi=95,Edi=94,EFlags=0x202;};
namespace techbp=assist::techbp;
namespace controlmode=assist::controlmode;
namespace controlparams {
using assist::controlparams::destination;
static bool guardedRead(uint32_t at,void*out,size_t n){return currentRead(at,out,n);}
template<class Read>static bool world(uint32_t h,uint32_t id,uint32_t s,int mode,int frame,Read read){return assist::controlparams::world(h,id,s,mode,frame,read);}
template<class Read>static bool house(uint32_t p,Read read){return assist::controlparams::house(p,read);}
}
struct Shared {uint32_t controlAll=controlmode::Parameters,stop=0;}storage,*sh=&storage;
struct Policy {int controlling=1;uint32_t house=0x10000,houseID=10,scenario=0x60000;int session=3,frame=100;}policy;
static bool fault=false;
static uint32_t controlPolicyRevision=debugentries::policyRevision;
static int controlExceptions=0;
static int baseline(Context&context,unsigned tag){
#include BASELINE_CONTROL_BRANCH
 return 0;
}
static int candidate(Context&context,unsigned tag){
#include CURRENT_CONTROL_BRANCH
 return 0;
}
static void seed(){
 memory.clear();storage={};sh=&storage;policy={};fault=false;failRead=0;controlPolicyRevision=debugentries::policyRevision;
 put<uint32_t>(0xA8022C,0x50000);put<int>(0xA80238,2);put<uint32_t>(0x50000,0x10000);put<uint32_t>(0x50004,0x20000);
 for(unsigned i=0;i<2;++i){const uint32_t p=0x10000+i*0x10000;put<uint32_t>(p,0x7E0000);put<uint32_t>(p+0x10,10+i);put<int>(p+0x30,int(i));put<uint8_t>(p+0x1F5,0);put<uint8_t>(p+0x1F6,0);put<uint8_t>(p+0x1F8,0);}
 put<uint32_t>(0xA83D4C,0x10000);put<uint32_t>(0xA8B230,0x60000);put<int>(0xA8B238,3);put<int>(0xA8ED84,101);put<uint8_t>(0xA8E9A0,1);put<uint32_t>(0xAC1198,0);put<uint32_t>(0x40000,0x650005);
}
static unsigned cases=0,changedCases=0;
static void compare(Context initial={},int oldExpected=1,int newExpected=1){
 const auto unchanged=memory;Context before=initial,after=initial;
 trace.clear();controlExceptions=0;const int oldResult=baseline(before,4);const Trace oldTrace=trace;const int oldCount=controlExceptions;
 assert(memory==unchanged);
 trace.clear();controlExceptions=0;const int newResult=candidate(after,4);
 assert(oldResult==oldExpected&&newResult==newExpected&&controlExceptions==oldCount&&controlExceptions==1);
 auto oldContext=initial,newContext=initial;uint32_t caller=0;
 if(oldResult||newResult){for(unsigned i=0;i<4;++i)caller|=uint32_t(memory.at(initial.Esp+i))<<(i*8);}
 if(oldResult)oldContext.Eip=controlparams::destination(caller);
 if(newResult)newContext.Eip=controlparams::destination(caller);
 assert(!std::memcmp(&before,&oldContext,sizeof before)&&!std::memcmp(&after,&newContext,sizeof after)&&memory==unchanged);
 if(newResult)assert((trace==Trace{{initial.Esp,4}}));
 if(oldResult)assert(oldTrace.size()==17);
 changedCases+=oldResult!=newResult;++cases;
}
int main(){
 const Trace complete={{0xA83D4C,4},{0x10010,4},{0xA8B230,4},{0xA8B238,4},{0xA8ED84,4},{0xA8E9A0,1},{0xAC1198,4},{0x101F5,1},{0x101F6,1},{0x101F8,1},{0xA8022C,4},{0xA80238,4},{0x20030,4},{0x50004,4},{0x20000,4},{0x20010,4},{0x40000,4}};
 for(uint32_t caller:{0x650005u,assist::controlparams::OwnerReturn,assist::controlparams::PlanningReturn}){
  seed();put<uint32_t>(0x40000,caller);compare();assert((trace==Trace{{0x40000,4}}));
  Context c;c.Ecx=policy.house;compare(c);
  for(uint32_t target:{0u,0xffffu,0x30000u,0x7fffffffu,0xffffffffu}){c.Ecx=target;compare(c,0,1);}
 }
 // Injecting an ordinal read failure has different meanings now: current
 // callback only has ordinal 1, the saved return address. The old 17-read
 // oracle remains pinned to pre.3 and is not claimed equivalent.
 for(unsigned n=1;n<=complete.size();++n){seed();failRead=n;compare({},0,n==1?0:1);}
 for(auto field:complete)for(size_t byte=0;byte<field.second;++byte){seed();memory.erase(field.first+uint32_t(byte));compare({},0,field.first==0x40000?0:1);}
 for(unsigned kind=0;kind<30;++kind){seed();Context c;
  switch(kind){
   case 0:policy.controlling=0;break;case 1:storage.controlAll=controlmode::Master;break;case 2:storage.stop=1;break;case 3:fault=true;break;
   case 4:put<uint32_t>(0xA83D4C,0x20000);break;case 5:put<uint32_t>(0x10010,42);break;case 6:put<uint32_t>(0xA8B230,0x61000);break;
   case 7:put<int>(0xA8B238,4);break;case 8:put<int>(0xA8ED84,99);break;case 9:put<uint8_t>(0xA8E9A0,0);break;
   case 10:put<uint8_t>(0x101F5,1);break;case 11:put<uint8_t>(0x101F6,1);break;case 12:put<uint8_t>(0x101F8,1);break;
   case 13:put<uint32_t>(0xAC1198,0x10000);break;case 14:put<uint32_t>(0xA8022C,0);break;case 15:put<int>(0xA80238,0);break;
   case 16:put<int>(0xA80238,33);break;case 17:put<int>(0x20030,-1);break;case 18:put<int>(0x20030,2);break;
   case 19:put<uint32_t>(0x50004,0x30000);break;case 20:put<uint32_t>(0x20000,0);break;case 21:put<uint32_t>(0x20010,0);break;
   case 22:put<uint32_t>(0x40000,0);break;case 23:c.Esp=0xffff;break;case 24:c.Esp=0x7ffffffc;break;
   case 25:c.Ecx=0xffff;break;case 26:policy.house=0;break;case 27:policy.scenario=0;break;case 28:++controlPolicyRevision;break;case 29:sh=nullptr;break;
  }
  const bool immediate=kind<=3||kind==22||kind==23||kind==24||kind==28||kind==29;
  compare(c,0,immediate?0:1);
 }
 // World changes are intentionally observed at the next maintenance
 // publication. Immediate shared gates still reject the very next hit.
 seed();compare();put<uint32_t>(0x10010,42);compare({},0,1);
 seed();compare();put<uint8_t>(0x101F6,1);compare({},0,1);
 seed();compare();storage.stop=1;compare({},0,0);
 std::printf("{\"pass\":true,\"branch_cases\":%u,\"intentionally_changed_outcomes\":%u,\"baseline_successful_reads_per_hit\":17,\"current_successful_reads_per_hit\":1,\"semantics_changed\":true,\"per_hit_equivalence_claimed\":false,\"hardware_exception_count_reduced\":false,\"windows_veh_measured\":false,\"game_process_tested\":false}\n",cases,changedCases);
}

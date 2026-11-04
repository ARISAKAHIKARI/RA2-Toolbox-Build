#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
#include <cstring>
#include "../source/control_mode_policy.h"
#include "../source/hook_registry.h"
#include "../../shared/source/master_control_policy.h"
using namespace assist;
static std::map<uint32_t,uint8_t> memory;
static hookregistry::Registry<> codeHookRegistry;
static bool read(uint32_t at,void*out,size_t n){for(size_t i=0;i<n;++i){auto p=memory.find(at+uint32_t(i));if(p==memory.end())return false;((uint8_t*)out)[i]=p->second;}return true;}
static void bytes(uint32_t at,const void*in,size_t n){for(size_t i=0;i<n;++i)memory[at+uint32_t(i)]=((const uint8_t*)in)[i];}
struct Shared{uint32_t controlAll=0;int playerControlStatus=0;};static Shared shared,*sh=&shared;
namespace playertest{constexpr bool Available=true;}
static bool riskControlActive=false,riskControlNativeJump=false,legacyPending=false,companions=true;
static uint32_t riskControlNativeTarget=0;
static bool controlHouseFlagsPending(){return legacyPending;}
static bool controlCompanionsRetired(){return companions;}
static bool riskContextReady(){return true;}
static int unselections=0;static void unselect(){++unselections;}
static void riskControlEntry(){}static void riskControlOwnerEntry(){}
struct RiskSite{uint32_t address;unsigned size;uint8_t before[16],after[16];};
static uint32_t failInstall=0,failRestore=0;static bool afterWrite=false;static unsigned calls=0;
struct Write{uint32_t at;bool restore;};static std::vector<Write>writes;
namespace gameentry{enum class Client{Assist};
static bool patch(Client,uint32_t at,const uint8_t*before,const uint8_t*after,unsigned n){
 ++calls;const auto t=codeHookRegistry.proposal(at,before,after,n);if(t==hookregistry::Transition::Deny)return false;
 bool restore=t==hookregistry::Transition::Restore;uint8_t actual[16]{};
 if(!read(at,actual,n)||memcmp(actual,before,n)){codeHookRegistry.markConflict(at);return false;}
 bool fail=at==(restore?failRestore:failInstall);
 if(fail&&!afterWrite)return false;
 bytes(at,after,n);codeHookRegistry.commit(at,before,after,n,t);writes.push_back({at,restore});return !fail;
}}
static bool restoreRiskSite(RiskSite&s){return !codeHookRegistry.active(s.address)||gameentry::patch(gameentry::Client::Assist,s.address,s.after,s.before,s.size);}
static bool installRiskSite(RiskSite&s){
 if(codeHookRegistry.active(s.address)){uint8_t b[16]{};if(read(s.address,b,s.size)&&!memcmp(b,s.after,s.size))return !codeHookRegistry.conflict(s.address);codeHookRegistry.markConflict(s.address);return false;}
 return gameentry::patch(gameentry::Client::Assist,s.address,s.before,s.after,s.size);
}
#include "../source/control_sites_bridge.h"
static const uint8_t nativeTail[]={0xB8,1,0,0,0,0xC3,0x8B,0x15,0x4C,0x3D,0xA8,0,0x33,0xC0,0x3B,0xCA,0x0F,0x94,0xC0,0xC3};
static const uint8_t entries[][5]={{0xA1,0x38,0xB2,0xA8,0},{0xE9,0xFB,0x2A,0x67,0},{0xE9,0x8B,0x2C,0x67,0}};
static void seed(int variant=1){memory.clear();codeHookRegistry={};shared={};writes.clear();controlSitesMode=0;riskControlActive=false;legacyPending=false;companions=true;failInstall=failRestore=0;afterWrite=false;
 for(const auto&p:patchcontext::Probes)bytes(p.address,p.bytes,p.size);
 for(const auto&s:mastercontrol::Sites)bytes(s.address,s.before,5);
 bytes(0x50B6F0,entries[variant],5);const uint8_t native[]={0x85,0xC0,0x75,0x1D},forward[]={0xCC,0xCC,0xCC,0xCC};bytes(0x50B6F5,variant?forward:native,4);bytes(0x50B710,nativeTail,sizeof nativeTail);
}
static void enabled(){shared.controlAll=controlmode::Master;controlSitesTick(true);}
static void stopped(){shared.controlAll=0;controlSitesTick(false);}
static bool at(uint32_t p,const uint8_t*b){uint8_t out[5]{};return read(p,out,5)&&!memcmp(out,b,5);}
int main(){
 for(int variant=0;variant<3;++variant){seed(variant);assert(mastercontrol::compatible(read));enabled();assert(shared.playerControlStatus==2&&codeHookRegistry.installed()==3&&!riskControlActive);
  assert(writes.size()==3&&writes[0].at==0x6339BA&&writes[1].at==0x637E40&&writes[2].at==0x50B6F0);
  for(const auto&s:mastercontrol::Sites)assert(at(s.address,s.after));
  const auto n=writes.size();controlSitesSuspend();controlSitesTick(true);assert(writes.size()==n&&shared.playerControlStatus==2);
  stopped();assert(shared.playerControlStatus==0&&!controlSitesOwned()&&at(0x50B6F0,entries[variant]));
  assert(writes[3].at==0x50B6F0&&writes[4].at==0x637E40&&writes[5].at==0x6339BA);
 }
 seed();shared.controlAll=controlmode::Native;controlSitesTick(true);assert(riskControlActive&&shared.playerControlStatus==1);
 enabled();assert(!riskControlActive&&shared.playerControlStatus==2&&codeHookRegistry.installed()==3);
 assert(writes[3].at==0x50B6F0&&writes[4].at==0x637E40&&writes[5].at==0x6339BA&&writes[8].at==0x50B6F0);
 shared.controlAll=controlmode::Native;controlSitesTick(true);assert(riskControlActive&&shared.playerControlStatus==1&&!controlTransitionPending());stopped();
 seed();companions=false;enabled();assert(shared.playerControlStatus==-2&&writes.empty());companions=true;controlSitesTick(true);assert(shared.playerControlStatus==2);stopped();
 seed();legacyPending=true;enabled();assert(writes.empty());legacyPending=false;controlSitesTick(true);assert(shared.playerControlStatus==2);stopped();
 // Failure on any install rolls back only owned bytes, including post-write failures.
 for(bool post:{false,true})for(const auto&s:mastercontrol::Sites){seed();afterWrite=post;failInstall=s.address;enabled();assert(shared.playerControlStatus==-2&&!controlSitesOwned());failInstall=0;enabled();assert(shared.playerControlStatus==2);stopped();}
 // Common restore failure MUST keep both strict CALLs; never switch mode early.
 seed();enabled();failRestore=0x50B6F0;shared.controlAll=controlmode::Native;controlSitesTick(true);assert(shared.playerControlStatus==-2&&codeHookRegistry.installed()==3&&controlTransitionPending());
 for(const auto&s:mastercontrol::Sites)assert(at(s.address,s.after));failRestore=0;controlSitesTick(true);assert(shared.playerControlStatus==1&&!controlTransitionPending());stopped();
 // Post-write failure may report error, but inactive common ownership proves it
 // is safe to retire strict sites. A later call recovers without a stale latch.
 seed();enabled();failRestore=0x50B6F0;afterWrite=true;stopped();assert(shared.playerControlStatus==-2&&!controlSitesOwned());failRestore=0;enabled();assert(shared.playerControlStatus==2);stopped();
 // Partial strict cleanup keeps Master pending and blocks native companions.
 seed();enabled();failRestore=0x637E40;stopped();assert(shared.playerControlStatus==-2&&codeHookRegistry.installed()==1&&controlTransitionPending());failRestore=0;stopped();assert(!controlTransitionPending());
 // Foreign mutation is never overwritten; common conflict retains strict sites.
 seed();enabled();memory[0x50B6F0]=0xCC;stopped();assert(shared.playerControlStatus==-2&&codeHookRegistry.installed()==3&&memory[0x50B6F0]==0xCC);
 seed();memory[0x50B6F1]^=1;enabled();assert(writes.empty()&&shared.playerControlStatus==-2);
 seed();memory[0x6339BA]^=1;enabled();assert(writes.empty()&&shared.playerControlStatus==-2);
 for(uint32_t p=0x50B710;p<0x50B724;++p){seed();memory[p]^=1;enabled();assert(writes.empty()&&shared.playerControlStatus==-2);}
 seed(0);memory[0x50B6F5]=0xCC;enabled();assert(writes.empty());
 seed();enabled();shared.controlAll=0;controlSitesSuspend();assert(!controlSitesOwned());
 seed();shared.controlAll=99;controlSitesTick(true);assert(writes.empty()&&shared.playerControlStatus==0);
 puts("PASS Master production bridge: 3 exact variants, ordered install/restore, Native transitions, suspension, preflight, companions, post-write errors, pending retry and foreign conflict preservation");
}

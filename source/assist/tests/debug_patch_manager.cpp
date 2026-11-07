// Execute the production manager against a dedicated synthetic low-VA arena.
// The runner supplies only OS-boundary stubs; no game process is opened.
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "../../shared/source/game_entry_manager.cpp"
static unsigned checks=0;static uint32_t token=0;
static void checked(bool value,unsigned line){++checks;if(!value){std::fprintf(stderr,"manager check %u failed at line %u\n",checks,line);std::abort();}}
#define check(value) checked((value),__LINE__)
static void put(uint32_t at,uint32_t value){memcpy((void*)(uintptr_t)at,&value,4);}
static void reset(){
 memset((void*)0x400000,0,0x800000);patches=assist::hookregistry::Registry<>{};memset(patchModules,0,sizeof patchModules);memset(patchErrors,0,sizeof patchErrors);
 policy={};busy=activeThread=activeModule=entries=reentrant=contended=wrongThread=0;boundThreadHandle=nullptr;boundThreadId=0;
 debugentries::activePolicy={};memset(debugentries::observations,0,sizeof debugentries::observations);fixtureUnreadable=0;fixtureFlush=true;fixtureProtect=true;
 patchcontext::verifiedCallDependency=nullptr;
 put(0xA8B238,0);put(0xA83D4C,0x900000);put(0xA8ED84,123);put(0xA8E9A0,1);put(0xA8B8C2,0);
 gameentry::Request r{};r.operation=gameentry::Operation::Enter;r.module=0x10000000;r.bindThread=1;
 check(ManageGameEntry(&r)==int(gameentry::Result::Ok));token=r.token;
}
static gameentry::Request observed(uint32_t at){gameentry::Request r{};r.address=at;r.module=0x10000000;r.bindThread=1;check(ManageGameEntry(&r)==int(gameentry::Result::Ok));return r;}
static int write(uint32_t at,const uint8_t*before,const uint8_t*after,unsigned n,uint32_t module=0x10000000){
 gameentry::Request r{};r.operation=gameentry::Operation::Patch;r.address=at;r.length=n;r.module=module;r.token=token;memcpy(r.before,before,n);memcpy(r.after,after,n);return ManageGameEntry(&r);
}
namespace gameentry {
static Request inspect(uint32_t at){Request r{};r.address=at;r.module=0x10000000;::ManageGameEntry(&r);return r;}
static bool patch(Client,uint32_t at,const uint8_t*before,const uint8_t*after,unsigned n){return ::write(at,before,after,n)==int(Result::Ok);}
}
using namespace assist;
static bool read(uint32_t at,void*out,size_t n){return patchRead(at,out,n);}
static int mark(uint32_t at){gameentry::Request r{};r.operation=gameentry::Operation::MarkConflict;r.address=at;r.module=0x10000000;r.token=token;return ManageGameEntry(&r);}
struct FixtureRegistry{bool active(uint32_t at){return gameentry::inspect(at).active;}bool conflict(uint32_t at){return gameentry::inspect(at).conflict;}void markConflict(uint32_t at){mark(at);}} codeHookRegistry;
struct RiskSite{uint32_t address;unsigned size;uint8_t before[16],after[16];};
#include "risk_site.inc"
#include "control_site.inc"
static void skip(uint32_t at,bool yes){auto next=debugentries::activePolicy;check(debugentries::setSkip(next,debugentries::id(debugentries::Kind::Patch,at),yes));check(debugentries::applyPolicy(next));}
static void seedFrame(){for(const auto&p:patchcontext::Probes)if(p.site==0x55DE81)memcpy((void*)(uintptr_t)p.address,p.bytes,p.size);
 const uint8_t original[]={0x89,0x15,0x84,0xED,0xA8,0};memcpy((void*)0x55DE81,original,6);}
static bool dependencyVerdict=true;static unsigned dependencyChecks=0;
static bool dependency(uint32_t at){++dependencyChecks;return at==0x4F7870&&dependencyVerdict;}
int main(){
 check(mmap((void*)0x400000,0x800000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)0x400000);
 check(mmap((void*)0x10000000,0x10000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)0x10000000);
 using namespace debugentries;using gameentry::Result;
 const uint8_t frame[]={0x89,0x15,0x84,0xED,0xA8,0};uint8_t detour[6]={0xE9,0,0,0,0,0x90};uint32_t relative=0x10001000-0x55DE81-5;memcpy(detour+1,&relative,4);
 reset();seedFrame();*(uint8_t*)0x55DE73^=1;
 check(write(0x55DE81,frame,detour,6)==int(Result::PatchDenied));check(!memcmp((void*)0x55DE81,frame,6));check(!patches.installed());
 check(observation(Kind::Patch,0x55DE81)->reason==Reason::Signature);
 skip(0x55DE81,true);fixtureUnreadable=0x55DE87;
 check(write(0x55DE81,frame,detour,6)==int(Result::PatchDenied));check(!patches.installed());check(observation(Kind::Patch,0x55DE81)->signature==Signature::Unreadable);
 fixtureUnreadable=0;check(write(0x55DE81,frame,detour,6)==int(Result::Ok));auto journal=observed(0x55DE81);
 check(journal.active&&journal.length==6&&!memcmp(journal.before,frame,6)&&!memcmp(journal.after,detour,6));check(observation(Kind::Patch,0x55DE81)->hook==Hook::Installed);
 skip(0x55DE81,false);journal=observed(0x55DE81);check(journal.active&&observation(Kind::Patch,0x55DE81)->signature==Signature::Unknown);
 put(0xA8E9A0,0);check(write(0x55DE81,journal.after,journal.before,journal.length)==int(Result::Ok));check(!observed(0x55DE81).active);check(!memcmp((void*)0x55DE81,frame,6));
 // Minimal data-operand template can journal a non-reference original.
 reset();const auto*s=nativeviewportpolicy::find(0x478BD1);uint8_t actual[16]{},after[16]{};memcpy(actual,s->before,s->n);memcpy(after,s->before,s->n);
 uint32_t originalData=0x887000,ourData=0x10002000;memcpy(actual+s->imm,&originalData,4);memcpy(after+s->imm,&ourData,4);memcpy((void*)0x478BD1,actual,s->n);
 observed(0x478BD1);check(observation(Kind::Patch,0x478BD1)->hook==Hook::Unavailable);check(observation(Kind::Patch,0x478BD1)->reason==Reason::Signature);
 check(write(s->at,actual,after,s->n)==int(Result::PatchDenied));skip(s->at,true);check(write(s->at,actual,after,s->n)==int(Result::Ok));
 journal=observed(s->at);check(journal.active&&!memcmp(journal.before,actual,s->n));skip(s->at,false);put(0xA8E9A0,0);
 check(write(s->at,journal.after,journal.before,journal.length)==int(Result::Ok));check(!memcmp((void*)(uintptr_t)s->at,actual,s->n));put(0xA8E9A0,1);
 check(write(s->at,actual,after,s->n)==int(Result::PatchDenied));skip(s->at,true);check(write(s->at,actual,after,s->n)==int(Result::Ok));
 *(uint8_t*)(uintptr_t)(s->at+2)^=1;journal=observed(s->at);check(journal.active&&observation(Kind::Patch,s->at)->hook==Hook::Conflict);check(observation(Kind::Patch,s->at)->reason==Reason::Foreign);
 uint8_t foreign[16]{};memcpy(foreign,(void*)(uintptr_t)s->at,s->n);check(write(s->at,journal.after,journal.before,journal.length)==int(Result::PatchDenied));check(!memcmp(foreign,(void*)(uintptr_t)s->at,s->n));check(patches.active(s->at));
 // Failed OS post-write work retains real ownership and reports pending.
 reset();seedFrame();fixtureFlush=false;check(write(0x55DE81,frame,detour,6)==int(Result::SystemFailure));journal=observed(0x55DE81);
 check(journal.active&&observation(Kind::Patch,0x55DE81)->hook==Hook::Pending);fixtureFlush=true;fixtureUnreadable=0x55DE81;
 observed(0x55DE81);check(observation(Kind::Patch,0x55DE81)->hook==Hook::Pending&&observation(Kind::Patch,0x55DE81)->reason==Reason::Unreadable);fixtureUnreadable=0;
 check(write(0x55DE81,journal.after,journal.before,6)==int(Result::Ok));check(!observed(0x55DE81).active);
 reset();seedFrame();skip(0x55DE81,true);check(write(0x55DE81,frame,detour,6,0x20000000)==int(Result::Busy));check(!patches.installed());
 fixtureProtect=false;check(write(0x55DE81,frame,detour,6)==int(Result::SystemFailure));check(!patches.installed()&&!memcmp((void*)0x55DE81,frame,6));
 check(write(0x646E90,frame,detour,5)==int(Result::PatchDenied));check(!patches.installed());
 // Extracted production common adapter captures the actual operand, then
 // retires through the manager journal when strict context checking resumes.
 reset();memcpy((void*)0x478BD1,actual,s->n);RiskSite risk{s->at,s->n,{},{}};memcpy(risk.before,s->before,s->n);memcpy(risk.after,after,s->n);
 skip(s->at,true);check(installRiskSite(risk));journal=observed(s->at);check(!memcmp(journal.before,actual,s->n));
 const auto reads=fixtureReads,queries=fixtureQueries;const uint32_t oldTick=observation(Kind::Patch,s->at)->tick;clockTick=123456;
 const auto cheap=gameentry::inspect(s->at);check(cheap.active&&fixtureReads==reads&&fixtureQueries==queries);check(observation(Kind::Patch,s->at)->tick==oldTick);
 observed(s->at);check(fixtureReads>reads&&fixtureQueries>queries);check(observation(Kind::Patch,s->at)->tick==clockTick);
 skip(s->at,false);check(!installRiskSite(risk));check(!patches.active(s->at));check(!memcmp((void*)(uintptr_t)s->at,actual,s->n));
 // A failed install has no active ownership. A later loader self-restoration
 // can be re-admitted without a permanent fictitious conflict record.
 reset();seedFrame();*(uint8_t*)0x55DE81^=1;
 check(write(0x55DE81,frame,detour,6)==int(Result::PatchDenied));check(!patches.conflict(0x55DE81)&&!patches.installed());
 memcpy((void*)0x55DE81,frame,6);check(write(0x55DE81,frame,detour,6)==int(Result::Ok));
 // Caller-reported dependency failure cannot poison an intact byte journal.
 check(mark(0x55DE81)==int(Result::Ok));check(!patches.conflict(0x55DE81));
 fixtureUnreadable=0x55DE81;check(mark(0x55DE81)==int(Result::SystemFailure));check(!patches.conflict(0x55DE81)&&patches.active(0x55DE81));
 fixtureUnreadable=0;check(write(0x55DE81,detour,frame,6)==int(Result::Ok));
 check(mark(0x55DE81)==int(Result::PatchDenied));check(!patches.conflict(0x55DE81));
 check(mark(0x6AA781)==int(Result::PatchDenied));check(patches.find(0x6AA781)<0);
 check(write(0x55DE81,frame,detour,6)==int(Result::Ok));*(uint8_t*)0x55DE85^=1;
 check(mark(0x55DE81)==int(Result::Ok));check(patches.conflict(0x55DE81));
 check(write(0x55DE81,detour,frame,6)==int(Result::PatchDenied));
 // A CLEANLY restored, non-conflicted slot starts a new original journal
 // after the current policy independently admits the changed operand.
 reset();memcpy((void*)(uintptr_t)s->at,actual,s->n);skip(s->at,true);
 check(write(s->at,actual,after,s->n)==int(Result::Ok));check(write(s->at,after,actual,s->n)==int(Result::Ok));
 uint8_t nextOriginal[16]{};memcpy(nextOriginal,actual,s->n);const uint32_t nextData=0x889000;memcpy(nextOriginal+s->imm,&nextData,4);memcpy((void*)(uintptr_t)s->at,nextOriginal,s->n);
 skip(s->at,false);check(write(s->at,nextOriginal,after,s->n)==int(Result::PatchDenied));
 skip(s->at,true);check(write(s->at,nextOriginal,after,s->n)==int(Result::Ok));journal=observed(s->at);
 check(!memcmp(journal.before,nextOriginal,s->n)&&memcmp(journal.before,actual,s->n));
 check(write(s->at,after,actual,s->n)==int(Result::PatchDenied));check(write(s->at,after,nextOriginal,s->n)==int(Result::Ok));
 check(!memcmp((void*)(uintptr_t)s->at,nextOriginal,s->n));
 // The extracted real control preflight separates context loss from byte
 // loss, retaining the ability to retire and later re-enable the same group.
 reset();RiskSite control{experimentalpatch::ControlOwner,5,{0xE8,0x31,0x7D,0xED,0xFF},{0xE8,0,0,0,0}};
 relative=0x10001000-control.address-5;memcpy(control.after+1,&relative,4);
 for(const auto&p:patchcontext::Probes)if(p.site==control.address)memcpy((void*)(uintptr_t)p.address,p.bytes,p.size);
 memcpy((void*)(uintptr_t)control.address,control.before,5);check(controlSiteReady(control));check(installRiskSite(control));
 *(uint8_t*)0x6339BF^=1;check(!controlSiteReady(control));check(!patches.conflict(control.address));check(restoreRiskSite(control));
 *(uint8_t*)0x6339BF^=1;check(installRiskSite(control));fixtureUnreadable=control.address;
 check(!controlSiteReady(control));check(!patches.conflict(control.address));fixtureUnreadable=0;check(restoreRiskSite(control));
 check(installRiskSite(control));*(uint8_t*)(uintptr_t)(control.address+4)^=1;
 check(!controlSiteReady(control));check(patches.conflict(control.address));check(!restoreRiskSite(control));
 // The two UI callsites accept a verified ORIGINAL-entry call contract.
 // It is independent of the Call bypass and never alters strict DR probes.
 for(uint32_t at:{experimentalpatch::TechDraw,experimentalpatch::TechPrune}){
  reset();for(const auto&p:patchcontext::Probes)if(p.site==at)memcpy((void*)(uintptr_t)p.address,p.bytes,p.size);
  *(uint8_t*)0x4F7877=0xE9;dependencyChecks=0;dependencyVerdict=true;
  check(!patchcontext::compatible(at,read));patchcontext::verifiedCallDependency=dependency;
  check(patchcontext::compatible(at,read));check(dependencyChecks==1);check(observation(Kind::Patch,at)->signature==Signature::Platform);
  check(!patchcontext::compatibleStrict(at,read));check(dependencyChecks==1);
  dependencyVerdict=false;Config onlyCall;check(setSkip(onlyCall,id(Kind::Call,0x4F7870),true));check(applyPolicy(onlyCall));
  check(!patchcontext::compatible(at,read));dependencyVerdict=true;fixtureUnreadable=0x4F7877;
  const auto calls=dependencyChecks;check(!patchcontext::compatible(at,read));check(dependencyChecks==calls);fixtureUnreadable=0;
  const uint32_t contextAt=at==experimentalpatch::TechDraw?0x6A97C3:0x6AA777;*(uint8_t*)(uintptr_t)contextAt^=1;
  check(!patchcontext::compatible(at,read));
 }
 std::printf("PASS: %u production manager admission/write/readback/journal/ownership checks; synthetic memory only\n",checks);
}

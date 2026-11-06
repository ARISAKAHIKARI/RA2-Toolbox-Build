// Production policies, no reconstructed admission model and no game process.
#include "../../shared/source/patch_context.h"
#include "../source/hook_registry.h"
#include <cstdio>
#include <cstdlib>
using namespace debugentries;
static unsigned checks=0;
static void check(bool value){++checks;if(!value){std::fprintf(stderr,"check %u failed\n",checks);std::abort();}}
int main(){
 auto zeros=[](uint32_t,void*out,size_t n){memset(out,0,n);return true;};
 unsigned patches=0,changed=0;
 for(const auto&e:Entries)if(e.kind==Kind::Patch&&experimentalpatch::size(e.address)){
  ++patches;Config strict{};applyPolicy(strict);
  const bool original=patchcontext::compatible(e.address,zeros);
  Config skip{};check(setSkip(skip,e.id,true));check(applyPolicy(skip));
  check(patchcontext::compatible(e.address,zeros));
  check(observation(Kind::Patch,e.address)->signature==Signature::Skipped);
  check(patchcontext::compatibleStrict(e.address,zeros)==original);
  check(!patchcontext::compatible(e.address,[](uint32_t,void*,size_t){return false;}));
  if(!original)++changed;
  check(signatureRequired(Kind::Call,e.address));check(signatureRequired(Kind::Breakpoint,e.address));
 }
 check(patches==82);check(changed>60);
 // Complete range reads continue after mismatch; the second pause probe is unreadable.
 Config skip{};setSkip(skip,id(Kind::Patch,0x55D360),true);applyPolicy(skip);unsigned reads=0;
 check(!patchcontext::compatible(0x55D360,[&](uint32_t at,void*out,size_t n){++reads;memset(out,0,n);return at!=0x55DECF;}));check(reads==2);
 using namespace assist::hookregistry;
 unsigned operands=0;
 for(const auto&s:nativeviewportpolicy::Sites)if(s.kind==nativeviewportpolicy::Kind::DataOperand){
  ++operands;Registry<> reg;uint8_t actual[16]{},after[16]{};memcpy(actual,s.before,s.n);memcpy(after,s.before,s.n);
  const uint32_t foreignData=0x887000,ourData=0x10002000;memcpy(actual+s.imm,&foreignData,4);memcpy(after+s.imm,&ourData,4);
  applyPolicy(Config{});check(reg.proposal(s.at,actual,after,s.n)==Transition::Deny);
  Config selected{};setSkip(selected,id(Kind::Patch,s.at),true);applyPolicy(selected);
  check(reg.proposal(s.at,actual,after,s.n)==Transition::Install);reg.commit(s.at,actual,after,s.n,Transition::Install);
  check(!memcmp(reg.slots[0].original,actual,s.n));
  applyPolicy(Config{});check(reg.proposal(s.at,after,actual,s.n)==Transition::Restore);
  check(reg.proposal(s.at,after,s.before,s.n)==Transition::Deny); // no stock-byte restore
  reg.commit(s.at,after,actual,s.n,Transition::Restore);
  check(reg.proposal(s.at,actual,after,s.n)==Transition::Deny); // strict reinstall cannot reuse skipped original
  applyPolicy(selected);check(reg.proposal(s.at,actual,after,s.n)==Transition::Install);
  actual[0]^=1;check(!admissibleOriginal(s.at,actual,s.n));check(observation(Kind::Patch,s.at)->reason==Reason::Structural);
 }
 check(operands==5);
 // Skip never changes fixed vtable target identity, overlap, or conflict ownership.
 const uint8_t slot[]={0x50,0x90,0x47,0},ours[]={0,0x20,0,0x10},foreign[]={0,0x30,0,0x10};Registry<> reg;
 Config v{};setSkip(v,id(Kind::Patch,0x7F74D8),true);applyPolicy(v);
 check(reg.proposal(0x7F74D8,foreign,ours,4)==Transition::Deny);check(observation(Kind::Patch,0x7F74D8)->reason==Reason::Structural);
 check(reg.proposal(0x7F74D8,slot,ours,4)==Transition::Install);reg.commit(0x7F74D8,slot,ours,4,Transition::Install);
 reg.markConflict(0x7F74D8);check(reg.proposal(0x7F74D8,ours,slot,4)==Transition::Deny);check(reg.active(0x7F74D8));
 std::printf("PASS: %u production patch checks; %u sites, %u context gates actually relaxed, %u journaled data operands\n",checks,patches,changed,operands);
}

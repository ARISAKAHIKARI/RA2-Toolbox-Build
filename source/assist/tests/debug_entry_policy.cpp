#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cassert>
#include <cstdio>
#include <set>
#include "../../shared/source/debug_entry_policy.h"
using namespace debugentries;
static unsigned checks=0;
static void check(bool value){++checks;assert(value);}
int main(){
 Config config;std::set<uint32_t> keys;
 for(const auto&e:Entries){
  check(keys.insert(e.id).second);check(e.id==id(e.kind,e.address));check(e.address<0x1000000);
  check(e.name&&*e.name&&e.group&&*e.group&&e.purpose&&*e.purpose&&e.features&&*e.features);
  check(required(config,e.id));
  if(e.flags&Checkable){check(setSkip(config,e.id,true));check(!required(config,e.id));}
  else check(!setSkip(config,e.id,true));
 }
 check(config.count<=Capacity);check(applyPolicy(config));
 for(const auto&e:Entries){check(signatureRequired(e.kind,e.address)==!(e.flags&Checkable));
  if(e.flags&Checkable){check(allow(e.kind,e.address,true,false));check(!allow(e.kind,e.address,false,true));}
 }
 check(!setSkip(config,0xffffffffu,true));
 check(signatureRequired(Kind::Call,0x123456));check(!allow(Kind::Call,0x123456,true,false));
 const uint32_t call=id(Kind::Call,0x4F7870),dr=id(Kind::Breakpoint,0x4F7870);
 check(call!=dr&&find(call)&&find(dr));
 check(signatureRequired(Kind::Call,0x714F7870)&&!observation(Kind::Call,0x714F7870));
 Config separate;check(setSkip(separate,call,true));check(applyPolicy(separate));
 check(!signatureRequired(Kind::Call,0x4F7870)&&signatureRequired(Kind::Breakpoint,0x4F7870));
 const uint32_t oldRevision=policyRevision;
 Config sameGeneration;check(sameGeneration.generation!=separate.generation);sameGeneration.generation=separate.generation;
 check(applyPolicy(sameGeneration)&&policyRevision!=oldRevision);check(signatureRequired(Kind::Call,0x4F7870));
 Config malformed;malformed.count=Capacity+1;const uint32_t revision=policyRevision;
 check(!applyPolicy(malformed)&&policyRevision==revision);
 Config mixed;mixed.count=3;mixed.skip[0]=call;mixed.skip[1]=call;mixed.skip[2]=0xffffffffu;
 check(applyPolicy(mixed)&&activePolicy.count==1&&activePolicy.skip[0]==call);
 clockTick=42;check(allow(Kind::Call,0x4F7870,true,false));
 auto*entry=observation(Kind::Call,0x4F7870);check(entry&&entry->signature==Signature::Skipped&&entry->hook==Hook::NA&&entry->tick==42);
 executableProbe=[](uint32_t,size_t){return false;};check(!allow(Kind::Call,0x4F7870,true,false));
 executableProbe=[](uint32_t,size_t){return true;};check(allow(Kind::Call,0x4F7870,true,false));executableProbe=nullptr;
 check(!allow(Kind::Call,0x4F7870,false,true));check(entry->signature==Signature::Unreadable);
 installation(Kind::Breakpoint,0x4F7870,Hook::Pending,Reason::OwnerPending,-9);
 Snapshot out;snapshot(out,43);check(out.count==Count&&out.tick==43&&out.policyGeneration==activePolicy.generation);
 const auto&o=out.entries[index(dr)];check(o.hook==Hook::Pending&&o.reason==Reason::OwnerPending&&o.error==-9);
 for(uint32_t i=0;i<Count;++i)check(out.entries[i].id==Entries[i].id);
 std::printf("PASS: %u catalogue, isolated-policy, invalid-input and truthful-observation checks\n",checks);
}

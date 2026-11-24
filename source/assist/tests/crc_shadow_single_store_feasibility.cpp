#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include "../source/crc_shadow_single_store_policy.h"

using namespace assist::crcshadowsingle;
static unsigned checks=0;
static void check(bool value,const char*name){
 if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::abort();}
 ++checks;
}
static Result nativeWrite(Scope&scope,uint32_t epoch,uint32_t house,uint32_t houseID,
                          uint8_t&field,uint8_t value){
 const Result decision=beforeStore(scope,epoch,house,houseID);
 bool pending=decision==Result::SuppressStore;
 if(!consumeSkip(pending))field=value;
 return decision;
}
int main(){
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"tool Reveal scope begins");
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::SuppressStore&&field==0&&s.suppressed==1,
        "only the matching tool House store is suppressed");
  end(s);check(!s.active,"scope cleanup clears state");
 }
 {
  Scope s;uint8_t field=1;
  check(begin(s,1,0x20000,7)==Result::Armed,"original-one scope begins");
  nativeWrite(s,1,0x20000,7,field,1);
  check(field==1,"suppression preserves an original one");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::NativeStore&&field==1,
        "natural Reveal outside tool scope executes original store");
 }
 {
  Scope s;uint8_t target=0,other=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"nested-House test scope begins");
  check(nativeWrite(s,1,0x21000,8,other,1)==Result::NativeStore&&other==1,
        "nested Reveal for another House remains native");
  check(nativeWrite(s,1,0x20000,7,target,1)==Result::SuppressStore&&target==0,
        "outer target House is still suppressed");
  check(s.suppressed==1,"only target House stores count as suppressed");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"identity-reuse scope begins");
  check(nativeWrite(s,1,0x20000,8,field,1)==Result::IdentityChanged&&field==1&&s.faulted,
        "same pointer with a different House identity is not suppressed");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"epoch-change scope begins");
  check(nativeWrite(s,2,0x20000,7,field,1)==Result::IdentityChanged&&field==1&&s.faulted,
        "world epoch change does not suppress a new-world store");
  end(s);
 }
 {
  Scope s;
  check(begin(s,1,0x20000,7)==Result::Armed,"matching abort scope begins");
  bool pending=false;
  check(abort(s,0x20000)&&s.faulted,
        "SEH abort identifies the exact target House");
  pending=true;check(consumeSkip(pending),"trampoline consumes fail-closed skip after callback fault");
  check(!consumeSkip(pending),"skip is one-shot");
  end(s);
 }
 {
  Scope s;
  check(begin(s,1,0x20000,7)==Result::Armed,"foreign abort scope begins");
  check(!abort(s,0x21000),
        "SEH fault in nested other-House reveal does not suppress its store");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"exception-before-store scope begins");
  end(s); // RuntimeScopedCall finally after native Reveal unwinds before the site.
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::NativeStore&&field==1&&!s.active,
        "finally clears scope so later natural Reveal is untouched");
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"repeated matching-store scope begins");
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::SuppressStore&&field==0,
        "first target write is suppressed");
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::NativeStore&&field==1&&s.suppressed==1,
        "a second same-House reveal inside the scope remains native");
  end(s);
 }
 {
  Scope s;
  check(begin(s,1,0x20000,7)==Result::Armed,"nested guard begins");
  check(begin(s,1,0x20000,7)==Result::Busy,"nested tool Reveal is rejected");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"stale-flag test begins");
  bool pending=beforeStore(s,1,0x20000,7)==Result::SuppressStore;
  end(s);                       // exception/early-return cleanup before trampoline
  pending=false;                // crcMapToolEnd clears the assembly-visible byte
  check(!consumeSkip(pending),"finally clears an unconsumed skip flag");
  check(nativeWrite(s,1,0x20000,7,field,1)==Result::NativeStore&&field==1,
        "no stale skip leaks into the next natural Reveal");
 }
 std::printf("CRC one-site store feasibility: %u checks PASS (synthetic target-identity model; no game/MP execution)\n",checks);
}

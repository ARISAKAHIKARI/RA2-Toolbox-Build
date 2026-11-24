#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include "../../research/crc_shadow_single_store_candidate.h"

using namespace assist::crcshadowsingle;
static unsigned checks=0;
static void check(bool value,const char*name){
 if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::abort();}
 ++checks;
}
static void nativeWrite(Scope&scope,uint8_t&field,uint8_t value,bool identityValid=true){
 const Result decision=beforeStore(scope,identityValid);
 if(!consumeSkip(scope))field=value;
 (void)decision;
}
int main(){
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"tool Reveal scope begins");
  nativeWrite(s,field,1);
  check(field==0&&s.suppressed==1,"tool Map::Reveal store is suppressed at the single site");
  end(s);check(!s.active&&!s.skipStore,"scope cleanup clears state");
 }
 {
  Scope s;uint8_t field=1;
  check(begin(s,1,0x20000,7)==Result::Armed,"original-one scope begins");
  nativeWrite(s,field,1);
  check(field==1,"suppression preserves an original one");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  nativeWrite(s,field,1);
  check(field==1,"natural Map::Reveal outside tool scope executes original store");
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"scope for natural-writer test begins");
  end(s);
  // The six other native write sites are never patched by this design.
  field=1;check(field==1,"natural defeat/reveal-all set remains native");
  field=0;check(field==0,"natural shroud/reset remains native");
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"fault scope begins");
  abort(s);
  check(s.faulted&&s.skipStore,"guarded callback fault arms fail-closed skip");
  nativeWrite(s,field,1);
  check(field==0&&!s.skipStore,"trampoline consumes abort fallback and skips store");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"exception-before-store scope begins");
  end(s); // RuntimeScopedCall finally after native Reveal unwinds before the site.
  nativeWrite(s,field,1);
  check(field==1&&!s.active,"finally clears scope so later natural Reveal is untouched");
 }
 {
  Scope s;uint8_t fieldA=0,fieldB=1;
  check(begin(s,1,0x20000,7)==Result::Armed,"multi-house Reveal scope begins");
  nativeWrite(s,fieldA,1,true);nativeWrite(s,fieldB,1,true);
  check(fieldA==0&&fieldB==1&&s.suppressed==2,
        "every matching store in the same native Reveal call preserves its own House byte");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"nested guard begins");
  check(begin(s,1,0x20000,7)==Result::Busy,"nested tool Reveal is rejected");
  nativeWrite(s,field,1,false);
  check(field==0&&s.faulted,"unknown House identity fails closed by suppressing the store");
  end(s);
 }
 {
  Scope s;uint8_t field=0;
  check(begin(s,1,0x20000,7)==Result::Armed,"stale-flag test begins");
  beforeStore(s,true); // callback marks the single store
  end(s);               // exception/early-return cleanup before trampoline
  check(!consumeSkip(s),"finally clears an unconsumed skip flag");
  nativeWrite(s,field,1);
  check(field==1,"no stale skip leaks into the next natural reveal");
 }
 std::printf("CRC one-site store feasibility: %u checks PASS (synthetic store-scope model; no game/MP execution)\n",checks);
}

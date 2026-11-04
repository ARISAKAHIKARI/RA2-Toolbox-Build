#define NOMINMAX
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include "../source/reveal_permission.h"
int main(){
 using namespace assist;
 int checks=0;
 auto check=[&](bool ok){assert(ok);++checks;};
 const uint32_t unrelated=AutoRepair|CustomCS|NativeAirWaypoints;
 volatile uint32_t flags=unrelated,request=41;
 // A newly enabled setting is not a request or a shared flags write itself.
 const bool freshlyEnabled=true;check(request==41&&flags==unrelated);
 // The actual production helper must publish permission before this callback.
 bool called=false;
 revealpermission::beforeRequest(flags,freshlyEnabled,[&]{
  check(flags==(unrelated|ExperimentalReveal));check(request==41);
  called=true;InterlockedIncrement(reinterpret_cast<volatile LONG*>(&request));
 });
 check(called&&request==42);
 // Other flags added between actions survive the permission publication.
 InterlockedOr(reinterpret_cast<volatile LONG*>(&flags),LONG(AdvancedInput));
 revealpermission::beforeRequest(flags,true,[&]{
  check(flags==(unrelated|AdvancedInput|ExperimentalReveal));
  InterlockedIncrement(reinterpret_cast<volatile LONG*>(&request));
 });
 check(request==43);
 // The permission operation itself never submits a command.
 revealpermission::publish(flags,false);check(flags==(unrelated|AdvancedInput)&&request==43);
 // First explicit action on a newly attached mapping has the same ordering.
 flags=0;request=0;
 revealpermission::beforeRequest(flags,true,[&]{
  check(flags==ExperimentalReveal&&request==0);
  InterlockedIncrement(reinterpret_cast<volatile LONG*>(&request));
 });
 check(request==1);
 std::printf("PASS: %d immediate reveal permission and request-order checks\n",checks);
}

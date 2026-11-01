#include "../source/air_path_policy.h"
#include "../source/shroud_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 using namespace assist;
 int checks=0;
 for(int session:{0,5,3,4}){
  // Compatibility was native in SP. It must now retain usable event routing
  // without either UI-gate or arrival replacement in every supported mode.
  assert(airpathpolicy::events(session,false,true,0));++checks;
  assert(!airpathpolicy::native(session,true,0));++checks;
  assert(!airpathpolicy::arrival(session,true,0));++checks;
  assert(airpathpolicy::events(session,true,false,0));++checks;
  for(unsigned mode:{1u,2u}){
   assert(airpathpolicy::native(session,true,mode));++checks;
   assert(!airpathpolicy::events(session,true,true,mode));++checks;
   assert(airpathpolicy::arrival(session,true,mode)==(mode==2));++checks;
  }
 }
 for(int session:{-1,1,2,6}){
  assert(!airpathpolicy::events(session,true,true,0));++checks;
  assert(!airpathpolicy::native(session,true,2));++checks;
 }
 for(int strategy:{1,2,5,6}){
  assert(shroudpolicy::materialMode(shroudpolicy::runtimeMode(strategy)));++checks;
  assert(shroudpolicy::fullPreview(strategy));++checks;
 }
 assert(shroudpolicy::runtimeMode(1)==shroudpolicy::runtimeMode(6));++checks;
 assert(shroudpolicy::runtimeMode(2)==shroudpolicy::runtimeMode(5));++checks;
 assert(shroudpolicy::runtimeMode(7)==2);++checks;
 assert(shroudpolicy::runtimeMode(8)==1);++checks;
 assert(shroudpolicy::runtimeMode(0)==0);++checks;
 assert(shroudpolicy::runtimeMode(3)==0);++checks;
 assert(shroudpolicy::runtimeMode(4)==3);++checks;
 std::printf("PASS: %d no-code default routes, explicit native alternatives and unsupported sessions\n",checks);
}

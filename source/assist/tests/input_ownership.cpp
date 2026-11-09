#include "../source/input_ownership.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace assist::inputownership;
static unsigned checks=0;
static void check(bool condition,const char*name){
 ++checks;
 if(!condition){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}
}
static bool owns(uint32_t deadline,uint32_t now){return ownsFormation(false,8,8,deadline,now);}
static void lease(uint32_t start,bool normalizedZero){
 const uint32_t deadline=afterGesture(start);
 check(deadline!=0,"published deadlines preserve zero sentinel");
 check(uint32_t(deadline-start)==(normalizedZero?201u:200u),"only zero normalization extends the lease");
 check(owns(deadline,start),"new lease owns input");
 check(owns(deadline,start+199),"lease protects the final ordinary millisecond");
 check(owns(deadline,start+200)==normalizedZero,"normal deadline expires at 200ms");
 check(!owns(deadline,deadline),"stored deadline is exclusive");
 check(!owns(deadline,start+201),"normalized deadline expires at 201ms");
 check(!owns(deadline,start+202),"expired lease cannot revive immediately");
}

int main(){
 check(!ownsFormation(true,0,0,0,10000),"unidentified active preview owns no input");
 check(!ownsFormation(true,7,8,0,10000),"active preview from another match owns no input");
 check(!ownsFormation(false,7,8,afterGesture(10000),10000),"lease from another match owns no input");
 check(!ownsFormation(false,8,0,afterGesture(10000),10000),"lease requires a current match");
 check(ownsFormation(true,8,8,0,10000),"current active preview retains protection without a lease");
 check(ownsFormation(true,8,8,0,0x90000000u),"current active preview remains protected at high uptime");
 check(ownsFormation(true,8,8,1,10000),"expired lease does not cancel a current active preview");
 for(uint32_t now:{0u,10000u,0x7fffffffu,0x80000000u,0x80000001u,0x90000000u,0xffffff38u,0xffffffffu})
  check(!owns(0,now),"unset deadline never owns input at any clock half or wrap boundary");
 check(!owns(1000,0x90000000u),"ancient nonzero deadline cannot revive after half a clock cycle");
 check(!owns(1202,1000),"deadline beyond the bounded lease is rejected");
 check(owns(1201,1000),"201ms normalization allowance is explicit");
 check(owns(1001,1000),"final live millisecond is protected");
 check(!owns(1000,1000),"deadline equality is expired");
 check(!owns(999,1000),"past deadline is expired");
 lease(10000,false);
 lease(0xffffff80u,false);
 lease(0xffffff38u,true);
 lease(0xfffffffeu,false);
 lease(0xffffffffu,false);
 std::printf("PASS input_ownership: %u checks; epoch, zero sentinel, 200ms leases, and normalized wrap boundary\n",checks);
}

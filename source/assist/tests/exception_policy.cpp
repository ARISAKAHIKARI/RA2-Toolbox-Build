#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/exception_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main(){
 using namespace assistguard;unsigned checks=0;auto check=[&](bool value){++checks;assert(value);};
 const unsigned boundaries[]={1,9,10,11,12,13,14,17,18,19,21,22,0,99};
 const unsigned controls[]={0x80000003u,0x80000004u,0x80000007u,0x40010005u,0x40010006u,0x40010008u,0x80010001u,0x406d1388u,0x80000026u,0x80000029u,0xc0000027u,0xe06d7363u,0xe0434352u,0xe0424242u};
 for(unsigned boundary:boundaries){
  for(unsigned code:controls)check(disposition(boundary,code,0)==Disposition::Search);
  for(unsigned flags:{2u,4u,8u,0x10u,0x20u,0x40u,0x66u,0x80u,0xfffffffeu})for(unsigned code:{0xc0000005u,0xc0000006u,0xc00000fdu})
   check(disposition(boundary,code,flags)==Disposition::Search);
 }
 for(unsigned boundary:{1u,10u,11u})for(unsigned code:{0xc0000005u,0xc0000006u,0xc0000094u,0xc00000fdu,0xc0000374u,0xc0000409u})
  check(disposition(boundary,code,0)==Disposition::ReportSearch);
 for(unsigned boundary:{9u,12u,13u,14u,17u,18u,19u,21u,22u}){
  for(unsigned code:{0xc0000005u,0xc0000006u})check(disposition(boundary,code,0)==Disposition::ReportHandle);
  for(unsigned code:{0xc000001du,0xc0000094u,0xc00000fdu,0xc0000374u,0xc0000409u})check(disposition(boundary,code,0)==Disposition::Search);
 }
 for(unsigned boundary:{0u,99u})check(disposition(boundary,0xc0000005u,0)==Disposition::Search);
 // The no-report VEH/native helpers use this exact record-based policy.
 Record record{};Pointers pointers{&record,nullptr};
 check(noReportAccess(nullptr)==0);check(noReportAccess(&pointers)==0);
 pointers.record=nullptr;check(noReportAccess(&pointers)==0);pointers.record=&record;
 for(unsigned code:controls){record.code=code;record.flags=0;check(noReportAccess(&pointers)==0);}
 for(unsigned code:{0xc0000005u,0xc0000006u}){
  record.code=code;
  for(unsigned flags:{0u,1u}){record.flags=flags;check(noReportAccess(&pointers)==1);}
  for(unsigned flags:{2u,4u,8u,0x10u,0x20u,0x40u,0x80u,0xfffffffeu}){record.flags=flags;check(noReportAccess(&pointers)==0);}
 }
 for(unsigned boundary:{1u,10u,11u})check(disposition(boundary,0xc0000005u,1)==Disposition::ReportSearch);
 for(unsigned boundary:{9u,12u,13u,14u,17u,18u,19u,21u,22u})check(disposition(boundary,0xc0000005u,1)==Disposition::ReportHandle);
 std::printf("PASS exception policy: %u checks; global faults forward, debug/control/unwind untouched, local access-only fallbacks\n",checks);
}

#include "../../shared/source/experimental_patch_policy.h"
#include "../source/hook_registry.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 using namespace experimentalpatch;using namespace assist::hookregistry;
 const uint32_t sites[]={Placement,AirType,AirArrival,ShroudDraw,FogDraw,C4Execution};
 const uint8_t original[][16]={{0xA1,0x4C,0x3D,0xA8,0},{0x50,0xCB,0x41,0},{0xC7,0x86,0xBC,0,0,0,3,0,0,0},{0x55,0x8B,0xEC,0x83,0xEC,0x34},{0xA0,0xC6,0xE7,0x89,0},{0x8A,0x81,0xC2,0x0E,0,0}};
 for(int i=0;i<6;++i){Registry<> registry;uint8_t patch[16];memset(patch,0x90,16);
  unsigned n=size(sites[i]);uint32_t target=0x10010000,offset=target-sites[i]-5;
  if(n==4)memcpy(patch,&target,4);else{patch[0]=0xE9;memcpy(patch+1,&offset,4);}
  assert(destination(sites[i],patch,n)==target);assert(destination(sites[i],patch,n-1)==0);
  assert(registry.proposal(sites[i],original[i],patch,n)==Transition::Install);
  assert(registry.proposal(sites[i],patch,original[i],n)==Transition::Deny);
  registry.commit(sites[i],original[i],patch,n,Transition::Install);
  assert(registry.active(sites[i]));assert(registry.proposal(sites[i],patch,original[i],n)==Transition::Restore);
  registry.commit(sites[i],patch,original[i],n,Transition::Restore);
  assert(!registry.active(sites[i]));registry.markConflict(sites[i]);
  assert(registry.proposal(sites[i],original[i],patch,n)==Transition::Deny);
 }
 // Neither old placement nor any retired input/simulation site is writable.
 for(uint32_t at:{0x464AC0u,0x646E90u,0x55D360u,0x6FFE00u,0x4D74E0u,0x6A97D2u})assert(size(at)==0);
 uint8_t invalid[10]={0xE9};assert(destination(AirArrival,invalid,10)==0);
 puts("PASS: six-site allowlist, shape/length checks, install/restore and sticky foreign conflict; retired sites denied");
}

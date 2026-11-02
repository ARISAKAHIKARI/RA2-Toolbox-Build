#include "../source/pause_view_policy.h"
#include "../../shared/source/patch_context.h"
#include "../source/hook_registry.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main(){
 using namespace assist;
 unsigned checks=0;auto expect=[&](bool value){++checks;assert(value);};
 pauseview::State s;
 s.observe(true,1,1,100);s.once();expect(!s.step&&!s.hold());
 s.toggle();expect(s.paused&&s.hold());
 s.once();expect(s.step&&!s.hold());
 s.once();expect(s.stepFrame==100);
 for(int i=0;i<20;++i){s.observe(true,1,1,100);expect(!s.hold()&&s.paused);}
 s.observe(true,1,1,101);expect(s.hold()&&!s.step&&!s.overshot);
 s.once();s.observe(true,1,1,104);expect(s.hold()&&s.overshot&&!s.step);
 s.once();expect(!s.hold()&&!s.overshot);
 s.toggle();expect(!s.paused&&!s.step&&!s.hold());
 for(int reason=0;reason<5;++reason){
  s.reset();s.observe(true,1,1,100);s.toggle();s.once();
  if(reason==0)s.observe(false,1,1,100); // focus/heartbeat/controller unavailable
  if(reason==1)s.observe(true,2,1,100);
  if(reason==2)s.observe(true,1,2,100);
  if(reason==3)s.observe(true,1,1,99);
  if(reason==4)s.observe(true,0,1,100);
  expect(!s.paused&&!s.step&&!s.overshot&&!s.hold());
 }
 for(int mode=-1;mode<10;++mode)expect(pauseview::singleplayer(mode)==(mode==0||mode==5));
 using namespace experimentalpatch;
 using namespace hookregistry;
 expect(size(PauseLoop)==5&&ownerAt(PauseLoop)==Owner::Frame);
 const uint8_t before[]={0xA0,0xA0,0xE9,0xA8,0};uint8_t after[5]={0xE9};
 uint32_t delta=0x10010000u-PauseLoop-5;memcpy(after+1,&delta,4);
 Registry<> registry;
 expect(registry.proposal(PauseLoop,before,after,5)==Transition::Install);
 registry.commit(PauseLoop,before,after,5,Transition::Install);
 expect(registry.active(PauseLoop));
 expect(registry.proposal(PauseLoop,after,before,5)==Transition::Restore);
 registry.commit(PauseLoop,after,before,5,Transition::Restore);
 expect(!registry.active(PauseLoop));registry.markConflict(PauseLoop);
 expect(registry.proposal(PauseLoop,before,after,5)==Transition::Deny);
 // The incoming prototype's TEST-at-entry assertion was false: displaced
 // MOV resumes at SUB ESP,1B4, before TEST and four register pushes.
 auto reader=[](uint32_t at,void*out,size_t n){for(const auto&p:patchcontext::Probes)
  if(p.site==PauseLoop&&p.address==at&&p.size==n){memcpy(out,p.bytes,n);return true;}return false;};
 expect(patchcontext::compatible(PauseLoop,reader));
 for(uint32_t changed:{0x55D365u,0x55DECFu})expect(!patchcontext::compatible(PauseLoop,[&](uint32_t at,void*out,size_t n){if(!reader(at,out,n))return false;if(at==changed)static_cast<uint8_t*>(out)[0]^=1;return true;}));
 printf("PASS: %u pause state/session/signature/ownership checks; no native game execution\n",checks);
}

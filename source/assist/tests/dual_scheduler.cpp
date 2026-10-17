#include <cassert>
#include <cstdint>
#include "../source/scheduler_policy.h"
#include "../source/frame_prologue.h"
#include "../source/frame_candidates.h"
int main(){
 using namespace scheduler;
 assert(messageMayPoll(Message,MessageActive,5000,4999));
 assert(!messageMayPoll(GameFrame,FrameActive,5000,4900));
 assert(messageMayPoll(GameFrame,FrameActive,5000,4700));
 assert(messageMayPoll(GameFrame,FrameConflict,5000,4999));
 assert(wantsFrame(GameFrame,true,false));
 assert(!wantsFrame(GameFrame,false,false));
 assert(!wantsFrame(GameFrame,true,true));
 const uint8_t stdPrologue[]={0x55,0x8b,0xec,0x83,0xec,0x20,0x90,0x90};
 assert(frameprologue::safeOverwrite(stdPrologue,sizeof stdPrologue)==6);
 const uint8_t knownEntry[]={0xa0,0xa0,0xe9,0xa8,0x00,0x81,0xec,0xb4};
 assert(frameprologue::safeOverwrite(knownEntry,sizeof knownEntry)==5);
 for(uint32_t id=1;id<=framecandidate::Count;++id){
  const auto* site=framecandidate::lookup(id);
  assert(site && framecandidate::validCallSite(id,site->context,21));
  assert(framecandidate::callTarget(site->at,site->context+8)==0x55D360);
  uint8_t altered[21];for(int i=0;i<21;++i)altered[i]=site->context[i];
  altered[0]^=1;
  assert(!framecandidate::validCallSite(id,altered,21));
 }
 const uint8_t patched[]={0xe9,0,0,0,0,0};
 assert(frameprologue::safeOverwrite(patched,sizeof patched)==0);
 const uint8_t call[]={0xe8,0,0,0,0,0};
 assert(frameprologue::safeOverwrite(call,sizeof call)==0);
 const uint8_t branch[]={0x55,0x8b,0xec,0x75,0x12,0x90};
 assert(frameprologue::safeOverwrite(branch,sizeof branch)==0);
 const uint8_t sib[]={0x8b,0x44,0x24,0x10,0x56,0x57};
 assert(frameprologue::safeOverwrite(sib,sizeof sib)==5);
}

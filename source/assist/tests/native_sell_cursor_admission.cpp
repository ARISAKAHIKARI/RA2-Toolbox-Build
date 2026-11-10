#include "../../shared/source/patch_context.h"
#include "../../shared/source/native_sell_policy.h"
#include "../source/hook_registry.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)),{});
 auto read=[&](uint32_t at,void*out,size_t n){
  if(at<0x400000||uint64_t(at-0x400000)+n>image.size())return false;
  memcpy(out,image.data()+at-0x400000,n);return true;
 };
 for(uint32_t at:{experimentalpatch::NativeSellCursor,experimentalpatch::NativeSellOwner}){
  if(!patchcontext::compatibleStrict(at,read))return 3;
  uint8_t original[16]{};const auto n=experimentalpatch::size(at);
  if(!read(at,original,n)||!assist::hookregistry::knownOriginal(at,original,n))return 4;
 }
 if(!nativesell::queueCompatible(read))return 5;
 if(nativesell::requiresNativeQueue(true,false)||!nativesell::requiresNativeQueue(true,true)||
    !nativesell::requiresNativeQueue(false,false)||!nativesell::requiresNativeQueue(false,true))return 6;
 // A platform replaces the downstream OutList append. The SP-AI adapter
 // consumes the Sell event and resumes at 4AC20C, before any of these bytes run.
 const uint8_t queueDetour[]={0xE9,0x6F,0x3E,0xC5,0x0F,0x90,0x90,0x90,0x90,0x90};
 memcpy(image.data()+0x4AC18C-0x400000,queueDetour,sizeof queueDetour);
 if(!patchcontext::compatibleStrict(experimentalpatch::NativeSellOwner,read)){
  std::puts("FAIL: unrelated downstream OutList replacement blocks native SP-AI Sell admission");return 1;
 }
 if(nativesell::queueCompatible(read))return 7;
 auto noQueueRead=[&](uint32_t at,void*out,size_t n){return at!=0x4AC18C&&read(at,out,n);};
 if(nativesell::queueCompatible(noQueueRead)||!patchcontext::compatibleStrict(experimentalpatch::NativeSellOwner,noQueueRead))return 8;
 unsigned checked=0;
 for(const auto&p:patchcontext::Probes){
  if(p.site!=experimentalpatch::NativeSellCursor&&p.site!=experimentalpatch::NativeSellOwner)continue;
  for(unsigned i=0;i<p.size;++i){
   auto&byte=image[p.address-0x400000+i];byte^=1;
   const bool allowed=patchcontext::compatibleStrict(p.site,read);byte^=1;
   if(allowed){std::printf("FAIL: required dependency %08X+%u was ignored\n",p.address,i);return 9;}
   ++checked;
  }
  auto blockedRead=[&](uint32_t at,void*out,size_t n){return at!=p.address&&read(at,out,n);};
  if(patchcontext::compatibleStrict(p.site,blockedRead))return 10;
 }
 // The SP exception never admits a replaced owned instruction at either site.
 for(uint32_t at:{experimentalpatch::NativeSellCursor,experimentalpatch::NativeSellOwner}){
  uint8_t foreign[16]{};memset(foreign,0x90,sizeof foreign);foreign[0]=0xE9;
  if(assist::hookregistry::structuralOriginal(at,foreign,experimentalpatch::size(at)))return 11;
 }
 std::printf("PASS: native SP-AI admission bypasses only the unused queue; %u required dependency-byte mutations rejected; human/MP queue gating retained\n",checked);
}

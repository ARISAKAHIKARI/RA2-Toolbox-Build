#include "../source/hook_registry.h"
#include "../../shared/source/experimental_patch_policy.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace assist::hookregistry;
int main(){
 const char*names[]={"Frame","CS","Cursor","NativeEvent","FootClick","Decide","MouseUp","Placement","Sidebar","NativeAir","RenderMask","C4Test","PlayerControl","Camera","Combat","NetworkTransport","NativeSell","NativeMega","ControlCommand","CrcShadow","Unknown"};
 static_assert(unsigned(Owner::Unknown)==20,"Update inventory owner names");
 struct Site{uint32_t at;unsigned size;Owner owner;};std::vector<Site> live,retired;
 for(uint32_t at=0x400000;at<0x800000;++at){const auto owner=ownerAt(at);unsigned n=experimentalpatch::size(at);
  if(n){assert(owner!=Owner::Unknown);assert(n<=16);live.push_back({at,n,owner});}
  else if(owner!=Owner::Unknown)retired.push_back({at,0,owner});
 }
 for(size_t i=1;i<live.size();++i)assert(live[i-1].at+live[i-1].size<=live[i].at);
 printf("{\"scope\":\"Source allowlist inventory; availability does not imply enabled or multiplayer-safe\",\"allowlisted_sites\":%zu,\"denied_legacy_sites\":%zu,\"sites\":[",live.size(),retired.size());
 for(size_t i=0;i<live.size();++i)printf("%s{\"address\":\"0x%06X\",\"length\":%u,\"owner\":\"%s\"}",i?",":"",live[i].at,live[i].size,names[unsigned(live[i].owner)]);
 printf("],\"denied_legacy\":[");for(size_t i=0;i<retired.size();++i)printf("%s{\"address\":\"0x%06X\",\"owner\":\"%s\"}",i?",":"",retired[i].at,names[unsigned(retired[i].owner)]);puts("]}");
}

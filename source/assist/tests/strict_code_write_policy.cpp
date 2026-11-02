// Compile the actual production manager with a test-only Win32 model.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "../../shared/source/game_entry_manager.cpp"
#include "../../shared/source/runtime_protocol.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void denyUnchanged(gameentry::Request r){
 unsigned char before[sizeof r],policyBefore[sizeof policy],registryBefore[sizeof patches];
 std::memcpy(before,&r,sizeof r);std::memcpy(policyBefore,&policy,sizeof policy);std::memcpy(registryBefore,&patches,sizeof patches);
 const auto callsBefore=mockwin::calls;
 const auto busyBefore=busy;
 CHECK(ManageGameEntry(&r)==int32_t(gameentry::Result::PatchDenied));
 CHECK(!std::memcmp(&r,before,sizeof r));
 CHECK(!std::memcmp(&policy,policyBefore,sizeof policy));
 CHECK(!std::memcmp(&patches,registryBefore,sizeof patches));
 CHECK(mockwin::calls==callsBefore&&busy==busyBefore);
}
int main(){
 using namespace gameentry;
 CHECK(!CodeWritesAllowed&&Version==4&&runtime::Version==5);
 CHECK(!strictCodeWritePolicy(nullptr));
 const uint32_t good[]={1,0,0},install[]={1,1,0},restore[]={1,0,1},old[]={0,0,0};
 CHECK(strictCodeWritePolicy(good)&&!strictCodeWritePolicy(install)&&!strictCodeWritePolicy(restore)&&!strictCodeWritePolicy(old));
 for(auto transition:{assist::hookregistry::Transition::Deny,assist::hookregistry::Transition::Install,assist::hookregistry::Transition::Restore})CHECK(!assist::hookregistry::mayApplyPatch(transition));
 CHECK(GameCodeWritePolicyInfo[0]==1&&GameCodeWritePolicyInfo[1]==0&&GameCodeWritePolicyInfo[2]==0);
 Request r{};r.operation=Operation::Patch;
 // Invalid targets must not be read even for an apparently legitimate patch.
 for(uint32_t address:{0u,1u,0x401000u,0x50B6F0u,0x7E2908u,0xFFFFFFFFu})
  for(uint32_t length:{0u,1u,5u,16u,17u,0xFFFFFFFFu})
   for(uint32_t client:{0u,1u,2u,3u,0xFFFFFFFFu}){
    r.address=address;r.length=length;r.client=Client(client);
    r.before[0]=0xE9;r.after[0]=0xA1;denyUnchanged(r);
   }
 r={};r.operation=Operation::Enter;r.client=Client::Assist;r.module=0x10000000;r.bindThread=1;
 CHECK(ManageGameEntry(&r)==int32_t(Result::Ok));
 Request active=r;active.operation=Operation::Validate;
 CHECK(ManageGameEntry(&active)==int32_t(Result::Ok));
 active.operation=Operation::Patch;active.address=0x50B6F0;active.length=5;
 denyUnchanged(active); // A valid execution lease does not authorize writes.
 // Seed legacy OWNERSHIP METADATA in the mock, never a real game patch.
 const uint8_t original[]={0xA1,0x38,0xB2,0xA8,0};
 const uint8_t redirect[]={0xE9,1,2,3,4};
 patches.commit(active.address,original,redirect,5,assist::hookregistry::Transition::Install);
 std::memcpy(active.before,redirect,5);std::memcpy(active.after,original,5);
 CHECK(patches.proposal(active.address,active.before,active.after,5)==assist::hookregistry::Transition::Restore);
 denyUnchanged(active); // Even a known owned restore is rejected.
 Request inspect{};inspect.operation=Operation::Inspect;inspect.module=r.module;
 CHECK(ManageGameEntry(&inspect)==int32_t(Result::Ok));
 CHECK(inspect.installed==1); // Never fake "clean" while legacy metadata exists.
 r.operation=Operation::Leave;CHECK(ManageGameEntry(&r)==int32_t(Result::Ok));
 busy=1;denyUnchanged(active);busy=0;
 Request legacy=active;legacy.version=3;
 CHECK(ManageGameEntry(&legacy)==int32_t(Result::Invalid));
 CHECK(ManageGameEntry(nullptr)==int32_t(Result::Invalid));
 wchar_t mapping[96]{};runtime::mapName(mapping,123,456);
 const wchar_t expected[]=L"Local\\RA2Runtime_v5_123_456_";
 CHECK(!std::memcmp(mapping,expected,sizeof expected));
 EntryRuntimeDetach();
 std::printf("PASS: %u strict manager checks; install/restore rejected before OS/target access; leases retained; old ABI isolated\n",checks);
}

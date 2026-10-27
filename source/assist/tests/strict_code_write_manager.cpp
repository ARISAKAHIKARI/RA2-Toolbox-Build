// Runs the actual manager, not a reimplementation of its policy. No game used.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../shared/source/game_entry_manager.cpp"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 using namespace assist::hookregistry;
 CHECK(GameCodeWritePolicyInfo[0]==1&&GameCodeWritePolicyInfo[1]==0&&GameCodeWritePolicyInfo[2]==0);
 for(unsigned i=0;i<256;++i)CHECK(!mayApplyPatch(static_cast<Transition>(i)));
 Request lease{};lease.operation=Operation::Enter;lease.module=0x20000;lease.bindThread=1;
 CHECK(ManageGameEntry(&lease)==int32_t(Result::Ok));
 unsigned char canary[16]={0x83,0xEC,0x70,0x32,0xC0};unsigned char saved[16];memcpy(saved,canary,16);
 const uint32_t addresses[]={0,1,0x646E90,0x50B6F0,0x7E2908,0xFFFFFFFFu,uint32_t(uintptr_t(canary))};
 for(uint32_t address:addresses)for(unsigned length=0;length<=32;++length){
  Request r=lease;r.operation=Operation::Patch;r.address=address;r.length=length;
  memset(r.before,0x83,16);memset(r.after,0x90,16);
  CHECK(ManageGameEntry(&r)==int32_t(Result::PatchDenied));
  CHECK(!memcmp(canary,saved,16));CHECK(!patches.installed());
 }
 // Even imported/legacy ownership metadata cannot authorize a restore.
 const uint8_t original[]={0x83,0xEC,0x70,0x32,0xC0},jump[]={0xE9,1,2,3,4};
 patches.commit(0x646E90,original,jump,5,Transition::Install);
 CHECK(patches.proposal(0x646E90,jump,original,5)==Transition::Restore);
 Request restore=lease;restore.operation=Operation::Patch;restore.address=0x646E90;restore.length=5;
 memcpy(restore.before,jump,5);memcpy(restore.after,original,5);
 CHECK(ManageGameEntry(&restore)==int32_t(Result::PatchDenied));CHECK(patches.installed()==1);
 Request inspect=lease;inspect.operation=Operation::Inspect;
 CHECK(ManageGameEntry(&inspect)==int32_t(Result::Ok));CHECK(inspect.installed==1);
 Request validate=lease;validate.operation=Operation::Validate;
 CHECK(ManageGameEntry(&validate)==int32_t(Result::Ok));
 Request leave=lease;leave.operation=Operation::Leave;CHECK(ManageGameEntry(&leave)==int32_t(Result::Ok));
 CHECK(ManageGameEntry(&restore)!=int32_t(Result::Ok));
 CHECK(ManageGameEntry(nullptr)==int32_t(Result::Invalid));
 Request bad{};bad.version=0;CHECK(ManageGameEntry(&bad)==int32_t(Result::Invalid));
 EntryRuntimeDetach();
 std::printf("PASS: %u actual-manager checks; install/restore/invalid targets denied, bytes unchanged, leases retained\n",checks);
}

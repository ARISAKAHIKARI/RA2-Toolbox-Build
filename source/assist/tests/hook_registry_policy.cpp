#include "../source/hook_registry.h"
#include <cassert>
#include <cstdio>
using namespace assist::hookregistry;
int main(){
 assert(NoInlineGamePatches);
 assert(!mayApplyPatch(Transition::Install));
 assert(!mayApplyPatch(Transition::Restore));
 assert(!mayApplyPatch(Transition::Deny));
 // This is a metadata model only; commit() below never touches game memory.
 Registry<> reg;
 const uint8_t sidebar[]={0xE8,0x99,0xE0,0xE4,0xFF};
 const uint8_t refresh[]={0xE8,0xEA,0xD0,0xE4,0xFF};
 const uint8_t redirected[]={0xE8,1,2,3,4};
 assert(ownerAt(0x6A97D2)==Owner::Sidebar&&ownerAt(0x6AA781)==Owner::Sidebar);
 assert(reg.proposal(0x6A97D2,sidebar,redirected,5)==Transition::Install);
 assert(reg.proposal(0x6AA781,refresh,redirected,5)==Transition::Install);
 assert(reg.proposal(0x6AA931,sidebar,redirected,5)==Transition::Deny);
 assert(reg.proposal(0x4F7870,sidebar,redirected,5)==Transition::Deny);
 const uint8_t orig[]={0x83,0xEC,0x70,0x32,0xC0};
 const uint8_t patch[]={0xE9,0x01,0x02,0x03,0x04};
 uint8_t unknown[]={0xE9,0,0,0,0};
 assert(ownerAt(0x646E90)==Owner::CS);
 assert(ownerAt(0xDEADBEEF)==Owner::Unknown);
 assert(reg.proposal(0x646E90,orig,patch,5)==Transition::Install);
 assert(reg.proposal(0x646E90,patch,orig,5)==Transition::Deny);
 assert(reg.proposal(0x646E90,unknown,orig,5)==Transition::Deny);
 reg.commit(0x646E90,orig,patch,5,Transition::Install);
 assert(reg.installed()==1);
 assert(reg.proposal(0x646E90,patch,orig,5)==Transition::Restore);
 assert(!mayApplyPatch(reg.proposal(0x646E90,patch,orig,5)));
 assert(reg.proposal(0x646E90,orig,patch,5)==Transition::Deny);
 reg.commit(0x646E90,patch,orig,5,Transition::Restore);
 assert(reg.installed()==0);
 assert(reg.proposal(0x646E90,orig,patch,5)==Transition::Install);
 reg.commit(0x646E90,orig,patch,5,Transition::Install);
 reg.commit(0x646E90,patch,orig,5,Transition::Restore);
 const uint8_t fresh[]={0xE9,9,8,7,6};
 assert(reg.proposal(0x646E90,orig,fresh,5)==Transition::Install);
 reg.commit(0x646E90,orig,fresh,5,Transition::Install);
 assert(reg.proposal(0x646E90,fresh,orig,5)==Transition::Restore);
 reg.markConflict(0x646E90);
 assert(reg.conflict(0x646E90));
 assert(reg.conflicts()==1);
 assert(reg.proposal(0x646E90,patch,orig,5)==Transition::Deny);
 assert(reg.proposal(0x646E90,orig,patch,5)==Transition::Deny);
 const uint8_t orig2[]={0x81,0xEC,0xF8,0,0,0};
 const uint8_t patch2[]={0xE9,1,2,3,4,0x90};
 assert(reg.proposal(0x4D74E0,orig2,patch2,6)==Transition::Install);
 reg.commit(0x4D74E0,orig2,patch2,6,Transition::Install);
 assert(reg.installed()==2);
 assert(reg.proposal(0x4D74E1,orig2,patch2,6)==Transition::Deny);
 const uint8_t frame[]={0xA0,0xA0,0xE9,0xA8,0};
 assert(reg.proposal(0x55D360,frame,patch,5)==Transition::Install);
 assert(reg.proposal(0x55D360,unknown,patch,5)==Transition::Deny);
 reg.markConflict(0x55D360);
 assert(reg.conflict(0x55D360));
 assert(reg.proposal(0x55D360,frame,patch,5)==Transition::Deny);
 puts("PASS: all patch writes denied, ownership metadata/conflicts remain diagnostic only");
}

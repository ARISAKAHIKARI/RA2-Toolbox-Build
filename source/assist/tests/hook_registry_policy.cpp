#include "../source/hook_registry.h"
#include "../../shared/source/experimental_patch_policy.h"
#include <cassert>
#include <iostream>
using namespace assist::hookregistry;
int main(){
 Registry<> reg;
 {
  Registry<> cursor;const uint8_t layout[]={0xA0,0x8B,0x47,0},target[]={0x10,0x20,0x30,0x10},foreign[]={0x11,0x22,0x33,0x10};
  assert(ownerAt(0x7F74C8)==Owner::Cursor&&experimentalpatch::size(0x7F74C8)==4&&experimentalpatch::destination(0x7F74C8,target,4)==0x10302010);
  assert(cursor.proposal(0x7F74C8,layout,target,4)==Transition::Install);
  assert(cursor.proposal(0x7F74C8,foreign,target,4)==Transition::Deny);
  cursor.commit(0x7F74C8,layout,target,4,Transition::Install);assert(cursor.active(0x7F74C8));
  assert(cursor.proposal(0x7F74C8,target,layout,4)==Transition::Restore);
  cursor.commit(0x7F74C8,target,layout,4,Transition::Restore);assert(!cursor.installed());
 }
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
 assert(reg.proposal(0x646E90,patch,orig,5)==Transition::Deny); // cannot restore foreign/unowned bytes
 assert(reg.proposal(0x646E90,unknown,orig,5)==Transition::Deny);
 reg.commit(0x646E90,orig,patch,5,Transition::Install);
 assert(reg.installed()==1);
 assert(reg.proposal(0x646E90,patch,orig,5)==Transition::Restore);
 assert(reg.proposal(0x646E90,orig,patch,5)==Transition::Deny); // no install while active
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
 // Separate code patches have separate owners; stale address is not writable.
 const uint8_t orig2[]={0x81,0xEC,0xF8,0,0,0};
 const uint8_t patch2[]={0xE9,1,2,3,4,0x90};
 assert(reg.proposal(0x4D74E0,orig2,patch2,6)==Transition::Install);
 reg.commit(0x4D74E0,orig2,patch2,6,Transition::Install);
 assert(reg.installed()==2);
 assert(reg.proposal(0x4D74E1,orig2,patch2,6)==Transition::Deny);
 const uint8_t frame[]={0xA0,0xA0,0xE9,0xA8,0};
 assert(reg.proposal(0x55D360,frame,patch,5)==Transition::Install);
 assert(reg.proposal(0x55D360,unknown,patch,5)==Transition::Deny);
 reg.markConflict(0x55D360); // a third party changed the site before our first write
 assert(reg.conflict(0x55D360));
 assert(reg.proposal(0x55D360,frame,patch,5)==Transition::Deny);
 // Distinct starting addresses do not authorize intersecting active ranges.
 Registry<> overlap;overlap.used=1;overlap.slots[0].address=0x6A97D0;overlap.slots[0].size=4;overlap.slots[0].active=true;
 assert(overlap.proposal(0x6A97D2,sidebar,redirected,5)==Transition::Deny);
 overlap.slots[0].conflict=true;assert(overlap.proposal(0x6A97D2,sidebar,redirected,5)==Transition::Deny);
 overlap.slots[0].active=false;assert(overlap.proposal(0x6A97D2,sidebar,redirected,5)==Transition::Install);
 std::cout<<"PASS: owned patch install/restore, sticky conflict, known original signatures and per-site isolation\n";
}

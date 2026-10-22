#include "signature_check.h"
#include <cassert>
#include <iostream>
int main(){
 probe::Snapshot s;s.bytes={0,0,0x53,0x56};s.valid={1,1,1,1};uint8_t expected[]={0x53,0x56};
 using S=probe::SignatureState;
 assert(probe::checkSignature(s,0x400000,0x400002,expected,2)==S::Match);
 s.bytes[2]=0xE9;assert(probe::checkSignature(s,0x400000,0x400002,expected,2)==S::Changed);
 s.valid[3]=0;assert(probe::checkSignature(s,0x400000,0x400002,expected,2)==S::Unreadable);
 assert(probe::checkSignature(s,0x400000,0x3fffff,expected,2)==S::Unreadable);
 assert(probe::checkSignature(s,0x400000,0x400004,expected,2)==S::Unreadable);
 uint32_t target=0;uint8_t jump[]={0xE9,0xfb,0xff,0xff,0xff};
 assert(probe::relativeJump(0x401000,jump,5,target)&&target==0x401000);
 assert(!probe::relativeJump(0x401000,jump,4,target));jump[0]=0x90;assert(!probe::relativeJump(0x401000,jump,5,target));
 std::cout<<"PASS signature match/change/unreadable distinction, bounds and rel32 decoding\n";
}

#include "input_lock.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace assist;
int main(){
 unsigned reads=0,writes=0;uint8_t lock=1;bool readable=true,writable=true,sticky=false;
 auto read=[&](uint32_t at,void*p,size_t n){assert(at==inputlock::Address&&n==1);++reads;if(!readable)return false;std::memcpy(p,&lock,n);return true;};
 auto write=[&](uint32_t at,const void*p,size_t n){assert(at==inputlock::Address&&n==1&&*(const uint8_t*)p==0);++writes;if(!writable)return false;if(!sticky)std::memcpy(&lock,p,n);return true;};
 auto tick=[&](bool enabled=true,bool playable=true){return inputlock::maintain(enabled,playable,read,write);};
 assert(tick(false)==inputlock::Off&&reads==0&&writes==0&&lock==1);
 assert(tick(true,false)==inputlock::Waiting&&reads==0&&writes==0&&lock==1);
 assert(tick()==inputlock::Unlocked&&reads==2&&writes==1&&lock==0);
 for(int i=0;i<60;++i)assert(tick()==inputlock::Watching);assert(writes==1);
 // Disabling does not reapply an old lock. A later trigger remains native.
 assert(tick(false)==inputlock::Off&&lock==0);lock=1;assert(tick(false)==inputlock::Off&&lock==1);
 assert(tick()==inputlock::Unlocked&&writes==2);lock=2;assert(tick()==inputlock::Unreadable&&writes==2&&lock==2);
 lock=1;readable=false;assert(tick()==inputlock::Unreadable&&writes==2);readable=true;writable=false;assert(tick()==inputlock::WriteFailed&&writes==3&&lock==1);
 writable=true;sticky=true;assert(tick()==inputlock::WriteFailed&&writes==4&&lock==1);sticky=false;assert(tick()==inputlock::Unlocked&&writes==5&&lock==0);
 std::cout<<"PASS input lock: conditional one-byte unlock, no idle rewrites, no relock, read/write/verification failures; controlled memory only\n";
}

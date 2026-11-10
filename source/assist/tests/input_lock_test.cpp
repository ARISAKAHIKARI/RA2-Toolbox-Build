#include "input_lock.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace assist;
int main(){
 unsigned reads=0,calls=0;uint32_t scenario=0x200000;uint8_t lock=1,global=1;bool readable=true,success=true,sticky=false,change=false;
 auto read=[&](uint32_t at,void*p,size_t n){++reads;if(!readable)return false;if(at==inputlock::Scenario){assert(n==4);memcpy(p,&scenario,4);}else if(at==scenario+inputlock::ScenarioLock){assert(n==1);memcpy(p,&lock,1);}else {assert(at==inputlock::Address&&n==1);memcpy(p,&global,1);}return true;};
 auto unlock=[&](uint32_t p){assert(p==scenario);++calls;if(!success)return false;if(!sticky)lock=global=0;if(change)scenario+=0x10000;return true;};
 auto tick=[&](bool e=true,bool p=true){return inputlock::maintain(e,p,read,unlock);};
 assert(tick(false)==inputlock::Off&&reads==0&&calls==0);
 assert(tick(true,false)==inputlock::Waiting&&reads==0&&calls==0);
 assert(tick()==inputlock::Unlocked&&calls==1&&lock==0&&global==0);
 global=1;for(int i=0;i<60;++i)assert(tick()==inputlock::Watching);assert(calls==1&&global==1); // modal lock kept
 lock=1;assert(tick()==inputlock::Unlocked&&calls==2);lock=2;assert(tick()==inputlock::Unreadable&&calls==2);
 lock=1;readable=false;assert(tick()==inputlock::Unreadable);readable=true;success=false;assert(tick()==inputlock::WriteFailed&&lock==1);
 success=true;sticky=true;assert(tick()==inputlock::WriteFailed);sticky=false;change=true;assert(tick()==inputlock::WriteFailed);
 assert(tick(false)==inputlock::Off&&lock==0);
 std::cout<<"PASS input locks: native callback, modal exclusion, idle, stale world and failure boundaries\n";
}

#include "../source/business_effects.h"
#include "../source/magnetron_cycle.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace assist;
int main(){
 using effects::Cargo;
 assert(effects::cargo(40,40,true)==Cargo::Pending);
 assert(effects::cargo(40,20,false)==Cargo::Pending);
 assert(effects::cargo(40,20,true)==Cargo::Decreasing);
 assert(effects::cargo(40,0,false)==Cargo::Pending);
 assert(effects::cargo(40,0,true)==Cargo::Delivered);
 assert(effects::cargo(40,-1,true)==Cargo::Unknown);
 assert(effects::cargo(40,std::numeric_limits<double>::quiet_NaN(),true)==Cargo::Unknown);
 assert(!magnetcycle::shotObserved(11,10,5,10,10,false));
 assert(!magnetcycle::shotObserved(11,10,5,10,9,false)); // unchanged rearm from old shot
 assert(magnetcycle::shotObserved(11,10,5,11,9,false));
 assert(!magnetcycle::shotObserved(10,10,5,11,9,true));
 using effects::Aircraft;
 assert(effects::aircraft(true,true,5,5,10,11)==Aircraft::TargetAcquired);
 assert(effects::aircraft(true,true,5,11,10,11)==Aircraft::ShotObserved);
 assert(effects::aircraft(true,false,5,11,10,11)==Aircraft::Pending);
 std::cout<<"PASS: cargo unloading requires dock evidence; no false old-ROF magnet shot; aircraft target acquisition differs from shot evidence\n";
}

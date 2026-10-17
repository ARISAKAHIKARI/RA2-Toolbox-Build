#include "../source/sidebar_policy.h"
#include <cassert>
#include <iostream>
using namespace assist::sidebarpolicy;
int main(){
 CameoSet s;assert(!s.contains(7,0));
 assert(s.add(6,0)&&s.contains(7,0));assert(s.add(1,31)&&s.contains(40,31));
 assert(!s.contains(40,32)&&!s.contains(16,31));assert(!s.add(40,-1)&&!s.add(70,0));
 s.clear();assert(!s.contains(7,0)&&!s.contains(40,31));
 for(int i=0;i<1024;++i)assert(s.add(7,i));
 assert(!s.add(7,1024));assert(s.contains(6,1023)&&!s.contains(6,1024));
 s.clear();assert(s.add(40,0)&&s.contains(1,0));
 std::cout<<"PASS: canonical sidebar identities, distinct types, bounded full-table lookup, clear and membership\n";
}

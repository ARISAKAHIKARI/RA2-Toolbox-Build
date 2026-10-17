#include "../source/tech_profile.h"
#include <cassert>
int main(){
 using namespace techprofile;
 auto p=preset();
 assert(p.contains(7,72)&&p.contains(16,8)&&p.contains(40,38));
 assert(!p.contains(7,0)&&!p.contains(3,72)&&!p.contains(40,999));
 auto parsed=parseIndexes("0 26 54 16 27 2,14;15 bad -1 4096 00034");
 assert(parsed.size()==9);assert(parsed.count(34));assert(!parsed.count(-1));
 Lists custom;custom.types[7]=parseIndexes("18 47");
 assert(allowed(custom,1,7,18,false));assert(!allowed(custom,1,7,72,false));
 assert(allowed(custom,2,16,999,true));assert(!allowed(custom,2,16,999,false));
 assert(allowed(custom,0,7,72,false));
}

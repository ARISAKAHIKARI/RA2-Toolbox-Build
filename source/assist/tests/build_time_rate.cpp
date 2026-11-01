#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/build_time_rate.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 using assist::buildtime::allowedRate;
 // Visible speeds 1..6. Single-player 6 explicitly follows 5.
 const unsigned sp[]={12,15,20,30,60,60},mp[]={12,15,20,30,45,60};
 for(int visible=1;visible<=6;++visible){for(int session:{0,5})assert(allowedRate(session,6-visible)==sp[visible-1]);for(int session:{3,4})assert(allowedRate(session,6-visible)==mp[visible-1]);}
 for(int session:{0,3,4,5}){assert(!allowedRate(session,-1));assert(!allowedRate(session,6));}
 assert(!allowedRate(2,1));assert(!allowedRate(-1,1));
 std::puts("PASS build time uses selected-speed ceilings; single-player unlimited equals speed 5; unknown input retains frame units.");
}

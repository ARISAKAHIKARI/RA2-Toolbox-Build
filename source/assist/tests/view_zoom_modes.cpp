#ifdef NDEBUG
#error Assertions are required
#endif
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../source/view_zoom.h"
using namespace assist;
int main(){unsigned checks=0;auto check=[&](bool value){++checks;assert(value);};
 for(int percent:viewzoom::Levels){check(viewzoom::valid(percent));check(viewzoom::Levels[viewzoom::index(percent)]==percent);}
 check(viewzoom::Count==11);check(viewzoom::index(100)==5);check(!viewzoom::valid(-1));check(!viewzoom::valid(51));
 check(viewzoom::step(50,-1)==0);check(viewzoom::step(0,-1)==0);check(viewzoom::step(400,1)==400);
 check(viewzoom::target({800,600},0)==viewzoom::Size{}); // legacy cannot interpret MAX
 check(viewzoom::nativeTarget({632,568},67)==viewzoom::Size{948,852});
 check(viewzoom::nativeTarget({632,568},80)==viewzoom::Size{790,710});
 check(viewzoom::nativeTarget({632,568},200)==viewzoom::Size{316,284});
 check(viewzoom::nativeTarget({632,568},300)==viewzoom::Size{211,189});
 check(viewzoom::nativeTarget({632,568},400)==viewzoom::Size{158,142});
 check(viewzoom::nativeTarget({632,568},0,{1200,435})==viewzoom::Size{1200,1078});
 check(viewzoom::nativeTarget({632,568},0,{600,1135})==viewzoom::Size{1263,1135});
 check(viewzoom::nativeTarget({632,568},0)==viewzoom::Size{});
 check(viewzoom::nativeTarget({632,568},0,{1,1})==viewzoom::Size{});
 check(viewzoom::validRaster(viewzoom::nativeTarget({1024,736},0,{12000,6135})));
 check(viewzoom::validRaster(viewzoom::nativeTarget({800,800},0,{12000,6135})));
 for(auto area:{viewzoom::Size{632,568},viewzoom::Size{1920,1080},viewzoom::Size{2560,1440}}){
  for(auto map:{viewzoom::Size{1200,435},viewzoom::Size{6000,3000},viewzoom::Size{12000,9000}}){
   auto result=viewzoom::nativeTarget(area,0,map);check(viewzoom::validRaster(result));
   check(result.width<=8192&&result.height<=8192&&int64_t(result.width)*result.height<=40000000);
   check(std::abs(double(result.width)/area.width-double(result.height)/area.height)<0.01);
  }
 }
 std::printf("{\"passed\":true,\"checks\":%u,\"assertions_active\":true}\n",checks);
}

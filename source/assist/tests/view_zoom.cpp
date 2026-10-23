#include "../source/view_zoom.h"
#include <cassert>
using namespace assist::viewzoom;
int main(){
 assert((target({1920,1080},50)==Size{3840,2160}));
 assert((target({1920,1080},75)==Size{2560,1440}));
 assert((target({1920,1080},100)==Size{1920,1080}));
 assert((target({1920,1080},125)==Size{1536,864}));
 assert((target({1920,1080},150)==Size{1280,720}));
 assert((target({3840,2160},50)==Size{7680,4320}));
 assert(!target({7680,4320},50).width);assert(!target({800,600},150).width);
 assert(!target({1920,1080},0).width);assert(!target({-1,1080},50).width);
 assert(step(50,-1)==50&&step(150,1)==150&&step(100,-1)==75&&step(100,1)==125);
 assert(sameAspect({1920,1080},{3840,2160}));assert(!sameAspect({1920,1080},{1280,1024}));
 assert(sameAspect({1366,768},target({1366,768},75)));
}

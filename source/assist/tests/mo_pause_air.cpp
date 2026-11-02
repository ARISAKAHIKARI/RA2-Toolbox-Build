#include "../source/pause_view_policy.h"
#include "../source/air_path_policy.h"
#include <cassert>
int main(){
 using namespace assist;
 pauseview::State state;
 state.observe(true,1,1,100);state.toggle();assert(state.hold());
 state.once();assert(!state.hold());assert(!state.hold());
 state.observe(true,1,1,101);assert(state.hold());
 state.once();state.once();assert(!state.hold());
 state.observe(true,1,1,102);assert(state.hold()&&!state.overshot);
 state.once();state.observe(true,1,1,105);assert(state.hold()&&state.overshot);
 state.observe(true,1,1,99);assert(!state.paused);
 state.toggle();state.observe(true,2,1,99);assert(!state.paused);
 state.toggle();state.observe(true,2,2,99);assert(!state.paused);
 state.toggle();state.observe(false,2,2,100);assert(!state.paused&&!state.step);
 assert(pauseview::singleplayer(0)&&pauseview::singleplayer(5));
 assert(!pauseview::singleplayer(3)&&!pauseview::singleplayer(4)&&!pauseview::singleplayer(6));
 assert(airpathpolicy::native(0,true,0)&&!airpathpolicy::arrival(0,true,0));
 assert(airpathpolicy::native(5,true,1)&&!airpathpolicy::arrival(5,true,1));
 assert(!airpathpolicy::native(3,true,0)&&airpathpolicy::events(3,false,true,0));
 assert(airpathpolicy::native(3,true,1)&&!airpathpolicy::arrival(3,true,1));
 assert(airpathpolicy::native(3,true,2)&&airpathpolicy::arrival(3,true,2));
 assert(!airpathpolicy::native(6,true,2)&&!airpathpolicy::events(6,false,true,2));
}

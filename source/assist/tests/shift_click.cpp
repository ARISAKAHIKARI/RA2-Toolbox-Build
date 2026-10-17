#include "../../cursor/source/features.h"
#include "../../cursor/source/perf_policy.h"
#include <cassert>
#include <iostream>
int main(){
 Burst b;b.down(10,20,true);assert(!b.ready(100));b.up(10,20,true,100,0);assert(b.left==4&&b.ready(100));
 for(int j=0;j<4;++j){assert(b.ready(100));b.sent(100,0);}assert(!b.left&&!b.ready(100));
 b.down(10,20,false);b.up(10,20,true,100,0);assert(!b.left); // Shift must be held at the actual down edge
 b.down(10,20,true);b.up(10,20,false,100,0);assert(!b.left); // release outside region/after focus or modifier change
 b.down(10,20,true);b.up(15,20,true,100,0);assert(!b.left); // drag
 b.down(10,20,true);b.up(14,20,true,100,25);assert(!b.ready(124)&&b.ready(125));b.sent(125,25);assert(b.left==3&&!b.ready(149));
 b.down(20,30,true);assert(!b.left);b.cancel();b.up(20,30,true,200,0);assert(!b.left);
 b.down(10,20,true);b.up(10,20,true,0xfffffff0u,25);assert(!b.ready(8)&&b.ready(9));
 assert(cursorperf::inputDeadline(33334,0,1000000,true)==4000); // 30 FPS drawing, 4 ms input polling
 assert(cursorperf::inputDeadline(2000,0,1000000,true)==2000); // 500 FPS unaffected
 assert(cursorperf::inputDeadline(33334,0,1000000,false)==33334);
 assert(cursorperf::inputDeadline(33334,0,0,true)==33334);
 std::cout<<"PASS: real Shift5 burst edges, release/drag/cancel, four extra clicks, intervals/wrap; physical polling independent of low display FPS without a second timer\n";
}

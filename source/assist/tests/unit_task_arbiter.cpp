#include "../source/unit_task_arbiter.h"
#include <cassert>
using namespace assist::taskarbiter;
int main(){
 FrameArbiter<2> a;a.begin(3,120);
 assert(a.canIssue(10,1));assert(a.record(10,1,Owner::AutoLoad));
 assert(!a.canIssue(10,1));assert(!a.canIssue(10,2));
 assert(a.record(11,0,Owner::Ore));assert(!a.canIssue(12,0));
 a.begin(3,121);assert(a.canIssue(10,1));
 assert(a.record(10,2,Owner::Unload));a.begin(4,121);
 assert(a.canIssue(10,2));a.clear();assert(a.canIssue(10,0));
}

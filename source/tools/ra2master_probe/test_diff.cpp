#include "diff_core.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace probe;
    Snapshot a{{1,2,3,4,5,6},{1,1,1,1,1,1},{1,1,1,1,1,1}}, b=a,c=a;
    b.bytes={9,9,9,9,9,9}; c.bytes[1]=8; a.valid[2]=0;
    b.stable[3]=0; c.valid[4]=0;
    auto r=runs(0,6,[&](size_t i){return reversible(a,b,c,i);});
    assert(r.size()==2 && r[0].offset==0 && r[0].length==1 && r[1].offset==5);
    assert(!changed(a,b,2)); assert(!available(a,6));
    assert(stableChanged(a,b,1) && !reversible(a,b,c,1)); // persistent change must remain visible
    assert(!stableChanged(a,b,2) && !stableChanged(a,b,3));
    assert(hex(b.bytes.data(),3)=="090909");
    auto longRun=runs(10,150,[](size_t){return true;});
    assert(longRun.size()==3 && longRun[0].length==64 && longRun[2].length==12);
    std::cout<<"PASS: reversible filtering, unread holes, unstable bytes, boundaries, chunking\n";
}

#include "../../cursor/source/snapshot_latch.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace cursorsnap;
int main(){Latch l;Sample s;s.frame=1;s.tick=10;s.seq=0x7ffffffe;assert(l.observe(s,10,32,32));
 s.seq=static_cast<int32_t>(0x80000000u);s.tick=11;assert(l.observe(s,10,32,32));
 s.seq=0x7ffffffe;s.tick=10;assert(!l.observe(s,10,32,32));
 s.seq=static_cast<int32_t>(0xfffffffeu);s.tick=12;assert(l.observe(s,10,32,32));
 s.seq=2;s.tick=13;assert(l.observe(s,10,32,32));
 s.seq=static_cast<int32_t>(0xfffffffeu);assert(!l.observe(s,10,32,32));
 for(uint32_t seq:{0u,3u,0x80000003u}){s.seq=static_cast<int32_t>(seq);assert(!l.observe(s,10,32,32));}
 s.seq=4;s.frame=-1;assert(!l.observe(s,10,32,32));s.frame=10;assert(!l.observe(s,10,32,32));
 l.clear();s.frame=1;s.seq=2;s.tick=0xfffffff0u;assert(l.observe(s,10,32,32));assert(l.age(16)==32);
 assert(cursorseq::next(0x7ffffffeu)==0x80000000u);assert(cursorseq::next(0xfffffffeu)==2);
 puts("PASS sequence sign/full wrap, stale/odd/zero rejection, invalid frame, session reset and clock wrap");
}

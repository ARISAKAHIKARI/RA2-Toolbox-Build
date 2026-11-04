#include "crc_shadow_model.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace crcshadowresearch;Model m;Identity a{1,0x20000,21,0},b{1,0x30000,31,1};
 m.frame(1,100);assert(m.beginTool(a,0,1));assert(m.crcValue(a,1)==0);
 assert(m.beginTool(a,1,1));assert(m.crcValue(a,1)==0); // Repeat must not bless our own value.
 m.naturalWrite(a,1);assert(m.crcValue(a,1)==1); // Real reveal while display is already1.
 m.naturalWrite(a,0);assert(m.crcValue(a,0)==0);assert(m.beginTool(a,0,1)&&m.crcValue(a,1)==0); // Real re-shroud updates baseline.
 assert(m.crcValue(b,1)==1);assert(m.beginTool(b,1,0));assert(m.crcValue(b,0)==1);
 auto reused=a;++reused.id;assert(m.crcValue(reused,1)==1); // Pointer/index alone is insufficient.
 m.unknownWrite(a);assert(m.crcValue(a,1)==1&&!m.beginTool(a,1,1));
 m.naturalWrite(a,0);assert(m.beginTool(a,0,1)&&m.crcValue(a,1)==0);
 m.frame(1,99);assert(m.crcValue(a,1)==1&&!m.beginTool(a,1,1));
 m.frame(2,0);assert(m.crcValue(a,1)==1);auto next=a;next.epoch=2;
 assert(m.beginTool(next,1,0)&&m.crcValue(next,0)==1);
 assert(m.crcValue(a,1)==1);auto bad=next;bad.index=32;assert(!m.beginTool(bad,0,1));
 assert(m.crcValue(next,2)==2);m.naturalWrite(next,2);assert(m.crcValue(next,0)==0&&!m.beginTool(next,0,1));
 m.frame(2,-1);assert(!m.beginTool(b,0,1)&&m.crcValue(next,1)==1);
 std::puts("PASS: original-value model; repeated tool writes, real same-value reveal, re-shroud, House isolation, pointer reuse, unknown-source blocking, frame rollback and new match. Research only.");
}

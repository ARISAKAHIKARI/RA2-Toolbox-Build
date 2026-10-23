#include "../../external_reader/source/sidebar_shapes.h"
#include <cassert>
#include <iostream>
using namespace external::sidebar;
struct Memory{std::vector<uint8_t>mem=std::vector<uint8_t>(0xC00000);uint32_t fail=0;int reads=0;
 bool operator()(uint32_t p,void*out,size_t n){++reads;if(p<0x10000||p+n>mem.size()||(fail&&p<=fail&&p+n>fail))return false;memcpy(out,mem.data()+p,n);return true;}
 template<class T>void put(uint32_t p,T v){memcpy(mem.data()+p,&v,sizeof v);}
};
int main(){Memory m;auto read=[&](uint32_t p,void*out,size_t n){return m(p,out,n);};uint32_t shp=0x600000,conv=0x610000,pal=0x620000;
 auto w=[&](uint32_t p,int v){m.put(p,v);};auto h=[&](uint32_t p,uint16_t v){m.put(p,v);};auto b=[&](uint32_t p,uint8_t v){m.put(p,v);};
 for(int i=0;i<4;++i){uint32_t q=0xB07C48+i*0x60;w(q+0x24,203+i);w(q+0x0C,2400+i*40);w(q+0x10,168);w(q+0x58,shp);w(q+0x50,conv);}
 w(conv+4,2);w(conv+0x174,pal);h(pal+2,0xF800);h(pal+4,0x07E0);
 h(shp+2,4);h(shp+4,2);h(shp+6,2);
 for(int i=0;i<2;++i){uint32_t f=shp+8+i*24;h(f+4,4);h(f+6,2);w(f+8,3);w(f+20,56+i*16);
  uint8_t bytes[16]={8,0,0,1,1,1,0,1,8,0,0,1,2,2,0,1};memcpy(m.mem.data()+shp+56+i*16,bytes,16);}
 Frame out;assert(sample(read,0,2560,1440,out));assert(out.x==2400&&out.y==168&&out.width==4&&out.height==2);assert(!out.pixels[0]&&!out.pixels[3]&&out.pixels[1]);assert(out.pixels[1]!=out.pixels[5]);
 b(0xB07C48+0x2C,1);assert(sample(read,0,2560,1440,out));
 uint32_t ref=0x630000;h(ref,65535);w(ref+12,shp);w(0xB07C48+0x58,ref);assert(sample(read,0,2560,1440,out));
 w(ref+12,0);assert(!sample(read,0,2560,1440,out)&&out.pixels.empty());w(ref+12,shp);
 // Red (normal), gold (selected), and all-backing (disabled) states.
 // Every tab deliberately shares a central stroke: comparing different tabs
 // would erase it. A warm-colored backing pixel must still remain untouched.
 h(pal+6,0xFD20);h(pal+8,0x4208);h(pal+10,0xA280);
 for(int i=0;i<4;++i){uint32_t shape=0x640000+i*0x100;w(0xB07C48+i*0x60+0x58,shape);h(shape+2,8);h(shape+4,2);h(shape+6,3);
  for(int f=0;f<3;++f){uint32_t q=shape+8+f*24;h(q+4,8);h(q+6,2);w(q+8,0);w(q+20,80+f*16);
   for(int k=0;k<16;++k)b(shape+80+f*16+k,(f!=2&&(k==i||k==12))?(f==1?3:1):(k==15?5:4));}
 }
 Cache cache;assert(cache.update(read,1000,1,2,2560,1440,15)==15);int calls=m.reads;assert(cache.update(read,1001,1,2,2560,1440,15)==15&&m.reads==calls);assert(!cache.frames[1].pixels.empty()&&!cache.frames[2].pixels.empty());
 assert(cache.frames[0].pixels[0]&&cache.frames[0].pixels[12]&&!cache.frames[0].pixels[15]&&!cache.frames[0].pixels[1]);auto yellow=cache.frames[0].pixels;assert(cache.update(read,1101,1,2,2560,1440,15,0)==15);assert(cache.frames[0].pixels!=yellow);
 assert(cache.update(read,1002,1,2,2560,1440,0)==0);assert(cache.update(read,1102,1,2,2560,1440,0)==0&&cache.frames[0].pixels.empty());
 auto rev=cache.revision;assert(cache.update(read,1200,1,3,2560,1440,9)==9&&cache.revision>rev);
 b(0xB07C48+0x1E,1);assert(cache.update(read,1400,1,3,2560,1440,9)==8);b(0xB07C48+0x1E,0);
 w(0xB07C48+0x58,shp);assert(!sample(read,0,1920,1080,out));m.fail=pal;assert(!sample(read,0,2560,1440,out));m.fail=0;
 b(0xB07C48+0x2C,0);h(shp+8+4,300);assert(!sample(read,0,2560,1440,out));h(shp+8+4,4);
 b(0xB07C48+0x2C,0);b(shp+56+3,10);assert(!sample(read,0,2560,1440,out));
 for(int format=0;format<=2;++format){w(shp+8+8,format);w(0xB07C48+0x58,shp);uint8_t bytes[14]={1,0,2,0,2,1,0,0};if(format==2){uint8_t prefixed[14]={8,0,1,0,1,2,0,1,6,0,2,1,0,2};memcpy(bytes,prefixed,14);}memcpy(m.mem.data()+shp+56,bytes,format==2?14:8);assert(sample(read,0,2560,1440,out));}
 std::cout<<"PASS: actual sidebar SHP frames 0/1/2, red/gold ink against own disabled frame, shared strokes and warm backing preserved, raw/RLE, references, palette recolor, transparency, button geometry, all four tabs/red idle/yellow paused, cache/read bounds/resize/round/disabled/read failure\n";
}

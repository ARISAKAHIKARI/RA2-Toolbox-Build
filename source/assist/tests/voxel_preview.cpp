#include "../../external_reader/source/voxel_preview.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <cstring>
using namespace external::voxel;
static std::vector<uint8_t> memory(2000000);constexpr uint32_t base=0x10000;
static bool read(uint32_t at,void*out,size_t n){if(at<base||uint64_t(at-base)+n>memory.size())return false;std::memcpy(out,memory.data()+at-base,n);return true;}
template<class T>void write(uint32_t at,T x){std::memcpy(memory.data()+at-base,&x,sizeof x);}
static void bytes(uint32_t at,const void*p,size_t n){std::memcpy(memory.data()+at-base,p,n);}
static Model capture(){Model m;assert(capturePart(read,base,base+0x100,m));return m;}
int main(int argc,char**argv){Library lib{};lib.headers=lib.tailers=1;lib.head=base+0x300;lib.tail=base+0x400;lib.body=base+0x1000;lib.size=19;write(base,lib);uint32_t motion[4]={0,1,1,base+0x200};bytes(base+0x100,motion,sizeof motion);float matrix[12]={1,0,0,0,0,1,0,0,0,0,1,0};bytes(base+0x200,matrix,sizeof matrix);
 Tail t{};t.start=lib.body;t.end=lib.body+4;t.data=lib.body+8;t.multiplier=.083333f;t.size[0]=t.size[1]=1;t.size[2]=4;t.normal=4;for(int n=0;n<8;++n){t.bounds[n*3]=(n&1)?1.f:-1.f;t.bounds[n*3+1]=(n&2)?1.f:-1.f;t.bounds[n*3+2]=(n&4)?4.f:0.f;}write(lib.tail,t);write<int32_t>(lib.body,0);write<int32_t>(lib.body+4,10);uint8_t span[]={0,4,16,1,17,1,18,1,19,1,4};bytes(lib.body+8,span,sizeof span);
 Model m=capture();assert(m.faces.size()==18);matrix[3]=8;bytes(base+0x200,matrix,sizeof matrix);auto moved=capture();for(size_t i=0;i<m.faces.size();++i)assert(std::abs(moved.faces[i].p[0].x-m.faces[i].p[0].x-8)<.001);matrix[3]=0;bytes(base+0x200,matrix,sizeof matrix);
 for(uint32_t broken:{0u,base-1,lib.body+100}){auto old=t.start;t.start=broken;write(lib.tail,t);Model bad;assert(!capturePart(read,base,base+0x100,bad));t.start=old;}write(lib.tail,t);
 memory[lib.body+8-base]=255;{Model bad;assert(!capturePart(read,base,base+0x100,bad));}bytes(lib.body+8,span,sizeof span);
 int reads=0;auto race=[&](uint32_t at,void*out,size_t n){bool ok=read(at,out,n);if(at==base&&++reads==2)static_cast<Library*>(out)->body+=4;return ok;};{Model bad;assert(!capturePart(race,base,base+0x100,bad));}
 std::array<uint32_t,256> pal{};for(int n=0;n<256;++n)pal[n]=0xff80d090;Image image;assert(render(m,0,pal,image));bool painted=false;for(uint32_t c:image.pixels)if(c){painted=true;assert((c>>24)==124&&((c>>16)&255)<=124&&((c>>8)&255)<=124&&(c&255)<=124);}assert(painted);
 // A constructed hull/turret/barrel scene exercises depth and direction in an
 // inspectable output without redistributing any game's copyrighted models.
 Model tank;auto block=[&](float x,float y,float z,float sx,float sy,float sz,int color){for(auto face:m.faces){for(auto&p:face.p){p.x=x+p.x*sx;p.y=y+p.y*sy;p.z=z+p.z*sz;}face.color=color;tank.faces.push_back(face);}};
 block(0,0,0,22,10,2,2);block(0,0,8,9,7,2.5,2);block(15,0,15,15,1.5,.6,0x10);pal[2]=0xff759151;pal[0x10]=0xffe6b450;assert(render(tank,.45,pal,image));FILE*f=argc>1?fopen(argv[1],"wb"):nullptr;if(f){fprintf(f,"P6\n%d %d\n255\n",image.width,image.height);for(uint32_t c:image.pixels){unsigned a=c>>24;uint8_t rgb[3]={uint8_t(((c>>16)&255)+(255-a)*20/255),uint8_t(((c>>8)&255)+(255-a)*26/255),uint8_t((c&255)+(255-a)*35/255)};fwrite(rgb,1,3,f);}fclose(f);}
 std::cout<<"PASS VXL spans, HVA translation once, bounds, malformed input, depth, premultiplied alpha\n";
}

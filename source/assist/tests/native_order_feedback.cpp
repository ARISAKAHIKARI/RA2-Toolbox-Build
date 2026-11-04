#include "../../external_reader/source/feedback_orders.h"
#include <cassert>
#include <cstdio>
using namespace external;
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0xC00000);size_t artReads=0;bool tear=false;int headerReads=0;
 template<class T>void put(uint32_t p,T value){std::memcpy(bytes.data()+p,&value,sizeof value);}
 bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>bytes.size())return false;if(p>=0x600000&&p<0x610000)++artReads;std::memcpy(out,bytes.data()+p,n);if(tear&&p==0xA8ECB8&&++headerReads==2)((uint32_t*)out)[4]=0;return true;}
};
int main(){
 using namespace nativefeedback;Memory memory;auto read=[&](uint32_t p,void*out,size_t n){return memory.read(p,out,n);};
 mainview::View view{0x20000,0x21000,0x22000,50,-100,2000,800,600,0,0,640,600};
 constexpr uint32_t source=0x10000,target=0x20000,cell=0x30000,owner=0x21000,pointers=0x40000,converter=0x70000,palette=0x71000;
 memory.put(0x843108,uint8_t(1));memory.put(0xB0EA80,40);memory.put(0xB0EA88,15);
 memory.put(0xA8ECB8+4,pointers);memory.put(0xA8ECB8+8,1);memory.put(0xA8ECB8+16,1);memory.put(pointers,source);
 memory.put(source,0x7F5C70u);memory.put(source+0x10,7u);memory.put(source+0x21C,owner);memory.put(source+0x83,uint8_t(1));
 memory.put(source+0x9C,73*256+128);memory.put(source+0xA0,73*256+128);memory.put(source+0x5A4,cell);
 memory.put(cell,0x7E4EECu);memory.put(cell+0x24,int16_t(74));memory.put(cell+0x26,int16_t(74));memory.put(cell+0x11B,int8_t(2));
 memory.put(0x87F6C4,converter);memory.put(converter+4,2u);memory.put(converter+0x174,palette);memory.put(palette+3*2,uint16_t(0x07e0));memory.put(palette+8*2,uint16_t(0xf800));
 const int shifts[]{11,3,0,3,5,2};std::memcpy(memory.bytes.data()+0x8A0DD0,shifts,sizeof shifts);
 std::vector<Stroke> lines;assert(actionLines(read,view,640,600,lines)&&lines.size()==1&&lines[0].color==0xff00fc00u);
 assert(lines[0].x0==100&&lines[0].y0==205&&lines[0].x1==100&&lines[0].y1==205);
 memory.put(target,0x7F5C70u);memory.put(target+0x9C,76*256+128);memory.put(target+0xA0,73*256+128);memory.put(source+0x2B4,target);
 assert(actionLines(read,view,640,600,lines)&&lines.size()==1&&lines[0].color==0xfff80000u&&lines[0].x1==190&&lines[0].y1==250);
 Canvas painted;Rect envelope;assert(paintInteractions({},lines,painted,envelope));assert(painted.pixels.size()>0);
 memory.put(0xB0EA80,35);assert(actionLines(read,view,640,600,lines)&&lines.empty()); // native timer expired
 memory.put(0xB0EA80,-1);assert(actionLines(read,view,640,600,lines)&&lines.size()==1); // paused countdown
 memory.put(source+0x21C,owner+4);assert(actionLines(read,view,640,600,lines)&&lines.empty());memory.put(source+0x21C,owner);
 memory.put(source+0x2B4,0u);memory.put(source+0x58C,0x50000u);memory.put(source+0x590,1);memory.put(source+0x598,1);memory.put(0x50000,target);
 assert(actionLines(read,view,640,600,lines)&&lines.size()==1&&lines[0].color==0xff00fc00u&&lines[0].x1==190);
 // A normal selection larger than 512 remains drawable. These are distinct
 // live objects, not duplicate vector entries or fabricated line segments.
 memory.put(source+0x58C,0u);memory.put(source+0x590,0);memory.put(source+0x598,0);
 for(unsigned n=0;n<600;++n){uint32_t object=0x100000+n*0x800;std::memcpy(memory.bytes.data()+object,memory.bytes.data()+source,0x700);memory.put(object+0x10,n+100u);memory.put(0x500000+n*4,object);}
 memory.put(0xA8ECB8+4,0x500000u);memory.put(0xA8ECB8+8,600u);memory.put(0xA8ECB8+16,600u);
 assert(actionLines(read,view,640,600,lines)&&lines.size()==600);
 memory.put(0xA8ECB8+4,pointers);memory.put(0xA8ECB8+8,1u);memory.put(0xA8ECB8+16,1u);
 memory.tear=true;assert(!actionLines(read,view,640,600,lines));memory.tear=false;
 memory.put(0x843108,uint8_t(0));assert(actionLines(read,view,640,600,lines)&&lines.empty());

 // Real foundation bits do not encode validity. Decode the actual cellsel
 // green/red-frame alpha union; color and MOD CanExist remain native.
 constexpr uint32_t shape=0x600000,foundation=0x50000,cells=0x100000,tile=0x300000;
 memory.put(Map+0xF4,64);memory.put(Map+0xF8,64);memory.put(Map+0xFC,10);memory.put(Map+0x100,10);memory.put(Map+0x104,1);memory.put(Map+0x108,1);
 memory.put(Map+0x13C,cells);memory.put(Map+0x140,512*512);memory.put(Map+0x1174,int16_t(64));memory.put(Map+0x1176,int16_t(64));
 memory.put(Map+0x117C,foundation);memory.put(Map+0x11A8,0x90000u);memory.put(0x8A03FC,shape);
 memory.put(foundation,int16_t(0));memory.put(foundation+2,int16_t(0));memory.put(foundation+4,int16_t(0x7fff));memory.put(foundation+6,int16_t(0x7fff));
 memory.put(cells+(64*512+64)*4,tile);memory.put(tile+0x24,int16_t(64));memory.put(tile+0x26,int16_t(64));memory.put(tile+0x11B,int8_t(0));memory.put(tile+0x11C,uint8_t(3));memory.put(tile+0x12C,uint8_t(2));
 memory.put(shape,uint16_t(0));memory.put(shape+2,uint16_t(8));memory.put(shape+4,uint16_t(8));memory.put(shape+6,uint16_t(6));
 for(int f=0;f<6;++f){uint32_t at=shape+8+f*24;memory.put(at,uint16_t(2));memory.put(at+2,uint16_t(2));memory.put(at+4,uint16_t(2));memory.put(at+6,uint16_t(2));memory.put(at+20,uint32_t(152+f*4));}
 const uint8_t green[]{1,0,0,1},red[]{0,2,2,0},copy[]{1,0,0,0};
 std::memcpy(memory.bytes.data()+shape+152,green,4);std::memcpy(memory.bytes.data()+shape+152+5*4,red,4);std::memcpy(memory.bytes.data()+shape+152+4,copy,4);
 view.cameraY=1875;FoundationStencil stencil;std::vector<Rect> holes;
 assert(stencil.capture(read,view,640,600,holes)&&holes.size()==2);
 assert(holes[0].x==98&&holes[0].y==42&&holes[0].w==2&&holes[0].h==1);
 assert(holes[1].x==98&&holes[1].y==43&&holes[1].w==2);
 auto reads=memory.artReads;assert(stencil.capture(read,view,640,600,holes)&&memory.artReads==reads);
 assert(stencil.capture(read,view,1280,1200,holes)&&holes.size()==4&&holes[0].x==196&&holes[0].w==4);
 // Camera movement reprojects the holes, rather than leaving a screen-space
 // transparent patch at the preceding viewport location.
 view.cameraX-=7;assert(stencil.capture(read,view,640,600,holes)&&holes[0].x==105);
 memory.put(tile+0x12C,uint8_t(0));assert(stencil.capture(read,view,640,600,holes)&&holes.empty());
 memory.put(Map+0x11A8,0u);memory.put(Map+0x1182,int16_t(64));memory.put(Map+0x1184,int16_t(64));memory.put(Map+0x118C,foundation);memory.put(Map+0x1194,0x90000u);
 memory.put(tile+0x12C,uint8_t(0x04));memory.put(tile+0x11C,uint8_t(0));assert(stencil.capture(read,view,640,600,holes)&&holes.size()==1&&holes[0].w==1);
 memory.put(Map+0x1194,0u);assert(stencil.capture(read,view,640,600,holes)&&holes.empty());
 std::puts("PASS: current native action-line flag/timer/targets/queue/colors, ownership and torn reads; exact active/copy foundation SHP alpha cutouts, scale/camera/cache, cancellation without guessing native legality");
}

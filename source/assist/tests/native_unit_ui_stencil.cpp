#include "../../external_reader/source/native_unit_ui_stencil.h"
#include <cassert>
#include <cstdio>
using namespace external;
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0xC00000);unsigned pixelReads=0,objectReads=0;bool tear=false;
 template<class T>void put(uint32_t p,T v){std::memcpy(bytes.data()+p,&v,sizeof v);}
 bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>bytes.size())return false;if(p>=0x600000+776&&p<0x601000)++pixelReads;
  std::memcpy(out,bytes.data()+p,n);if(tear&&p==0x10000&&n==0x300&&++objectReads==2){uint32_t changed=8;std::memcpy(static_cast<uint8_t*>(out)+0x10,&changed,4);}return true;}
};
int main(){
 using namespace nativefeedback;using namespace nativeeffects;Memory m;auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};
 constexpr uint32_t object=0x10000,type=0x20000,owner=0x30000,pointers=0x40000,child=0x50000,childType=0x52000,shape=0x600000;
 m.put(0xA8ECB8+4,pointers);m.put(0xA8ECB8+8,1u);m.put(0xA8ECB8+16,1u);m.put(pointers,object);m.put(0xA83D4C,owner);m.put(0xA8ED84,50);m.put(0xB0CD48,.0866025403784);
 m.put(object,0x7E22A4u);m.put(object+0x10,7u);m.put(object+0x6C4,type);m.put(object+0x21C,owner);m.put(object+0x83,uint8_t(1));m.put(object+0x90,uint8_t(1));m.put(object+0x6C,100);
 m.put(object+0x9C,10000);m.put(object+0xA0,11000);m.put(object+0xA4,128);m.put(object+0x2FC,3);m.put(type,0x7E2868u);m.put(0x7E2868+0x74,0x716290u);
 m.put(type+0x3D4,1);m.put(type+0x684,5);m.put(0xAC1480,shape);
 m.put(shape,uint16_t(0));m.put(shape+2,uint16_t(8));m.put(shape+4,uint16_t(8));m.put(shape+6,uint16_t(32));
 for(int f=0;f<32;++f){uint32_t at=shape+8+f*24;m.put(at,uint16_t(2));m.put(at+2,uint16_t(2));m.put(at+4,uint16_t(3));m.put(at+6,uint16_t(2));m.put(at+20,uint32_t(776+f*6));
  const uint8_t pixel[]{1,0,0,0,0,0};std::memcpy(m.bytes.data()+shape+776+f*6,pixel,6);}
 const uint8_t ammo[]{1,0,2,0,3,0},alternate[]{0,1,0,0,0,0},passenger[]{1,0,1,0,0,0},empty[]{0,0,0,0,0,1};
 std::memcpy(m.bytes.data()+shape+776+13*6,ammo,6);std::memcpy(m.bytes.data()+shape+776+12*6,alternate,6);
 std::memcpy(m.bytes.data()+shape+776+5*6,passenger,6);std::memcpy(m.bytes.data()+shape+776,empty,6);
 mainview::View view{0x20000,0x21000,0x22000,50,-200,1000,800,600,0,0,640,600};Object actor{object,7,type,owner,{70,200,30,30}};
 UnitUIStencil stencil;std::vector<Rect> holes;
 assert(stencil.capture(read,view,640,600,{actor},holes)&&mergeCutouts(holes,640,600)&&holes.size()==6);
 assert(holes[0].x==66&&holes[0].y==224&&holes[0].w==3&&holes[0].h==1);assert(holes[3].x==67&&holes[3].y==225&&holes[3].w==1);
 auto decoded=m.pixelReads;holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&m.pixelReads==decoded);
 view.cameraX-=7;holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&mergeCutouts(holes,640,600)&&holes[0].x==73);view.cameraX+=7;
 holes.clear();assert(stencil.capture(read,view,1280,1200,{actor},holes)&&mergeCutouts(holes,1280,1200)&&holes[0].x==132&&holes[0].y==448&&holes[0].w==6&&holes[0].h==1);
 m.put(object+0x2FC,0);holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());
 m.put(type+0x684,8);m.put(type+0x3E4,4);m.put(object+0x2FC,5);holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==4&&holes[0].x==66&&holes[3].x==78);
 // IFV uses its passenger's configured native pip frame; empty slots use 0.
 m.put(object,0x7F5C70u);m.put(type,0x7F6218u);m.put(0x7F6218+0x74,0x716290u);m.put(type+0x3D4,3);m.put(type+0x5E0,1);m.put(type+0x3E4,0);m.put(object+0x2FC,-1);
 holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1&&holes[0].x==68&&holes[0].y==228);
 m.put(object+0x114,1);m.put(object+0x118,child);m.put(child,0x7EB058u);m.put(child+0x10,44u);m.put(child+0x11C,object);m.put(child+0x6C0,childType);m.put(childType+0x380,1.0);m.put(childType+0xDFC,5u);
 holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==2&&holes[0].x==66&&holes[0].y==227&&holes[1].x==68);
 m.put(type+0x5E0,3);m.put(type+0x805,uint8_t(1));holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==4&&holes[2].x==80&&holes[3].x==84);
 m.put(type+0x805,uint8_t(0));m.put(childType+0x380,2.0);holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==4); // Size padding native frame3
 m.tear=true;m.objectReads=0;holes={{1,2,3,4}};assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1&&holes[0].x==1);m.tear=false;
 m.put(child+0x30,child);holes={{1,2,3,4}};assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1);m.put(child+0x30,0u);
 m.put(child+0x11C,0u);holes.clear();assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());m.put(child+0x11C,object);
 m.put(object+0x83,uint8_t(0));holes.clear();assert(!stencil.capture(read,view,640,600,{actor},holes));m.put(object+0x83,uint8_t(1));
 auto enemy=actor;enemy.owner=owner+4;holes.clear();assert(stencil.capture(read,view,640,600,{enemy},holes)&&holes.empty());
 m.put(0xB0CD48,std::numeric_limits<double>::quiet_NaN());holes={{1,2,3,4}};assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1);
 m.put(0xA8ECB8+16,0u);holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());
 std::puts("PASS native unit UI: selected owned aircraft ammo, wrap, IFV passenger/empty/Size/native frame, gap spacing; exact SHP alpha/camera/2x/h1/cache; atomic stale/cycle/parent/scale rejection; no screen capture or game writes");
}

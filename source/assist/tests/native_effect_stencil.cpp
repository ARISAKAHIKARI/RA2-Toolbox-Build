#include "../../external_reader/source/native_effect_stencil.h"
#include <cassert>
#include <cstdio>
using namespace external;
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0xC00000);size_t artReads=0;bool tear=false,failScale=false;unsigned clockReads=0;
 template<class T>void put(uint32_t p,T value){std::memcpy(bytes.data()+p,&value,sizeof value);}
 bool read(uint32_t p,void*out,size_t n){if((failScale&&p==0xB0CD48)||uint64_t(p)+n>bytes.size())return false;if(p>=0x600000&&p<0x610000)++artReads;std::memcpy(out,bytes.data()+p,n);if(tear&&p==0xA8ED84&&n==4&&++clockReads==2){int value;std::memcpy(&value,out,4);++value;std::memcpy(out,&value,4);}return true;}
};
int main(){
 using namespace nativefeedback;using namespace nativeeffects;
 Memory m;auto read=[&](uint32_t p,void*out,size_t n){return m.read(p,out,n);};
 constexpr uint32_t object=0x10000,type=0x20000,loco=0x30000,shape=0x600000;
 m.put(object,0x7F5C70u);m.put(object+0x10,7u);m.put(object+0x6C4,type);m.put(object+0x6D2,uint8_t(1));m.put(type+0xE0E,uint8_t(1));
 m.put(object+0x9C,10000);m.put(object+0xA0,11000);m.put(object+0xA4,128);m.put(object+0x538,3);
 m.put(object+0x674,loco);m.put(loco,0x7E7EB0u);m.put(loco+8,object);m.put(GatherShape,shape);m.put(0xA8ED84,50);
 m.put(0xB0CD48,.0866025403784);
 nativefeedback::Object actor{object,7,type,0,{10,10,20,20}};GatherState state;
 assert(gather(read,actor,50,state)&&state.frame==113&&state.point.x==10000&&state.point.y==10970);
 const int dx[]{0,21,30,21,-1,-22,-30,-22},dy[]{-30,-22,0,21,30,21,-1,-22};
 for(unsigned direction=0;direction<8;++direction){auto p=gatherPoint({10000,11000,128},direction);assert(p.x==10000+dx[direction]&&p.y==11000+dy[direction]&&p.z==128);}
 for(int raw=0;raw<65536;raw+=4096){m.put(object+0x388,uint16_t(raw));assert(gather(read,actor,50,state));unsigned direction=(((unsigned(raw)>>12)+1)>>1)&7;assert(state.frame==(7-int(direction))*15+8);}
 m.put(object+0x388,uint16_t(0));
 for(int phase:{8192,65536,std::numeric_limits<int>::max(),-1}){m.put(object+0x538,phase);assert(gather(read,actor,50,state));int32_t sum=int32_t(uint32_t(phase)+50u);assert(state.frame==105+sum%15);}m.put(object+0x538,3);
 m.put(object+0x6D2,uint8_t(0));assert(!gather(read,actor,50,state));m.put(object+0x6D2,uint8_t(1));
 m.put(type+0xE0E,uint8_t(0));assert(!gather(read,actor,50,state));m.put(type+0xE0E,uint8_t(1));
 m.put(object+0x270,uint8_t(1));assert(!gather(read,actor,50,state));m.put(object+0x270,uint8_t(0));
 m.put(object+0x278,1u);assert(!gather(read,actor,50,state));m.put(object+0x278,0u);
 m.put(loco,0x123456u);assert(!gather(read,actor,50,state));m.put(loco,0x7E7EB0u);
 m.put(loco+0x30,12000);m.put(loco+0x3C,12000);assert(!gather(read,actor,50,state));m.put(loco+0x30,0);m.put(loco+0x3C,0);
 m.put(object+0x39C,int16_t(8));m.put(object+0x390,48);m.put(object+0x398,5);assert(!gather(read,actor,50,state));m.put(object+0x39C,int16_t(0));
 m.put(object+0x10,8u);assert(!gather(read,actor,50,state));m.put(object+0x10,7u);
 m.put(shape,uint16_t(0));m.put(shape+2,uint16_t(8));m.put(shape+4,uint16_t(8));m.put(shape+6,uint16_t(120));
 for(int frame=0;frame<120;++frame){uint32_t at=shape+8+frame*24;m.put(at,uint16_t(2));m.put(at+2,uint16_t(2));m.put(at+4,uint16_t(3));m.put(at+6,uint16_t(2));m.put(at+20,uint32_t(2888+frame*6));const uint8_t pixels[]{1,0,2,0,3,0};std::memcpy(m.bytes.data()+shape+2888+frame*6,pixels,6);}
 mainview::View view{0x20000,0x21000,0x22000,50,-200,1000,800,600,0,0,640,600};
 EffectStencil stencil;std::vector<Rect> holes{{1,2,3,4}};
 assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==4&&holes.front().x==1);
 const auto first=holes[1];assert(first.x==85&&first.y==215&&first.w==1&&first.h==1);
 assert(holes[2].x==87&&holes[2].y==215&&holes[3].x==86&&holes[3].y==216);
 size_t decoded=m.artReads;holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes));assert(m.artReads-decoded==2); // header checks, no pixel decode
 view.cameraX-=7;holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes[0].x==first.x+7);
 holes.clear();assert(stencil.capture(read,view,1280,1200,{actor},holes)&&holes[0].w==2&&holes[0].h==1&&holes.size()==6);
 m.put(object+0x6D2,uint8_t(0));holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());m.put(object+0x6D2,uint8_t(1));
 m.tear=true;m.clockReads=0;holes={{1,2,3,4}};assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1&&holes[0].x==1);m.tear=false;
 m.failScale=true;assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1&&holes[0].x==1);
 m.put(object+0x6D2,uint8_t(0));holes.clear();assert(stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());m.put(object+0x6D2,uint8_t(1));m.failScale=false;
 for(double invalid:{0.,std::numeric_limits<double>::quiet_NaN()}){m.put(0xB0CD48,invalid);holes={{1,2,3,4}};assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.size()==1&&holes[0].x==1);}m.put(0xB0CD48,.0866025403784);
 m.put(GatherShape,0u);holes.clear();assert(!stencil.capture(read,view,640,600,{actor},holes)&&holes.empty());
 std::puts("PASS: native mining SHP current phase/facing, certified idle Drive admission, hidden/stale rejection; exact indexed-alpha spans, frame/cache/camera/scale, atomic failure; no live-game image certification");
}

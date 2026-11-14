#include "../source/native_pip_core.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>
using namespace nativepip;
struct IO {
 std::unordered_map<uint32_t,uint8_t> bytes;unsigned writes=0,failWrite=0;bool failRead=false;
 bool read(uint32_t p,void*out,size_t n){if(failRead)return false;for(size_t i=0;i<n;++i){auto it=bytes.find(p+uint32_t(i));if(it==bytes.end())return false;static_cast<uint8_t*>(out)[i]=it->second;}return true;}
 bool write(uint32_t p,const void*data,size_t n){if(++writes==failWrite)return false;for(size_t i=0;i<n;++i)bytes[p+uint32_t(i)]=static_cast<const uint8_t*>(data)[i];return true;}
 template<class T>void set(uint32_t p,const T&v){assert(write(p,&v,sizeof v));}
 template<class T>T get(uint32_t p){T v{};assert(read(p,&v,sizeof v));return v;}
};
constexpr uint32_t Tactical=0x100000;static constexpr uint32_t buffers[]={0x200000,0x210000,0x200000,0x220000,0x230000};
static IO world(){IO io;for(unsigned i=0;i<0x1000;++i)io.bytes[Tactical+i]=uint8_t(i);for(unsigned i=0;i<10;++i)io.set(Tactical+CameraOffsets[i],int32_t(100+i));for(unsigned i=0;i<5;++i)io.set(SurfaceSlots[i],uint32_t(0x300000+i*0x10000));io.set(Bounds,Rect{168,0,1000,700});io.set(Cache,Rect{168,0,1000,700});io.set(DirtyCount,int32_t(3));io.set(0xB0CE7C,uint32_t(0x350000));io.set(0xB0CE80,int32_t(6));for(unsigned i=0;i<60;++i)io.bytes[0x350000+i]=uint8_t(i);io.set(FullRedraw,int32_t(0));io.set(Tactical+0xDB0,int32_t(500));io.set(Tactical+0xE0,int32_t(800));for(unsigned i=0;i<6000;++i)io.bytes[Selectable+i]=uint8_t(i*3);io.writes=0;return io;}
int main(){
 for(unsigned fail=0;fail<=22;++fail){auto io=world();const auto before=io.bytes;Transaction tx;assert(tx.capture(io,Tactical));io.failWrite=fail;if(tx.enter(io,buffers,1200,1500)){assert(!fail);for(unsigned phase=0;phase<3;++phase){assert(io.get<Rect>(Bounds).w==RasterWidth);assert(io.get<uint32_t>(SurfaceSlots[0])==buffers[0]);assert(io.get<int32_t>(Tactical+0xDB0)==0);io.bytes[Selectable+phase]=99;io.bytes[Tactical+0xE4+phase]=98;}}
  io.failWrite=0;assert(tx.restore(io));assert(!tx.borrowed());assert(io.bytes==before);const auto writes=io.writes;assert(tx.restore(io)&&writes==io.writes);
 }
 for(int which=0;which<4;++which){auto io=world();Transaction tx;if(which==0)io.set(Tactical+0xDB0,int32_t(501));if(which==1)io.set(Tactical+0xE0,int32_t(801));if(which==2)io.failRead=true;if(which==3)io.set(SurfaceSlots[1],uint32_t(0));const auto before=io.bytes;assert(!tx.capture(io,Tactical));assert(!tx.enter(io,buffers,1200,1500));assert(tx.restore(io));assert(io.bytes==before);}
 auto io=world();const auto before=io.bytes;Transaction tx;assert(tx.capture(io,Tactical)&&tx.enter(io,buffers,1200,1500));io.failWrite=io.writes+1;assert(!tx.restore(io)&&tx.borrowed());io.failWrite=0;assert(tx.restore(io)&&io.bytes==before);
 // Renderer reallocation keeps the new backing; restore payload there.
 auto moved=world();Transaction movedTx;assert(movedTx.capture(moved,Tactical)&&movedTx.enter(moved,buffers,100,100));
 moved.set(0xB0CE7C,uint32_t(0x360000));moved.set(0xB0CE80,int32_t(2));
 assert(!movedTx.restore(moved)&&movedTx.borrowed()&&moved.get<int32_t>(DirtyCount)==0);
 moved.set(0xB0CE80,int32_t(12));assert(movedTx.restore(moved)&&!movedTx.borrowed());
 assert(moved.get<uint32_t>(0xB0CE7C)==0x360000&&moved.get<int32_t>(DirtyCount)==3);
 for(unsigned i=0;i<60;++i)assert(moved.bytes.at(0x360000+i)==uint8_t(i));
 Cadence c;assert(c.due(100,1));c.completed(100,1,1000);assert(!c.due(200,1));assert(!c.due(116,2));assert(c.due(117,2));c.completed(200,2,5000);assert(!c.due(249,3)&&c.due(250,3));c.completed(0xfffffff0u,5,1000);assert(!c.due(0,6)&&c.due(1,6));c.reset();assert(c.due(0,0));
 std::puts("PASS native PiP transaction: all partial-entry write failures, complete byte restoration, camera/adjacent zoom bytes, selection/visible tables, invalid layouts, retryable restoration and measured-cost cadence incl. clock wrap; synthetic memory, no engine execution");
}

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include "../source/minimap_viewport.h"

struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 unsigned rectReads=0;bool changeSecondRect=false;
 bool operator()(uint32_t at,void*out,size_t n){
  auto*p=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){auto it=bytes.find(at+uint32_t(i));if(it==bytes.end())return false;p[i]=it->second;}
  if(at==0x886FA0&&n==16&&++rectReads==2&&changeSecondRect)++static_cast<int*>(out)[3];return true;
 }
 template<class T>void set(uint32_t at,T value){const auto*p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[at+uint32_t(i)]=p[i];}
 void rect(uint32_t at,int x,int y,int w,int h){int values[]={x,y,w,h};for(int i=0;i<4;++i)set(at+4*i,values[i]);rectReads=0;}
};
struct ViewportShared {
 volatile long viewportSerial=2;
 volatile uint32_t viewportActive=0,viewportGeneration=1,viewportHostCapabilities=0,viewZoomBusy=0;
 int32_t viewportPhysical[4]{},viewportLogical[4]{};
};
int main(){
 using namespace assist;
 Memory memory;ViewportShared shared;constexpr uint32_t scenario=0x200000,house=0x300000,tactical=0x400000;
 auto read=[&](uint32_t at,void*out,size_t n){return memory(at,out,n);};
 external::radar::Geometry geometry;geometry.width=geometry.height=64;geometry.bounds={64,64,2,2,60,56};
 geometry.offsetX=59;geometry.offsetY=69;geometry.rawWidth=geometry.rw=120;geometry.rawHeight=geometry.rh=114;
 geometry.factor=1;geometry.cells=0x500000;geometry.count=512*512;geometry.house=house;assert(geometry.valid());
 for(auto pair:{std::pair{0xA8B230u,scenario},std::pair{0xA83D4Cu,house},std::pair{0x887324u,tactical}})memory.set(pair.first,pair.second);
 memory.set(0xA8ED84,10);memory.set<uint8_t>(0xA8E9A0,1);memory.set<uint8_t>(0xA8E378,1);memory.set(0xA8EDA0,0);memory.set(0xA8ED8C,0);
 memory.set(0x8A00A4,1920);memory.set(0x8A00A8,1080);
 const auto&bounds=geometry.bounds;int fields[]={bounds.width,bounds.height,bounds.left,bounds.top,bounds.columns,bounds.rows};
 for(int i=0;i<6;++i)memory.set(external::mapbounds::Map+0xF4+4*i,fields[i]);
 memory.set(tactical+0xB0,-1770);memory.set(tactical+0xB4,1035);
 memory.rect(0x886FA0,168,16,1700,1032);
 external::mainview::View view;assert(minimapview::sample(&shared,read,geometry,view));
 assert(view.width==1700&&view.height==1032&&view.x==168&&view.y==16);
 radargate::Mark rectangle;assert(minimapview::rectangle(geometry,view,506,275,rectangle));
 // The viewport's destination UI origin is not an extra world offset.
 assert(rectangle.left==1&&rectangle.top==1&&rectangle.right==239&&rectangle.bottom==165);
 int x=0,y=0;assert(minimapview::navigate(geometry,view,505,274,506,275,x,y));
 assert(x==950&&y==2244); // Native center: right=1800, bottom=2760, R half=850/516.
 int oldX=0,oldY=0;assert(external::radar::navigate(geometry,505,274,506,275,1760,1080,oldX,oldY));
 assert(oldX!=x&&oldY!=y); // A fixed sidebar subtraction cannot reach the same center.
 // Odd native dimensions retain the Runtime's integer half-size convention.
 memory.rect(0x886FA0,0,16,1759,1031);assert(minimapview::sample(&shared,read,geometry,view));
 assert(minimapview::navigate(geometry,view,505,274,506,275,x,y));assert(x==920&&y==2244);
 memory.set(tactical+0xB0,1830-1759);memory.set(tactical+0xB4,2745-1031);
 assert(minimapview::sample(&shared,read,geometry,view));assert(minimapview::rectangle(geometry,view,506,275,rectangle));
 assert(rectangle.right==505&&rectangle.bottom==274); // Exclusive end reaches the final interior pixel.
 // Active zoom publishes logical R dimensions, independent of physical A.
 shared.viewportActive=1;shared.viewportGeneration=2;shared.viewportSerial=4;
 const int physical[]={168,16,1700,1032},logical[]={0,0,1361,825};
 for(int i=0;i<4;++i){shared.viewportPhysical[i]=physical[i];shared.viewportLogical[i]=logical[i];}
 memory.rect(0x886FA0,0,0,1361,825);assert(minimapview::sample(&shared,read,geometry,view));
 assert(view.width==1361&&view.height==825&&view.physical().w==1700&&view.physical().h==1032);
 assert(minimapview::navigate(geometry,view,505,274,506,275,x,y));assert(x==1119&&y==2347);
 // Missing or transitioning reads do not invent a replacement rectangle.
 shared.viewportSerial=5;assert(!minimapview::sample(&shared,read,geometry,view));shared.viewportSerial=6;
 memory.changeSecondRect=true;memory.rectReads=0;assert(!minimapview::sample(&shared,read,geometry,view));memory.changeSecondRect=false;
 memory.set(0xA83D4C,house+4);assert(!minimapview::sample(&shared,read,geometry,view));memory.set(0xA83D4C,house);
 memory.set(external::mapbounds::Map+0x100,3);assert(!minimapview::sample(&shared,read,geometry,view));memory.set(external::mapbounds::Map+0x100,2);
 memory.bytes.erase(tactical+0xB4);assert(!minimapview::sample(&shared,read,geometry,view));
 std::puts("PASS: minimap uses stable native/zoom viewport, current world, native odd-size navigation and exclusive rectangle endpoints");
}

#include "../../external_reader/source/radar_map.h"
#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace external;
static std::vector<uint8_t> memory(0x1000000);
static bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}
template<class T>static void put(uint32_t p,T v){memcpy(memory.data()+p,&v,sizeof v);}
int main(int argc,char**){
 if(argc>1){mapbounds::Bounds b;int x,y,level;while(std::cin>>b.width>>b.height>>b.left>>b.top>>b.columns>>b.rows>>x>>y>>level){mapbounds::TileOrigin p;bool ok=b.tileOrigin(x,y,level,p);std::cout<<ok<<' '<<p.x<<' '<<p.y<<'\n';}return 0;}
 auto extent=mapbounds::nativeViewport(97,108,double(.3913043439388275f));assert(extent.valid());
 assert(extent.width>7436&&extent.width<7437&&extent.height>4140&&extent.height<4141);
 assert(extent.backingWidth()==7437&&extent.backingHeight()==4141);
 assert(!mapbounds::nativeViewport(0,108,1).valid()&&!mapbounds::nativeViewport(97,108,0).valid());
 mapbounds::Bounds bounds{131,146,3,5,124,135};mapbounds::TileOrigin border;
 assert(bounds.legal(68,73)&&!bounds.usable(68,73)&&bounds.tileOrigin(68,73,6,border));
 assert(border.x==(68-73)*30-30&&border.y==(68+73-6)*15);assert(!bounds.tileOrigin(65,66,0,border));
 // A supported signed negative height near the south viewport edge must
 // move the TMP down by 15, not up by 255*15.
 constexpr uint32_t cell=0x200000,cells=0x100000,house=0x300000;constexpr int x=5,y=5;
 put(radar::Map+0xF4,4);put(radar::Map+0xF8,4);put(radar::Map+0x104,4);put(radar::Map+0x108,4);put(radar::Map+0x13C,cells);put(radar::Map+0x140,4096);put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,4);put(radar::Map+0x1498,4);put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);
 put(0xA83D4C,house);put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x11B,int8_t(-1));put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));put(cell+0xEC,2);
 radar::Sampler<decltype(&read)> sampler(&read);assert(sampler.step(1,4096,true));auto frame=sampler.snapshot(1);assert(frame.terrain&&frame.terrain->art[y*512+x].level==-1);
 assert(frame.terrain->colors[y*512+x]==radar::terrainColor(2,-1,false));const auto&g=frame.terrain->geo;auto viewport=g.nativeViewport();assert(viewport.width==240&&viewport.height==120);mapbounds::TileOrigin at;assert(g.tileOrigin(x,y,frame.terrain->art[y*512+x].level,at)&&at.x==90&&at.y==105);
 // Native TMP plane: 29 nonempty rows/900 pixels. Preserve its actual
 // clipped edge instead of inserting invalid cells or cropping other columns.
 voxel::Image image;image.width=60;image.height=30;image.x=image.y=0;image.pixels.resize(1800);size_t diamondPixels=0;
 for(int yy=0;yy<30;++yy){int n=4*std::min(yy+1,29-yy);for(int xx=(60-n)/2;xx<(60+n)/2;++xx){image.pixels[yy*60+xx]=0xff2266aa;++diamondPixels;}}assert(diamondPixels==900);
 camera::Surface canvas;assert(canvas.reset(viewport.backingWidth(),viewport.backingHeight()));canvas.blit(image,float(at.x),float(at.y),1,1,0,{},0);
 assert(std::count(canvas.color.begin()+105*240,canvas.color.end(),0xff2266aa)>400);
 double u,v;g.project(x+.5,y+.5,u,v);assert(u*viewport.width==at.x+30&&v*viewport.height+15==at.y+15);
 auto flat=frame.terrain->art[y*512+x];flat.level=0;mapbounds::TileOrigin other;assert(g.tileOrigin(x,y,flat.level,other)&&at.y==other.y+15);
 auto original=g.nativeViewport();for(int level:{-1,0,2,6,14}){mapbounds::TileOrigin edge;assert(g.tileOrigin(x,y,level,edge));auto current=g.nativeViewport();assert(current.width==original.width&&current.height==original.height);}
 // A physically valid edge cell that remains shrouded is excluded in native
 // fog mode; explicit reveal displays it. This is separate from usable bounds.
 put(cell+0x120,int8_t(-2));assert(sampler.step(0,4096,true));auto masked=sampler.snapshot(2);
 assert(masked.terrain&&masked.terrain->colors[y*512+x]==0);
 assert(sampler.step(1,4096,true));auto unmasked=sampler.snapshot(3);
 assert(unmasked.terrain&&unmasked.terrain->colors[y*512+x]==radar::terrainColor(2,-1,false));
 std::cout<<"PASS signed south-edge height, native TMP origin/900-pixel footprint, exact fractional viewport with ceil backing, physical legal border retention and stable navigation domain\n";
}

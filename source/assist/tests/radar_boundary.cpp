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
 auto estimate=mapbounds::nativeViewport(97,108,double(.3913043439388275f));assert(estimate.valid());
 assert(estimate.width>7436&&estimate.width<7437&&estimate.height>4140&&estimate.height<4141);
 assert(estimate.backingWidth()==7437&&estimate.backingHeight()==4141);
 assert(!mapbounds::nativeViewport(0,108,1).valid()&&!mapbounds::nativeViewport(97,108,0).valid());
 mapbounds::Bounds bounds{131,146,3,5,124,135};mapbounds::TileOrigin border;
 assert(bounds.legal(68,73)&&!bounds.usable(68,73)&&bounds.tileOrigin(68,73,6,border));
 assert(border.x==(68-73)*30-30&&border.y==(68+73-6)*15);assert(!bounds.tileOrigin(65,66,0,border));
 // A supported signed negative height near the south viewport edge must
 // move the TMP down by 15, not up by 255*15.
 constexpr uint32_t cell=0x200000,cells=0x100000,house=0x300000;constexpr int x=5,y=5;
 put(radar::Map+0xF4,4);put(radar::Map+0xF8,4);put(radar::Map+0x104,4);put(radar::Map+0x108,4);put(radar::Map+0x13C,cells);put(radar::Map+0x140,4096);put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,4);put(radar::Map+0x1498,4);put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);
 put(0xA83D4C,house);put(0xA8ED84,1);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x11B,int8_t(-1));put(cell+0x12C,uint8_t(0x10));put(cell+0x140,uint8_t(0x01));put(cell+0xEC,2);
 // This geometry fixture has verified empty material catalogues. Native -1
 // denotes absent art; default zero would name an invalid first entry and
 // the production catalogue-completeness guard correctly rejects the bank.
 put(cell+0x38,-1);put(cell+0x44,-1);put(cell+0x48,-1);
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
 put(cell+0x12C,uint8_t(0));assert(sampler.step(0,4096,true));auto masked=sampler.snapshot(2);
 assert(masked.terrain&&masked.terrain->colors[y*512+x]==0);
 assert(sampler.step(1,4096,true));auto unmasked=sampler.snapshot(3);
 assert(unmasked.terrain&&unmasked.terrain->colors[y*512+x]==radar::terrainColor(2,-1,false));
 // Native raw RGB backing is integer-sized. The generated 97x108 surface's
 // factor does not give the exact 248x276 flat-cell admission rectangle.
 radar::Geometry captured{131,146,124,144,97,108,.3913043439388275f,0x100000,0x300000,512*512,
                          {131,146,3,5,124,135},248,276};
 assert(captured.valid()&&captured.nativeViewport().width==7440&&captured.nativeViewport().height==4140);
 assert(!captured.flatVisible(10,136)&&captured.flatVisible(11,136)); // x=-2/-1: half-open 2-cell width
 assert(captured.flatVisible(136,13)&&!captured.flatVisible(136,12)); // x=247/248
 assert(!captured.flatVisible(71,72)&&captured.flatVisible(72,72)); // y=-1/0
 assert(captured.flatVisible(210,209)&&!captured.flatVisible(210,210)); // y=275/276
 mapbounds::TileOrigin highOutside;assert(captured.tileOrigin(210,210,2,highOutside));
 assert(highOutside.y==4110&&highOutside.y<captured.nativeViewport().height); // visually raised, but flat cell is outside
 // The native SetVisibleRect has already inset the live playfield by west 2,
 // north 2, east 2 and south 6. In reveal mode, exclude physically legal
 // cells beyond each live edge; high ground can still qualify at the south.
 constexpr uint32_t borderCells=0x100000,firstBorderCell=0x400000;
 std::fill(memory.begin()+borderCells,memory.begin()+borderCells+512*512*4,0);
 put(radar::Map+0xF4,24);put(radar::Map+0xF8,24);put(radar::Map+0xFC,2);put(radar::Map+0x100,2);
 put(radar::Map+0x104,20);put(radar::Map+0x108,16);put(radar::Map+0x140,512*512);
 put(radar::Map+0x1488,1.f);put(radar::Map+0x1490,24);put(radar::Map+0x1498,24);put(radar::Map+0x14A4,64);put(radar::Map+0x14A8,40);put(radar::Map+0x1240,64);put(radar::Map+0x1244,40);
 auto liveBounds=mapbounds::sample(&read);assert(liveBounds.left==2&&liveBounds.top==2&&
  liveBounds.width-liveBounds.left-liveBounds.columns==2&&liveBounds.height-liveBounds.top-liveBounds.rows==6);
 struct Probe {int x,y,level;bool playable,insideRaw;};
 constexpr std::array<Probe,7> probes{{{20,20,0,true,true},{13,13,0,false,true},{8,28,0,false,true},
                                     {28,8,0,false,true},{31,32,0,false,true},{32,31,1,true,true},
                                     {32,32,2,true,false}}};
 for(size_t i=0;i<probes.size();++i){const auto&q=probes[i];uint32_t p=firstBorderCell+uint32_t(i)*0x1000;
  assert(liveBounds.legal(q.x,q.y)&&liveBounds.usable(q.x,q.y,q.level)==q.playable);
  put(borderCells+(q.y*512+q.x)*4,p);put(p+0x24,int16_t(q.x));put(p+0x26,int16_t(q.y));
  put(p+0x11B,int8_t(q.level));put(p+0x12C,uint8_t(0x10));put(p+0x140,uint8_t(0x01));put(p+0xEC,2);
  put(p+0x38,-1);put(p+0x44,-1);put(p+0x48,-1);
 }
 radar::Sampler<decltype(&read)> revealed(&read);for(int n=0;n<64;++n)assert(revealed.step(1,4096,true));
 auto clipped=revealed.snapshot(4);assert(clipped.terrain);
 for(const auto&q:probes){
  assert((clipped.terrain->colors[q.y*512+q.x]!=0)==(q.playable&&q.insideRaw));
  assert((clipped.terrain->baseColors[q.y*512+q.x]!=0)==q.insideRaw);
 }
 // Initial native radar fill may still contain explored physical edge cells;
 // the extra playfield mask belongs specifically to explicit unmask/reveal.
 radar::Sampler<decltype(&read)> native(&read);for(int n=0;n<64;++n)assert(native.step(0,4096,true));
 auto explored=native.snapshot(5);assert(explored.terrain);
 for(const auto&q:probes)assert((explored.terrain->colors[q.y*512+q.x]!=0)==q.insideRaw);
 // A shrouded, nonusable edge remains hidden in both modes, even though the
 // native RGB backing may contain its terrain color before the fog pass.
 put(firstBorderCell+4*0x1000+0x12C,uint8_t(0));
 radar::Sampler<decltype(&read)> fogged(&read);for(int n=0;n<64;++n)assert(fogged.step(0,4096,true));
 assert(fogged.snapshot(6).terrain->colors[32*512+31]==0);
 std::cout<<"PASS signed south-edge TMP, native raw 2x1 flat clip and exact backing dimensions, four-side playfield reveal, explored/shrouded border policy\n";
}

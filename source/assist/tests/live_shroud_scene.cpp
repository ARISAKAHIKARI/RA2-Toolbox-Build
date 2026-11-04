#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace external;
namespace {
constexpr uint32_t Cells=0x100000,Ground=0x200000,Clear=0x201000,House=0x300000,Tile=0x400000,Scenario=0x800000;
std::vector<uint8_t>memory(0x1000000);
size_t cellReads=0;bool lookupReadable=true,advanceDuringMask=false;
template<class T>void put(uint32_t p,T v){std::memcpy(memory.data()+p,&v,sizeof v);}
bool bytes(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size()||(!lookupReadable&&p==0x7F4194))return false;
 if(advanceDuringMask&&n==4096&&p>=0x600000&&p<0x700000){put(0xA8ED84,18);advanceDuringMask=false;}
 if(n==0x11D)++cellReads;std::memcpy(out,memory.data()+p,n);return true;}
struct LiveRead {bool operator()(uint32_t p,void*out,size_t n)const{return bytes(p,out,n);}bool fresh(uint32_t p,void*out,size_t n)const{return bytes(p,out,n);}};
bool shape(bool,int f,voxel::Image&i){i={};i.canvasWidth=i.width=60;i.canvasHeight=i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,0xFE);
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)if(std::abs(x-29.5)/30.+std::abs(y-14.5)/15.<1.)i.indices[y*60+x]=f==15?2:127;
 return true;}
struct Assets {
 struct ActorArt{};int loads=0,overlays=0;
 static size_t cost(const ActorArt&){return 1;}uint64_t revision(){return 1;}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,voxel::Image&i){++loads;i={};i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool shroud(bool fog,int f,voxel::Image&i){return shape(fog,f,i);}
 bool overlay(uint32_t,uint32_t,int frame,voxel::Image&i,bool=false){++overlays;i={};i.width=i.height=4;i.x=i.y=-2;i.pixels.assign(16,frame?0xffa020a0:0xff20a030);i.depth.assign(16,0);i.writeDepth=false;return true;}
 void buildingPose(uint32_t,uint32_t,visual::Pose&){}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 bool renderActor(const ActorArt&,const visual::Pose&,voxel::Image&){return false;}
};
void setup(){
 put(radar::Map+0xF4,64);put(radar::Map+0xF8,64);put(radar::Map+0xFC,0);put(radar::Map+0x100,0);put(radar::Map+0x104,64);put(radar::Map+0x108,64);
 put(radar::Map+0x13C,Cells);put(radar::Map+0x140,512*512);put(radar::Map+0x1488,1.f);
 put(radar::Map+0x1490,4);put(radar::Map+0x1498,124);put(radar::Map+0x14A4,8);put(radar::Map+0x14A8,8);put(radar::Map+0x1240,8);put(radar::Map+0x1244,8);
 put(0xA83D4C,House);put(0xA8B230,Scenario);put(0xA8ED84,10);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));
 put(Scenario+0x352C,100);for(int n=0;n<3;++n)put(Scenario+0x3534+n*4,100);
 put(Cells+(64*512+64)*4,Ground);put(Ground+0x24,int16_t(64));put(Ground+0x26,int16_t(64));put(Ground+0x11B,int8_t(2));put(Ground+0xEC,0);put(Ground+0x44,0);
 put(Cells+(63*512+63)*4,Clear);put(Clear+0x24,int16_t(63));put(Clear+0x26,int16_t(63));put(Clear+0x38,-1);put(Clear+0x44,-1);put(Clear+0x12C,uint8_t(0x10));put(Clear+0x140,uint8_t(1));
 put(0xA8ED28+4,0x500000u);put(0xA8ED28+8,1);put(0xA8ED28+16,1);put(0x500000,Tile);
 put(0xA83D80+4,0x500010u);put(0xA83D80+8,1);put(0xA83D80+16,1);put(0x500010,0x700000u);
 for(int n=0;n<256;++n)put(0x7F4194+n,int8_t(n?1:0));
}
}
int main(){
 setup();radar::Sampler<decltype(&bytes)>sampler(&bytes);
 for(int n=0;n<64;++n)assert(sampler.step(0,4096,true));auto f=sampler.snapshot(100);assert(f.terrain);
 const size_t source=64*512+64,dest=63*512+63;
 assert(f.terrain->colors[source]==0&&f.terrain->art[source].type==Tile&&f.terrain->art[source].sourceColor&&f.terrain->art[source].overlayType==0x700000);
 Assets assets;radar::SceneRenderer<Assets>renderer;
 auto image=renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true});assert(image);
 const size_t pixel=40*240+120;if(image->pixels[pixel]!=0xffa03020){const std::vector<uint8_t>*v=nullptr;renderer.feedbackVisibility(v);std::printf("pixel=%08x mask=%u sourceLevel=%d destFrame=%d loads=%d\n",image->pixels[pixel],v?unsigned((*v)[pixel]):999,int(f.terrain->art[source].level),int(f.terrain->shade[dest].shroud),assets.loads);std::fflush(stdout);}assert(image->pixels[pixel]==0xffa03020); // raised hidden SOURCE enters clear DESTINATION
 assert((image->pixels[70*240+120]&0xffffff)!=0xa03020); // source's own unknown flat region stays covered
 auto stats=renderer.nativeStats();const int loads=assets.loads,overlayLoads=assets.overlays;
 // Keep the exact terrain bank/logic observation. Visibility alone changes
 // now and must not wait for another 32 bounded whole-map terrain passes.
 put(0xA8ED84,11);put(Clear+0x12C,uint8_t(0));
 auto covered=renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true});assert(covered&&covered->pixels[pixel]!=image->pixels[pixel]);
 assert(renderer.nativeStats().cells==stats.cells&&renderer.nativeStats().presents==stats.presents&&assets.loads==loads&&assets.overlays==overlayLoads);
 put(Clear+0x12C,uint8_t(0x10));put(0xA8ED84,12);
 auto opened=renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true});assert(opened&&opened->pixels[pixel]==image->pixels[pixel]);
 assert(renderer.nativeStats().cells==stats.cells&&renderer.nativeStats().presents==stats.presents&&assets.loads==loads);
 // The real sampler may now publish its next whole terrain bank. A visibility
 // flag change cannot add/remove overlay source metadata or rebuild terrain.
 put(Ground+0x12C,uint8_t(0x10));put(0xA8ED84,13);
 for(int n=0;n<32;++n)assert(sampler.step(0,8192,true));f=sampler.snapshot(200);
 assert(!f.terrain->art[source].sourceHidden&&f.terrain->art[source].overlayType==0x700000);
 assert(renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true}));
 assert(renderer.nativeStats().cells==stats.cells&&renderer.nativeStats().presents==stats.presents&&assets.loads==loads&&assets.overlays==overlayLoads);
 // Resource art/frame changes are real source changes and must invalidate.
 put(Ground+0x11E,uint8_t(1));for(int n=0;n<32;++n)assert(sampler.step(0,8192,true));f=sampler.snapshot(300);
 assert(renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true}));
 assert(renderer.nativeStats().cells>stats.cells&&assets.loads==loads&&assets.overlays==overlayLoads+1);
 stats=renderer.nativeStats();
 // A bank-level shroud/actor-admission update also must retain static pages.
 auto changed=std::make_shared<radar::Terrain>(*f.terrain);changed->shade[dest]={-2,-1};changed->colors[dest]=0;f.terrain=changed;
 assert(renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true}));
 assert(renderer.nativeStats().cells==stats.cells&&renderer.nativeStats().presents==stats.presents&&assets.loads==loads);
 lookupReadable=false;assert(!renderer.render(f,240,120,LiveRead{},assets,{0,0,240,120,true}));lookupReadable=true;
 // Snapshot collection is based on SHP pixel intersection, including the
 // right half and top half which the coarse RGB flatVisible gate excludes.
 radar::Terrain t;t.geo=f.terrain->geo;t.shade.resize(512*512);t.shade[source]={-1,-1};t.geo.offsetX=8;
 assert(!t.geo.flatVisible(64,64)&&t.geo.shroudSourceVisible(64,64));radar::shroudmask::Raster mask;
 assert(mask.build(t,240,120,240,120,false,shape)&&mask.alpha[75*240+225]==127);
 t.geo.offsetX=4;t.geo.offsetY=129;assert(!t.geo.flatVisible(64,64)&&t.geo.shroudSourceVisible(64,64));
 assert(mask.build(t,240,120,240,120,false,shape)&&mask.alpha[5*240+120]==127);
 // Moving the visibility crop requires current local sources, never an old
 // mask projected with the previous camera's origin.
 radar::shroudmask::Live live;radar::shroudmask::Snapshot a,b;
 cellReads=0;assert(live.capture(LiveRead{},f.terrain->geo,0,0,120,90,a));assert(cellReads<100);
 assert(live.capture(LiveRead{},f.terrain->geo,120,0,120,90,b));assert(a.cells!=b.cells);
 // Main-view source coverage must not rasterize an entire unknown map just
 // to keep raised TMP pixels available. Exercise the actual renderer with a
 // budget far below the complete map and pan inside/outside its cached scope.
 auto large=std::make_shared<radar::Terrain>();large->geo={128,128,128,128,256,256,1,Cells,House,512*512,{128,128,0,0,128,128},256,256};
 large->colors.resize(512*512);large->art.resize(512*512);large->shade.assign(512*512,{-2,-1});size_t allSources=0;
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(large->geo.legal(x,y)){auto&art=large->art[size_t(y)*512+x];art.type=1;art.sourceColor=0x66794b;art.sourceHidden=true;++allSources;}
 radar::Frame local;local.terrain=large;local.tick=100;Assets localAssets;radar::SceneRenderer<Assets>localRenderer(512);
 auto offline=[](uint32_t,void*,size_t){return false;};
 auto draw=[&](double x,double y){std::shared_ptr<const radar::Scene>result;for(int n=0;n<16&&!result;++n){result=localRenderer.render(local,240,120,offline,localAssets,{x,y,240,120,true});local.tick+=16;}return result;};
 assert(draw(1000,1000));auto limited=localRenderer.nativeStats();assert(limited.cells<allSources/4&&limited.pages<384);
 assert(draw(1016,1000));assert(localRenderer.nativeStats().cells==limited.cells&&localAssets.loads==1);
 assert(draw(2000,1400));auto moved=localRenderer.nativeStats();assert(moved.cells>limited.cells&&moved.cells<allSources/2&&moved.pages<384&&localAssets.loads==1);
 // Real visibility reads use a capture-local fresh page epoch. A contiguous
 // native Cell pool needs far fewer remote reads than one read per cell, but
 // the next capture must still see a bit change even in the SAME logic frame.
 put(radar::Map+0xF4,128);put(radar::Map+0xF8,128);put(radar::Map+0x104,128);put(radar::Map+0x108,128);
 put(radar::Map+0x1490,128);put(radar::Map+0x1498,128);put(radar::Map+0x1240,256);put(radar::Map+0x1244,256);
 uint32_t pool=0x600000;for(int y=70;y<=95;++y)for(int x=75;x<=100;++x){put(Cells+(y*512+x)*4,pool);put(pool+0x24,int16_t(x));put(pool+0x26,int16_t(y));put(pool+0x12C,uint8_t(0x10));put(pool+0x140,uint8_t(1));pool+=0x148;}
 radar::shroudmask::Snapshot dense1,dense2;assert(live.capture(LiveRead{},large->geo,3300,500,900,450,dense1));auto io=live.stats();
 assert(io.calls<dense1.cells.size()&&io.pages<100&&io.bytes<2*1024*1024);
 uint32_t changedCell=0;assert(bytes(Cells+(80*512+86)*4,&changedCell,4));put(changedCell+0x12C,uint8_t(0));
 assert(live.capture(LiveRead{},large->geo,3300,500,900,450,dense2)&&!dense1.sameMask(dense2));
 advanceDuringMask=true;assert(!live.capture(LiveRead{},large->geo,3300,500,900,450,dense2));put(0xA8ED84,13);
 std::printf("visibility crop cells=%zu freshCalls=%zu freshPages=%zu bytes=%zu; terrain sources=%llu/%zu pages=%zu\n",dense1.cells.size(),io.calls,io.pages,io.bytes,static_cast<unsigned long long>(limited.cells),allSources,limited.pages);
 std::puts("PASS: hidden raised TMP destination masking, immediate live shroud updates without terrain rebuild, failed lookup rejection, sparse crop sampling and actual SHP boundary intersections");
}

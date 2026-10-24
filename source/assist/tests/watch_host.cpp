// Production watch scheduler with constructed game memory and an asset backend
// double. Tests selection independence, fairness and cache invalidation, not GDI.
#include "../../external_reader/source/watch_view.h"
#include "../../external_reader/source/voxel_preview.h"
#include <map>
#include <set>
#include <string>
#include <cassert>
#include <iostream>
using DWORD=uint32_t;using BYTE=uint8_t;
static DWORD clockValue=1000,pid=7,paintRevision=0;static bool enabled=true;static int captures=0,renders=0,draws=0,tileCalls=0;
static DWORD GetTickCount(){return clockValue;}static uint32_t commandMatch(){return 1;}static bool foregroundGame(){return true;}static bool formationInputOwns(){return false;}static void notify(const std::wstring&){}static void critical(const std::wstring&){}
static struct {bool watchEnabled=true,watchDanger=false,renderReveal=false;int mapStrategy=0;uint32_t watchKeys[3]={87,87,86};}extra;
static struct {int chat=0,renderStatus=1,width=800,height=600;}snap;
static std::vector<uint8_t> memory(0x1800000);
static struct {bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}}externalReader;
struct Color {template<class... T>Color(T...){}};struct SolidBrush {SolidBrush(Color){}};struct Pen {Pen(Color,float){}};struct PointF {float x,y;};struct RectF {float x,y,w,h;RectF(float a,float b,float c,float d):x(a),y(b),w(c),h(d){}};struct Bitmap {Bitmap(int,int,int,int,BYTE*){}};
struct ColorMatrix {float m[5][5]{};};static ColorMatrix drawnMatrix;static std::vector<RectF> drawnRects;static std::vector<float> drawnGains;struct ImageAttributes {void SetColorMatrix(ColorMatrix*m){drawnMatrix=*m;}};
constexpr int PixelFormat32bppPARGB=1,CombineModeIntersect=1,UnitPixel=2;
struct Graphics {int Save(){return 0;}void Restore(int){}void SetClip(RectF,int){}void TranslateTransform(float,float){}void FillRectangle(SolidBrush*,float,float,float,float){}void DrawRectangle(Pen*,float,float,float,float){}void FillPolygon(SolidBrush*,PointF*,int){}void DrawEllipse(Pen*,float,float,float,float){}void DrawImage(Bitmap*,RectF rect,float,float,float,float,int,ImageAttributes*){++draws;drawnRects.push_back(rect);drawnGains.push_back(drawnMatrix.m[0][0]);}};
namespace gameassets {
 static std::string failure;
 struct ActorArt {external::voxel::Model model;int marker=0;};
 static struct {size_t count(){return 1;}}files;
 static void update(){}static bool palette(bool,std::array<uint32_t,256>&){return false;}static std::string text(uint32_t,int){return {};}
 static bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,external::voxel::Image&image){++tileCalls;image.width=image.height=1;image.pixels={pal[3]};return true;}
 static bool sprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,external::voxel::Image&){return false;}
 static bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 static bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&art){++captures;if(id==1)return false;art.marker=id;return true;}
 static bool renderActor(const ActorArt&,const external::visual::Pose&pose,external::voxel::Image&image){++renders;image.width=image.height=1;image.pixels={uint32_t(0xff000000|pose.body<<8|pose.turret)};return true;}
}
#include "../source/watch_host.h"
int main(){
 auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 auto vec=[&](uint32_t p,uint32_t storage,const std::vector<uint32_t>&items){put(p+4,storage);put(p+8,int(items.size()));put(p+16,int(items.size()));memcpy(memory.data()+storage,items.data(),items.size()*4);};
 constexpr uint32_t cells=0x200000;put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);uint32_t cell=0x400000;
 for(int y=10;y<=50;++y)for(int x=10;x<=50;++x){put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));cell+=0x150;}
 std::vector<uint32_t> actors;for(int i=0;i<300;++i){uint32_t p=0x10000+i*0x800,t=0x800000+i*0x2000;actors.push_back(p);put(p,0x7F5C70u);put(p+0x10,i+1);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x6C4,t);put(p+0x21C,0x180000u);put(p+0x9C,(30+i%20)*256+128);put(p+0xA0,(30+i/20)*256+128);put(p+0x83,uint8_t(i==0));put(t+0xB0,1u);put(t+0xCA1,uint8_t(1));put(p+0x388,uint16_t(i*1024));put(p+0x3A0,uint16_t(16384));}
 vec(0xA8EC78,0x190000,actors);vec(0xA8ED28,0x1A0000,{0xF00000});watchhost::process=pid;watchhost::epoch=1;watchhost::tracks[0].target={actors[0],1};
 for(int i=0;i<24;++i){clockValue+=50;watchhost::update();}
 assert(watchhost::views[0].frame.actors.size()==300);assert(watchhost::actorArt.size()==300);
 for(const auto&a:watchhost::views[0].frame.actors){auto it=watchhost::images.find(watchhost::imageKey(a));assert(it!=watchhost::images.end());assert(it->second.ready==(a.target.id!=1));}
 Graphics g;watchhost::drawView(g,watchhost::views[0]);assert(draws==299); // no schematic marker for failed asset
 int before=captures,oldRenders=renders;put(actors[1]+0x388,uint16_t(32768));put(actors[1]+0x3A0,uint16_t(49152));clockValue+=50;watchhost::update();assert(captures==before&&renders>oldRenders);
 auto a=std::find_if(watchhost::views[0].frame.actors.begin(),watchhost::views[0].frame.actors.end(),[](auto&a){return a.target.id==2;});assert(a!=watchhost::views[0].frame.actors.end());assert(watchhost::images.at(watchhost::imageKey(*a)).image.pixels[0]==0xff001018);
 // All 299 valid models turn together: the 16-item work budget must retain
 // the previous real image for actors whose new pose is still waiting.
 for(auto p:actors)put(p+0x388,uint16_t(2048));clockValue+=50;watchhost::update();draws=0;watchhost::drawView(g,watchhost::views[0]);assert(draws==299);assert(watchhost::images.size()<=600);
 assert(tileCalls==0&&watchhost::tiles.empty()); // no transparent success cache before palette arrives
 put(0xABBED0+9,uint8_t(32));clockValue+=50;watchhost::update();assert(tileCalls==1&&watchhost::tiles.begin()->second.ready&&watchhost::tiles.begin()->second.image.pixels[0]!=0);
 // A live lighting change affects every cached actor without any recapture.
 put(0xA8B230,0xC00000u);put(0xC0352C,80);for(int off:{0x3534,0x3538,0x353C})put(0xC00000+off,100);
 before=captures;oldRenders=renders;clockValue+=50;watchhost::update();watchhost::drawView(g,watchhost::views[0]);assert(captures==before&&drawnMatrix.m[0][0]==.8f&&drawnMatrix.m[3][3]==1.f);
 // Three windows share every model/pose cache, with independent centered boxes.
 before=captures;watchhost::tracks[1].target={actors[1],2};watchhost::tracks[2].target={actors[2],3};clockValue+=50;watchhost::update();
 assert(captures==before&&watchhost::actorArt.size()==300);for(auto&v:watchhost::views)assert(v.shown&&v.frame.actors.size()==300);
 assert(watchhost::views[0].box.y<watchhost::views[1].box.y&&watchhost::views[1].box.y<watchhost::views[2].box.y);
 watchhost::close(1);assert(!watchhost::tracks[1]&&!watchhost::views[1].shown&&watchhost::views[0].shown&&watchhost::views[2].shown);
 assert(watchhost::views[0].box.w==320&&watchhost::views[2].box.y-watchhost::views[0].box.y==264);
 watchhost::close(2);
 // Elevated overlay and an actor on that tile share the native height basis.
 put(0xB0CD48,.144337567);clockValue+=50;watchhost::update();assert(watchhost::views[0].frame.zScale==.144337567);
 auto actor=watchhost::views[0].frame.actors[1];auto&f=watchhost::views[0].frame;f.radius=1;f.side=3;f.cx=actor.x/256;f.cy=actor.y/256;actor.x=f.cx*256+128;actor.y=f.cy*256+128;actor.z=208;actor.light.intensity=1200;f.actors={actor};f.cells.assign(9,0);f.cells[4]=0xff112233;f.tiles.assign(9,{});f.tiles[4].level=2;f.tiles[4].overlayType=0xdead;f.tiles[4].overlayY=-31;f.tiles[4].overlayLight.intensity=1500;
 auto&overlay=watchhost::overlays[{0xdead,0}];overlay.ready=true;overlay.image.width=overlay.image.height=1;overlay.image.pixels={0xffabcdef};
 drawnRects.clear();drawnGains.clear();watchhost::drawView(g,watchhost::views[0]);assert(drawnRects.size()==2);assert(drawnRects[0].y==watchhost::Height/2.f-15.5f&&drawnGains[0]==1.5f);assert(drawnRects[1].y==watchhost::Height/2.f&&drawnGains[1]==1.2f);
 auto read=[](uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);};put(0x900140,0x80u);put(0x9102A8,uint8_t(1));assert(external::watch::overlayOffset(read,0x900000,0x910000,1,9)==-43);put(0x900140,0u);assert(external::watch::overlayOffset(read,0x900000,0x910000,239,0)==-27);
 extra.watchEnabled=false;clockValue+=50;watchhost::update();assert(!watchhost::shown);watchhost::reset();assert(watchhost::images.empty()&&watchhost::actorArt.empty());
 std::cout<<"PASS 300 unique types regardless of selection, failed-focus fairness, actual-image-only drawing, body/turret cache invalidation without asset reload, reset\n";
}

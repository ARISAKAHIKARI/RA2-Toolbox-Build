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
static struct {bool watchEnabled=true,watchDanger=false,renderReveal=false;int mapStrategy=0;}extra;
static struct {int chat=0,renderStatus=1,width=800,height=600;}snap;
static std::vector<uint8_t> memory(0x1800000);
static struct {bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}}externalReader;
struct Color {template<class... T>Color(T...){}};struct SolidBrush {SolidBrush(Color){}};struct Pen {Pen(Color,float){}};struct PointF {float x,y;};struct RectF {RectF(float,float,float,float){}};struct Bitmap {Bitmap(int,int,int,int,BYTE*){}};
constexpr int PixelFormat32bppPARGB=1,CombineModeIntersect=1;
struct Graphics {int Save(){return 0;}void Restore(int){}void SetClip(RectF,int){}void TranslateTransform(float,float){}void FillRectangle(SolidBrush*,float,float,float,float){}void DrawRectangle(Pen*,float,float,float,float){}void FillPolygon(SolidBrush*,PointF*,int){}void DrawEllipse(Pen*,float,float,float,float){}void DrawImage(Bitmap*,RectF){++draws;}};
namespace gameassets {
 static std::string failure;
 struct ActorArt {external::voxel::Model model;int marker=0;};
 static struct {size_t count(){return 1;}}files;
 static void update(){}static bool palette(bool,std::array<uint32_t,256>&){return false;}static std::string text(uint32_t,int){return {};}
 static bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,external::voxel::Image&image){++tileCalls;image.width=image.height=1;image.pixels={pal[3]};return true;}
 static bool sprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,external::voxel::Image&){return false;}
 static bool overlay(uint32_t,uint32_t,int,external::voxel::Image&){return false;}
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
 vec(0xA8EC78,0x190000,actors);vec(0xA8ED28,0x1A0000,{0xF00000});watchhost::process=pid;watchhost::epoch=1;watchhost::pinned={actors[0],1};
 for(int i=0;i<24;++i){clockValue+=50;watchhost::update();}
 assert(watchhost::frame.actors.size()==300);assert(watchhost::actorArt.size()==300);
 for(const auto&a:watchhost::frame.actors){auto it=watchhost::images.find(watchhost::imageKey(a));assert(it!=watchhost::images.end());assert(it->second.ready==(a.target.id!=1));}
 Graphics g;watchhost::draw(g);assert(draws==299); // no schematic marker for failed asset
 int before=captures,oldRenders=renders;put(actors[1]+0x388,uint16_t(32768));put(actors[1]+0x3A0,uint16_t(49152));clockValue+=50;watchhost::update();assert(captures==before&&renders>oldRenders);
 auto a=std::find_if(watchhost::frame.actors.begin(),watchhost::frame.actors.end(),[](auto&a){return a.target.id==2;});assert(a!=watchhost::frame.actors.end());assert(watchhost::images.at(watchhost::imageKey(*a)).image.pixels[0]==0xff001018);
 // All 299 valid models turn together: the 16-item work budget must retain
 // the previous real image for actors whose new pose is still waiting.
 for(auto p:actors)put(p+0x388,uint16_t(2048));clockValue+=50;watchhost::update();draws=0;watchhost::draw(g);assert(draws==299);assert(watchhost::images.size()<=600);
 assert(tileCalls==0&&watchhost::tiles.empty()); // no transparent success cache before palette arrives
 put(0xABBED0+9,uint8_t(32));clockValue+=50;watchhost::update();assert(tileCalls==1&&watchhost::tiles.begin()->second.ready&&watchhost::tiles.begin()->second.image.pixels[0]!=0);
 extra.watchEnabled=false;clockValue+=50;watchhost::update();assert(!watchhost::shown);watchhost::reset();assert(watchhost::images.empty()&&watchhost::actorArt.empty());
 std::cout<<"PASS 300 unique types regardless of selection, failed-focus fairness, actual-image-only drawing, body/turret cache invalidation without asset reload, reset\n";
}

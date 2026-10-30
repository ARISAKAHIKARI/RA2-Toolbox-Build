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
static DWORD clockValue=1000,pid=7,paintRevision=0;static bool enabled=true;static int captures=0,renders=0,draws=0,tileCalls=0,shroudLoads=0;
static DWORD GetTickCount(){return clockValue;}static uint32_t commandEpoch=1,displayEpoch=1;static bool secondAvailable=false;static uint32_t commandMatch(){return commandEpoch;}static uint32_t displayMatch(){return displayEpoch;}static bool secondaryDisplayAvailable(){return secondAvailable;}static bool foregroundGame(){return true;}static bool formationInputOwns(){return false;}static void notify(const std::wstring&){}static void critical(const std::wstring&){}
static struct {bool watchEnabled=true,watchSecondary=false,watchDanger=false,renderReveal=false;int mapStrategy=0;uint32_t watchKeys[3]={87,87,86};}extra;
static struct {int chat=0,renderStatus=1,width=800,height=600;}snap;
static std::vector<uint8_t> memory(0x1800000);
static uint32_t blockedAddress=0,staleLifeAddress=0,occupantDiscoveryReads=0;
static struct {bool readFresh(uint32_t p,void*out,size_t n){if(p==0xA8EB44&&n==4)++occupantDiscoveryReads;if(p==blockedAddress||uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}bool read(uint32_t p,void*out,size_t n){if(!readFresh(p,out,n))return false;if(p==staleLifeAddress)memset(out,0,n);return true;}}externalReader;
struct Color {template<class... T>Color(T...){}};struct SolidBrush {SolidBrush(Color){}};struct Pen {Pen(Color,float){}};struct PointF {float x,y;};struct RectF {float x,y,w,h;RectF(float a,float b,float c,float d):x(a),y(b),w(c),h(d){}};struct Bitmap {int width,height;Bitmap(int w,int h,int,int,BYTE*):width(w),height(h){}};
struct ColorMatrix {float m[5][5]{};};static int presentedInterpolation=-1,presentedPixelOffset=-1;static std::vector<std::pair<int,int>> drawnSources;static ColorMatrix drawnMatrix;static std::vector<RectF> drawnRects,drawnEllipses;static std::vector<float> drawnGains;static std::vector<RectF> excluded;struct ImageAttributes {void SetColorMatrix(ColorMatrix*m){drawnMatrix=*m;}};
constexpr int PixelFormat32bppPARGB=1,CombineModeIntersect=1,CombineModeExclude=3,UnitPixel=2,InterpolationModeNearestNeighbor=5,PixelOffsetModeHalf=4;
struct Graphics {int Save(){return 0;}void Restore(int){}void SetClip(RectF r,int mode){if(mode==CombineModeExclude)excluded.push_back(r);}void TranslateTransform(float,float){}void SetInterpolationMode(int value){presentedInterpolation=value;}void SetPixelOffsetMode(int value){presentedPixelOffset=value;}void FillRectangle(SolidBrush*,float,float,float,float){}void DrawRectangle(Pen*,float,float,float,float){}void FillPolygon(SolidBrush*,PointF*,int){}void DrawEllipse(Pen*,float x,float y,float w,float h){drawnEllipses.emplace_back(x,y,w,h);}void DrawImage(Bitmap*bitmap,RectF rect,float,float,float,float,int,ImageAttributes*){++draws;drawnSources.emplace_back(bitmap->width,bitmap->height);drawnRects.push_back(rect);drawnGains.push_back(drawnMatrix.m[0][0]);}};
namespace gameassets {
 static bool partial=false;
 static uint64_t assetGeneration=1;static uint32_t forcedColor=0;
 static uint64_t revision(){return assetGeneration;}
 static std::wstring lightingStatus(){return L"test";}
 static std::string failure;
 struct ActorArt {external::voxel::Model model;int marker=0;};
 static struct {size_t count(){return 1;}}files;
 static void update(){}static bool palette(bool,std::array<uint32_t,256>&){return false;}static std::string text(uint32_t,int){return {};}
 static bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,external::voxel::Image&image){++tileCalls;image.width=image.height=1;image.pixels={pal[3]};return true;}
 static bool sprite(uint32_t,uint32_t,const std::array<uint32_t,256>&,int,external::voxel::Image&){return false;}
 static int buildingFrame(uint32_t,uint32_t){return 0;}
 static bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 static bool captureAnimation(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,ActorArt&){return false;}
 static bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&art){++captures;if(id==1)return false;art.marker=id;return true;}
 static size_t artBytes(const ActorArt&a){return a.model.faces.size()*sizeof(external::voxel::Face);}
 static bool renderActor(const ActorArt&,const external::visual::Pose&pose,external::voxel::Image&image){++renders;image.width=image.height=2;image.incomplete=partial;uint32_t color=pose.buildingFlags||pose.doorFrame>=0?uint32_t(0xff000000|pose.buildingFlags<<8|(pose.doorFrame+1)):uint32_t(0xff000000|pose.body<<8|pose.turret);image.pixels.assign(4,forcedColor?forcedColor:color);return true;}
}
#include "../source/watch_engine.h"
struct Backend {
 using ActorArt=gameassets::ActorArt;std::string&failure=gameassets::failure;
 void identity(uint32_t,uint32_t){}uint64_t revision(){return gameassets::revision();}
 bool palette(bool b,std::array<uint32_t,256>&p){return gameassets::palette(b,p);}
 bool tile(uint32_t t,unsigned s,const std::array<uint32_t,256>&p,external::voxel::Image&i){return gameassets::tile(t,s,p,i);}
 bool overlay(uint32_t t,uint32_t p,int f,external::voxel::Image&i,bool b=false){return gameassets::overlay(t,p,f,i,b);}
 bool shroud(bool,int f,external::voxel::Image&i){++shroudLoads;i.canvasWidth=60;i.canvasHeight=30;i.width=60;i.height=30;i.x=-30;i.y=-15;i.indices.assign(1800,f==15?2:127);return true;}
 bool captureActor(uint32_t p,uint32_t t,uint32_t id,ActorArt&a){return gameassets::captureActor(p,t,id,a);}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t b,uint32_t bid,ActorArt&a){return gameassets::captureAnimation(p,t,id,b,bid,a);}
 bool renderActor(const ActorArt&a,const external::visual::Pose&p,external::voxel::Image&i){return gameassets::renderActor(a,p,i);}
 int buildingFrame(uint32_t p,uint32_t t){return gameassets::buildingFrame(p,t);}
 void buildingPose(uint32_t p,uint32_t t,external::visual::Pose&pose){pose.frame=buildingFrame(p,t);}
 static size_t cost(const ActorArt&a){return gameassets::artBytes(a);}
} backend;
static watchengine::Renderer<decltype(externalReader),Backend> engine(externalReader,backend);
int main(){
 auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 auto vec=[&](uint32_t p,uint32_t storage,const std::vector<uint32_t>&items){put(p+4,storage);put(p+8,int(items.size()));put(p+16,int(items.size()));memcpy(memory.data()+storage,items.data(),items.size()*4);};
 constexpr uint32_t cells=0x200000;put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);uint32_t cell=0x400000;
 put(external::radar::Map+0xF4,32);put(external::radar::Map+0xF8,64);put(external::radar::Map+0x104,32);put(external::radar::Map+0x108,64);
 for(int y=10;y<=50;++y)for(int x=10;x<=50;++x){put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));put(cell+0x12C,uint8_t(0x10));put(cell+0x140,uint8_t(1));cell+=0x150;}
 std::vector<uint32_t> actors;for(int i=0;i<300;++i){uint32_t p=0x10000+i*0x800,t=0x800000+i*0x2000;actors.push_back(p);put(p,0x7F5C70u);put(p+0x10,i+1);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x6C4,t);put(p+0x21C,0x180000u);put(p+0x9C,(30+i%20)*256+128);put(p+0xA0,(30+i/20)*256+128);put(p+0x83,uint8_t(i==0));put(t+0xB0,1u);put(t+0xCA1,uint8_t(1));put(p+0x388,uint16_t(i*1024));put(p+0x3A0,uint16_t(16384));}
 vec(0xA8EC78,0x190000,actors);vec(0xA8ED28,0x1A0000,{0xF00000});engine.process=pid;engine.epoch=1;engine.tracks[0].target={actors[0],1};
 for(int i=0;i<24;++i){clockValue+=50;engine.update(clockValue);}
 assert(engine.views[0].frame.actors.size()==300);assert(engine.actorArt.size()==300);
 for(const auto&a:engine.views[0].frame.actors){auto it=engine.images.find(engine.imageKey(a));assert(it!=engine.images.end());assert(it->second.ready==(a.target.id!=1));}
 Graphics g;engine.drawView(g,engine.views[0]);assert(draws==1&&shroudLoads>0&&engine.views[0].raster.color.size()==640*512); // no schematic marker for failed asset
 auto&firstMask=engine.views[0].shroudRaster;assert(firstMask.builds()==1&&firstMask.hits()==0);
 auto&changedImage=engine.images.at(engine.imageKey(engine.views[0].frame.actors[1])).image;
 uint32_t previousPixel=changedImage.pixels[0];changedImage.pixels[0]^=0x00010101u;
 engine.drawView(g,engine.views[0]);assert(firstMask.builds()==1&&firstMask.hits()==1);
 changedImage.pixels[0]=previousPixel; // Same mask is applied to recomposed actor art.
 int before=captures,oldRenders=renders;put(actors[1]+0x388,uint16_t(32768));put(actors[1]+0x3A0,uint16_t(49152));clockValue+=50;engine.update(clockValue);assert(captures==before&&renders>oldRenders);
 auto a=std::find_if(engine.views[0].frame.actors.begin(),engine.views[0].frame.actors.end(),[](auto&a){return a.target.id==2;});assert(a!=engine.views[0].frame.actors.end());assert(engine.images.at(engine.imageKey(*a)).image.pixels[0]==0xff001018);
 // All 299 valid models turn together: the time budget must retain the
 // previous real image for actors whose new pose is still waiting.
 for(auto p:actors)put(p+0x388,uint16_t(2048));clockValue+=50;engine.update(clockValue);draws=0;engine.drawView(g,engine.views[0]);assert(draws==1&&engine.views[0].raster.color.size()==640*512);assert(engine.images.size()<=600);
 assert(tileCalls==0&&engine.tiles.empty()); // no transparent success cache before palette arrives
 put(0xABBED0+9,uint8_t(32));clockValue+=50;engine.update(clockValue);assert(tileCalls==1&&engine.tiles.begin()->second.ready&&engine.tiles.begin()->second.image.pixels[0]!=0);
 // A live lighting change affects every cached actor without any recapture.
 put(0xA8B230,0xC00000u);put(0xC0352C,80);for(int off:{0x3534,0x3538,0x353C})put(0xC00000+off,100);
 before=captures;oldRenders=renders;clockValue+=50;engine.update(clockValue);engine.drawView(g,engine.views[0]);assert(captures==before&&engine.views[0].frame.light.ambient==80);
 // Three windows share every model/pose cache, with independent centered boxes.
 before=captures;engine.tracks[1].target={actors[1],2};engine.tracks[2].target={actors[2],3};clockValue+=50;engine.update(clockValue);
 assert(captures==before&&engine.actorArt.size()==300);for(auto&v:engine.views)assert(v.shown&&v.frame.actors.size()==300);
 for(auto&v:engine.views)engine.drawView(g,v);
 assert(engine.views[1].shroudRaster.builds()==1&&engine.views[2].shroudRaster.builds()==1);
 auto firstViewBuilds=engine.views[0].shroudRaster.builds(),firstViewHits=engine.views[0].shroudRaster.hits();
 engine.drawView(g,engine.views[0]);assert(engine.views[0].shroudRaster.builds()==firstViewBuilds&&engine.views[0].shroudRaster.hits()==firstViewHits+1);
 assert(engine.views[0].box.y<engine.views[1].box.y&&engine.views[1].box.y<engine.views[2].box.y);
 // Alternate a temporarily unreadable third focus at the overlay cadence.
 // Its last complete scene and all three boxes must survive every failed read.
 std::array<external::watch::Track,3> requestedTracks;std::copy(std::begin(engine.tracks),std::end(engine.tracks),requestedTracks.begin());engine.syncTracks(42,requestedTracks,true);
 external::watch::Box stableBoxes[3];for(int i=0;i<3;++i)stableBoxes[i]=engine.views[i].box;
 for(int n=0;n<120;++n){
  blockedAddress=n%2?actors[2]+0x74:0;put(0xA8ED84,100+n);auto priorFocus=engine.views[2].frame.focus;auto priorTick=engine.views[2].tick;
  engine.update(clockValue+=2);for(int i=0;i<3;++i){auto b=engine.views[i].box,s=stableBoxes[i];assert(engine.tracks[i]&&engine.views[i].shown&&b.x==s.x&&b.y==s.y&&b.w==s.w&&b.h==s.h);}
  assert(engine.views[2].frame.focus==priorFocus);if(blockedAddress)assert(engine.views[2].tick==priorTick);else assert(engine.views[2].tick==clockValue);
 }
 blockedAddress=external::radar::Map+0x13C;put(0xA8ED84,500);engine.update(clockValue+=2);for(auto&v:engine.views)assert(v.shown&&v.frame.focus);blockedAddress=0;
 // A transport/cloak transition retains occupancy, while actual death removes
 // the slot exactly once. Older queued UI snapshots cannot revive that slot.
 put(actors[2]+0x11C,0x170000u);engine.update(clockValue+=2);assert(engine.views[2].shown&&engine.tracks[2]);put(actors[2]+0x11C,0u);
 put(actors[2]+0x6C,0);engine.update(clockValue+=2);assert(!engine.tracks[2]&&!engine.views[2].shown&&engine.views[0].box.w==320);
 put(actors[2]+0x6C,100);for(int n=0;n<4;++n){engine.syncTracks(42,requestedTracks,true);engine.update(clockValue+=2);assert(!engine.tracks[2]&&!engine.views[2].shown);}
 engine.syncTracks(43,requestedTracks,true);engine.update(clockValue+=2);assert(engine.tracks[2]&&engine.views[2].shown);
 engine.tracks[1]={};engine.views[1]={};engine.arrange();assert(!engine.tracks[1]&&!engine.views[1].shown&&engine.views[0].shown&&engine.views[2].shown);
 assert(engine.views[0].box.w==320&&engine.views[2].box.y-engine.views[0].box.y==264);
 engine.tracks[2]={};engine.views[2]={};engine.arrange();
 // Every overlay tick refreshes coordinates, even inside the same simulation
 // frame; existing art is reused and no 50 ms limiter suppresses a frame.
 uint32_t priorPaint=paintRevision;for(int i=0;i<200;++i){clockValue+=2;if(i%8==0)put(0xA8ED84,i/8+1);put(actors[1]+0x9C,31*256+128+i*2);engine.update(clockValue);assert(engine.views[0].tick==clockValue);}
 assert(paintRevision==priorPaint); // camera animation does not reraster the unrelated full-screen overlay
 // Elevated overlay and an actor on that tile share the native height basis.
 put(0xB0CD48,.144337567);clockValue+=50;engine.update(clockValue);assert(engine.views[0].frame.zScale==.144337567);
 auto actor=engine.views[0].frame.actors[1];auto&f=engine.views[0].frame;f.radius=1;f.side=3;f.cx=actor.x/256;f.cy=actor.y/256;actor.x=f.cx*256+128;actor.y=f.cy*256+128;actor.z=208;actor.light.intensity=1200;f.actors={actor};engine.views[0].positions.clear();f.cells.assign(9,0);f.cells[4]=0xff112233;f.tiles.assign(9,{});f.shade.assign(9,{});f.shade[4]={-1,-1};f.tiles[4].level=2;f.tiles[4].overlayType=0xdead;f.tiles[4].overlayY=-31;f.tiles[4].overlayLight.intensity=1500;
 auto&overlay=engine.overlays[{0xdead,0}];overlay.ready=true;overlay.image.width=overlay.image.height=1;overlay.image.pixels={0xffabcdef};
 // One composed canvas keeps native-height positioning and pixel depth.
 f.focus=actor.target;f.center.target=actor.target;overlay.image.width=overlay.image.height=8;overlay.image.pixels.assign(64,0xffabcdef);
 auto&actorImage=engine.images[engine.imageKey(actor)];actorImage.ready=true;actorImage.image=overlay.image;actorImage.image.pixels.assign(64,0xff204080);
 f.reveal=true; // The following independent Z tests use hand-built one-cell art.
 drawnRects.clear();engine.drawView(g,engine.views[0]);assert(drawnRects.size()==1&&drawnRects[0].w==640);
 auto&canvas=engine.views[0].raster;assert(canvas.color[228*640+322]==external::camera::lit(0xffabcdef,0,false,f.tiles[4].overlayLight));
 assert(canvas.color[258*640+322]==external::camera::lit(0xff204080,0,false,actor.light));
 // Two overlapping standing shapes contain nearer and farther pixels within
 // the same row. Whole-image ordering cannot produce this split result.
 f.tiles[4].overlayType=0;actor.light={};f.actors={actor};auto second=actor;second.target={0xeeee,999};second.type=0xaaaa;second.kind=4;second.light={};f.actors.push_back(second);
 auto&firstImage=engine.images[engine.imageKey(actor)];firstImage.ready=true;firstImage.image={};firstImage.image.width=4;firstImage.image.height=2;firstImage.image.pixels.assign(8,0xffe04020);firstImage.image.depth={5,5,1,1,5,5,1,1};
 auto&secondImage=engine.images[engine.imageKey(second)];secondImage.ready=true;secondImage.image=firstImage.image;secondImage.image.pixels.assign(8,0xff20b0e0);secondImage.image.depth.assign(8,3);
 engine.drawView(g,engine.views[0]);assert(canvas.color[256*640+321]==0xffe04020&&canvas.color[256*640+323]==0xff20b0e0);
 auto read=[](uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);};put(0x900140,0x80u);put(0x9102A8,uint8_t(1));assert(external::watch::overlayOffset(read,0x900000,0x910000,1,9)==-43);put(0x900140,0u);assert(external::watch::overlayOffset(read,0x900000,0x910000,239,0)==-27);
 // Same type/owner and body frame may have independent factory door/roof
 // states. Share source bytes while retaining separate rendered poses.
 external::watch::Frame factories;factories.tiles.clear();external::watch::Actor fa;fa.kind=3;fa.type=0xB00000;fa.owner=0x180000;fa.target={0x160000,2001};fa.pose.buildingFlags=3;fa.pose.doorFrame=2;auto fb=fa;fb.target={0x161000,2002};fb.pose.doorFrame=6;factories.actors={fa,fb};
 before=captures;oldRenders=renders;engine.refreshAssets(factories,clockValue+=50);assert(captures==before+1&&renders==oldRenders+2);assert(engine.imageKey(fa)!=engine.imageKey(fb));assert(engine.images.at(engine.imageKey(fa)).image.pixels!=engine.images.at(engine.imageKey(fb)).image.pixels);
 before=captures;oldRenders=renders;engine.refreshAssets(factories,clockValue+=50);assert(captures==before&&renders==oldRenders);
 factories.actors[0].pose.buildingFlags=7;engine.refreshAssets(factories,clockValue+=50);assert(captures==before&&renders==oldRenders+1);
 factories.actors[0].pose.variant=-2;engine.refreshAssets(factories,clockValue+=50);assert(captures==before+1&&std::get<6>(engine.displayed.at({0x160000,2001}))==-2);
 factories.actors[0].pose.variant=-1;engine.refreshAssets(factories,clockValue+=50);assert(captures==before+1&&std::get<6>(engine.displayed.at({0x160000,2001}))==-1);
 factories.actors[0].pose.doorFrame=7;gameassets::partial=true;engine.refreshAssets(factories,clockValue+=50);auto incomplete=engine.imageKey(factories.actors[0]);assert(engine.images.at(incomplete).ready&&engine.images.at(incomplete).image.incomplete&&engine.displayed.at({0x160000,2001})==incomplete);
 oldRenders=renders;before=captures;gameassets::partial=false;engine.refreshAssets(factories,clockValue+=50);assert(renders==oldRenders);engine.refreshAssets(factories,clockValue+=2000);assert(renders==oldRenders+1&&captures==before&&!engine.images.at(incomplete).image.incomplete);
 // A unit at the top playable edge remains above the clamped camera center.
 // Render the legal scenery beyond the usable boundary through the actual
 // sampler and canvas: the first rows must not become an artificial black band.
 put(external::radar::Map+0xF4,131);put(external::radar::Map+0xF8,146);put(external::radar::Map+0xFC,3);put(external::radar::Map+0x100,5);put(external::radar::Map+0x104,124);put(external::radar::Map+0x108,135);
 put(actors[0]+0x9C,69*256+128);put(actors[0]+0xA0,73*256+128);put(actors[0]+0xA4,0);vec(0xA8EC78,0x190000,{actors[0]});
 cell=0xD00000;for(int y=45;y<=105;++y)for(int x=40;x<=100;++x){put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x38,-1);put(cell+0x44,-1);put(cell+0xEC,2);put(cell+0x10C,uint16_t(1000));put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));cell+=0x150;}
 watchengine::Renderer<decltype(externalReader),Backend>::View edge;edge.shown=true;edge.frame=external::watch::sample(read,{actors[0],1},true,20);
 assert(edge.frame.focus&&edge.frame.cameraY==2296&&edge.frame.bounds.usable(69,73));
 drawnEllipses.clear();engine.drawView(g,edge);assert(drawnEllipses.size()==1);
 assert(drawnEllipses[0].x+20==320&&drawnEllipses[0].y+10==105.f);
 for(int y=0;y<80;++y)for(int x=64;x<576;++x)assert(edge.raster.color[y*640+x]!=0xff080c08u&&"legal border scenery must fill the viewport above the playable edge");
 // VPL content changes the asset revision without changing actor or pose.
 // Evict both retained headings and their source art, then stay cached again.
 {
  engine.reset();external::watch::Frame recovery;recovery.tiles.clear();external::watch::Actor cachedActor;
  cachedActor.kind=2;cachedActor.type=0x20000;cachedActor.owner=0x180000;cachedActor.target={0x10000,2};recovery.actors={cachedActor};
  gameassets::forcedColor=0xff102030;engine.refreshAssets(recovery,clockValue);auto original=engine.imageKey(recovery.actors[0]);
  recovery.actors[0].pose.body=1;engine.refreshAssets(recovery,clockValue+=10);auto other=engine.imageKey(recovery.actors[0]);
  assert(engine.images.size()==2&&engine.actorArt.size()==1);
  recovery.actors[0].pose.body=0;engine.refreshAssets(recovery,clockValue+=10);int oldCaptures=captures,oldRendered=renders;
  gameassets::forcedColor=0xffa0b0c0;engine.refreshAssets(recovery,clockValue+=10);
  assert(captures==oldCaptures&&renders==oldRendered&&engine.images.at(original).image.pixels[0]==0xff102030);
  ++gameassets::assetGeneration;engine.refreshAssets(recovery,clockValue+=10);
  assert(captures==oldCaptures+1&&renders==oldRendered+1&&engine.images.size()==1&&!engine.images.count(other));
  assert(engine.actorArt.size()==1&&engine.displayed.size()==1&&engine.displayed.at({0x10000,2})==original);
  assert(engine.images.at(original).image.pixels[0]==0xffa0b0c0);oldCaptures=captures;oldRendered=renders;
  for(int n=0;n<20;++n)engine.refreshAssets(recovery,clockValue);
  assert(captures==oldCaptures&&renders==oldRendered);gameassets::forcedColor=0;
 }
 engine.enabled=false;clockValue+=50;engine.update(clockValue);assert(!engine.shown);engine.reset();assert(engine.images.empty()&&engine.actorArt.empty());
 std::cout<<"PASS 300 unique types, retained poses, independent door/roof frames, cache transitions, clamped off-center focus with drawable border scenery and reset\n";
 return 0;
}


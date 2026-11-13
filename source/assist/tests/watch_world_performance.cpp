#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#include "../../external_reader/source/camera_assets.h"
#include "../source/watch_scene.h"
#include "watch_world_reference.h"
#include <cassert>
#include <cstdio>
using namespace external;
static std::vector<uint8_t> memory(0x2000000);
static uint64_t reads=0;static uint32_t failAt=0;static bool flipChrono=false;static unsigned chronoReads=0;
template<class T>static void put(uint32_t p,const T&v){assert(size_t(p)+sizeof v<=memory.size());std::memcpy(memory.data()+p,&v,sizeof v);}
struct Read {
 bool operator()(uint32_t at,void*out,size_t n)const{++reads;if(at<0x10000||uint64_t(at)+n>memory.size()||(failAt>=at&&failAt<uint64_t(at)+n))return false;if(at==0xA9FAB0&&flipChrono&&++chronoReads==2)put(0xA9FAB0,1);std::memcpy(out,memory.data()+at,n);return true;}
 bool read(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}
 bool readFresh(uint32_t at,void*out,size_t n){return (*this)(at,out,n);}
 bool imagePath(wchar_t*,DWORD*){return false;}
};
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static std::shared_ptr<const voxel::Image> testSprite(uint32_t color,int w=12,int h=16){
 auto out=std::make_shared<voxel::Image>();out->width=w;out->height=h;out->x=-w/2;out->y=-h;out->pixels.assign(size_t(w)*h,color);out->indices.assign(size_t(w)*h,1);out->nativeDepth.enabled=true;out->nativeDepth.gradient=nativesurface::Gradient::Flat;return out;
}
static std::shared_ptr<radar::Terrain> makeTerrain(){
 auto out=std::make_shared<radar::Terrain>();auto&g=out->geo;g.width=g.height=g.offsetX=g.offsetY=64;g.rw=g.rh=g.rawWidth=g.rawHeight=128;g.factor=1;g.cells=0x100000;g.house=0x400000;g.count=512*512;g.bounds={64,64,0,0,64,64};assert(g.valid());out->reveal=true;out->colors.resize(512*512);out->baseColors.resize(512*512);out->art.resize(512*512);
 unsigned count=0;for(int y=1;y<512;++y)for(int x=1;x<512;++x){mapbounds::TileOrigin at;if(!g.tileOrigin(x,y,0,at))continue;auto i=y*512+x;auto&c=out->art[i];c.cell=0x1000000+count++*0x200;c.type=0x250000;out->colors[i]=out->baseColors[i]=0x193347;put(c.cell+0x104,uint32_t(65536));put(c.cell+0x10A,std::array<int16_t,3>{{1000,1000,1000}});put(g.cells+i*4,c.cell);put(c.cell+0x24,std::array<int16_t,2>{{int16_t(x),int16_t(y)}});}
 put(0xA83D4C,g.house);put(radar::Map+0x13C,g.cells);put(radar::Map+0x140,g.count);put(radar::Map+0x1240,g.rawWidth);put(radar::Map+0x1244,g.rawHeight);put(radar::Map+0xF4,std::array<int,6>{{64,64,0,0,64,64}});put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(0xA8B230,uint32_t(0x600000));put(0x60352C,100);put(0x603534,std::array<int,3>{{100,100,100}});put(0xB0CD48,.5);return out;
}
static radar::Frame frame(const std::shared_ptr<radar::Terrain>&t,int tick,size_t count){
 radar::Frame out;out.terrain=t;out.frame=tick;out.tick=tick;out.markerScanValid=out.posesPrepared=true;out.observedMs=double(tick);out.markers.resize(count);
 for(size_t i=0;i<count;++i){auto&m=out.markers[i];m.object=0xC00000+uint32_t(i)*0x300;m.id=uint32_t(i)+1;m.type=0x260000;m.owner=t->geo.house;m.x=(48+int(i%32))*256+(tick%7)*11;m.y=(48+int(i/32)%32)*256+(tick%5)*9;m.kind=2;m.color=0xff445566;m.displayOrder=int(count-i);m.pose.frame=tick%3;
  if(i%97==0){m.kind=3;m.building=true;m.tetherRedraw.push_back({m.object+0x300,m.id+1});}else if(i%97==1)m.tetherHidden=true;
  put(m.object,uint32_t(m.kind==3?0x7E3EBC:0x7F5C70));put(m.object+0x10,m.id);put(m.object+0x6C,100);put(m.object+0x74,uint8_t(1));put(m.object+0x90,uint8_t(1));put(m.object+0x94,m.layer);put(m.object+0x9C,std::array<int,3>{{m.x,m.y,m.z}});put(m.object+0x21C,m.owner);put(m.object+(m.kind==3?0x520:0x6C4),m.type);
 }put(0xA8ED84,tick);return out;
}
static std::shared_ptr<worldscene::Snapshot> world(const radar::Frame&f,const camera::NativeCanvas&ground){
 auto out=std::make_shared<worldscene::Snapshot>();out->ground=ground.fork();out->state=f;out->profile.motion=false;out->profile.gpu=false;out->terrainRevision=1;
 static auto shapes=std::array<std::shared_ptr<const voxel::Image>,3>{{testSprite(0xff8040e0),testSprite(0xff4080e0),testSprite(0xffe08040)}};
 camera::ObjectCanvas::Draw prefix;prefix.image=shapes[0];prefix.x=1940;prefix.y=1000;out->instances.push_back(prefix);
 for(const auto&m:f.markers){camera::ObjectCanvas::Draw d;d.object=m.object;d.id=m.id;d.image=shapes[f.frame%3];d.x=float((double(m.x)-m.y)*30/256.+f.terrain->geo.offsetX*30);d.y=float((double(m.x)+m.y)*15/256.-f.terrain->geo.offsetY*15);d.base=d.y;d.light.ambient=700+f.frame%4*100;out->instances.push_back(d);if(m.id%31==0){d.drop=3;d.y+=2;d.material.mixStrength=128;out->instances.push_back(d);}}
 prefix.x+=30;out->instances.push_back(prefix);if(!f.markers.empty())out->missing.push_back(f.markers.back());return out;
}
template<class A,class B>static void equal(const A&a,const B&b,const std::shared_ptr<const radar::Scene>&x,const std::shared_ptr<const radar::Scene>&y){
 assert(x&&y&&x->width==y->width&&x->height==y->height&&x->pixels==y->pixels);assert(a.canvas_.nativeDepth()==b.canvas_.nativeDepth());auto aa=a.canvas_.nativeAlpha(),bb=b.canvas_.nativeAlpha();assert(bool(aa)==bool(bb));if(aa)assert(*aa==*bb);
 assert(a.presentedState().frame==b.presentedState().frame&&a.presentedState().markers.size()==b.presentedState().markers.size());
}
static void pipeline(const std::shared_ptr<radar::Terrain>&t){
 camera::NativeCanvas ground;assert(ground.reset(3840,1920));ground.retainGround(true);auto tile=testSprite(0xff3c6a30,60,30);for(int y=0;y<1920;y+=30)for(int x=0;x<3840;x+=60)ground.terrainSource(size_t(y/30)*64+x/60,tile,float(x+30),float(y+30),float(y+30),{},0);
 std::array<worldscene::BeforePipeline,3> old;std::array<worldscene::InspectedPipeline,3> current;Read read;double a=0,b=0,sorting=0;size_t checks=0;
 std::shared_ptr<worldscene::Snapshot> latest;for(int n=1;n<=22;++n){latest=world(frame(t,n,2048),ground);double orderAt=now();worldscene::InstanceOrder order(latest);double orderMs=now()-orderAt;auto expected=latest->instances;tetherdraw::replay(expected,latest->state.markers);assert(*order.forWorld(latest)==expected);assert(!order.forWorld({}));
  double before=0,after=orderMs;for(int i=0;i<3;++i){mainview::View view{0x10000,t->geo.house,0x10000,n,-320+i*160,1700+i*50,640,512,0,0,640,512};std::shared_ptr<const radar::Scene>x,y;
   auto prior=[&]{double begin=now();x=old[i].draw(latest,view,320,256,read);before+=now()-begin;};auto next=[&]{double begin=now();y=current[i].draw(latest,view,320,256,read,nullptr,0,&order);after+=now()-begin;};if(n&1){prior();next();}else{next();prior();}equal(old[i],current[i],x,y);++checks;
  }if(n>2){a+=before;b+=after;sorting+=orderMs;}}
 std::printf("PASS three-camera actual Pipeline: %zu RGBA/Z/A pairs, 2048 actors, changing position/light/image; all-camera mean_ms=%.6f->%.6f (new includes common ordering %.6f); Frame copies=3->0 ordering=3->1\n",checks,a/20,b/20,sorting/20);
 // Wrong-owner plans (including chrono retaining the previous world) must
 // use the original replay path, not silently draw the incoming world's art.
 auto other=world(frame(t,23,2048),ground);worldscene::InstanceOrder wrong(other);mainview::View view{0x10000,t->geo.house,0x10000,23,-320,1700,640,512,0,0,640,512};
 auto x=old[0].draw(latest,view,160,128,read);auto y=current[0].draw(latest,view,160,128,read,nullptr,0,&wrong);equal(old[0],current[0],x,y);
 auto live=frame(t,24,2037);live.markers[0].id+=500;live.markers[1].material.mixStrength=64;x=old[0].draw(latest,view,160,128,read,&live);y=current[0].draw(latest,view,160,128,read,&live,0,&wrong);equal(old[0],current[0],x,y);
 // Return to borrowed storage, then switch to chrono's retained old owner.
 view.frame=25;x=old[0].draw(latest,view,160,128,read);y=current[0].draw(latest,view,160,128,read,nullptr,0,&wrong);equal(old[0],current[0],x,y);
 auto extras=std::make_shared<sceneextras::Snapshot>();extras->chronoValid=true;extras->chrono.phase=1;extras->chrono.duration=20;extras->chrono.remaining=18;extras->chrono.source=0x10000;other->state.extras=extras;view.frame=26;
 x=old[0].draw(other,view,160,128,read);y=current[0].draw(other,view,160,128,read,nullptr,0,&wrong);equal(old[0],current[0],x,y);assert(current[0].presentedState().frame==latest->state.frame);
 // The public reference remains backed by an owning shared_ptr, even if a
 // later draw fails validation and the caller releases its source.
 worldscene::Pipeline owner;auto held=world(frame(t,40,8),ground);std::weak_ptr<const worldscene::Snapshot>weak=held;view.frame=40;assert(owner.draw(held,view,160,128,read));held.reset();assert(!weak.expired()&&owner.presentedState().markers.size()==8);assert(!owner.draw({},view,160,128,read));assert(!weak.expired()&&owner.presentedState().frame==40);owner.reset();assert(weak.expired()&&!owner.presentedState().terrain&&owner.presentedState().markers.empty());
 std::puts("PASS ordered body/shadow/tether redraw + hidden/native list order; live removal/identity/material; wrong owner; actual chrono retained owner; borrowed/live/reset/public lifetime");
}
using BaseAssets=cameraassets::Context<Read>;
struct Assets:BaseAssets {using BaseAssets::BaseAssets;
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(0xff2266aa);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&p,voxel::Image&out){out=*testSprite(p[1],60,30);for(int y=0;y<30;++y)for(int x=0;x<60;++x)out.pixels[y*60+x]^=uint32_t(((x+2*y)%7)*16);return true;}
 bool captureActor(uint32_t,uint32_t,uint32_t,ActorArt&,const visual::Pose*,int,uint32_t){return true;}
 bool refreshActorPalette(ActorArt&,bool&changed){changed=false;return true;}
 bool renderSharedActor(const ActorArt&,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out){static auto images=std::array<std::shared_ptr<const voxel::Image>,3>{{testSprite(0xff8040e0),testSprite(0xff4080e0),testSprite(0xffe08040)}};out=images[size_t(std::max(0,pose.frame))%images.size()];return true;}
};
static void watchRun(const std::shared_ptr<radar::Terrain>&t){
 Read read;Assets aa(read),bb(read);watchscene::BeforeRenderer<Read,Assets>old(read,aa);watchscene::Renderer<Read,Assets>current(read,bb);presentation::Profile p;p.gpu=false;p.motion=false;old.features(p);current.features(p);old.process=current.process=7;old.epoch=current.epoch=9;old.revealMap=current.revealMap=true;
 double a=0,b=0,ap=0,bp=0,ad=0,bd=0;uint64_t ar=0,br=0;int completed=0;
 for(int n=100;n<124;++n){auto value=frame(t,n,1024);for(int i=0;i<3;++i){auto&m=value.markers[360+i*35];GameView game(read);m.pose=visual::pose(game,m.object,m.type,m.kind,n);auto draw=nativedraw::capture(read,m.object,m.type,m.kind,t->geo.house,n);m.material=draw.material;m.waterline=draw.waterline;}auto f=std::make_shared<const radar::Frame>(std::move(value));radar::sceneBank().publish(7,9,true,f);for(int i=0;i<3;++i){auto&m=f->markers[360+i*35];old.tracks[i].target=current.tracks[i].target={m.object,m.id};}
  put(0x60352C,85+n%4*5); // New presentation must still capture ambient light.
  if(n==100){bool ready=false;for(int warm=0;warm<32&&!ready;++warm){assert(old.update(n)&&current.update(n));ready=true;for(int i=0;i<3;++i)ready=ready&&old.views[i].scene&&current.views[i].scene;}assert(ready);}
  uint64_t ro=0,rn=0;bool ox=false,ny=false;auto before=[&]{reads=0;ox=old.update(n);ro=reads;};auto after=[&]{reads=0;ny=current.update(n);rn=reads;};if(n&1){before();after();}else{after();before();}assert(ox&&ny&&old.sharedSource&&current.sharedSource);
  for(int i=0;i<3;++i){if(!old.views[i].scene||!current.views[i].scene)std::printf("missing scene n=%d view=%d shown=%d/%d prepared=%d/%d attempted=%d/%d total=%f/%f\n",n,i,old.views[i].shown,current.views[i].shown,old.timings.prepared,current.timings.prepared,old.timings.attempted[i],current.timings.attempted[i],old.timings.total,current.timings.total);assert(old.views[i].scene&&current.views[i].scene);assert(old.views[i].scene->pixels==current.views[i].scene->pixels);assert(old.views[i].frame.cameraX==current.views[i].frame.cameraX&&old.views[i].frame.cameraY==current.views[i].frame.cameraY);}
  if(n>=104){++completed;a+=old.timings.total;b+=current.timings.total;ap+=old.timings.prepare;bp+=current.timings.prepare;for(int i=0;i<3;++i){ad+=old.timings.draw[i];bd+=current.timings.draw[i];}ar+=ro;br+=rn;}
 }
 std::printf("PASS actual watch Renderer %d updates x 3 cameras, 1024 actors, full prepare-world/dynamic ambient/pose/camera: total_mean_ms=%.6f->%.6f prepare=%.6f->%.6f cameras=%.6f->%.6f reads=%llu->%llu\n",completed,a/completed,b/completed,ap/completed,bp/completed,ad/completed,bd/completed,(unsigned long long)(ar/completed),(unsigned long long)(br/completed));
 // Keep exactly one immutable bank while the observed focus moves in
 // sub-cell increments. The old actual Renderer repeatedly overwrites its
 // fresh anchor with this bank; both implementations must advance together.
 auto bank=std::make_shared<const radar::Frame>(frame(t,200,1024));radar::sceneBank().publish(7,9,true,bank);
 for(int i=0;i<3;++i){auto&m=bank->markers[360+i*35];old.tracks[i].target=current.tracks[i].target={m.object,m.id};}
 for(int warm=0;warm<12;++warm){assert(old.update(200)&&current.update(200));}
 const auto sampled=bank->markers[360];double previous=current.views[0].frame.cameraX;auto previousPixels=current.views[0].scene->pixels;
 for(int step=1;step<=8;++step){put(sampled.object+0x9C,std::array<int,3>{{sampled.x+step*32,sampled.y,24}});assert(old.update(200+step)&&current.update(200+step));
  assert(old.views[0].frame.cameraX==current.views[0].frame.cameraX);assert(old.views[0].scene->pixels==current.views[0].scene->pixels);assert(current.views[0].frame.cameraX>previous);assert(current.views[0].frame.center.x==sampled.x+step*32&&current.views[0].frame.center.z==24);assert(current.views[0].scene->pixels!=previousPixels);previous=current.views[0].frame.cameraX;previousPixels=current.views[0].scene->pixels;
  assert(bank->markers[360].x==sampled.x&&bank->markers[360].z==sampled.z&&bank->frame==200); // no source publication mutation
 }
 auto held=current.views[0].scene;const auto target=current.tracks[0].target;failAt=sampled.object+0x9C;assert(current.update(209));assert(current.views[0].scene==held&&current.tracks[0].target==target);failAt=0;put(sampled.object+0x9C,std::array<int,3>{{sampled.x+9*32,sampled.y,24}});assert(current.update(210)&&current.views[0].frame.center.x==sampled.x+9*32);
 std::puts("PASS actual Renderer stale-bank regression: 8 sub-cell steps full and cropped cameras+body move identically; native observed elevation; no bank mutation; failed target retains image+tracking and recovers");
}
static void anchorCases(const std::shared_ptr<radar::Terrain>&t){
 Read read;auto sample=frame(t,300,8);auto carrier=sample.markers[2];constexpr uint32_t infantry=0xE00000,infantryId=50000;
 put(infantry,uint32_t(0x7EB058));put(infantry+0x10,infantryId);put(infantry+0x6C,100);put(infantry+0x90,uint8_t(1));put(infantry+0x11C,carrier.object);put(carrier.object+0x114,1);put(carrier.object+0x118,infantry);
 put(carrier.object+0x9C,std::array<int,3>{{carrier.x+48,carrier.y,48}});
 // A real Drive shadow capture must use the new native position too.
 constexpr uint32_t loco=0x700000;put(carrier.object+0x674,loco);put(loco,uint32_t(0x7E7EB0));put(loco+8,carrier.object);put(0x89E7C0,256);put(0xB45188,voxelposture::identity());put(carrier.type+0x236,uint8_t(1));put(carrier.type+0xB0,uint32_t(0x730000));put(carrier.type+0xB4,uint32_t(0x740000));put(0x740008,3);put(carrier.type+0xBC,uint32_t(0));
 std::array<watch::Anchor,3> anchors;anchors[0]=watch::anchor(read,{infantry,infantryId});assert(anchors[0].target.pointer==carrier.object&&anchors[0].x==carrier.x+48);
 auto data=sample;assert(watchscene::currentAnchors(data,anchors,read)&&anchors[0].located);assert(data.markers[2].x==carrier.x+48&&data.markers[2].z==48&&data.markers[2].pose.shadow.known&&data.markers[2].pose.shadow.posture.available&&data.markers[2].pose.shadow.drop==24);
 assert(data.markers.size()==sample.markers.size()); // no passenger drawn outside its host
 // Stationary transforms/materials still refresh. Null turret HVA is a
 // legitimate optional fallback even though this reader rejects page zero.
 put(carrier.object+0x388,uint16_t(0x2345));put(carrier.type+0xCA1,uint8_t(1));put(carrier.object+0x3A0,uint16_t(0x4567));put(carrier.object+0x270,uint8_t(1));put(carrier.object+0x94,3);
 assert(watchscene::currentAnchors(data,anchors,read,true)&&anchors[0].located);assert(data.markers[2].pose.bodyFine==35&&data.markers[2].pose.turretFine==69&&data.markers[2].material.translucency==2&&data.markers[2].layer==3);
 put(carrier.object+0x9C,std::array<int,3>{{carrier.x+56,carrier.y,48}});anchors[0]=watch::anchor(read,{infantry,infantryId});assert(watchscene::currentAnchors(data,anchors,read,true)&&anchors[0].located&&data.markers[2].pose.bodyFine==35&&data.markers[2].x==carrier.x+56);
 assert(watchscene::currentAnchors(data,anchors,read,false)&&anchors[0].located&&data.markers[2].pose.bodyFine==-1&&data.markers[2].pose.turretFine==-1);put(carrier.object+0x9C,std::array<int,3>{{carrier.x+48,carrier.y,48}});anchors[0]=watch::anchor(read,{infantry,infantryId});
 auto frozen=data;put(carrier.object+0x10,carrier.id+1);assert(watchscene::currentAnchors(data,anchors,read)&&!anchors[0].located&&data.markers[2].x==frozen.markers[2].x);put(carrier.object+0x10,carrier.id);
 anchors[0]=watch::anchor(read,{infantry,infantryId});const auto cellIndex=size_t(anchors[0].y/256)*512+anchors[0].x/256;const uint32_t address=t->geo.cells+uint32_t(cellIndex)*4;uint32_t cell=0;assert(read(address,&cell,4));put(address,cell+0x200);data=sample;assert(watchscene::currentAnchors(data,anchors,read)&&!anchors[0].located&&data.markers[2].x==carrier.x);put(address,cell);
 // Garrison resolves the verified building instead of the hidden infantry.
 auto building=sample.markers[0];put(infantry+0x11C,uint32_t(0));put(infantry+0x81,uint8_t(1));put(building.object+0x688,std::array<uint32_t,4>{{0x300000,1,0,1}});put(0x300000,infantry);
 watch::Anchor prior{{building.object,building.id},building.x,building.y,building.z,true,3};anchors={};anchors[0]=watch::anchor(read,{infantry,infantryId},prior);assert(anchors[0].target.pointer==building.object);data=sample;assert(watchscene::currentAnchors(data,anchors,read)&&anchors[0].located&&data.markers[0].x==building.x);
 // Chrono preserves the old world/view even when a current native anchor is newer.
 put(0xA9FAB0,1);anchors={};anchors[0]={{carrier.object,carrier.id},carrier.x+48,carrier.y,48,true,2};data=sample;assert(watchscene::currentAnchors(data,anchors,read)&&anchors[0].located&&anchors[0].x==carrier.x&&data.markers[2].x==carrier.x);put(0xA9FAB0,0);
 // No valid scene/world is fabricated from a transient read failure.
 anchors={};anchors[0]={{carrier.object,carrier.id},carrier.x+48,carrier.y,48,true,2};data=sample;failAt=0xA83D4C;assert(!watchscene::currentAnchors(data,anchors,read)&&data.markers[2].x==carrier.x);failAt=0;
 flipChrono=true;chronoReads=0;assert(!watchscene::currentAnchors(data,anchors,read));flipChrono=false;put(0xA9FAB0,0);
 std::puts("PASS current-anchor capture: verified carrier/garrison; observed Drive shadow drop; stationary pose/material/layer; exact fine angles in both motion states; null turret HVA/low-page rejection; pointer reuse and replaced Cell rejected; chrono held/transition rejected; world read failure retains staged source");
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);xbrzadapter::gpuPresentation(false);auto t=makeTerrain();pipeline(t);watchRun(t);anchorCases(t);}

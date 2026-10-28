#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <cstdio>
#include <map>
struct Memory {
 std::map<uint32_t,uint8_t> bytes;
 template<class T>void put(uint32_t p,T value){const auto*b=reinterpret_cast<const uint8_t*>(&value);for(size_t n=0;n<sizeof value;++n)bytes[p+uint32_t(n)]=b[n];}
 bool operator()(uint32_t p,void*out,size_t n)const{auto*b=static_cast<uint8_t*>(out);for(size_t k=0;k<n;++k){auto found=bytes.find(p+uint32_t(k));if(found==bytes.end())return false;b[k]=found->second;}return true;}
};
struct Assets {
 struct ActorArt {uint32_t object=0,color=0;};
 uint32_t color=0xff80c040,terrainColor=0xff808080,overlayColor=0xff4040c0,lastOverlay=overlayColor;
 std::map<uint32_t,uint32_t> objectColors;bool readable=true;int captures=0,renders=0,tiles=0,overlays=0,passes=0;
 uint64_t revision(){return 1;}static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 void beginPresentation(){++passes;}
 bool palette(bool,std::array<uint32_t,256>&out){out.fill(terrainColor);out[0]=0;return true;}
 bool refreshActorPalette(ActorArt&art,bool&changed){changed=false;if(!readable)return false;uint32_t next=objectColors.count(art.object)?objectColors.at(art.object):color;changed=art.color!=next;art.color=next;return true;}
 bool refreshOverlayPalette(uint32_t,uint32_t,int,bool,bool&changed){changed=lastOverlay!=overlayColor;lastOverlay=overlayColor;return readable;}
 bool captureActor(uint32_t p,uint32_t,uint32_t,ActorArt&art){++captures;art.object=p;bool changed=false;return refreshActorPalette(art,changed);}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&art){return captureActor(p,t,id,art);}
 bool renderActor(const ActorArt&art,const external::visual::Pose&,external::voxel::Image&out){++renders;out={};out.width=out.height=2;out.pixels.assign(4,art.color);out.writeDepth=false;return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&pal,external::voxel::Image&out){++tiles;out={};out.width=60;out.height=30;out.x=-30;out.y=-15;out.pixels.assign(1800,pal[1]);out.writeDepth=false;return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&out,bool=false){++overlays;out={};out.width=out.height=2;out.pixels.assign(4,overlayColor);out.writeDepth=false;return true;}
 bool shroud(bool,int,external::voxel::Image&){return false;}
 void buildingPose(uint32_t,uint32_t,external::visual::Pose&){}
};
int main(){
 using namespace external;using namespace radar;
 constexpr uint32_t cell=0x10000,cellList=0x20000,type=0x30000,owner=0x40000,vector=0x50000,schemeList=0x60000,scheme=0x70000,convert=0x71000,terrainConvert=0x72000,scenario=0x80000,rules=0x90000;
 Memory memory;memory.put(0xA8B230,scenario);memory.put(scenario+0x352C,100);for(int n=0;n<3;++n)memory.put(scenario+0x3534+n*4,100);
 memory.put(Map+0x13C,cellList);memory.put(Map+0x140,512*512);memory.put(cellList+(7*512+7)*4,cell);memory.put(cell+0x24,int16_t(7));memory.put(cell+0x26,int16_t(7));memory.put(cell+0x104,65536u);memory.put(cell+0x34,terrainConvert);
 for(int n=0;n<3;++n)memory.put(cell+0x10A+n*2,int16_t(1000));memory.put(cell+0x140,0u);memory.put(cell+0x11B,int8_t(0));
 memory.put(type+0xDF0,vector);memory.put(owner+0x16054,0);memory.put(vector+4,schemeList);memory.put(vector+16,1);memory.put(schemeList,scheme);memory.put(scheme+0x30C,convert);
 for(uint32_t c:{convert,terrainConvert}){memory.put(c+0x1B0,uint8_t(0));for(int n=0;n<3;++n)memory.put(c+0x198+n*4,1000);}
 memory.put(0x8871E0,rules);memory.put(rules+0x17D4,0);
 auto terrain=std::make_shared<Terrain>();terrain->geo={8,8,6,7,8,8,1,0x10000,owner,512*512,{8,8,0,0,8,8},8,8};terrain->reveal=true;terrain->colors.resize(512*512);terrain->art.resize(512*512);terrain->shade.resize(512*512);terrain->colors[7*512+7]=0x808080;terrain->art[7*512+7].type=1;terrain->art[7*512+7].cell=cell;
 Frame frame;frame.terrain=terrain;frame.frame=42;frame.tick=100;Marker m;m.object=0xa0000;m.id=1;m.type=type;m.owner=owner;m.kind=2;m.x=m.y=7*256;m.layer=3;frame.markers={m};Assets assets;SceneRenderer<Assets> renderer;
 SceneRegion crop{0,0,240,120,true};
 auto draw=[&](){return renderer.render(frame,240,120,memory,assets,crop,0,true);};
 auto initial=draw();assert(initial);const size_t actor=105*240+180,ground=110*240+190;auto actorColor=initial->pixels[actor],groundColor=initial->pixels[ground];int captures=assets.captures,renders=assets.renders,loads=assets.tiles;
 memory.put(cell+0x10C,int16_t(500));auto local=draw();assert(local&&local->pixels[ground]!=groundColor&&local->pixels[actor]==actorColor);assert(assets.captures==captures&&assets.renders==renders&&assets.tiles==loads);
 memory.put(terrainConvert+0x198,500);auto localTint=draw();assert(localTint&&localTint->pixels[ground]!=local->pixels[ground]);assert(assets.tiles==loads);
 memory.put(convert+0x198,500);auto ownerTint=draw();assert(ownerTint&&ownerTint->pixels[actor]!=actorColor&&ownerTint->pixels[ground]==localTint->pixels[ground]);assert(assets.captures==captures&&assets.renders==renders);
 memory.put(rules+0x17D4,200);auto extra=draw();assert(extra&&extra->pixels[actor]!=ownerTint->pixels[actor]);assert(assets.captures==captures&&assets.renders==renders);
 assets.color=0xffc08040;auto palette=draw();assert(palette&&palette->pixels[actor]!=extra->pixels[actor]);assert(assets.captures==captures&&assets.renders==renders+1);
 assets.terrainColor=0xffc0c0c0;auto terrainPalette=draw();assert(terrainPalette&&terrainPalette->pixels[ground]!=palette->pixels[ground]);assert(assets.tiles==loads+1&&assets.captures==captures);
 assets.readable=false;assert(!draw());assets.readable=true;assets.color=0xff4080c0;auto recovered=draw();assert(recovered&&recovered->pixels[actor]!=terrainPalette->pixels[actor]&&assets.captures==captures);
 // Same type/owner animations retain their independent color-source bindings.
 memory.put(convert+0x198,1000);memory.put(convert+0x16C,53);
 for(uint32_t object:{0xa0000u,0xb0000u}){memory.put(object+0xD4,convert);memory.put(object+0xFC,1000);memory.put(object+0x196,uint8_t(0));}
 for(uint32_t field:{0x355u,0x35Du,0x361u})memory.put(type+field,uint8_t(0));
 frame.markers[0].kind=5;frame.markers[0].x=6*256;frame.markers.push_back(frame.markers[0]);frame.markers[1].object=0xb0000;frame.markers[1].id=2;frame.markers[1].x=7*256;
 assets.objectColors[0xa0000]=0xffc00000;assets.objectColors[0xb0000]=0xff0000c0;auto animations=draw();assert(animations);assert(animations->pixels[90*240+150]==0xffc00000&&animations->pixels[105*240+180]==0xff0000c0);
 int animCaptures=assets.captures;assets.objectColors[0xa0000]=0xff00c000;auto changedAnimation=draw();assert(changedAnimation&&changedAnimation->pixels[90*240+150]==0xff00c000&&changedAnimation->pixels[105*240+180]==0xff0000c0&&assets.captures==animCaptures);
 int animationRenders=assets.renders;memory.put(0xa0000+0xFC,500);auto animationIntensity=draw();assert(animationIntensity&&animationIntensity->pixels[90*240+150]!=changedAnimation->pixels[90*240+150]&&animationIntensity->pixels[105*240+180]==changedAnimation->pixels[105*240+180]);
 constexpr uint32_t alternate=0x73000;memory.put(alternate+0x1B0,uint8_t(0));memory.put(alternate+0x16C,53);for(int n=0;n<3;++n)memory.put(alternate+0x198+n*4,n==1?400:1000);memory.put(0xa0000+0xD4,alternate);
 auto animationConverter=draw();assert(animationConverter&&animationConverter->pixels[90*240+150]!=animationIntensity->pixels[90*240+150]&&animationConverter->pixels[105*240+180]==animationIntensity->pixels[105*240+180]);assert(assets.captures==animCaptures&&assets.renders==animationRenders);
 // Multi-pass terrain light validation returns no stale lit bank mid-scan.
 // Use actual legal sources inside the main-view coverage. The old fixture
 // filled 4097 illegal/offscreen array slots, which local coverage correctly
 // excludes before they can consume a rendering budget.
 auto large=std::make_shared<Terrain>();large->geo={128,128,128,128,256,256,1,0x10000,owner,512*512,{128,128,0,0,128,128},256,256};large->reveal=true;
 large->colors.resize(512*512);large->art.resize(512*512);large->shade.resize(512*512);crop={2800,-64,2000,2000,true};
 auto coverage=terraincoverage::Region::plan(crop.x,crop.y,crop.width,crop.height,7680,3840);int supplied=0;
 for(int y=1;y<512&&supplied<4097;++y)for(int x=1;x<512&&supplied<4097;++x){mapbounds::TileOrigin at;
  if(large->geo.tileOrigin(x,y,0,at)&&coverage.tile(at.x,at.y)){large->colors[size_t(y)*512+x]=0x808080;++supplied;}
 }
 assert(supplied==4097);large->colors[64*512+65]=0x808080;large->art[64*512+65]=terrain->art[7*512+7];
 frame.terrain=large;frame.markers.clear();assert(!draw());assert(draw());
 assert(draw()); // visible crop stays available while offscreen probe is partial
 memory.put(scenario+0x352C,50);memory.put(cell+0x10C,int16_t(700));assert(!draw());std::shared_ptr<const Scene> global;for(int n=0;n<6&&!global;++n)global=draw();assert(global);
 memory.put(cell+0x10C,int16_t(600));assert(!draw());memory.put(cell+0x10C,int16_t(650));assert(!draw());assert(!draw()); // reject a bank completed from the previous local-light snapshot
 std::shared_ptr<const Scene> coherent;for(int n=0;n<8&&!coherent;++n)coherent=draw();assert(coherent);
 assert(frame.frame==42&&frame.tick==100&&assets.revision()==1);
 std::puts("PASS: same-frame local terrain/owner tint/rule lighting and live palette changes, retained geometry, per-object animation palettes, failed-read recovery and no stale multipass background");
}

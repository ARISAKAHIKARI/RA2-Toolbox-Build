#include "../../external_reader/source/radar_scene.h"
#include <cassert>
#include <iostream>
#include <functional>
struct Assets {
 struct ActorArt {int id=0;bool anim=false;};struct Files {uint64_t value=1;uint64_t revision(){return value;}}files;int loads=0,captures=0,renders=0;std::set<int> seen;std::map<int,std::set<int>> frames;std::set<std::pair<int,int>> buildingPoses;bool small=false,partial=false;
 uint32_t forcedColor=0,overlayColor=0;
 uint64_t revision(){return files.revision();}
 static size_t cost(const ActorArt&){return sizeof(ActorArt);}
 bool palette(bool,std::array<uint32_t,256>&p){p.fill(0xffffffff);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){++loads;i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.assign(1800,0xffa03020);i.depth.assign(1800,0);return true;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&i,bool=false){if(!overlayColor)return false;i.width=i.height=60;i.x=i.y=-30;i.pixels.assign(3600,overlayColor);i.writeDepth=false;return true;}
 int buildingFrame(uint32_t,uint32_t){return 0;}
 void buildingPose(uint32_t p,uint32_t t,external::visual::Pose&pose){pose.frame=buildingFrame(p,t);}
 bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&a){++captures;a.id=id;return true;}
 bool captureAnimation(uint32_t,uint32_t,uint32_t id,uint32_t,uint32_t,ActorArt&a){a.id=id;a.anim=true;return true;}
 bool renderActor(const ActorArt&a,const external::visual::Pose&pose,external::voxel::Image&i){++renders;seen.insert(a.id);frames[a.id].insert(pose.frame);if(pose.buildingFlags)buildingPoses.emplace(pose.buildingFlags,pose.doorFrame);i.width=small?8:60;i.height=small?8:180;i.x=-i.width/2;i.y=-i.height/2;i.pixels.assign(i.width*i.height,forcedColor?forcedColor:(a.anim?0xffaaff00:0xff00aa00));i.depth.assign(i.width*i.height,a.anim?100:5);i.writeDepth=false;i.incomplete=partial;return true;}
};

struct PatternAssets:Assets {
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){++loads;i.width=60;i.height=30;i.x=-30;i.y=-15;i.pixels.resize(1800);i.depth.assign(1800,0);for(int y=0;y<30;++y)for(int x=0;x<60;++x)i.pixels[y*60+x]=((x+y)&1)?0xffffffff:0xff000000;return true;}
};
struct WideAssets:Assets {
 int tileWidth=384;bool largeActor=false;
 bool tile(uint32_t type,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&i){++loads;i.width=tileWidth;i.height=1;i.x=-tileWidth/2;i.y=0;i.pixels.assign(tileWidth,type==1?0xffa03020:0xff2040a0);i.depth.assign(tileWidth,0);return true;}
 bool renderActor(const ActorArt&a,const external::visual::Pose&pose,external::voxel::Image&i){bool ok=Assets::renderActor(a,pose,i);i.width=largeActor?1024:60;i.height=largeActor?128:30;i.x=-i.width/2;i.y=-i.height/2;i.pixels.assign(i.width*i.height,0xff00aa00);i.depth.assign(i.pixels.size(),5);return ok;}
};
struct EffectsAssets:Assets {
 struct Files{uint64_t value=1;bool load(const char*,std::vector<uint8_t>&){return false;}}files;
 std::function<bool(uint32_t,void*,size_t)> read=[](uint32_t,void*,size_t){return false;};int projectiles=0;
 uint64_t revision(){return files.value;}
 bool captureProjectile(uint32_t,uint32_t,uint32_t id,ActorArt&a){++projectiles;a.id=id;return true;}
};
int main(){
 using namespace external::radar;Assets assets;SceneRenderer<Assets> renderer;auto read=[](uint32_t,void*,size_t){return false;};
 auto terrain=std::make_shared<Terrain>();terrain->geo={64,64,64,64,128,128,1,0x10000,0x20000,512*512,{64,64,0,0,64,64},128,128};terrain->colors.resize(512*512);terrain->art.resize(512*512);
 terrain->colors[64*512+64]=0xa03020;terrain->art[64*512+64].type=1;Frame frame;frame.terrain=terrain;frame.tick=100;
 std::shared_ptr<const Scene> scene=renderer.render(frame,128,64,read,assets);
 for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,128,64,read,assets);
 assert(scene&&scene->width==128&&scene->height==64&&assets.loads==1);int loads=assets.loads;
 // A legal southern border cell may sit outside the gameplay area. Preserve
 // its actual TMP when resolved, but never synthesize a ground diamond when
 // the sampler has no tile for that cell (type 0).
 {
  constexpr int edgeX=95,edgeY=96;const size_t edge=size_t(edgeY)*512+edgeX;
  auto south=std::make_shared<Terrain>(*terrain);south->geo.bounds.rows=62;
  assert(south->geo.legal(edgeX,edgeY)&&!south->geo.bounds.usable(edgeX,edgeY));
  south->colors.assign(512*512,0);south->art.assign(512*512,{});south->colors[edge]=0x996633;
  Assets edgeAssets;SceneRenderer<Assets> edgeRenderer;Frame edgeFrame;edgeFrame.terrain=south;edgeFrame.tick=100;
  auto missing=edgeRenderer.render(edgeFrame,128,64,read,edgeAssets);
  assert(missing&&edgeAssets.loads==0&&std::all_of(missing->pixels.begin(),missing->pixels.end(),[](uint32_t p){return p==0xff080c08;}));
  auto resolved=std::make_shared<Terrain>(*south);resolved->art[edge].type=1;edgeFrame.terrain=resolved;
  auto drawn=edgeRenderer.render(edgeFrame,128,64,read,edgeAssets);
  assert(drawn&&edgeAssets.loads==1&&drawn->pixels[63*128+63]!=0xff080c08);
 }
 Marker m;m.u=m.v=.5;m.x=m.y=64*256+128;m.object=0x10000;m.type=0x20000;m.id=1;m.kind=2;frame.markers={m};
 scene=renderer.render(frame,128,64,read,assets);assert(scene&&std::find(scene->pixels.begin(),scene->pixels.end(),0xff00aa00)!=scene->pixels.end()&&assets.loads==loads);
 auto old=scene;auto replacement=std::make_shared<Terrain>(*terrain);frame.terrain=replacement;scene=renderer.render(frame,128,64,read,assets);assert(scene&&old->pixels.size()==128*64); // completed background stays visible while refreshing
 scene=renderer.render(frame,64,32,read,assets);assert(!scene||scene->width==64);
 scene.reset();for(int i=0;i<80&&!scene;++i)scene=renderer.render(frame,64,32,read,assets);assert(scene&&scene->width==64&&scene->height==32&&assets.loads==loads);
 scene=renderer.render(frame,1600,800,read,assets);assert(scene&&scene->width==1600&&assets.loads==loads);
 ++assets.files.value;int oldLoads=assets.loads;renderer.render(frame,64,32,read,assets);assert(assets.loads>oldLoads);assert(!renderer.render(frame,100000,2,read,assets));
 // Continuous motion formerly exhausted the first eight decode slots forever.
 renderer.reset();assets.small=true;assets.seen.clear();frame.markers.clear();
 for(int n=0;n<32;++n){auto moving=m;moving.id=n+1;moving.object=0x10000+n*0x800;moving.type=0x50000+n*0x800;moving.u=.05+n*.025;moving.v=.25;frame.markers.push_back(moving);}
 auto pump=m;pump.kind=5;pump.id=99;pump.object=0x80000;pump.type=0x90000;pump.u=pump.v=.75;pump.pose.frame=0;frame.markers.push_back(pump);
 for(int tick=0;tick<12;++tick){for(auto&marker:frame.markers)if(marker.kind!=5)marker.pose.frame=tick;frame.tick+=16;scene=renderer.render(frame,1024,512,read,assets);}
 assert(assets.seen.size()==33&&assets.seen.count(99));
 assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)!=scene->pixels.end());
 for(int tick=0;tick<4;++tick){for(auto&marker:frame.markers)++marker.pose.frame;frame.tick+=16;scene=renderer.render(frame,1024,512,read,assets);assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)!=scene->pixels.end());} // last good anim stays visible under budget
 assert(assets.frames[99].count(0)&&assets.frames[99].count(1));
 // A body later in XY order must not paint over its attached animation.
 renderer.reset();assets.small=false;frame.markers={m,pump};frame.markers[0].u=frame.markers[0].v=.5;frame.markers[0].kind=3;frame.markers[1].u=frame.markers[1].v=.5;frame.markers[1].x=m.x-1;frame.markers[1].y=m.y-1;
 scene=renderer.render(frame,128,64,read,assets);assert(scene->pixels[32*128+64]==0xffaaff00);
 // Removing/hiding an attachment must remove its retained pose immediately.
 frame.markers.resize(1);scene=renderer.render(frame,128,64,read,assets);assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xffaaff00)==scene->pixels.end());
 // Ground animation is NOT a blanket foreground overlay. Native YSortAdjust
 // is in leptons and must order it among units even though neither writes Z.
 renderer.reset();auto front=m;front.x=m.x+256;front.y=m.y+256;front.u=front.v=.5;front.kind=2;front.layer=2;
 pump.x=m.x-128;pump.y=m.y-128;pump.u=pump.v=.5;pump.layer=2;pump.sort=362;
 frame.markers={front,pump};scene=renderer.render(frame,128,64,read,assets);assert(scene->pixels[32*128+64]==0xff00aa00);
 frame.markers[0].x=m.x-256;frame.markers[0].y=m.y-256;scene=renderer.render(frame,128,64,read,assets);assert(scene->pixels[32*128+64]==0xffaaff00);
 frame.markers[0]=front;frame.markers[1].layer=3;scene=renderer.render(frame,128,64,read,assets);assert(scene->pixels[32*128+64]==0xffaaff00);
 renderer.reset();auto fa=m;fa.kind=3;fa.pose.buildingFlags=3;fa.pose.doorFrame=2;fa.u=.3;auto fb=fa;fb.id=2;fb.object+=0x1000;fb.u=.7;fb.pose.doorFrame=6;frame.markers={fa,fb};int captures=assets.captures;
 scene=renderer.render(frame,128,64,read,assets);assert(scene&&assets.captures==captures+1&&assets.buildingPoses.count({3,2})&&assets.buildingPoses.count({3,6}));
 frame.markers[0].pose.buildingFlags=7;scene=renderer.render(frame,128,64,read,assets);assert(assets.captures==captures+1&&assets.buildingPoses.count({7,2}));
 frame.markers[0].pose.doorFrame=7;assets.partial=true;scene=renderer.render(frame,128,64,read,assets);assert(std::find(scene->pixels.begin(),scene->pixels.end(),0xff00aa00)!=scene->pixels.end());int renders=assets.renders;assets.partial=false;frame.tick+=50;scene=renderer.render(frame,128,64,read,assets);assert(assets.renders==renders);frame.tick+=2000;scene=renderer.render(frame,128,64,read,assets);assert(assets.renders==renders+1&&assets.captures==captures+1);
 // Same-size trigger crop: no old terrain/objects may leak through while the
 // renderer switches coordinate domains. Expanding restores cached tile art.
 frame.markers.clear();auto crop=std::make_shared<Terrain>(*terrain);crop->geo.bounds.left=40;crop->geo.bounds.columns=24;crop->colors[64*512+64]=0;frame.terrain=crop;
 scene=renderer.render(frame,128,64,read,assets);assert(scene&&std::all_of(scene->pixels.begin(),scene->pixels.end(),[](uint32_t p){return p==0xff080c08;}));
 frame.terrain=terrain;scene=renderer.render(frame,128,64,read,assets);assert(scene&&std::find(scene->pixels.begin(),scene->pixels.end(),0xffa03020)!=scene->pixels.end());
 // Restoring VPL changes the asset revision while actor identity/pose stay
 // fixed. Both previously cached headings must use the recovered source.
 {
  Assets recoveredAssets;SceneRenderer<Assets> recoveredRenderer;Frame recovery;recovery.terrain=terrain;recovery.tick=100;recovery.markers={m};
  recoveredAssets.forcedColor=0xff102030;auto restored=recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  assert(restored&&restored->pixels[32*128+64]==0xff102030);
  recovery.markers[0].pose.body=1;recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  recovery.markers[0].pose.body=0;recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  int oldCaptures=recoveredAssets.captures,oldRendered=recoveredAssets.renders;
  recoveredAssets.forcedColor=0xffa0b0c0;restored=recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  assert(recoveredAssets.captures==oldCaptures&&recoveredAssets.renders==oldRendered&&restored->pixels[32*128+64]==0xff102030);
  ++recoveredAssets.files.value;restored=recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  assert(restored&&recoveredAssets.captures==oldCaptures+1&&recoveredAssets.renders==oldRendered+1&&restored->pixels[32*128+64]==0xffa0b0c0);
  recovery.markers[0].pose.body=1;restored=recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  assert(restored&&recoveredAssets.captures==oldCaptures+1&&recoveredAssets.renders==oldRendered+2&&restored->pixels[32*128+64]==0xffa0b0c0);
  oldCaptures=recoveredAssets.captures;oldRendered=recoveredAssets.renders;
  for(int n=0;n<20;++n)recoveredRenderer.render(recovery,128,64,read,recoveredAssets);
  assert(recoveredAssets.captures==oldCaptures&&recoveredAssets.renders==oldRendered);
 }
 // A shroud-policy change invalidates the completed background even though
 // projection, house, output size and all source art remain identical.
 // Make the replacement larger than one sampling/render budget: its first
 // unfinished pass must not publish pixels from the old reveal setting.
 {
  Assets policyAssets;SceneRenderer<Assets> policyRenderer;Frame policy;policy.tick=100;
  auto unmasked=std::make_shared<Terrain>(*terrain);unmasked->reveal=true;policy.terrain=unmasked;
  auto shown=policyRenderer.render(policy,128,64,read,policyAssets);assert(shown);int loaded=policyAssets.loads;
  auto native=std::make_shared<Terrain>(*unmasked);native->reveal=false;
  for(size_t i=0;i<4097;++i)native->colors[i]=0x446644;
  policy.terrain=native;auto pending=policyRenderer.render(policy,128,64,read,policyAssets);assert(!pending);
  auto masked=policyRenderer.render(policy,128,64,read,policyAssets);assert(masked);
  assert(policyAssets.loads==loaded); // policy changes keep decoded art warm
  policy.terrain=unmasked;shown=policyRenderer.render(policy,128,64,read,policyAssets);assert(shown&&policyAssets.loads==loaded);
 }
 // Native resource/wall lighting is selected by the stored overlay type,
 // while the pooled sprite has its own type/cache key. Terrain is dark here.
 {
  std::vector<uint8_t> memory(0x40000);auto put=[&](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
  auto bytes=[&](uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;};
  constexpr uint32_t cell=0x10000,source=0x20000,draw=0x30000;put(cell+0x104,65536u);put(cell+0x10A,int16_t(1000));put(cell+0x10C,int16_t(0));put(cell+0x10E,int16_t(500));
  auto lightTerrain=std::make_shared<Terrain>(*terrain);auto&art=lightTerrain->art[64*512+64];art.cell=cell;art.overlayType=draw;art.overlaySourceType=source;
  Assets lightAssets;lightAssets.overlayColor=0xffc08040;Frame lit;lit.terrain=lightTerrain;lit.tick=100;SceneRenderer<Assets> lightRenderer;
  memory[source+0x2A9]=1;auto bright=lightRenderer.render(lit,128,64,bytes,lightAssets);assert(bright&&std::find(bright->pixels.begin(),bright->pixels.end(),lightAssets.overlayColor)!=bright->pixels.end());
  memory[source+0x2A9]=0;memory[draw+0x2A9]=1;lightRenderer.reset();auto dark=lightRenderer.render(lit,128,64,bytes,lightAssets);assert(dark&&std::find(dark->pixels.begin(),dark->pixels.end(),lightAssets.overlayColor)==dark->pixels.end());
  memory[source+0x2A8]=1;lightRenderer.reset();bright=lightRenderer.render(lit,128,64,bytes,lightAssets);assert(bright&&std::find(bright->pixels.begin(),bright->pixels.end(),lightAssets.overlayColor)!=bright->pixels.end());
  put(cell+0x140,0x80u);lightRenderer.reset();auto bridge=lightRenderer.render(lit,128,64,bytes,lightAssets);external::lighting::State bridgeLight;bridgeLight.intensity=500;uint32_t expected=external::camera::lit(lightAssets.overlayColor,0,false,bridgeLight);assert(bridge&&std::find(bridge->pixels.begin(),bridge->pixels.end(),expected)!=bridge->pixels.end());
 }

 // The production renderer composes the source checkerboard at native size
 // before one final filter. Per-sprite low-resolution nearest selection would
 // produce a solid black or white pixel instead of the exact 50% average.
 {
  PatternAssets pattern;SceneRenderer<PatternAssets> nativeRenderer;Frame f;f.terrain=terrain;f.tick=100;
  auto image=nativeRenderer.render(f,128,64,read,pattern);assert(image&&image->pixels[32*128+64]==0xff808080);
  auto stats=nativeRenderer.nativeStats();assert(stats.width==3840&&stats.height==1920&&stats.cells==1&&stats.pages<8);
  // Output resizing and identical immutable sampling banks retain native
  // color/Z pages and source art; only final presentation is regenerated.
  auto same=std::make_shared<Terrain>(*terrain);f.terrain=same;nativeRenderer.render(f,128,64,read,pattern);
  auto unchanged=nativeRenderer.nativeStats();assert(unchanged.cells==stats.cells&&unchanged.presents==stats.presents&&pattern.loads==1);
  auto resized=nativeRenderer.render(f,512,256,read,pattern);auto after=nativeRenderer.nativeStats();assert(resized&&after.cells==stats.cells&&after.presents==stats.presents+1&&pattern.loads==1);
  assert(nativeRenderer.render(f,512,256,read,pattern)==resized); // unchanged no-actor scene is shared, not copied
 }
 // Constrained budgets exercise the exact production convergence path. An
 // old native bank can be released while its small published Scene survives;
 // pending work then resumes, without switching to scaled source rendering.
 {
  WideAssets wide;SceneRenderer<WideAssets> limited(8);Frame f;f.terrain=terrain;f.tick=100;
  auto oldScene=limited.render(f,128,64,read,wide);assert(oldScene&&limited.nativeStats().pages==4);
  auto changed=std::make_shared<Terrain>(*terrain);changed->art[64*512+64].type=2;f.terrain=changed;
  assert(!limited.render(f,128,64,read,wide)&&!limited.capacityExceeded());assert(oldScene->pixels.size()==128*64);
  auto complete=limited.render(f,128,64,read,wide);assert(complete&&!limited.capacityExceeded()&&limited.nativeStats().pages==4&&limited.nativeStats().resident==4);
  wide.largeActor=true;f.markers={m};auto overloaded=limited.render(f,128,64,read,wide);assert(!overloaded&&limited.capacityExceeded());int rendered=wide.renders;
  f.markers.clear();assert(limited.render(f,128,64,read,wide)&&!limited.capacityExceeded());
  wide.largeActor=false;f.markers={m};++f.markers[0].pose.frame;auto recovered=limited.render(f,128,64,read,wide);assert(recovered&&!limited.capacityExceeded()&&wide.renders==rendered+1);
  // A single source plane beyond the configured resident budget is an
  // explicit blocked result, not endless reallocation/retry of the same cell.
  WideAssets huge;huge.tileWidth=1000;SceneRenderer<WideAssets> blocked(8);f.markers.clear();f.terrain=terrain;
  assert(!blocked.render(f,128,64,read,huge)&&blocked.capacityExceeded()&&blocked.nativeStats().resident==0);int loadsBefore=huge.loads;
  for(int i=0;i<20;++i)assert(!blocked.render(f,128,64,read,huge)&&blocked.capacityExceeded());assert(huge.loads==loadsBefore);
  auto empty=std::make_shared<Terrain>(*terrain);empty->colors[64*512+64]=0;f.terrain=empty;assert(blocked.render(f,128,64,read,huge)&&!blocked.capacityExceeded());
 }
 // Projectiles use their dedicated capture/light path. Repair cursor sprites
 // compose last, ignore arbitrary world depth, and disappear immediately.
 {
  EffectsAssets effects;SceneRenderer<EffectsAssets> effectsRenderer;Frame f;f.terrain=terrain;f.tick=100;
  auto bullet=m;bullet.kind=6;bullet.id=777;effects.forcedColor=0xff0000ff;f.markers={bullet};
  auto image=effectsRenderer.render(f,128,64,read,effects);assert(image&&effects.projectiles==1&&effects.captures==0&&image->pixels[32*128+64]==0xff0000ff);
  std::vector<uint8_t> memory(0x80000);auto put=[&](uint32_t at,auto value){memcpy(memory.data()+at,&value,sizeof value);};
  constexpr uint32_t shape=0x60000,converter=0x50000,palette=0x70000;
  put(converter,0x7E5358u);put(converter+4,2);put(converter+0x16C,1);put(converter+0x174,palette);put(palette+2,uint16_t(0xF800));put(palette+4,uint16_t(0x07E0));
  put(shape+2,uint16_t(60));put(shape+4,uint16_t(60));put(shape+6,uint16_t(7));
  for(int frame=0;frame<7;++frame){uint32_t header=shape+8+frame*24,offset=8+7*24+frame*3600;put(header+4,uint16_t(60));put(header+6,uint16_t(60));put(header+20,offset);std::fill(memory.begin()+shape+offset,memory.begin()+shape+offset+3600,uint8_t(frame%2+1));}
  effects.read=[&](uint32_t at,void*out,size_t n){if(uint64_t(at)+n>memory.size())return false;memcpy(out,memory.data()+at,n);return true;};
  external::repairvisuals::Sprite repair;repair.x=m.x;repair.y=m.y;repair.shape=shape;repair.converter=converter;repair.frame=0;repair.tick=1;f.repairs={repair};
  image=effectsRenderer.render(f,128,64,read,effects);assert(image&&image->pixels[32*128+64]==0xffff0000);
  ++f.repairs[0].tick;f.repairs[0].frame=1;image=effectsRenderer.render(f,128,64,read,effects);assert(image&&image->pixels[32*128+64]==0xff00ff00);
  f.repairs.clear();image=effectsRenderer.render(f,128,64,read,effects);assert(image&&image->pixels[32*128+64]==0xff0000ff);
  f.markers.clear();image=effectsRenderer.render(f,128,64,read,effects);assert(image&&std::find(image->pixels.begin(),image->pixels.end(),0xff0000ff)==image->pixels.end());
 }
 std::cout<<"PASS scene publication, native animation frame-zero/advance, independent door/roof cache, shared source art, fair retained poses, ground/air layers, shroud policy, resource/wall/bridge light, native area presentation/cache/budget recovery, projectile/repair effects, resize and reset\n";
}


#pragma once
#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/terrain_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include "label_motion.h"
#include "../../external_reader/source/camera_raster.h"
#include "../../external_reader/source/camera_repair.h"
#include "../../external_reader/source/watch_shroud_mask.h"
#include <memory>
#include <tuple>
#include <functional>
namespace watchengine {
template<class T,class=void>struct HasShroud:std::false_type{};
template<class T>struct HasShroud<T,std::void_t<decltype(std::declval<T&>().shroud(false,0,std::declval<external::voxel::Image&>()))>>:std::true_type{};
template<class Assets>auto beginPalettePass(Assets&a,int)->decltype(a.beginPresentation(),void()){a.beginPresentation();}
template<class Assets>void beginPalettePass(Assets&,long){}
template<class Assets,class Art>auto refreshPalette(Assets&a,Art&art,bool&changed,int)->decltype(a.refreshActorPalette(art,changed),bool()){return a.refreshActorPalette(art,changed);}
template<class Assets,class Art>bool refreshPalette(Assets&,Art&,bool&changed,long){changed=false;return true;}
template<class Assets>auto refreshOverlay(Assets&a,uint32_t t,uint32_t p,int f,bool terrain,bool&changed,int)->decltype(a.refreshOverlayPalette(t,p,f,terrain,changed),bool()){return a.refreshOverlayPalette(t,p,f,terrain,changed);}
template<class Assets>bool refreshOverlay(Assets&,uint32_t,uint32_t,int,bool,bool&changed,long){changed=false;return true;}
template<class Art>auto isVoxelArt(const Art&a,int)->decltype(bool(a.voxel)){return bool(a.voxel);}
template<class Art>bool isVoxelArt(const Art&,long){return false;}
template<class Art>auto voxelFaces(const Art&a,int)->decltype(a.model.faces.size()){return a.model.faces.size();}
template<class Art>size_t voxelFaces(const Art&,long){return 0;}
template<class Assets>auto voxelRevision(const Assets&a,int)->decltype(a.voxelLightingRevision(),a.voxelLightingAvailable(),std::pair<bool,uint64_t>{}){return {a.voxelLightingAvailable(),a.voxelLightingRevision()};}
template<class Assets>std::pair<bool,uint64_t> voxelRevision(const Assets&,long){return {false,0};}
template<class Reader,class Assets>class Renderer {
public:
 Reader& reader;Assets& assets;std::function<bool(uint32_t,void*,size_t)> read;
 Renderer(Reader&r,Assets&a):reader(r),assets(a),read([&r](uint32_t p,void*out,size_t n){return r.read(p,out,n);}){}
 bool enabled=true,danger=false,revealMap=false,suppressed=false;int width=800,height=600;

struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false,reveal=false;int sampleFrame=-1;uint32_t tick=0,revision=0;
 std::unique_ptr<assist::visual::Motion> motion;std::map<uint32_t,assist::visual::LabelView> positions;mutable external::camera::Surface raster;mutable external::watch::shroudmask::Raster shroudRaster;};
external::watch::Track tracks[external::watch::MaximumWindows];
View views[external::watch::MaximumWindows];
external::watch::Target automatic;bool autoSuppressed=false;
struct Cached {external::voxel::Image image;DWORD retry=0;uint64_t used=0;bool ready=false;std::string issue;};
using ArtKey=std::tuple<uint32_t,uint32_t,int,int,uint32_t,uint32_t>;
using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int,int,int,int,uint32_t,uint32_t,uint64_t,int,int,int>;
struct ArtCached {typename Assets::ActorArt art;DWORD retry=0;bool ready=false;std::string issue;};
std::map<ArtKey,ArtCached> actorArt;
std::map<ImageKey,Cached> images;
std::map<std::pair<uint32_t,uint32_t>,ImageKey> displayed;
size_t actorCursor=0;DWORD diagnosticAt=0;uint64_t assetRevision=0;
uint64_t voxelLightRevision=0;bool voxelLightKnown=false;
ArtKey artKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.pose.variant,a.kind>=5?a.target.pointer:0,a.kind>=5?a.target.id:0};}
ImageKey imageKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.kind==4?0:a.pose.body,a.kind==4?0:a.pose.turret,a.kind==4?a.stage:a.pose.frame,a.pose.variant,a.pose.buildingFlags,a.pose.doorFrame,a.kind>=5?a.target.pointer:0,a.kind>=5?a.target.id:0,external::visual::postureSignature(a.pose),a.pose.turretFrame,a.pose.bodyFine,a.pose.turretFine};}
static ArtKey imageArtKey(const ImageKey&k){return {std::get<0>(k),std::get<1>(k),std::get<2>(k),std::get<6>(k),std::get<9>(k),std::get<10>(k)};}
void discardPaletteImages(const ArtKey&key){
 for(auto i=images.begin();i!=images.end();)if(imageArtKey(i->first)==key)i=images.erase(i);else ++i;
 for(auto i=displayed.begin();i!=displayed.end();)if(imageArtKey(i->second)==key)i=displayed.erase(i);else ++i;
}
std::map<std::pair<uint32_t,uint32_t>,Cached> tiles,overlays;
std::array<uint32_t,256> terrainPalette{};bool terrainPaletteEverReady=false,terrainPaletteUnavailable=false;
external::lighting::State sceneLight;
external::repairvisuals::Assets repairAssets;
uint64_t serial=0;DWORD process=0;uint32_t epoch=0;bool shown=false;
uint32_t controlRevision=0;bool haveControls=false;
// Depth, shadows and sprite coverage are resolved at native game pixels.
// The immutable result is reduced only when the actual window is presented.
static constexpr int Width=external::watch::NativeViewWidth,Height=external::watch::NativeViewHeight;
void reset(){for(auto&t:tracks)t={};for(auto&v:views)v={};automatic={};autoSuppressed=false;haveControls=false;voxelLightKnown=false;images.clear();displayed.clear();actorArt.clear();actorCursor=0;diagnosticAt=0;tiles.clear();overlays.clear();terrainPalette={};terrainPaletteEverReady=terrainPaletteUnavailable=false;repairAssets.reset();shown=false;}
void syncTracks(uint32_t revision,const std::array<external::watch::Track,3>&next,bool suppress){
 // Pending snapshots may predate the worker's death/succession decision. Only
 // a new user control revision can replace that authoritative tracking state.
 if(haveControls&&controlRevision==revision)return;
 std::copy(next.begin(),next.end(),tracks);autoSuppressed=suppress;controlRevision=revision;haveControls=true;
}
void arrange(){bool visible[3]{};external::watch::Box boxes[3]{};bool wasShown=shown;shown=false;for(int i=0;i<3;++i)shown|=visible[i]=views[i].shown;external::watch::arrange(visible,width,height,boxes);bool changed=wasShown!=shown;for(int i=0;i<3;++i){auto&a=views[i].box;auto b=boxes[i];changed|=a.x!=b.x||a.y!=b.y||a.w!=b.w||a.h!=b.h;a=b;}(void)changed;}
void refreshAssets(external::watch::Frame&frame,DWORD now){
 assets.identity(process,epoch);beginPalettePass(assets,0);
 if(assetRevision!=assets.revision()){assetRevision=assets.revision();voxelLightKnown=false;images.clear();displayed.clear();actorArt.clear();tiles.clear();overlays.clear();terrainPalette={};for(auto&view:views)view.shroudRaster.reset();actorCursor=0;}
 // Only palette-dependent poses are discarded. Keep captured VXL/SHP
 // geometry, but never display its old colors during an unreadable update.
 std::set<ArtKey> unavailablePalettes;
 for(auto&entry:actorArt)if(entry.second.ready){bool changed=false;
  bool valid=refreshPalette(assets,entry.second.art,changed,0);
  if(changed||!valid)discardPaletteImages(entry.first);
  if(!valid)unavailablePalettes.insert(entry.first);
 }
 std::set<uint32_t> unavailableOverlays;
 std::set<std::pair<uint32_t,int>> checkedOverlays,checkedTerrain;
 for(const auto&t:frame.tiles)if(t.overlayType&&checkedOverlays.insert({t.overlayType,t.overlayFrame}).second){bool changed=false;uint32_t pointer=0;read(t.overlayType+0xA4,&pointer,4);
  bool valid=refreshOverlay(assets,t.overlayType,pointer,t.overlayFrame,false,changed,0);
  if(changed||!valid)for(auto i=overlays.begin();i!=overlays.end();)if(i->first.first==t.overlayType)i=overlays.erase(i);else ++i;
  if(!valid)unavailableOverlays.insert(t.overlayType);
 }
 std::set<uint32_t> unavailableTerrain;
 for(const auto&a:frame.actors)if(a.kind==4&&checkedTerrain.insert({a.type,a.stage}).second){bool changed=false;uint32_t pointer=0;read(a.type+0xA4,&pointer,4);
  bool valid=refreshOverlay(assets,a.type,pointer,a.stage,true,changed,0);
  if(changed||!valid){for(auto i=images.begin();i!=images.end();)if(std::get<0>(i->first)==a.type&&std::get<2>(i->first)==4)i=images.erase(i);else ++i;
   for(auto i=displayed.begin();i!=displayed.end();)if(std::get<0>(i->second)==a.type&&std::get<2>(i->second)==4)i=displayed.erase(i);else ++i;}
  if(!valid)unavailableTerrain.insert(a.type);
 }
 sceneLight=frame.light;
 std::array<uint32_t,256> palette{};bool paletteRead=external::terrain::palette(read,0xABBED0,palette)||assets.palette(true,palette);
 if(paletteRead){terrainPaletteEverReady=true;terrainPaletteUnavailable=false;
  if(palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(std::get<2>(it->first)==4)it=images.erase(it);else ++it;}
 }else if(terrainPaletteEverReady){
  // A previously verified live source disappearing must not silently retain
  // old colored terrain or certify a coarse placeholder as its replacement.
  terrainPaletteUnavailable=true;terrainPalette={};tiles.clear();overlays.clear();
 }

 bool paletteReady=std::any_of(terrainPalette.begin(),terrainPalette.end(),[](uint32_t c){return c!=0;});std::string paletteIssue=paletteReady?"":assets.failure;
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;std::set<std::pair<uint32_t,uint32_t>> needed;
  for(const auto&t:frame.tiles){uint32_t p=tmp?t.type:t.overlayType;auto key=std::make_pair(p,uint32_t(tmp?t.sub:t.overlayFrame));if(p&&needed.insert(key).second)wanted.push_back(key);}
  if(cache.size()>2048){for(auto it=cache.begin();it!=cache.end();)if(!needed.count(it->first))it=cache.erase(it);else ++it;}
  size_t pixels=0;for(const auto&item:cache)pixels+=external::camera::pixels(item.second.image);if(pixels>3*1024*1024){for(auto i=cache.begin();i!=cache.end()&&pixels>2*1024*1024;)if(!needed.count(i->first)){pixels-=external::camera::pixels(i->second.image);i=cache.erase(i);}else ++i;}int work=0;
  for(auto key:wanted){if(!tmp&&unavailableOverlays.count(key.first))continue;auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
   uint32_t pointer=0;read(key.first+0xA4,&pointer,4);
   bool ok=tmp?assets.tile(key.first,key.second,terrainPalette,image):assets.overlay(key.first,pointer,int(key.second),image);
   std::string memoryReason;
   if(!ok&&tmp)ok=external::terrain::tile(read,pointer,key.second,terrainPalette,image,&memoryReason);
   c.issue=ok?"":assets.failure+(memoryReason.empty()?"":" memory="+memoryReason);
   if(ok&&pixels+external::camera::pixels(image)<=4*1024*1024){pixels+=external::camera::pixels(image);external::camera::opaque(image);c.image=std::move(image);c.ready=true;}
  }
 };if(paletteReady)fetchStatic(tiles,true);fetchStatic(overlays,false);
 std::set<ArtKey> artWanted;std::set<ImageKey> wanted;
 for(const auto&a:frame.actors){artWanted.insert(artKey(a));wanted.insert(imageKey(a));}
 if(actorArt.size()>512){for(auto it=actorArt.begin();it!=actorArt.end();)if(!artWanted.count(it->first))it=actorArt.erase(it);else ++it;}
 // Keep each actor's last real rendered pose until its new frame is ready.
 // Animated SHP frames otherwise vanish every time the work budget is full.
 std::map<std::pair<uint32_t,uint32_t>,ImageKey> visible;for(const auto&a:frame.actors)visible[{a.target.pointer,a.target.id}]=imageKey(a);
 for(auto it=displayed.begin();it!=displayed.end();){auto current=visible.find(it->first);
  if(current==visible.end()||std::get<0>(current->second)!=std::get<0>(it->second)||std::get<1>(current->second)!=std::get<1>(it->second)||std::get<2>(current->second)!=std::get<2>(it->second)||std::get<6>(current->second)!=std::get<6>(it->second))it=displayed.erase(it);
  else{wanted.insert(it->second);++it;}}

 if(images.size()>2048){for(auto it=images.begin();it!=images.end();)if(!wanted.count(it->first))it=images.erase(it);else ++it;}
 // Fair work queue across every sampled unit, with no selected-unit filter or
 // 32-type cap. A failed first model cannot starve the remaining viewport.
 size_t pixels=0,faces=0,artBytes=0;for(const auto&i:images)pixels+=external::camera::pixels(i.second.image);for(const auto&i:actorArt){faces+=i.second.art.model.faces.size();artBytes+=Assets::cost(i.second.art);}
 if(pixels>24*1024*1024){for(auto i=images.begin();i!=images.end()&&pixels>16*1024*1024;)if(!wanted.count(i->first)){pixels-=external::camera::pixels(i->second.image);i=images.erase(i);}else ++i;}
 if(artBytes>96*1024*1024){for(auto i=actorArt.begin();i!=actorArt.end()&&artBytes>64*1024*1024;)if(!artWanted.count(i->first)){artBytes-=Assets::cost(i->second.art);faces-=i->second.art.model.faces.size();i=actorArt.erase(i);}else ++i;}
 // Reuse an identical pose only while its current VPL signature is verified.
 // Unreadable/unknown lighting keeps the conservative per-pass redraw path.
 std::set<ArtKey> liveVoxelArt;
 for(const auto&source:actorArt)if(source.second.ready&&isVoxelArt(source.second.art,0))liveVoxelArt.insert(source.first);
 const auto light=voxelRevision(assets,0);const bool lightChanged=!light.first||!voxelLightKnown||voxelLightRevision!=light.second;
 voxelLightKnown=light.first;voxelLightRevision=light.second;
 if(lightChanged)for(const auto&key:liveVoxelArt)discardPaletteImages(key);
 pixels=0;for(const auto&i:images)pixels+=external::camera::pixels(i.second.image);
 std::set<ImageKey> voxelDrawn;size_t liveFaces=0,livePoses=0;
 for(const auto&a:frame.actors){auto key=artKey(a);if(!liveVoxelArt.count(key)||unavailablePalettes.count(key)||!voxelDrawn.insert(imageKey(a)).second)continue;
  auto cached=images.find(imageKey(a));if(cached!=images.end()&&cached->second.ready&&!cached->second.image.incomplete){cached->second.used=++serial;continue;}
  auto source=actorArt.find(key);size_t facesForPose=voxelFaces(source->second.art,0);
  // Hard per-presentation work bounds. Old RGBA was already discarded, so
  // excess actors stay hidden rather than retaining obsolete light/poses.
  if(livePoses>=256||facesForPose>2*1024*1024-liveFaces)continue;
  ++livePoses;liveFaces+=facesForPose;external::voxel::Image image;
  if(!assets.renderActor(source->second.art,a.pose,image))continue;
  size_t count=external::camera::pixels(image);if(pixels+count>32*1024*1024)continue;
  pixels+=count;external::camera::opaque(image);auto&c=images[imageKey(a)];c.image=std::move(image);c.ready=true;c.used=++serial;c.issue=c.image.incomplete?assets.failure:"";
 }
 int work=0;DWORD started=GetTickCount();size_t n=frame.actors.size();
 for(size_t scanned=0;scanned<n;++scanned){
  const auto&a=frame.actors[actorCursor++%n];if(liveVoxelArt.count(artKey(a)))continue;if(unavailablePalettes.count(artKey(a))||(a.kind==4&&unavailableTerrain.count(a.type)))continue;auto&c=images[imageKey(a)];c.used=++serial;
  if((c.ready&&!c.image.incomplete)||(c.retry&&int32_t(now-c.retry)<0))continue;
  if(work>=512||(work&&DWORD(GetTickCount()-started)>=6))break;
  ++work;external::voxel::Image image;bool ok=false;
  if(a.kind==4){uint32_t shp=0;read(a.type+0xA4,&shp,4);ok=assets.overlay(a.type,shp,a.stage,image,true);}
  else{
   auto&source=actorArt[artKey(a)];
   if(!source.ready&&(!source.retry||int32_t(now-source.retry)>=0)){
    source.retry=now+2000;typename Assets::ActorArt art;
    if((a.kind==external::projectiles::Kind?external::projectiles::captureArt(assets,a.target.pointer,a.type,a.target.id,art):a.kind==5?assets.captureAnimation(a.target.pointer,a.type,a.target.id,a.parent.pointer,a.parent.id,art):assets.captureActor(a.target.pointer,a.type,a.target.id,art))&&faces+art.model.faces.size()<=4*1024*1024&&artBytes+Assets::cost(art)<=128*1024*1024){faces+=art.model.faces.size();artBytes+=Assets::cost(art);source.art=std::move(art);source.ready=true;}source.issue=source.ready?"":assets.failure;
   }
   if(source.ready)ok=assets.renderActor(source.art,a.pose,image);else assets.failure=source.issue;
  }
  c.issue=ok&&!image.incomplete?"":assets.failure;c.retry=ok&&!image.incomplete?0:now+2000;if(!ok)continue;
  size_t previous=external::camera::pixels(c.image),next=external::camera::pixels(image);
  if(pixels-previous+next>32*1024*1024){c.retry=now+2000;continue;}
  pixels=pixels-previous+next;
  external::camera::opaque(image);
  c.image=std::move(image);c.ready=true;
 }
 for(const auto&a:frame.actors){auto key=imageKey(a);auto i=images.find(key);if(i!=images.end()&&i->second.ready)displayed[{a.target.pointer,a.target.id}]=key;}

}
std::unique_ptr<assist::Snapshot> sample_=std::make_unique<assist::Snapshot>();
std::array<assist::visual::LabelView,assist::MaxLabels> motionOutput_{};
void updateMotion(View&view,uint32_t now,int frame){
 auto&sample=*sample_;auto*output=motionOutput_.data();
 if(!view.motion)view.motion=std::make_unique<assist::visual::Motion>();
 sample.match=epoch;sample.frame=frame;sample.sampledTick=now;sample.cameraX=sample.cameraY=0;
 sample.labels=std::min<int>(assist::MaxLabels,view.frame.actors.size());
 for(int i=0;i<sample.labels;++i){const auto&a=view.frame.actors[i];sample.label[i]={a.target.id,int((int64_t(a.x)-a.y)*30/256),int((int64_t(a.x)+a.y)*15/256)-external::screenHeight(a.z,view.frame.zScale)+external::camera::anchorY(a.kind)};}
 view.motion->update(sample,frame);int n=view.motion->render(now,output);view.positions.clear();for(int i=0;i<n;++i)view.positions[output[i].id]=output[i];view.tick=now;
}
void update(uint32_t now){
 bool active=enabled;
 if(!active){for(auto&v:views)v.shown=false;shown=false;return;}
 int frame=0;read(0xA8ED84,&frame,4);bool reveal=revealMap;
 external::watch::Frame resources;resources.tiles.clear();resources.actors.clear();bool haveManual=false;
 auto lifeRead=[this](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);};
 for(auto&t:tracks){bool had=bool(t);external::watch::follow(lifeRead,t);if(had&&!t)autoSuppressed=true;haveManual|=bool(t);}
 if(!haveManual&&!autoSuppressed)automatic=external::watch::choose(read,{},automatic,danger);else automatic={};
 for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
  bool targetChanged=!(target==v.frame.focus);
  if(targetChanged){v.frame={};v.positions.clear();if(v.motion)v.motion->clear();v.sampleFrame=-1;}
  // Window occupancy follows the tracking lifetime, not whether this one read
  // produced a picture. Commit only a complete candidate; refresh can mutate
  // actors before discovering that a later read requires a full resample.
  v.shown=bool(target);bool sampled=false;
  if(target){
   external::watch::Frame next;
   if(!targetChanged&&frame==v.sampleFrame&&reveal==v.reveal){next=v.frame;sampled=external::watch::refresh(read,next);}
   if(!sampled){auto center=v.frame.center;if(tracks[i]&&tracks[i].located)center={tracks[i].spatial,tracks[i].x,tracks[i].y,tracks[i].z,true,tracks[i].spatialKind,tracks[i].discoveryFrame,tracks[i].searched,tracks[i].discoveryCursor};next=external::watch::sample(read,target,reveal,20,center);sampled=next.focus==target;}
   if(sampled){v.frame=std::move(next);v.sampleFrame=frame;v.reveal=reveal;for(auto&a:v.frame.actors)if(a.kind==3)assets.buildingPose(a.target.pointer,a.type,a.pose);updateMotion(v,now,frame);++v.revision;}
  }else{v.positions.clear();if(v.motion)v.motion->clear();}
  if(v.shown&&v.frame.focus){resources.light=v.frame.light;resources.tiles.insert(resources.tiles.end(),v.frame.tiles.begin(),v.frame.tiles.end());resources.actors.insert(resources.actors.end(),v.frame.actors.begin(),v.frame.actors.end());}
 }
 arrange();if(shown)refreshAssets(resources,now);
}
void drawView(Graphics&g,const View&view){
 const auto&frame=view.frame;
 if(!view.shown)return;auto state=g.Save();g.SetClip(RectF(0,0,float(Width),float(Height)),CombineModeIntersect);
 SolidBrush bg(terrainPaletteUnavailable?Color(255,0,0,0):Color(255,8,12,8));g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));
 if(terrainPaletteUnavailable){view.raster.reset(Width,Height,0xff000000u);g.Restore(state);return;}

 // Overscan the diamond map in world space and crop only at the rectangular
 // viewport. There is no separate title, status/footer or fixed 11x11 diamond.
 const float ox=Width/2.f,oy=Height/2.f,scale=1.f;int side=frame.side,radius=frame.radius,centerLevel=frame.tiles[radius*side+radius].level;
 auto position=[&](const external::watch::Actor&a){auto i=view.positions.find(a.target.id);if(i!=view.positions.end())return std::pair<float,float>{i->second.x,i->second.y};return std::pair<float,float>{float((int64_t(a.x)-a.y)*30/256),float((int64_t(a.x)+a.y)*15/256-external::screenHeight(a.z,frame.zScale)+external::camera::anchorY(a.kind))};};
 float cameraX=(frame.cx-frame.cy)*30.f,cameraY=(frame.cx+frame.cy+1)*15.f-centerLevel*15.f;
 if(frame.cameraReady){cameraX=float(frame.cameraX);cameraY=float(frame.cameraY);}
 // The camera and units use the same interpolated position. Crossing a cell
 // boundary must not jump the entire second view by one tile.
 for(const auto&a:frame.actors)if(a.target==frame.center.target){auto p=position(a);cameraX=p.first;cameraY=p.second;break;}
 // Follow the LIVE native visible rectangle, including mission-trigger
 // changes. Small maps settle at their midpoint instead of alternating ends.
 double boundedX=cameraX,boundedY=cameraY;
 if(frame.bounds.clampCamera(boundedX,boundedY,Width/scale,Height/scale)){cameraX=float(boundedX);cameraY=float(boundedY);}
 auto&canvas=view.raster;canvas.reset(Width,Height);
 std::vector<int> tileOrder;tileOrder.reserve(side*side);for(int n=0;n<side*side;++n)if(frame.cells[n])tileOrder.push_back(n);
 std::stable_sort(tileOrder.begin(),tileOrder.end(),[&](int a,int b){return a%side+a/side<b%side+b/side;});
 for(int n:tileOrder){int x=frame.cx+n%side-radius,y=frame.cy+n/side-radius;const auto&t=frame.tiles[n];
  float px=ox+((x-y)*30-cameraX)*scale,py=oy+((x+y+1)*15-t.level*15-cameraY)*scale,base=float((x+y)*15);
  if(!frame.cells[n]||px<-256||px>Width+256||py<-256||py>Height+256)continue;auto found=tiles.find({t.type,uint32_t(t.sub)});
  if(found!=tiles.end()&&found->second.ready)canvas.blit(found->second.image,px,py,scale,scale,base,t.light,0);
  else canvas.diamond(px,py,scale,scale,base,frame.cells[n],t.light);
  auto over=overlays.find({t.overlayType,uint32_t(t.overlayFrame)});
  if(over!=overlays.end()&&over->second.ready)canvas.blit(over->second.image,px,py+t.overlayY*scale,scale,scale,base+(t.bridge?60:0),t.overlayLight,2);
 }
 std::vector<const external::watch::Actor*> order;for(const auto&a:frame.actors)order.push_back(&a);
 std::stable_sort(order.begin(),order.end(),[](const auto*a,const auto*b){return external::camera::orderKey(a->layer,a->kind,a->x,a->y,a->sort)<external::camera::orderKey(b->layer,b->kind,b->x,b->y,b->sort);});
 float focusX=-100,focusY=-100;
 for(const auto*actor:order){const auto&a=*actor;int cx=a.x/256-frame.cx+radius,cy=a.y/256-frame.cy+radius;if(cx<0||cy<0||cx>=side||cy>=side)continue;
  auto p=position(a);float x=ox+(p.first-cameraX)*scale,y=oy+(p.second-cameraY)*scale;
  auto found=images.find(imageKey(a));if(found==images.end()||!found->second.ready){auto old=displayed.find({a.target.pointer,a.target.id});if(old!=displayed.end())found=images.find(old->second);}
  if(found!=images.end()&&found->second.ready)canvas.blit(found->second.image,x,y,scale,scale,float((int64_t(a.x)+a.y)*15/256.)-15+external::camera::anchorY(a.kind)-a.drawZ+(a.onBridge?60:0),a.light,a.kind==4?12:(a.kind==3||a.kind==5)?2:1,
    a.pose.shadow.known?float(a.pose.shadow.drop):float(std::max(0,external::screenHeight(a.z,frame.zScale)-(frame.tiles[cy*side+cx].level+(a.onBridge?4:0))*15)));
  if(a.target==frame.center.target){focusX=x;focusY=y;}
 }
 // Repair wrenches are native status sprites, not AnimClass world objects.
 // Their cursor palette and no-Z draw follow all terrain/unit composition.
 for(const auto&sprite:frame.repairs)if(auto image=repairAssets.image(assets,sprite)){
  float x=ox+((int64_t(sprite.x)-sprite.y)*30.f/256-cameraX)*scale;
  float y=oy+((int64_t(sprite.x)+sprite.y)*15.f/256-external::screenHeight(sprite.z,frame.zScale)-cameraY)*scale;
  canvas.blit(*image,x,y,scale,scale,0,external::lighting::State{});
 }
 // Match the game's alpha buffer after composing all world pixels, before
 // drawing the watch UI. This also covers raised TMP edges and tall actors.
 if constexpr(HasShroud<Assets>::value){if(!frame.reveal&&
    !view.shroudRaster.apply(frame,cameraX,cameraY,Width,Height,canvas.color,
                       [&](bool fog,int index,external::voxel::Image&image){return assets.shroud(fog,index,image);})){std::fill(canvas.color.begin(),canvas.color.end(),0xff000000u);}}
 // Camera limits constrain its center, not drawable map scenery. Surface
 // rasterization already clips the scene to this rectangular viewport.
 if(!canvas.color.empty()){Bitmap bitmap(Width,Height,Width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(canvas.color.data()));
  g.DrawImage(&bitmap,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(Width),float(Height),UnitPixel,nullptr);}
 if(focusX>-100){Pen ring(Color(255,255,224,111),2.6f);g.DrawEllipse(&ring,focusX-20,focusY-10,40.f,20.f);}

 Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),6.f);g.DrawRectangle(&border,3.f,3.f,float(Width-6),float(Height-6));g.Restore(state);
}
};
}


#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/terrain_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include "label_motion.h"
#include "../../external_reader/source/camera_raster.h"
#include <memory>
#include <tuple>
#include <functional>
namespace watchengine {
template<class Reader,class Assets>class Renderer {
public:
 Reader& reader;Assets& assets;std::function<bool(uint32_t,void*,size_t)> read;
 Renderer(Reader&r,Assets&a):reader(r),assets(a),read([&r](uint32_t p,void*out,size_t n){return r.read(p,out,n);}){}
 bool enabled=true,danger=false,revealMap=false,suppressed=false;int width=800,height=600;

struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false,reveal=false;int sampleFrame=-1;uint32_t tick=0,revision=0;
 std::unique_ptr<assist::visual::Motion> motion;std::map<uint32_t,assist::visual::LabelView> positions;mutable external::camera::Surface raster;};
external::watch::Track tracks[external::watch::MaximumWindows];
View views[external::watch::MaximumWindows];
external::watch::Target automatic;bool autoSuppressed=false;
struct Cached {external::voxel::Image image;DWORD retry=0;uint64_t used=0;bool ready=false;std::string issue;};
using ArtKey=std::tuple<uint32_t,uint32_t,int,int>;
using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int,int>;
struct ArtCached {typename Assets::ActorArt art;DWORD retry=0;bool ready=false;std::string issue;};
std::map<ArtKey,ArtCached> actorArt;
std::map<ImageKey,Cached> images;
std::map<std::pair<uint32_t,uint32_t>,ImageKey> displayed;
size_t actorCursor=0;DWORD diagnosticAt=0;uint64_t assetRevision=0;
ArtKey artKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.pose.variant};}
ImageKey imageKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.kind==4?0:a.pose.body,a.kind==4?0:a.pose.turret,a.kind==4?a.stage:a.pose.frame,a.pose.variant};}
std::map<std::pair<uint32_t,uint32_t>,Cached> tiles,overlays;
std::array<uint32_t,256> terrainPalette{};
external::lighting::State sceneLight;
uint64_t serial=0;DWORD process=0;uint32_t epoch=0;bool shown=false;
static constexpr int Width=320,Height=256;
void reset(){for(auto&t:tracks)t={};for(auto&v:views)v={};automatic={};autoSuppressed=false;images.clear();displayed.clear();actorArt.clear();actorCursor=0;diagnosticAt=0;tiles.clear();overlays.clear();terrainPalette={};shown=false;}
void arrange(){bool visible[3]{};external::watch::Box boxes[3]{};bool wasShown=shown;shown=false;for(int i=0;i<3;++i)shown|=visible[i]=views[i].shown;external::watch::arrange(visible,width,height,boxes);bool changed=wasShown!=shown;for(int i=0;i<3;++i){auto&a=views[i].box;auto b=boxes[i];changed|=a.x!=b.x||a.y!=b.y||a.w!=b.w||a.h!=b.h;a=b;}(void)changed;}
void refreshAssets(external::watch::Frame&frame,DWORD now){
 assets.identity(process,epoch);
 if(assetRevision!=assets.revision()){assetRevision=assets.revision();images.clear();displayed.clear();actorArt.clear();tiles.clear();overlays.clear();terrainPalette={};actorCursor=0;}
 sceneLight=frame.light;
 std::array<uint32_t,256> palette{};if((external::terrain::palette(read,0xABBED0,palette)||assets.palette(true,palette))&&palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(std::get<2>(it->first)==4)it=images.erase(it);else ++it;}
 bool paletteReady=std::any_of(terrainPalette.begin(),terrainPalette.end(),[](uint32_t c){return c!=0;});std::string paletteIssue=paletteReady?"":assets.failure;
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;for(const auto&t:frame.tiles){uint32_t p=tmp?t.type:t.overlayType;if(p)wanted.emplace_back(p,uint32_t(tmp?t.sub:t.overlayFrame));}
  if(cache.size()>2048){for(auto it=cache.begin();it!=cache.end();)if(std::find(wanted.begin(),wanted.end(),it->first)==wanted.end())it=cache.erase(it);else ++it;}
  size_t pixels=0;for(const auto&item:cache)pixels+=external::camera::pixels(item.second.image);if(pixels>3*1024*1024){for(auto i=cache.begin();i!=cache.end()&&pixels>2*1024*1024;)if(std::find(wanted.begin(),wanted.end(),i->first)==wanted.end()){pixels-=external::camera::pixels(i->second.image);i=cache.erase(i);}else ++i;}int work=0;
  for(auto key:wanted){auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
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
 int work=0;DWORD started=GetTickCount();size_t n=frame.actors.size();
 for(size_t scanned=0;scanned<n;++scanned){
  const auto&a=frame.actors[actorCursor++%n];auto&c=images[imageKey(a)];c.used=++serial;
  if(c.ready||(c.retry&&int32_t(now-c.retry)<0))continue;
  if(work>=16||(work&&DWORD(GetTickCount()-started)>=6))break;
  ++work;external::voxel::Image image;bool ok=false;
  if(a.kind==4){uint32_t shp=0;read(a.type+0xA4,&shp,4);ok=assets.overlay(a.type,shp,a.stage,image,true);}
  else{
   auto&source=actorArt[artKey(a)];
   if(!source.ready&&(!source.retry||int32_t(now-source.retry)>=0)){
    source.retry=now+2000;typename Assets::ActorArt art;
    if((a.kind==5?assets.captureAnimation(a.target.pointer,a.type,a.target.id,a.parent.pointer,a.parent.id,art):assets.captureActor(a.target.pointer,a.type,a.target.id,art))&&faces+art.model.faces.size()<=4*1024*1024&&artBytes+Assets::cost(art)<=128*1024*1024){faces+=art.model.faces.size();artBytes+=Assets::cost(art);source.art=std::move(art);source.ready=true;}source.issue=source.ready?"":assets.failure;
   }
   if(source.ready)ok=assets.renderActor(source.art,a.pose,image);else assets.failure=source.issue;
  }
  c.issue=ok?"":assets.failure;c.retry=ok?0:now+2000;if(!ok)continue;
  if(pixels+external::camera::pixels(image)>32*1024*1024){c.retry=now+2000;continue;}
  pixels+=image.pixels.size();
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
 for(auto&t:tracks){bool had=bool(t);external::watch::follow(read,t);if(had&&!t)autoSuppressed=true;haveManual|=bool(t);}
 if(!haveManual&&!autoSuppressed)automatic=external::watch::choose(read,{},automatic,danger);else automatic={};
 for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
  bool targetChanged=!(target==v.frame.focus);
  if(targetChanged||frame!=v.sampleFrame||reveal!=v.reveal||!external::watch::refresh(read,v.frame))v.frame=external::watch::sample(read,target,reveal,20);
  if(targetChanged&&v.motion)v.motion->clear();v.sampleFrame=frame;v.reveal=reveal;v.shown=bool(v.frame.focus);
  if(v.shown){for(auto&a:v.frame.actors)if(a.kind==3)a.pose.frame=assets.buildingFrame(a.target.pointer,a.type);updateMotion(v,now,frame);++v.revision;}else{v.positions.clear();if(v.motion)v.motion->clear();}
  if(v.shown){resources.light=v.frame.light;resources.tiles.insert(resources.tiles.end(),v.frame.tiles.begin(),v.frame.tiles.end());resources.actors.insert(resources.actors.end(),v.frame.actors.begin(),v.frame.actors.end());}
 }
 arrange();if(shown)refreshAssets(resources,now);
}
void drawView(Graphics&g,const View&view){
 const auto&frame=view.frame;
 if(!view.shown)return;auto state=g.Save();g.SetClip(RectF(0,0,float(Width),float(Height)),CombineModeIntersect);
 SolidBrush bg(Color(255,8,12,8));g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));
 // Overscan the diamond map in world space and crop only at the rectangular
 // viewport. There is no separate title, status/footer or fixed 11x11 diamond.
 const float ox=Width/2.f,oy=Height/2.f,scale=.5f;int side=frame.side,radius=frame.radius,centerLevel=frame.tiles[radius*side+radius].level;
 auto position=[&](const external::watch::Actor&a){auto i=view.positions.find(a.target.id);if(i!=view.positions.end())return std::pair<float,float>{i->second.x,i->second.y};return std::pair<float,float>{float((int64_t(a.x)-a.y)*30/256),float((int64_t(a.x)+a.y)*15/256-external::screenHeight(a.z,frame.zScale)+external::camera::anchorY(a.kind))};};
 float cameraX=(frame.cx-frame.cy)*30.f,cameraY=(frame.cx+frame.cy+1)*15.f-centerLevel*15.f;
 // The camera and units use the same interpolated position. Crossing a cell
 // boundary must not jump the entire second view by one tile.
 for(const auto&a:frame.actors)if(a.target==frame.focus){auto p=position(a);cameraX=p.first;cameraY=p.second;break;}
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
    float(std::max(0,external::screenHeight(a.z,frame.zScale)-(frame.tiles[cy*side+cx].level+(a.onBridge?4:0))*15)));
  if(a.target==frame.focus){focusX=x;focusY=y;}
 }
 if(!canvas.color.empty()){Bitmap bitmap(Width,Height,Width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(canvas.color.data()));
  g.DrawImage(&bitmap,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(Width),float(Height),UnitPixel,nullptr);}
 if(focusX>-100){Pen ring(Color(255,255,224,111),1.3f);g.DrawEllipse(&ring,focusX-10,focusY-5,20.f,10.f);}

 Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),3.f);g.DrawRectangle(&border,1.5f,1.5f,float(Width-3),float(Height-3));g.Restore(state);
}
};
}

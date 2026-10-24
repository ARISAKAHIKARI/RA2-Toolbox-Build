#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/terrain_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include "label_motion.h"
#include "../../external_reader/source/camera_raster.h"
#include <memory>
#include <tuple>
namespace watchhost {
struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false,reveal=false;int sampleFrame=-1;uint32_t tick=0,revision=0;
 std::unique_ptr<assist::visual::Motion> motion;std::map<uint32_t,assist::visual::LabelView> positions;mutable external::camera::Surface raster;};
static external::watch::Track tracks[external::watch::MaximumWindows];
static View views[external::watch::MaximumWindows];
static external::watch::Target automatic;static bool autoSuppressed=false;
struct Cached {external::voxel::Image image;DWORD retry=0;uint64_t used=0;bool ready=false;std::string issue;};
using ArtKey=std::tuple<uint32_t,uint32_t,int,int>;
using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int,int>;
struct ArtCached {gameassets::ActorArt art;DWORD retry=0;bool ready=false;std::string issue;};
static std::map<ArtKey,ArtCached> actorArt;
static std::map<ImageKey,Cached> images;
static std::map<std::pair<uint32_t,uint32_t>,ImageKey> displayed;
static size_t actorCursor=0;static DWORD diagnosticAt=0;
static ArtKey artKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.pose.variant};}
static ImageKey imageKey(const external::watch::Actor&a){return {a.type,a.owner,a.kind,a.kind==4?0:a.pose.body,a.kind==4?0:a.pose.turret,a.kind==4?a.stage:a.pose.frame,a.pose.variant};}
static std::map<std::pair<uint32_t,uint32_t>,Cached> tiles,overlays;
static std::array<uint32_t,256> terrainPalette{};
static external::lighting::State sceneLight;
static uint64_t serial=0;static DWORD process=0;static uint32_t epoch=0;static bool shown=false;
static constexpr int Width=320,Height=256;
static bool read(uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);}
static void reset(){for(auto&t:tracks)t={};for(auto&v:views)v={};automatic={};autoSuppressed=false;images.clear();displayed.clear();actorArt.clear();actorCursor=0;diagnosticAt=0;tiles.clear();overlays.clear();terrainPalette={};if(shown){shown=false;++paintRevision;}}
static void arrange(){bool visible[3]{};external::watch::Box boxes[3]{};bool wasShown=shown;shown=false;for(int i=0;i<3;++i)shown|=visible[i]=views[i].shown;external::watch::arrange(visible,snap.width,snap.height,boxes);bool changed=wasShown!=shown;for(int i=0;i<3;++i){auto&a=views[i].box;auto b=boxes[i];changed|=a.x!=b.x||a.y!=b.y||a.w!=b.w||a.h!=b.h;a=b;}if(changed)++paintRevision;}
static void close(int slot){if(slot<0||slot>=3)return;tracks[slot]={};views[slot]={};automatic={};autoSuppressed=true;arrange();++paintRevision;}
static void pin(uint32_t key){
 if(!enabled||!extra.watchEnabled||!foregroundGame()||snap.chat||formationInputOwns())return;
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 // Refresh dead slots before deciding whether this key's group is full.
 for(auto&t:tracks)external::watch::follow(read,t);
 int slot=-1;auto result=external::watch::add(tracks,extra.watchKeys,key,external::watch::capture(read),slot);
 using Result=external::watch::AddResult;
 if(result==Result::NoTarget){notify(L"请先选中单位，或将鼠标放到单位上。");return;}
 if(result==Result::Full){notify(L"此快捷键绑定的跟踪窗口已满；Shift＋右击窗口可关闭。");return;}
 if(result==Result::AlreadyTracked){notify(L"此单位已在该快捷键的窗口中跟踪。");return;}
 automatic={};autoSuppressed=true;++paintRevision;notify(L"已添加跟踪窗口 "+std::to_wstring(slot+1)+L"；左击跳转，Shift＋右击关闭。");
}
static void refreshAssets(external::watch::Frame&frame,DWORD now){
 gameassets::update();
 sceneLight=frame.light;
 std::array<uint32_t,256> palette{};if((external::terrain::palette(read,0xABBED0,palette)||gameassets::palette(true,palette))&&palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(std::get<2>(it->first)==4)it=images.erase(it);else ++it;}
 bool paletteReady=std::any_of(terrainPalette.begin(),terrainPalette.end(),[](uint32_t c){return c!=0;});std::string paletteIssue=paletteReady?"":gameassets::failure;
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;for(const auto&t:frame.tiles){uint32_t p=tmp?t.type:t.overlayType;if(p)wanted.emplace_back(p,uint32_t(tmp?t.sub:t.overlayFrame));}
  for(auto it=cache.begin();it!=cache.end();)if(std::find(wanted.begin(),wanted.end(),it->first)==wanted.end())it=cache.erase(it);else ++it;
  size_t pixels=0;for(const auto&item:cache)pixels+=external::camera::pixels(item.second.image);int work=0;
  for(auto key:wanted){auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
   uint32_t pointer=0;read(key.first+0xA4,&pointer,4);
   bool ok=tmp?gameassets::tile(key.first,key.second,terrainPalette,image):gameassets::overlay(key.first,pointer,int(key.second),image);
   std::string memoryReason;
   if(!ok&&tmp)ok=external::terrain::tile(read,pointer,key.second,terrainPalette,image,&memoryReason);
   c.issue=ok?"":gameassets::failure+(memoryReason.empty()?"":" memory="+memoryReason);
   if(ok&&pixels+external::camera::pixels(image)<=4*1024*1024){pixels+=external::camera::pixels(image);external::camera::opaque(image);c.image=std::move(image);c.ready=true;}
  }
 };if(paletteReady)fetchStatic(tiles,true);fetchStatic(overlays,false);
 std::set<ArtKey> artWanted;std::set<ImageKey> wanted;
 for(const auto&a:frame.actors){artWanted.insert(artKey(a));wanted.insert(imageKey(a));}
 for(auto it=actorArt.begin();it!=actorArt.end();)if(!artWanted.count(it->first))it=actorArt.erase(it);else ++it;
 // Keep each actor's last real rendered pose until its new frame is ready.
 // Animated SHP frames otherwise vanish every time the work budget is full.
 std::set<std::pair<uint32_t,uint32_t>> visible;for(const auto&a:frame.actors)visible.insert({a.target.pointer,a.target.id});
 for(auto it=displayed.begin();it!=displayed.end();)if(!visible.count(it->first))it=displayed.erase(it);else{wanted.insert(it->second);++it;}
 for(auto it=images.begin();it!=images.end();)if(!wanted.count(it->first))it=images.erase(it);else ++it;
 // Fair work queue across every sampled unit, with no selected-unit filter or
 // 32-type cap. A failed first model cannot starve the remaining viewport.
 size_t pixels=0,faces=0,artBytes=0;for(const auto&i:images)pixels+=external::camera::pixels(i.second.image);for(const auto&i:actorArt){faces+=i.second.art.model.faces.size();artBytes+=gameassets::artBytes(i.second.art);}
 int work=0;DWORD started=GetTickCount();size_t n=frame.actors.size();
 for(size_t scanned=0;scanned<n;++scanned){
  const auto&a=frame.actors[actorCursor++%n];auto&c=images[imageKey(a)];c.used=++serial;
  if(c.ready||(c.retry&&int32_t(now-c.retry)<0))continue;
  if(work>=16||(work&&DWORD(GetTickCount()-started)>=6))break;
  ++work;external::voxel::Image image;bool ok=false;
  if(a.kind==4){uint32_t shp=0;read(a.type+0xA4,&shp,4);ok=gameassets::overlay(a.type,shp,a.stage,image,true);}
  else{
   auto&source=actorArt[artKey(a)];
   if(!source.ready&&(!source.retry||int32_t(now-source.retry)>=0)){
    source.retry=now+2000;gameassets::ActorArt art;
    if((a.kind==5?gameassets::captureAnimation(a.target.pointer,a.type,a.target.id,art):gameassets::captureActor(a.target.pointer,a.type,a.target.id,art))&&faces+art.model.faces.size()<=4*1024*1024&&artBytes+gameassets::artBytes(art)<=128*1024*1024){faces+=art.model.faces.size();artBytes+=gameassets::artBytes(art);source.art=std::move(art);source.ready=true;}source.issue=source.ready?"":gameassets::failure;
   }
   if(source.ready)ok=gameassets::renderActor(source.art,a.pose,image);else gameassets::failure=source.issue;
  }
  c.issue=ok?"":gameassets::failure;c.retry=ok?0:now+2000;if(!ok)continue;
  if(pixels+external::camera::pixels(image)>32*1024*1024){c.retry=now+2000;continue;}
  pixels+=image.pixels.size();
  external::camera::opaque(image);
  c.image=std::move(image);c.ready=true;
 }
 for(const auto&a:frame.actors){auto key=imageKey(a);auto i=images.find(key);if(i!=images.end()&&i->second.ready)displayed[{a.target.pointer,a.target.id}]=key;}
 if(!diagnosticAt||DWORD(now-diagnosticAt)>=5000){
  diagnosticAt=now;size_t actorsReady=0,tileReady=0,overlayReady=0,overlayCount=0;std::string missing;
  for(const auto&a:frame.actors){auto i=images.find(imageKey(a));if(i!=images.end()&&i->second.ready)++actorsReady;else if(missing.empty())missing=gameassets::text(a.type+0x24,24);}
  for(const auto&t:frame.tiles){auto i=tiles.find({t.type,uint32_t(t.sub)});if(i!=tiles.end()&&i->second.ready)++tileReady;else if(missing.empty()&&t.type)missing=gameassets::text(t.type+0x2F5,14);}
  for(const auto&t:frame.tiles)if(t.overlayType){++overlayCount;auto i=overlays.find({t.overlayType,uint32_t(t.overlayFrame)});if(i!=overlays.end()&&i->second.ready)++overlayReady;}
  critical(L"watch assets archives="+std::to_wstring(gameassets::files.count())+L" actors="+std::to_wstring(actorsReady)+L"/"+std::to_wstring(frame.actors.size())+L" terrain="+std::to_wstring(tileReady)+L"/"+std::to_wstring(frame.tiles.size())+L" overlays="+std::to_wstring(overlayReady)+L"/"+std::to_wstring(overlayCount)+L" palette="+std::to_wstring(paletteReady)+L" missing="+std::wstring(missing.begin(),missing.end()));
  if(!paletteReady)critical(L"watch palette reason="+std::wstring(paletteIssue.begin(),paletteIssue.end()));
  int reported=0;for(const auto&a:frame.actors){auto i=images.find(imageKey(a));if(i==images.end()||i->second.ready)continue;auto name=gameassets::text(a.type+0x24,24),issue=i->second.issue;critical(L"watch actor="+std::wstring(name.begin(),name.end())+L" kind="+std::to_wstring(a.kind)+L" frame="+std::to_wstring(a.pose.frame)+L" reason="+std::wstring(issue.begin(),issue.end()));if(++reported==3)break;}
  reported=0;for(const auto&t:tiles)if(!t.second.ready){auto issue=t.second.issue;critical(L"watch tile type="+std::to_wstring(t.first.first)+L" sub="+std::to_wstring(t.first.second)+L" reason="+std::wstring(issue.begin(),issue.end()));if(++reported==3)break;}
  reported=0;for(const auto&t:overlays)if(!t.second.ready){auto issue=t.second.issue;critical(L"watch overlay type="+std::to_wstring(t.first.first)+L" frame="+std::to_wstring(t.first.second)+L" reason="+std::wstring(issue.begin(),issue.end()));if(++reported==3)break;}
 }

}
static void updateMotion(View&view,uint32_t now,int frame){
 static assist::Snapshot sample;static assist::visual::LabelView output[assist::MaxLabels];
 if(!view.motion)view.motion=std::make_unique<assist::visual::Motion>();
 sample.match=epoch;sample.frame=frame;sample.sampledTick=now;sample.cameraX=sample.cameraY=0;
 sample.labels=std::min<int>(assist::MaxLabels,view.frame.actors.size());
 for(int i=0;i<sample.labels;++i){const auto&a=view.frame.actors[i];sample.label[i]={a.target.id,int((int64_t(a.x)-a.y)*30/256),int((int64_t(a.x)+a.y)*15/256)-external::screenHeight(a.z,view.frame.zScale)};}
 view.motion->update(sample,frame);int n=view.motion->render(now,output);view.positions.clear();for(int i=0;i<n;++i)view.positions[output[i].id]=output[i];view.tick=now;
}
static void update(uint32_t now){
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 if(!extra.watchEnabled||!extra.watchDanger)autoSuppressed=false;
 bool active=enabled&&extra.watchEnabled&&!snap.chat&&snap.renderStatus==1&&snap.width>=360&&snap.height>=320;
 if(!active){for(auto&v:views)v.shown=false;if(shown){shown=false;++paintRevision;}return;}
 int frame=0;read(0xA8ED84,&frame,4);bool reveal=extra.renderReveal||extra.mapStrategy==3;
 external::watch::Frame resources;resources.tiles.clear();resources.actors.clear();bool haveManual=false;
 for(auto&t:tracks){bool had=bool(t);external::watch::follow(read,t);if(had&&!t)autoSuppressed=true;haveManual|=bool(t);}
 if(!haveManual&&!autoSuppressed)automatic=external::watch::choose(read,{},automatic,extra.watchDanger);else automatic={};
 for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
  bool targetChanged=!(target==v.frame.focus);
  if(targetChanged||frame!=v.sampleFrame||reveal!=v.reveal||!external::watch::refresh(read,v.frame))v.frame=external::watch::sample(read,target,reveal,20);
  if(targetChanged&&v.motion)v.motion->clear();v.sampleFrame=frame;v.reveal=reveal;v.shown=bool(v.frame.focus);
  if(v.shown){for(auto&a:v.frame.actors)if(a.kind==3)a.pose.frame=gameassets::buildingFrame(a.target.pointer,a.type);updateMotion(v,now,frame);++v.revision;}else{v.positions.clear();if(v.motion)v.motion->clear();}
  if(v.shown){resources.light=v.frame.light;resources.tiles.insert(resources.tiles.end(),v.frame.tiles.begin(),v.frame.tiles.end());resources.actors.insert(resources.actors.end(),v.frame.actors.begin(),v.frame.actors.end());}
 }
 arrange();if(shown)refreshAssets(resources,now);
}
static void drawView(Graphics&g,const View&view){
 const auto&frame=view.frame;
 if(!view.shown)return;auto state=g.Save();g.SetClip(RectF(0,0,float(Width),float(Height)),CombineModeIntersect);
 SolidBrush bg(Color(255,8,12,8));g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));
 // Overscan the diamond map in world space and crop only at the rectangular
 // viewport. There is no separate title, status/footer or fixed 11x11 diamond.
 const float ox=Width/2.f,oy=Height/2.f,scale=.5f;int side=frame.side,radius=frame.radius,centerLevel=frame.tiles[radius*side+radius].level;
 auto position=[&](const external::watch::Actor&a){auto i=view.positions.find(a.target.id);if(i!=view.positions.end())return std::pair<float,float>{i->second.x,i->second.y};return std::pair<float,float>{float((int64_t(a.x)-a.y)*30/256),float((int64_t(a.x)+a.y)*15/256-external::screenHeight(a.z,frame.zScale))};};
 float cameraX=(frame.cx-frame.cy)*30.f,cameraY=(frame.cx+frame.cy+1)*15.f-centerLevel*15.f;
 // The camera and units use the same interpolated position. Crossing a cell
 // boundary must not jump the entire second view by one tile.
 for(const auto&a:frame.actors)if(a.target==frame.focus){auto p=position(a);cameraX=p.first;cameraY=p.second;break;}
 auto&canvas=view.raster;canvas.reset(Width,Height);
 for(int n=0;n<side*side;++n){int x=frame.cx+n%side-radius,y=frame.cy+n/side-radius;const auto&t=frame.tiles[n];
  float px=ox+((x-y)*30-cameraX)*scale,py=oy+((x+y+1)*15-t.level*15-cameraY)*scale,base=float((x+y)*15);
  if(!frame.cells[n]||px<-256||px>Width+256||py<-256||py>Height+256)continue;auto found=tiles.find({t.type,uint32_t(t.sub)});
  if(found!=tiles.end()&&found->second.ready)canvas.blit(found->second.image,px,py,scale,scale,base,t.light,0);
  else canvas.diamond(px,py,scale,scale,base,frame.cells[n],t.light);
  auto over=overlays.find({t.overlayType,uint32_t(t.overlayFrame)});
  if(over!=overlays.end()&&over->second.ready)canvas.blit(over->second.image,px,py+t.overlayY*scale,scale,scale,base+(t.bridge?60:0),t.overlayLight,2);
 }
 std::vector<const external::watch::Actor*> order;for(const auto&a:frame.actors)order.push_back(&a);
 std::stable_sort(order.begin(),order.end(),[](const auto*a,const auto*b){return int64_t(a->x)+a->y<int64_t(b->x)+b->y;});
 float focusX=-100,focusY=-100;
 for(const auto*actor:order){const auto&a=*actor;int cx=a.x/256-frame.cx+radius,cy=a.y/256-frame.cy+radius;if(cx<0||cy<0||cx>=side||cy>=side)continue;
  auto p=position(a);float x=ox+(p.first-cameraX)*scale,y=oy+(p.second-cameraY)*scale;
  auto found=images.find(imageKey(a));if(found==images.end()||!found->second.ready){auto old=displayed.find({a.target.pointer,a.target.id});if(old!=displayed.end())found=images.find(old->second);}
  if(found!=images.end()&&found->second.ready)canvas.blit(found->second.image,x,y,scale,scale,float((int64_t(a.x)+a.y)*15/256.)-15-a.drawZ+(frame.tiles[cy*side+cx].bridge?60:0),a.light,a.kind==4?12:(a.kind==3||a.kind==5)?2:1,
    float(std::max(0,external::screenHeight(a.z,frame.zScale)-(frame.tiles[cy*side+cx].level+(frame.tiles[cy*side+cx].bridge?4:0))*15)));
  if(a.target==frame.focus){focusX=x;focusY=y;}
 }
 if(!canvas.color.empty()){Bitmap bitmap(Width,Height,Width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(canvas.color.data()));
  g.DrawImage(&bitmap,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(Width),float(Height),UnitPixel,nullptr);}
 if(focusX>-100){Pen ring(Color(255,255,224,111),1.3f);g.DrawEllipse(&ring,focusX-10,focusY-5,20.f,10.f);}

 Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),3.f);g.DrawRectangle(&border,1.5f,1.5f,float(Width-3),float(Height-3));g.Restore(state);
}
}

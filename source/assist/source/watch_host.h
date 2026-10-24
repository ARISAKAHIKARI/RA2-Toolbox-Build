#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/terrain_preview.h"
#include "../../external_reader/source/scene_lighting.h"
#include <tuple>
namespace watchhost {
struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false;};
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
static uint64_t serial=0;static DWORD last=0,process=0;static uint32_t epoch=0;static bool shown=false;
static constexpr int Width=320,Height=256;
static bool read(uint32_t p,void*out,size_t n){return externalReader.read(p,out,n);}
static void reset(){for(auto&t:tracks)t={};for(auto&v:views)v={};automatic={};autoSuppressed=false;images.clear();displayed.clear();actorArt.clear();actorCursor=0;diagnosticAt=0;tiles.clear();overlays.clear();terrainPalette={};last=0;if(shown){shown=false;++paintRevision;}}
static void arrange(){bool visible[3]{};external::watch::Box boxes[3]{};shown=false;for(int i=0;i<3;++i)shown|=visible[i]=views[i].shown;external::watch::arrange(visible,snap.width,snap.height,boxes);for(int i=0;i<3;++i)views[i].box=boxes[i];}
static void close(int slot){if(slot<0||slot>=3)return;tracks[slot]={};views[slot]={};automatic={};autoSuppressed=true;last=0;arrange();++paintRevision;}
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
 automatic={};autoSuppressed=true;last=0;++paintRevision;notify(L"已添加跟踪窗口 "+std::to_wstring(slot+1)+L"；左击跳转，Shift＋右击关闭。");
}
static void refreshAssets(external::watch::Frame&frame,DWORD now){
 gameassets::update();
 sceneLight=frame.light;
 std::array<uint32_t,256> palette{};if((external::terrain::palette(read,0xABBED0,palette)||gameassets::palette(true,palette))&&palette!=terrainPalette){terrainPalette=palette;tiles.clear();overlays.clear();for(auto it=images.begin();it!=images.end();)if(std::get<2>(it->first)==4)it=images.erase(it);else ++it;}
 bool paletteReady=std::any_of(terrainPalette.begin(),terrainPalette.end(),[](uint32_t c){return c!=0;});std::string paletteIssue=paletteReady?"":gameassets::failure;
 auto fetchStatic=[&](auto&cache,bool tmp){
  std::vector<std::pair<uint32_t,uint32_t>> wanted;for(const auto&t:frame.tiles){uint32_t p=tmp?t.type:t.overlayType;if(p)wanted.emplace_back(p,uint32_t(tmp?t.sub:t.overlayFrame));}
  for(auto it=cache.begin();it!=cache.end();)if(std::find(wanted.begin(),wanted.end(),it->first)==wanted.end())it=cache.erase(it);else ++it;
  size_t pixels=0;for(const auto&item:cache)pixels+=item.second.image.pixels.size();int work=0;
  for(auto key:wanted){auto&c=cache[key];if(c.ready||work>=4||(c.retry&&int32_t(now-c.retry)<0))continue;++work;c.retry=now+2000;external::voxel::Image image;
   uint32_t pointer=0;read(key.first+0xA4,&pointer,4);
   bool ok=tmp?gameassets::tile(key.first,key.second,terrainPalette,image):gameassets::overlay(key.first,pointer,int(key.second),image);
   std::string memoryReason;
   if(!ok&&tmp)ok=external::terrain::tile(read,pointer,key.second,terrainPalette,image,&memoryReason);
   c.issue=ok?"":gameassets::failure+(memoryReason.empty()?"":" memory="+memoryReason);
   if(ok&&pixels+image.pixels.size()<=4*1024*1024){pixels+=image.pixels.size();for(auto&p:image.pixels)if(unsigned alpha=p>>24)p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);c.image=std::move(image);c.ready=true;}
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
 size_t pixels=0,faces=0;for(const auto&i:images)pixels+=i.second.image.pixels.size();for(const auto&i:actorArt)faces+=i.second.art.model.faces.size();
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
    if(gameassets::captureActor(a.target.pointer,a.type,a.target.id,art)&&faces+art.model.faces.size()<=4*1024*1024){faces+=art.model.faces.size();source.art=std::move(art);source.ready=true;}source.issue=source.ready?"":gameassets::failure;
   }
   if(source.ready)ok=gameassets::renderActor(source.art,a.pose,image);else gameassets::failure=source.issue;
  }
  c.issue=ok?"":gameassets::failure;c.retry=ok?0:now+2000;if(!ok)continue;
  if(pixels+image.pixels.size()>32*1024*1024){c.retry=now+2000;continue;}
  pixels+=image.pixels.size();
  for(auto&p:image.pixels)if(unsigned alpha=p>>24)p=0xff000000u|(std::min(255u,((p>>16)&255)*255/alpha)<<16)|(std::min(255u,((p>>8)&255)*255/alpha)<<8)|std::min(255u,(p&255)*255/alpha);
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
static void update(){
 if(process!=pid||epoch!=commandMatch()){reset();process=pid;epoch=commandMatch();}
 if(!extra.watchEnabled||!extra.watchDanger)autoSuppressed=false;
 bool active=enabled&&extra.watchEnabled&&!snap.chat&&snap.renderStatus==1&&snap.width>=360&&snap.height>=320;
 if(!active){for(auto&v:views)v.shown=false;if(shown){shown=false;++paintRevision;}return;}
 DWORD now=GetTickCount();if(last&&DWORD(now-last)<50){arrange();return;}last=now;
 external::watch::Frame resources;resources.tiles.clear();resources.actors.clear();bool haveManual=false;
 for(auto&t:tracks){bool had=bool(t);external::watch::follow(read,t);if(had&&!t)autoSuppressed=true;haveManual|=bool(t);}
 if(!haveManual&&!autoSuppressed)automatic=external::watch::choose(read,{},automatic,extra.watchDanger);else automatic={};
 for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
  v.frame=external::watch::sample(read,target,extra.renderReveal||extra.mapStrategy==3,20);v.shown=bool(v.frame.focus);
  if(v.shown){resources.light=v.frame.light;resources.tiles.insert(resources.tiles.end(),v.frame.tiles.begin(),v.frame.tiles.end());resources.actors.insert(resources.actors.end(),v.frame.actors.begin(),v.frame.actors.end());}
 }
 arrange();++paintRevision;if(shown)refreshAssets(resources,now);
}
static void drawView(Graphics&g,const View&view){
 const auto&frame=view.frame;
 if(!view.shown)return;auto state=g.Save();g.SetClip(RectF(0,0,float(Width),float(Height)),CombineModeIntersect);
 SolidBrush bg(Color(255,8,12,8));g.FillRectangle(&bg,0.f,0.f,float(Width),float(Height));
 // Overscan the diamond map in world space and crop only at the rectangular
 // viewport. There is no separate title, status/footer or fixed 11x11 diamond.
 const float ox=Width/2.f,oy=Height/2.f,scale=.5f;int side=frame.side,radius=frame.radius,centerLevel=frame.tiles[radius*side+radius].level;
 auto image=[&](external::voxel::Image&i,float x,float y,const external::lighting::State&light){auto gains=light.gains();ColorMatrix matrix={};for(int n=0;n<5;++n)matrix.m[n][n]=n<3?gains[n]:1.f;ImageAttributes attributes;attributes.SetColorMatrix(&matrix);Bitmap bitmap(i.width,i.height,i.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(i.pixels.data()));g.DrawImage(&bitmap,RectF(x+i.x*scale,y+i.y*scale,i.width*scale,i.height*scale),0.f,0.f,float(i.width),float(i.height),UnitPixel,&attributes);};
 struct Item {float depth;int tile,actor;};std::vector<Item> order;
 for(int n=0;n<side*side;++n)order.push_back({float(n%side+n/side-2*radius),n,-1});
 for(size_t n=0;n<frame.actors.size();++n){const auto&a=frame.actors[n];order.push_back({float(a.x+a.y)/256-frame.cx-frame.cy-1,-1,int(n)});}
 std::stable_sort(order.begin(),order.end(),[](const Item&a,const Item&b){return a.depth<b.depth;});
 for(const auto&item:order){if(item.tile>=0){int n=item.tile,x=n%side-radius,y=n/side-radius;const auto&t=frame.tiles[n];float px=ox+(x-y)*30*scale,py=oy+(x+y)*15*scale-(t.level-centerLevel)*15*scale;
   if(!frame.cells[n]||px<-128||px>Width+128||py<-128||py>Height+128)continue;auto found=tiles.find({t.type,uint32_t(t.sub)});
   if(found!=tiles.end()&&found->second.ready)image(found->second.image,px,py,t.light);
   else{SolidBrush ground(Color(external::lighting::pixel(frame.cells[n],t.light)));PointF diamond[]={{px,py-15*scale},{px+30*scale,py},{px,py+15*scale},{px-30*scale,py}};g.FillPolygon(&ground,diamond,4);}
   auto over=overlays.find({t.overlayType,uint32_t(t.overlayFrame)});if(over!=overlays.end()&&over->second.ready)image(over->second.image,px,py+t.overlayY*scale,t.overlayLight);
  }else{const auto&a=frame.actors[item.actor];int cx=a.x/256-frame.cx+radius,cy=a.y/256-frame.cy+radius;if(cx<0||cy<0||cx>=side||cy>=side)continue;
   float x=ox+(float(a.x-a.y)/256-frame.cx+frame.cy)*30*scale,y=oy+item.depth*15*scale-(float(external::screenHeight(a.z,frame.zScale))-centerLevel*15)*scale;
   auto found=images.find(imageKey(a));
   if(found==images.end()||!found->second.ready){auto old=displayed.find({a.target.pointer,a.target.id});if(old!=displayed.end())found=images.find(old->second);}
   if(found!=images.end()&&found->second.ready)image(found->second.image,x,y,a.light);
   if(a.target==frame.focus){Pen ring(Color(255,255,224,111),1.3f);g.DrawEllipse(&ring,x-10,y-5,20.f,10.f);}
  }
 }
 Pen border(Color(255,frame.dangerous?255:105,frame.dangerous?167:195,frame.dangerous?62:225),3.f);g.DrawRectangle(&border,1.5f,1.5f,float(Width-3),float(Height-3));g.Restore(state);
}
}

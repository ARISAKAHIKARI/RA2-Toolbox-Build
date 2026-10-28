#pragma once
#include "radar_map.h"
#include "watch_view.h"
#include "camera_raster.h"
#include "native_canvas.h"
#include "radar_shroud_mask.h"
#include "camera_repair.h"
#include <map>
#include <tuple>
#include <set>
namespace external { namespace radar {
struct SceneRegion {double x=0,y=0,width=0,height=0;bool active=false;};
template<class Assets>class SceneRenderer {
 struct Cached {voxel::Image image;bool ready=false;uint32_t retry=0;};
 using ArtKey=std::tuple<uint32_t,uint32_t,int,int>;
 using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int,int,int,int>;
 using ObjectKey=std::pair<uint32_t,uint32_t>;
 static ArtKey artKey(const Marker&m){return {m.type,m.owner,m.kind,m.pose.variant};}
 static ImageKey imageKey(const Marker&m){return {m.type,m.owner,m.kind,m.pose.body,m.pose.turret,m.pose.frame,m.pose.variant,m.pose.buildingFlags,m.pose.doorFrame};}
 std::map<std::pair<uint32_t,unsigned>,Cached> tiles_,overlays_;
 std::map<ArtKey,typename Assets::ActorArt> art_;
 std::map<ImageKey,Cached> sprites_;
 std::map<ObjectKey,ImageKey> displayed_;size_t actorCursor_=0;repairvisuals::Assets repairAssets_;
 std::shared_ptr<const Terrain> building_,complete_,blocked_;bool dynamicFailure_=false;
 struct Paint {lighting::State light,overlayLight;int overlayOffset=0;bool bridge=false;uint64_t signature=0;};
 std::shared_ptr<camera::NativeCanvas::Budget> nativeBudget_=std::make_shared<camera::NativeCanvas::Budget>();
 camera::NativeCanvas pending_,background_;std::shared_ptr<const Scene> baseScene_,maskedBaseScene_;
 shroudmask::Raster mask_;std::shared_ptr<const Terrain> maskSource_;bool maskFog_=false;
 std::vector<size_t> cells_,baseCells_;std::vector<Paint> prepared_;std::vector<uint64_t> painted_;size_t cursor_=0,baseCursor_=0;
 bool probing_=false,probeChanged_=false,pendingIncomplete_=false,backgroundIncomplete_=false;uint32_t backgroundRetry_=0;int width_=0,height_=0;double spanX_=0,spanY_=0;uint64_t revision_=0;
 lighting::State backgroundLight_;uint64_t nativeCells_=0,basePresents_=0;SceneRegion region_{};
 static uint64_t mix(uint64_t h,uint64_t v){return (h^v)*1099511628211ull;}
 static uint64_t lightKey(uint64_t h,const lighting::State&s){h=mix(h,uint32_t(s.ambient));h=mix(h,uint32_t(s.intensity));for(int c:s.tint)h=mix(h,uint32_t(c));return h;}
 static bool sameSource(const Terrain&a,const Terrain&b){
  if(!a.geo.same(b.geo)||a.reveal!=b.reveal||a.colors!=b.colors||a.baseColors!=b.baseColors||a.shade!=b.shade||a.art.size()!=b.art.size())return false;
  for(size_t n=0;n<a.art.size();++n){const auto&x=a.art[n];const auto&y=b.art[n];if(x.cell!=y.cell||x.type!=y.type||x.overlayType!=y.overlayType||x.overlaySourceType!=y.overlaySourceType||x.overlay!=y.overlay||x.sub!=y.sub||x.level!=y.level||x.overlayFrame!=y.overlayFrame)return false;}return true;
 }
 void clearBackground(){building_.reset();complete_.reset();blocked_.reset();dynamicFailure_=false;pending_={};background_={};baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();mask_={};cells_.clear();baseCells_.clear();prepared_.clear();painted_.clear();cursor_=baseCursor_=0;probing_=probeChanged_=pendingIncomplete_=backgroundIncomplete_=false;backgroundRetry_=0;}

public:
 explicit SceneRenderer(size_t pages=camera::NativeCanvas::MaximumPages){nativeBudget_->maximum=std::clamp<size_t>(pages,1,camera::NativeCanvas::MaximumPages);}
 bool capacityExceeded()const{return bool(blocked_)||dynamicFailure_;}
 // Read-only metadata from the sprite actually displayed; no second decode.
 bool feedbackBounds(const Marker&m,int&x,int&y,int&w,int&h)const{
  auto found=sprites_.find(imageKey(m));
  if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
  if(found==sprites_.end()||!found->second.ready)return false;
  const auto&i=found->second.image;x=i.x;y=i.y;w=i.width;h=i.height;
  return w>0&&h>0&&w<=2048&&h<=2048;
 }
 bool feedbackVisibility(const std::vector<uint8_t>*&out)const{
  out=nullptr;if(!complete_)return false;if(complete_->reveal)return true;
  if(maskSource_!=complete_||mask_.width!=width_||mask_.height!=height_||mask_.alpha.size()!=size_t(width_)*height_)return false;
  out=&mask_.alpha;return true;
 }
 struct NativeStats {int width,height;size_t pages,resident;uint64_t cells,presents;};
 NativeStats nativeStats()const{return {background_.width(),background_.height(),background_.pageCount(),nativeBudget_->pages,nativeCells_,basePresents_};}
 void reset(){tiles_.clear();overlays_.clear();art_.clear();sprites_.clear();displayed_.clear();actorCursor_=0;repairAssets_.reset();clearBackground();width_=height_=0;spanX_=spanY_=0;revision_=0;nativeCells_=basePresents_=0;}
 template<class Read>std::shared_ptr<const Scene> render(const Frame&frame,int width,int height,Read read,Assets&assets,SceneRegion region={}){
  if(!frame.terrain||!frame.terrain->geo.valid()||frame.terrain->art.size()!=512*512||width<3||height<3||width>(region.active?8192:2048)||height>(region.active?8192:2048)||uint64_t(width)*height>16*1024*1024)return {};
  if(revision_!=assets.revision()){reset();revision_=assets.revision();}
  if(region.active&&(!std::isfinite(region.x)||!std::isfinite(region.y)||!std::isfinite(region.width)||!std::isfinite(region.height)||region.width<=0||region.height<=0||std::abs(region.x)>1e7||std::abs(region.y)>1e7||region.width>32768||region.height>32768))return {};
  if(width!=width_||height!=height_||region.x!=region_.x||region.y!=region_.y||region.width!=region_.width||region.height!=region_.height||region.active!=region_.active){width_=width;height_=height;region_=region;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
  const auto&domain=frame.terrain->geo;auto viewport=domain.nativeViewport();double exactX=viewport.width,exactY=viewport.height;
  if(!std::isfinite(exactX)||!std::isfinite(exactY)||exactX<=0||exactY<=0||exactX>camera::NativeCanvas::MaximumDimension||exactY>camera::NativeCanvas::MaximumDimension)return {};
  // Pixel storage is rounded outwards; the final filter still uses the exact
  // native viewport. No ceil-induced pixel row becomes part of the view.
  int nativeWidth=viewport.backingWidth(),nativeHeight=viewport.backingHeight();
  if((complete_&&(!complete_->geo.same(domain)||complete_->reveal!=frame.terrain->reveal))||(building_&&(!building_->geo.same(domain)||building_->reveal!=frame.terrain->reveal))||exactX!=spanX_||exactY!=spanY_)clearBackground();
  spanX_=exactX;spanY_=exactY;dynamicFailure_=false;
  if(blocked_){if(blocked_==frame.terrain||sameSource(*blocked_,*frame.terrain)){blocked_=frame.terrain;return {};}blocked_.reset();}
  lighting::State scene=lighting::sample(read);GameView<Read> game(read);
  if(!building_&&(complete_!=frame.terrain||backgroundLight_!=scene||(backgroundIncomplete_&&int32_t(frame.tick-backgroundRetry_)>=0))){
   building_=frame.terrain;cursor_=baseCursor_=0;cells_.clear();baseCells_.clear();prepared_.clear();probeChanged_=pendingIncomplete_=false;
   for(size_t n=0;n<building_->colors.size();++n)if(building_->colors[n])cells_.push_back(n);
   if(building_->reveal&&building_->baseColors.size()==building_->colors.size())
    for(size_t n=0;n<building_->baseColors.size();++n)
     if(building_->baseColors[n]&&!building_->colors[n])baseCells_.push_back(n);
   std::stable_sort(cells_.begin(),cells_.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});
   // New sampling banks commonly contain identical terrain. Compare evaluated
   // per-cell lighting before deciding to rebuild millions of native pixels.
   probing_=!(backgroundIncomplete_&&int32_t(frame.tick-backgroundRetry_)>=0)&&complete_&&sameSource(*complete_,*building_)&&painted_.size()==cells_.size();prepared_.resize(cells_.size());
   if(!probing_&&!pending_.reset(nativeWidth,nativeHeight,nativeBudget_))return {};
  }
  size_t staticPixels=0;for(const auto&i:tiles_)staticPixels+=camera::pixels(i.second.image);for(const auto&i:overlays_)staticPixels+=camera::pixels(i.second.image);
  auto prepare=[&](size_t index){const auto&cell=building_->art[index];Paint value;value.light=lighting::cell(read,cell.cell,scene,lighting::Layer::Terrain);value.overlayLight=value.light;
   if(cell.overlayType){uint32_t source=cell.overlaySourceType?cell.overlaySourceType:cell.overlayType;value.overlayOffset=watch::overlayOffset(read,cell.cell,source,cell.overlay,cell.overlayFrame);value.bridge=(game.template get<uint32_t>(cell.cell+0x140)&0x80)!=0;
    if(value.bridge)value.overlayLight=lighting::cell(read,cell.cell,scene,lighting::Layer::Bridge);
    else if(game.template get<uint8_t>(source+0x2A9)){value.overlayLight=scene;value.overlayLight.intensity=1000;}
    else if(game.template get<uint8_t>(source+0x2A8))value.overlayLight=lighting::cell(read,cell.cell,scene,lighting::Layer::Object,false);
   }
   value.signature=lightKey(lightKey(1469598103934665603ull,value.light),value.overlayLight);value.signature=mix(mix(value.signature,uint32_t(value.overlayOffset)),value.bridge);return value;
  };
  if(building_&&probing_){size_t end=std::min(cells_.size(),cursor_+size_t(4096));for(;cursor_<end;++cursor_){prepared_[cursor_]=prepare(cells_[cursor_]);probeChanged_|=prepared_[cursor_].signature!=painted_[cursor_];}
   if(cursor_==cells_.size()){
    if(!probeChanged_){complete_=building_;building_.reset();backgroundLight_=scene;cursor_=0;prepared_.clear();}
    else{probing_=false;cursor_=baseCursor_=0;if(!pending_.reset(nativeWidth,nativeHeight,nativeBudget_))return {};}
   }
  }
  if(building_&&!probing_){
   const auto&terrain=*building_;const auto&geo=terrain.geo;
   // Native radar RGB exists for the full legal/raw backing, including the
   // inward playfield margin. Reveal paints that coarse backing at the fringe
   // while still excluding nonusable TMPs, overlays, objects and effects.
   size_t baseEnd=std::min(baseCells_.size(),baseCursor_+size_t(4096));
   for(;baseCursor_<baseEnd;++baseCursor_){size_t index=baseCells_[baseCursor_];int x=int(index%512),y=int(index/512);
    int flatX=x-y+geo.offsetX,flatY=x+y-geo.offsetY;
    pending_.underlayRect(flatX*30,flatY*15,(flatX+2)*30,(flatY+1)*15,terrain.baseColors[index]);
    if(pending_.failed())break;
   }
   if(baseCursor_<baseCells_.size()&&!pending_.failed())return {};
   std::array<uint32_t,256> palette{};bool paletteReady=assets.palette(true,palette);int loads=0;size_t end=std::min(cells_.size(),cursor_+size_t(4096));
   for(;cursor_<end&&!pending_.failed();++cursor_){size_t index=cells_[cursor_];int x=int(index%512),y=int(index/512);const auto&cell=terrain.art[index];
    mapbounds::TileOrigin origin;if(!geo.tileOrigin(x,y,int(cell.level),origin))continue;float px=float(origin.x+30),py=float(origin.y+15),base=float((x+y)*15);
    // The original radar has a flat 2x1 RGB cell footprint beneath its
    // scene. A TMP diamond (especially on a raised edge) does not cover that
    // whole footprint; keep its native ground color in the uncovered pixels.
    int flatX=x-y+geo.offsetX,flatY=x+y-geo.offsetY;
    uint32_t baseColor=terrain.baseColors.size()==terrain.colors.size()&&terrain.baseColors[index]?terrain.baseColors[index]:terrain.colors[index];
    pending_.underlayRect(flatX*30,flatY*15,(flatX+2)*30,(flatY+1)*15,baseColor);
    auto&paint=prepared_[cursor_];if(!probeChanged_)paint=prepare(index);
    if(cell.type){
     auto&tile=tiles_[{cell.type,cell.sub}];
     if(!tile.ready&&paletteReady&&loads<64&&(!tile.retry||int32_t(frame.tick-tile.retry)>=0)){++loads;tile.ready=assets.tile(cell.type,cell.sub,palette,tile.image);if(tile.ready){auto n=camera::pixels(tile.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{tile.ready=false;tile.image={};}}tile.retry=frame.tick+2000;}
     if(!tile.ready&&paletteReady&&loads>=64&&!tile.retry)break;
     if(tile.ready)pending_.blit(tile.image,px,py,1,1,base,paint.light,0);
     else{pendingIncomplete_=true;pending_.diamond(px,py,1,1,base,0xff000000u|baseColor,paint.light);}
    } // A cell without a resolved tile has no ground sprite to paint.
    if(cell.overlayType){auto&overlay=overlays_[{cell.overlayType,cell.overlayFrame}];
     if(!overlay.ready&&loads<64&&(!overlay.retry||int32_t(frame.tick-overlay.retry)>=0)){++loads;overlay.ready=assets.overlay(cell.overlayType,game.template get<uint32_t>(cell.overlayType+0xA4),cell.overlayFrame,overlay.image);if(overlay.ready){auto n=camera::pixels(overlay.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{overlay.ready=false;overlay.image={};}}overlay.retry=frame.tick+2000;}
     if(!overlay.ready&&loads>=64&&!overlay.retry)break;
     if(overlay.ready)pending_.blit(overlay.image,px,py+paint.overlayOffset,1,1,base+(paint.bridge?60:0),paint.overlayLight,2);else pendingIncomplete_=true;
    }
    if(pending_.failed())break;++nativeCells_;
   }
   if(pending_.failed()){
    // Release the old native bank before retrying the current cell. The worker
    // may keep its last small published scene; never silently minify sources.
    if(background_.pageCount()){background_={};complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();pending_.clearFailure();}
    else{blocked_=building_;building_.reset();pending_={};prepared_.clear();cells_.clear();baseCells_.clear();cursor_=baseCursor_=0;complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
    return {};
   }
   if(cursor_==cells_.size()){background_=std::move(pending_);background_.clean();painted_.resize(prepared_.size());for(size_t n=0;n<prepared_.size();++n)painted_[n]=prepared_[n].signature;prepared_.clear();complete_=building_;building_.reset();backgroundLight_=scene;backgroundIncomplete_=pendingIncomplete_;backgroundRetry_=frame.tick+2000;baseScene_.reset();maskedBaseScene_.reset();}
  }
  if(!complete_)return {};
  if(!baseScene_){auto base=std::make_shared<Scene>();base->width=width;base->height=height;if(!(region.active?background_.presentRegion(width,height,base->pixels,region.x,region.y,region.width,region.height):background_.present(width,height,base->pixels,spanX_,spanY_)))return {};baseScene_=std::move(base);++basePresents_;}
  // Native Cell::DrawShroud places 60x30 alpha sprites on the flat cell
  // projection, after the world is drawn. Cache their final-presentation mask
  // separately from the original-size terrain so units and repairs cannot
  // escape into unseen neighboring cells. All frames use their raw SHP bytes.
  const bool masked=!complete_->reveal&&complete_->shade.size()==512*512;
  uint32_t options=game.template get<uint32_t>(0xA8B230);
  bool fog=masked&&options>=0x10000&&(game.template get<uint8_t>(options+1)&0x10)&&
   game.template get<uint8_t>(complete_->geo.house+0x1F5)==0;
  if(masked&&(maskSource_!=complete_||maskFog_!=fog||mask_.width!=width||mask_.height!=height)){
   bool same=maskSource_&&maskFog_==fog&&mask_.width==width&&mask_.height==height&&sameSource(*maskSource_,*complete_);
   if(!same){if(!mask_.build(*complete_,width,height,region.active?region.width:spanX_,region.active?region.height:spanY_,fog,
     [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},region.active?region.x:0,region.active?region.y:0))return {};
    maskedBaseScene_.reset();}
   maskSource_=complete_;maskFog_=fog;
  }
  auto view=background_.fork();const auto&geo=frame.terrain->geo;int work=0;size_t pixels=0;std::vector<const Marker*> fallbacks;
  auto markers=frame.markers;
  if(region.active)markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){
   double x=(double(m.x)-m.y)*30/256.+geo.offsetX*30,y=(double(m.x)+m.y)*15/256.-geo.offsetY*15-screenHeight(m.z,projectionZScale(read));
   return x<region.x-512||x>region.x+region.width+512||y<region.y-512||y>region.y+region.height+512;
  }),markers.end());
  for(auto&m:markers)if(m.kind==3)assets.buildingPose(m.object,m.type,m.pose);
  std::map<ObjectKey,ImageKey> visible;std::set<ImageKey> wanted;std::set<ArtKey> artWanted;
  for(const auto&m:markers){auto key=imageKey(m);visible[{m.object,m.id}]=key;wanted.insert(key);artWanted.insert(artKey(m));}
  for(auto i=displayed_.begin();i!=displayed_.end();){auto current=visible.find(i->first);
   if(current==visible.end()||std::get<0>(current->second)!=std::get<0>(i->second)||std::get<1>(current->second)!=std::get<1>(i->second)||std::get<2>(current->second)!=std::get<2>(i->second)||std::get<6>(current->second)!=std::get<6>(i->second))i=displayed_.erase(i);
   else{wanted.insert(i->second);++i;}}
  if(sprites_.size()>1024){for(auto i=sprites_.begin();i!=sprites_.end();)if(!wanted.count(i->first))i=sprites_.erase(i);else ++i;}
  if(art_.size()>512){for(auto i=art_.begin();i!=art_.end();)if(!artWanted.count(i->first))i=art_.erase(i);else ++i;}
  for(const auto&i:sprites_)pixels+=camera::pixels(i.second.image);size_t artBytes=0;for(const auto&i:art_)artBytes+=Assets::cost(i.second);
  if(pixels>6*1024*1024){for(auto i=sprites_.begin();i!=sprites_.end()&&pixels>4*1024*1024;)if(!wanted.count(i->first)){pixels-=camera::pixels(i->second.image);i=sprites_.erase(i);}else ++i;}
  if(artBytes>96*1024*1024){for(auto i=art_.begin();i!=art_.end()&&artBytes>64*1024*1024;)if(!artWanted.count(i->first)){artBytes-=Assets::cost(i->second);i=art_.erase(i);}else ++i;}
  // Rotate the decode budget independently of draw order. Moving units at the
  // start of the map must not starve an oil pump or construction-yard anim.
  for(size_t scanned=0;scanned<markers.size()&&work<8;++scanned){const auto&m=markers[actorCursor_++%markers.size()];auto&cached=sprites_[imageKey(m)];
   if((cached.ready&&!cached.image.incomplete)||(cached.retry&&int32_t(frame.tick-cached.retry)<0))continue;
   ++work;voxel::Image image;bool ready=false;auto source=art_.find(artKey(m));
   if(m.kind==4)ready=assets.overlay(m.type,game.template get<uint32_t>(m.type+0xA4),m.pose.frame,image,true);
   else{if(source==art_.end()){typename Assets::ActorArt art;if((m.kind==6?projectiles::captureArt(assets,m.object,m.type,m.id,art):m.kind==5?assets.captureAnimation(m.object,m.type,m.id,m.parent,m.parentId,art):assets.captureActor(m.object,m.type,m.id,art))&&artBytes+Assets::cost(art)<=128*1024*1024){artBytes+=Assets::cost(art);source=art_.emplace(artKey(m),std::move(art)).first;}}
    if(source!=art_.end())ready=assets.renderActor(source->second,m.pose,image);}
   size_t previous=camera::pixels(cached.image),next=camera::pixels(image);
   if(ready&&pixels-previous+next<=8*1024*1024){camera::opaque(image);pixels=pixels-previous+next;cached.image=std::move(image);cached.ready=true;}
   cached.retry=cached.ready&&!cached.image.incomplete?0:frame.tick+2000;
  }
  for(const auto&m:markers){auto key=imageKey(m);auto i=sprites_.find(key);if(i!=sprites_.end()&&i->second.ready)displayed_[{m.object,m.id}]=key;}
  std::vector<const Marker*> order;for(const auto&m:markers)order.push_back(&m);
  // Native ground animations share object order (Layer, GetYSort). A blanket
  // final animation pass incorrectly paints pumps/fire over foreground units.
  std::stable_sort(order.begin(),order.end(),[](const Marker*a,const Marker*b){return camera::orderKey(a->layer,a->kind,a->x,a->y,a->sort)<camera::orderKey(b->layer,b->kind,b->x,b->y,b->sort);});
  for(const auto*marker:order){const auto&m=*marker;if(m.x<0||m.y<0||m.x>=512*256||m.y>=512*256)continue;
   auto found=sprites_.find(imageKey(m));if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
   float px=float((double(m.x)-m.y)*30/256.+geo.offsetX*30),py=float((double(m.x)+m.y)*15/256.-geo.offsetY*15+camera::anchorY(m.kind)-screenHeight(m.z,projectionZScale(read)));
   if(found!=sprites_.end()&&found->second.ready){auto light=m.kind==6?projectiles::light(read,uint32_t(m.pose.variant)):m.kind==5?lighting::animation(read,m.object,m.type,m.x,m.y,m.z,scene):m.kind==4?lighting::terrainObject(read,m.type,m.x,m.y,m.z,scene):lighting::object(read,m.type,m.owner,m.kind,m.x,m.y,m.z,scene);uint32_t cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;bool bridge=m.onBridge;int level=game.template get<int8_t>(cell+0x11B);
    view.blit(found->second.image,px,py,1,1,float((int64_t(m.x)+m.y)*15/256.)-15+camera::anchorY(m.kind)-m.drawZ+(bridge?60:0),light,m.kind==4?12:(m.kind==3||m.kind==5)?2:1,float(std::max(0,screenHeight(m.z,projectionZScale(read))-(level+(bridge?4:0))*15)));}
   else if(m.kind<4)fallbacks.push_back(&m);
  }
  // Native status sprites use their cursor palette and are drawn after all
  // world objects without either reading or writing the world depth buffer.
  const lighting::State statusLight;
  for(const auto&sprite:frame.repairs)if(const auto*image=repairAssets_.image(assets,sprite)){
   float x=float((double(sprite.x)-sprite.y)*30/256.+geo.offsetX*30),y=float((double(sprite.x)+sprite.y)*15/256.-geo.offsetY*15-screenHeight(sprite.z,projectionZScale(read)));
   view.blit(*image,x,y,1,1,0,statusLight);
  }
  // Keep physical border scenery; the native radar projection and output
  // canvas crop the image. Camera movement limits are not a pixel mask.
  if(tiles_.size()>4096)tiles_.clear();if(overlays_.size()>4096)overlays_.clear();
  if(view.failed()){dynamicFailure_=true;return {};}
  if(!view.dirtyPages()&&fallbacks.empty()){
   if(!masked)return baseScene_;
   if(!maskedBaseScene_){auto out=std::make_shared<Scene>(*baseScene_);mask_.apply(out->pixels);maskedBaseScene_=std::move(out);}
   return maskedBaseScene_;
  }
  auto out=std::make_shared<Scene>(*baseScene_);if(!(region.active?view.presentRegion(width,height,out->pixels,region.x,region.y,region.width,region.height):view.presentDirty(width,height,out->pixels,spanX_,spanY_))){dynamicFailure_=true;return {};}
  // Missing-art dots are presentation annotations, not minified game sprites.
  for(const auto*m:fallbacks){int x=int(((double(m->x)-m->y)*30/256.+geo.offsetX*30-(region.active?region.x:0))*width/(region.active?region.width:spanX_)),y=int(((double(m->x)+m->y)*15/256.-geo.offsetY*15-(region.active?region.y:0))*height/(region.active?region.height:spanY_));for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)out->pixels[size_t(yy)*width+xx]=0xff000000u|m->color;}
  if(masked)mask_.apply(out->pixels);
  return out;
 }
};
}}

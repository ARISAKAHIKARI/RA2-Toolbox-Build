#pragma once
#include "radar_map.h"
#include "watch_view.h"
#include "camera_raster.h"
#include <map>
#include <tuple>
#include <set>
namespace external { namespace radar {
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
 std::map<ObjectKey,ImageKey> displayed_;size_t actorCursor_=0;
 std::shared_ptr<const Terrain> building_,complete_;
 camera::Surface pending_,background_;std::vector<size_t> cells_;size_t cursor_=0;int width_=0,height_=0;uint64_t revision_=0;
public:
 void reset(){tiles_.clear();overlays_.clear();art_.clear();sprites_.clear();displayed_.clear();actorCursor_=0;building_.reset();complete_.reset();pending_={};background_={};cells_.clear();cursor_=0;width_=height_=0;revision_=0;}
 template<class Read>std::shared_ptr<const Scene> render(const Frame&frame,int width,int height,Read read,Assets&assets){
  if(!frame.terrain||!frame.terrain->geo.valid()||frame.terrain->art.size()!=512*512||width<3||height<3||width>2048||height>2048)return {};
  if(revision_!=assets.revision()){reset();revision_=assets.revision();}
  if(width!=width_||height!=height_){building_.reset();complete_.reset();pending_={};background_={};cells_.clear();cursor_=0;width_=width;height_=height;}
  // A trigger can shrink/expand VisibleRect while the cell pointer, match and
  // output size stay unchanged. Never carry an old-domain background forward.
  if((complete_&&!complete_->geo.same(frame.terrain->geo))||(building_&&!building_->geo.same(frame.terrain->geo))){building_.reset();complete_.reset();pending_={};background_={};cells_.clear();cursor_=0;}
  if(!building_&&complete_!=frame.terrain){building_=frame.terrain;cursor_=0;pending_.reset(width,height);cells_.clear();for(size_t n=0;n<building_->colors.size();++n)if(building_->colors[n])cells_.push_back(n);std::stable_sort(cells_.begin(),cells_.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});}
  size_t staticPixels=0;for(const auto&i:tiles_)staticPixels+=camera::pixels(i.second.image);for(const auto&i:overlays_)staticPixels+=camera::pixels(i.second.image);
  lighting::State scene=lighting::sample(read);GameView<Read> game(read);
  auto gain=[&](const Geometry&g){return std::pair<float,float>{float(width)*g.factor/(30*g.rw),float(height)*g.factor/(15*g.rh)};};
  if(building_){
   const auto&terrain=*building_;const auto&geo=terrain.geo;auto scale=gain(geo);
   std::array<uint32_t,256> palette{};bool paletteReady=assets.palette(true,palette);
   int loads=0;size_t end=std::min(cells_.size(),cursor_+size_t(4096));
   for(;cursor_<end;++cursor_){size_t index=cells_[cursor_];int x=int(index%512),y=int(index/512);const auto&cell=terrain.art[index];double u,v;geo.project(x+.5,y+.5,u,v);
    float px=float(u*width),py=float(v*height)-cell.level*15*scale.second,base=float((x+y)*15);
    auto light=lighting::cell(read,cell.cell,scene,lighting::Layer::Terrain);
    auto&tile=tiles_[{cell.type,cell.sub}];
    if(!tile.ready&&cell.type&&paletteReady&&loads<64&&(!tile.retry||int32_t(frame.tick-tile.retry)>=0)){++loads;tile.ready=assets.tile(cell.type,cell.sub,palette,tile.image);if(tile.ready){auto n=camera::pixels(tile.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{tile.ready=false;tile.image={};}}tile.retry=frame.tick+2000;}
    if(!tile.ready&&cell.type&&paletteReady&&loads>=64&&!tile.retry)break;
    if(tile.ready)pending_.blit(tile.image,px,py,scale.first,scale.second,base,light,0);
    else pending_.diamond(px,py,scale.first,scale.second,base,0xff000000u|terrain.colors[index],light);
    if(cell.overlayType){auto&overlay=overlays_[{cell.overlayType,cell.overlayFrame}];
     if(!overlay.ready&&loads<64&&(!overlay.retry||int32_t(frame.tick-overlay.retry)>=0)){++loads;overlay.ready=assets.overlay(cell.overlayType,game.template get<uint32_t>(cell.overlayType+0xA4),cell.overlayFrame,overlay.image);if(overlay.ready){auto n=camera::pixels(overlay.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{overlay.ready=false;overlay.image={};}}overlay.retry=frame.tick+2000;}
     if(!overlay.ready&&loads>=64&&!overlay.retry)break;
     if(overlay.ready){int offset=watch::overlayOffset(read,cell.cell,cell.overlayType,cell.overlay,cell.overlayFrame);auto olight=light;if(game.template get<uint32_t>(cell.cell+0x140)&0x80)olight=lighting::cell(read,cell.cell,scene,lighting::Layer::Bridge);
      pending_.blit(overlay.image,px,py+offset*scale.second,scale.first,scale.second,base+((game.template get<uint32_t>(cell.cell+0x140)&0x80)?60:0),olight,2);}
    }
   }
   if(cursor_==cells_.size()){background_=std::move(pending_);complete_=building_;building_.reset();}
  }
  if(!complete_)return {};
  camera::Surface view=background_;const auto&geo=frame.terrain->geo;auto scale=gain(geo);int work=0;size_t pixels=0;
  auto markers=frame.markers;for(auto&m:markers)if(m.kind==3)assets.buildingPose(m.object,m.type,m.pose);
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
   else{if(source==art_.end()){typename Assets::ActorArt art;if((m.kind==5?assets.captureAnimation(m.object,m.type,m.id,m.parent,m.parentId,art):assets.captureActor(m.object,m.type,m.id,art))&&artBytes+Assets::cost(art)<=128*1024*1024){artBytes+=Assets::cost(art);source=art_.emplace(artKey(m),std::move(art)).first;}}
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
   float px=float(m.u*width),py=float(m.v*height)+(camera::anchorY(m.kind)-screenHeight(m.z,projectionZScale(read)))*scale.second;
   if(found!=sprites_.end()&&found->second.ready){auto light=m.kind==5?lighting::animation(read,m.object,m.type,m.x,m.y,m.z,scene):m.kind==4?lighting::at(read,m.x,m.y,m.z,scene,true):lighting::object(read,m.type,m.owner,m.kind,m.x,m.y,m.z,scene);uint32_t cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;bool bridge=m.onBridge;int level=game.template get<uint8_t>(cell+0x11B);
    view.blit(found->second.image,px,py,scale.first,scale.second,float((int64_t(m.x)+m.y)*15/256.)-15+camera::anchorY(m.kind)-m.drawZ+(bridge?60:0),light,m.kind==4?12:(m.kind==3||m.kind==5)?2:1,float(std::max(0,screenHeight(m.z,projectionZScale(read))-(level+(bridge?4:0))*15)));}
   else if(m.kind<4){int x=int(m.u*width),y=int(m.v*height);for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)view.color[size_t(yy)*width+xx]=0xff000000u|m.color;}
  }
  geo.bounds.clip(view.color,width,height,double(geo.offsetX)*geo.factor*width/geo.rw,-double(geo.offsetY)*geo.factor*height/geo.rh,scale.first,scale.second);
  if(tiles_.size()>4096)tiles_.clear();if(overlays_.size()>4096)overlays_.clear();
  auto out=std::make_shared<Scene>();out->width=width;out->height=height;out->pixels=std::move(view.color);return out;
 }
};
}}

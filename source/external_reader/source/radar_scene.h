#pragma once
#include "radar_map.h"
#include "watch_view.h"
#include "camera_raster.h"
#include <map>
#include <tuple>
namespace external { namespace radar {
template<class Assets>class SceneRenderer {
 struct Cached {voxel::Image image;bool ready=false;uint32_t retry=0;};
 using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int>;
 std::map<std::pair<uint32_t,unsigned>,Cached> tiles_,overlays_;
 std::map<std::tuple<uint32_t,uint32_t,int>,typename Assets::ActorArt> art_;
 std::map<ImageKey,Cached> sprites_;
 std::shared_ptr<const Terrain> building_,complete_;
 camera::Surface pending_,background_;std::vector<size_t> cells_;size_t cursor_=0;int width_=0,height_=0;uint64_t revision_=0;
public:
 void reset(){tiles_.clear();overlays_.clear();art_.clear();sprites_.clear();building_.reset();complete_.reset();pending_={};background_={};cells_.clear();cursor_=0;width_=height_=0;revision_=0;}
 template<class Read>std::shared_ptr<const Scene> render(const Frame&frame,int width,int height,Read read,Assets&assets){
  if(!frame.terrain||frame.terrain->art.size()!=512*512||width<3||height<3||width>1024||height>1024)return {};
  if(width!=width_||height!=height_||revision_!=assets.files.revision()){reset();width_=width;height_=height;revision_=assets.files.revision();}
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
    if(!tile.ready&&cell.type&&paletteReady&&loads<12&&(!tile.retry||int32_t(frame.tick-tile.retry)>=0)){++loads;tile.ready=assets.tile(cell.type,cell.sub,palette,tile.image);if(tile.ready){auto n=camera::pixels(tile.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{tile.ready=false;tile.image={};}}tile.retry=frame.tick+2000;}
    if(tile.ready)pending_.blit(tile.image,px,py,scale.first,scale.second,base,light,0);
    else pending_.diamond(px,py,scale.first,scale.second,base,0xff000000u|terrain.colors[index],light);
    if(cell.overlayType){auto&overlay=overlays_[{cell.overlayType,cell.overlayFrame}];
     if(!overlay.ready&&loads<12&&(!overlay.retry||int32_t(frame.tick-overlay.retry)>=0)){++loads;overlay.ready=assets.overlay(cell.overlayType,game.template get<uint32_t>(cell.overlayType+0xA4),cell.overlayFrame,overlay.image);if(overlay.ready){auto n=camera::pixels(overlay.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{overlay.ready=false;overlay.image={};}}overlay.retry=frame.tick+2000;}
     if(overlay.ready){int offset=watch::overlayOffset(read,cell.cell,cell.overlayType,cell.overlay,cell.overlayFrame);auto olight=light;if(game.template get<uint32_t>(cell.cell+0x140)&0x80)olight=lighting::cell(read,cell.cell,scene,lighting::Layer::Bridge);
      pending_.blit(overlay.image,px,py+offset*scale.second,scale.first,scale.second,base+((game.template get<uint32_t>(cell.cell+0x140)&0x80)?60:0),olight,2);}
    }
   }
   if(cursor_==cells_.size()){background_=std::move(pending_);complete_=building_;building_.reset();}
  }
  if(!complete_)return {};
  camera::Surface view=background_;const auto&geo=frame.terrain->geo;auto scale=gain(geo);int work=0;size_t pixels=0;
  for(const auto&i:sprites_)pixels+=camera::pixels(i.second.image);size_t artBytes=0;for(const auto&i:art_)artBytes+=Assets::cost(i.second);
  auto markers=frame.markers;for(auto&m:markers)if(m.kind==3)m.pose.frame=assets.buildingFrame(m.object,m.type);
  std::vector<const Marker*> order;for(const auto&m:markers)order.push_back(&m);
  std::stable_sort(order.begin(),order.end(),[](const Marker*a,const Marker*b){return int64_t(a->x)+a->y<int64_t(b->x)+b->y;});
  for(const auto*marker:order){const auto&m=*marker;ImageKey key{m.type,m.owner,m.pose.body,m.pose.turret,m.pose.frame,m.pose.variant};auto&cached=sprites_[key];
   if(!cached.ready&&work<8&&pixels<8*1024*1024&&(!cached.retry||int32_t(frame.tick-cached.retry)>=0)){++work;auto artKey=std::make_tuple(m.type,m.owner,m.pose.variant);auto source=art_.find(artKey);
    if(m.kind==4)cached.ready=assets.overlay(m.type,game.template get<uint32_t>(m.type+0xA4),m.pose.frame,cached.image,true);
    else{if(source==art_.end()){typename Assets::ActorArt art;if((m.kind==5?assets.captureAnimation(m.object,m.type,m.id,art):assets.captureActor(m.object,m.type,m.id,art))&&artBytes+Assets::cost(art)<=128*1024*1024){artBytes+=Assets::cost(art);source=art_.emplace(artKey,std::move(art)).first;}}
     if(source!=art_.end())cached.ready=assets.renderActor(source->second,m.pose,cached.image);}
    if(cached.ready){camera::opaque(cached.image);pixels+=camera::pixels(cached.image);}cached.retry=frame.tick+2000;
   }
   float px=float(m.u*width),py=float(m.v*height)-screenHeight(m.z,projectionZScale(read))*scale.second;
   if(cached.ready){auto light=m.normalLight?lighting::State{}:lighting::at(read,m.x,m.y,m.z,scene,m.kind==4);uint32_t cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;bool bridge=m.onBridge;int level=game.template get<uint8_t>(cell+0x11B);
    view.blit(cached.image,px,py,scale.first,scale.second,float((int64_t(m.x)+m.y)*15/256.)-15-m.drawZ+(bridge?60:0),light,m.kind==4?12:(m.kind==3||m.kind==5)?2:1,float(std::max(0,screenHeight(m.z,projectionZScale(read))-(level+(bridge?4:0))*15)));}
   else if(m.kind<4){int x=int(m.u*width),y=int(m.v*height);for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)view.color[size_t(yy)*width+xx]=0xff000000u|m.color;}
  }
  if(sprites_.size()>2048||pixels>8*1024*1024)sprites_.clear();if(art_.size()>512)art_.clear();if(tiles_.size()>4096)tiles_.clear();if(overlays_.size()>4096)overlays_.clear();
  auto out=std::make_shared<Scene>();out->width=width;out->height=height;out->pixels=std::move(view.color);return out;
 }
};
}}

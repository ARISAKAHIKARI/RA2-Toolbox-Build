#pragma once
#include "native_canvas.h"
#include "gpu_shroud.h"
#include "world_effects.h"
namespace external { namespace camera {
// Sparse native A, built only for changed 128px pages. This keeps A before
// palette conversion and before area filtering without a whole-map buffer.
class NativePageMasks {
 struct Alpha {int x=0,y=0;std::shared_ptr<const voxel::Image>image;bool operator==(const Alpha&o)const{return x==o.x&&y==o.y&&image==o.image;}};
 struct Entry {std::vector<radar::shroudmask::Cell>cells;std::vector<Alpha>alphas;bool masked=false,fog=false;std::shared_ptr<const std::vector<uint8_t>>pixels;};
 std::map<uint32_t,Entry>cache_;gpushroud::Builder builder_;radar::Geometry geo_{};
public:
 void reset(){cache_.clear();builder_.reset();geo_={};}
 template<class Load,class AlphaLoad>bool prepare(NativeCanvas&canvas,const radar::Terrain&terrain,bool masked,bool fog,const worldfx::Snapshot*effects,bool particles,double zscale,Load load,AlphaLoad alphaLoad){
  if(!geo_.same(terrain.geo)){reset();geo_=terrain.geo;}auto keys=canvas.pageKeys();std::map<uint32_t,std::vector<radar::shroudmask::Cell>>cells;std::map<uint32_t,std::vector<Alpha>>alphas;
  auto visit=[&](int l,int t,int r,int b,auto&&fn){l=std::clamp(l,0,canvas.width());t=std::clamp(t,0,canvas.height());r=std::clamp(r,0,canvas.width());b=std::clamp(b,0,canvas.height());if(l>=r||t>=b)return;for(int y=t/128;y<=(b-1)/128;++y)for(int x=l/128;x<=(r-1)/128;++x)fn(uint32_t(y)*256+uint32_t(x));};
  if(masked){for(size_t n=0;n<terrain.shade.size();++n){if(terrain.shade[n].shroud==127||!geo_.shroudSourceVisible(int(n%512),int(n/512)))continue;mapbounds::TileOrigin p;if(!geo_.shroudOrigin(int(n%512),int(n/512),p))continue;visit(p.x,p.y,p.x+60,p.y+30,[&](uint32_t key){cells[key].push_back({uint32_t(n),terrain.shade[n]});});}}
  if(effects&&particles)for(const auto&a:effects->alphas){auto image=alphaLoad(a.image);if(!image||image->indices.size()!=size_t(image->width)*image->height)return false;
   int x=int((int64_t(a.point.x)-a.point.y)*30/256)+geo_.offsetX*30+a.dx+image->x+image->canvasWidth/2,y=int((int64_t(a.point.x)+a.point.y)*15/256)-geo_.offsetY*15-screenHeight(a.point.z,zscale)+a.dy+image->y+image->canvasHeight/2;Alpha alpha{x,y,image};
   visit(x,y,x+image->width,y+image->height,[&](uint32_t key){alphas[key].push_back(alpha);});}
  auto pages=std::make_shared<NativeCanvas::AlphaPages>();for(uint32_t key:keys){auto&entry=cache_[key];auto&c=cells[key];auto&a=alphas[key];int px=int(key%256)*128,py=int(key/256)*128;
   if(!entry.pixels||entry.masked!=masked||entry.fog!=fog||entry.cells!=c||entry.alphas!=a){auto pixels=std::make_shared<std::vector<uint8_t>>(128*128,127);
    if(masked){std::shared_ptr<const gpushroud::Plan>plan;if(!builder_.build(geo_,c,128,128,128,128,fog,load,plan,px,py)||!gpushroud::reference(*plan,*pixels))return false;}
    for(const auto&alpha:a){int l=std::max(px,alpha.x),t=std::max(py,alpha.y),r=std::min(px+128,alpha.x+alpha.image->width),b=std::min(py+128,alpha.y+alpha.image->height);for(int y=t;y<b;++y)for(int x=l;x<r;++x){size_t at=size_t(y-py)*128+x-px,from=size_t(y-alpha.y)*alpha.image->width+x-alpha.x;(*pixels)[at]=uint8_t(std::min(255u,unsigned((*pixels)[at])*alpha.image->indices[from]/127u));}}
    entry={c,a,masked,fog,pixels};
   }(*pages)[key]=entry.pixels;
  }
  if(cache_.size()>keys.size()+512){std::set<uint32_t>live(keys.begin(),keys.end());for(auto at=cache_.begin();at!=cache_.end();)if(!live.count(at->first))at=cache_.erase(at);else ++at;}
  canvas.nativeAlpha(std::move(pages));return true;
 }
};
}}

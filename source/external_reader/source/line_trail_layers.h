#pragma once
#include "radar_map.h"
#include "object_canvas.h"
#include <map>
namespace external { namespace sceneextras {
// Vera20K surface_line.rs integer 3-axis walk, emitted once per immutable
// native ring observation. GPU composition reads destination color/Z. Repeat
// XY samples occupy different planes; they never collapse into one alpha.
class TrailLayers {
 std::shared_ptr<const Snapshot>source_;std::vector<camera::ObjectCanvas::Draw>draws_;double scale_=0;int ox_=0,oy_=0;
 static bool same(const std::shared_ptr<const Snapshot>&a,const std::shared_ptr<const Snapshot>&b){
  if(a==b)return true;if(!a||!b||a->trailsValid!=b->trailsValid||a->trails.size()!=b->trails.size())return false;
  for(size_t n=0;n<a->trails.size();++n){const auto&x=a->trails[n];const auto&y=b->trails[n];if(x.object!=y.object||x.slot!=y.slot||x.color!=y.color||std::memcmp(x.nodes.data(),y.nodes.data(),sizeof x.nodes))return false;}return true;
 }
public:
 void reset(){source_.reset();draws_.clear();scale_=0;}
 template<class Draw>void paint(const radar::Frame&frame,double scale,Draw draw){
  if(!frame.extras||!frame.terrain)return;const auto&geo=frame.terrain->geo;
  if(!same(source_,frame.extras)||scale_!=scale||ox_!=geo.offsetX||oy_!=geo.offsetY){draws_.clear();source_=frame.extras;scale_=scale;ox_=geo.offsetX;oy_=geo.offsetY;size_t samples=0,planes=0;
   for(auto trail=source_->trails.rbegin();trail!=source_->trails.rend();++trail)for(int segment=0;segment<31;++segment){const auto&a=trail->nodes[(trail->slot+segment)&31];const auto&b=trail->nodes[(trail->slot+segment+1)&31];
    if(!(a.point.x||a.point.y||a.point.z)||!(b.point.x||b.point.y||b.point.z)||!a.strength)break;if(a.strength<8)continue;
    auto project=[&](nativefeedback::Point3 p){return std::array<int,3>{{int((int64_t(p.x)-p.y)*30/256)+ox_*30,int((int64_t(p.x)+p.y)*15/256)-oy_*15-screenHeight(p.z,scale),-2-screenHeight(p.z,scale)}};};
    auto p=project(a.point),q=project(b.point);if(p[0]>q[0])std::swap(p,q);int lengths[3]={q[0]-p[0],std::abs(q[1]-p[1]),std::abs(q[2]-p[2])},direction[3]={1,q[1]<p[1]?-1:1,q[2]<p[2]?-1:1};
    int dominant=lengths[2]>lengths[0]&&lengths[2]>lengths[1]?2:lengths[0]>lengths[1]?0:1,count=lengths[dominant],errors[3]={-count,-count,-count};if(count<=0||count>8192||samples+count>2*1024*1024)continue;samples+=count;
    using Tile=std::pair<int,int>;std::map<Tile,std::vector<voxel::Image>>tiles;
    for(int n=0;n<count;++n){for(int axis=0;axis<3;++axis)if(axis!=dominant)errors[axis]+=2*lengths[axis];
     if(p[0]>=0&&p[1]>=0&&p[0]<geo.rawWidth*30&&p[1]<geo.rawHeight*15){auto&layers=tiles[{p[0]/32,p[1]/32}];size_t at=size_t(p[1]&31)*32+(p[0]&31);size_t layer=0;while(layer<layers.size()&&layers[layer].pixels[at])++layer;
      if(layer<layers.size()||planes<8192){if(layer==layers.size()){++planes;voxel::Image image;image.width=image.height=32;image.pixels.assign(1024,0);image.depth.assign(1024,0);image.writeDepth=false;layers.push_back(std::move(image));}
      auto&image=layers[layer];image.pixels[at]=0xff000000u|trail->color;image.depth[at]=float(p[1]+oy_*15-p[2]);}
     }
     for(int axis=0;axis<3;++axis)if(axis==dominant||errors[axis]>0){p[axis]+=direction[axis];if(axis!=dominant)errors[axis]-=2*count;}
    }
    for(auto&tile:tiles)for(auto&image:tile.second){camera::ObjectCanvas::Draw d;d.image=std::make_shared<const voxel::Image>(std::move(image));d.x=float(tile.first.first*32);d.y=float(tile.first.second*32);d.material.mixStrength=std::clamp(a.strength,1,255);draws_.push_back(std::move(d));}
   }
  }
  source_=frame.extras;
  for(const auto&d:draws_)draw(*d.image,d.x,d.y,1,1,d.base,d.light,0,0,d.image,vulkancompute::MotionIdentity{},d.material);
 }
};
}}

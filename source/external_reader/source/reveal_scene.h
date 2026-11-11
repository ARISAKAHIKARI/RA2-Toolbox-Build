#pragma once
#include "radar_map.h"
#include "main_viewport.h"
#include "scene_order.h"
#include "../../shared/source/sparse_patch_mask.h"
#include <unordered_map>
namespace external { namespace reveal {
inline uint64_t identity(const radar::Marker&m){return (uint64_t(m.object)<<32)|m.id;}
inline radar::Frame apply(const radar::Frame&native){
 auto out=native;if(native.revealed.empty())return out;
 std::unordered_map<uint64_t,const radar::Revealed*> replacements;
 for(const auto&r:native.revealed)replacements.emplace(identity(r.shown),&r);
 for(auto&m:out.markers){auto found=replacements.find(identity(m));if(found!=replacements.end()){const auto order=m.displayOrder;const bool tether=m.tetherHidden;auto redraw=m.tetherRedraw;m=found->second->shown;m.displayOrder=order;m.tetherHidden=tether;m.tetherRedraw=std::move(redraw);replacements.erase(found);}}
 for(const auto&r:native.revealed)if(replacements.count(identity(r.shown)))out.markers.push_back(r.shown);
 return out;
}
inline bool nearView(const radar::Marker&m,const mainview::View&v,double zscale){
 const double x=(double(m.x)-m.y)*30/256.-v.cameraX;
 const double y=(double(m.x)+m.y)*15/256.-screenHeight(m.z,zscale)-v.cameraY;
 return x>=-512&&x<=v.width+512&&y>=-512&&y<=v.height+512;
}
// Project every image part, including displaced ground shadows. Original and
// true shapes both contribute; a large tree cannot survive around a small tank.
inline bool rectangles(const voxel::Image&image,const radar::Marker&m,const mainview::View&v,
 int width,int height,double zscale,std::vector<overlaycomposition::Rect>&out,int nesting=0,double drop=0){
 if(nesting>8||image.incomplete||image.rasterScale<1||image.rasterScale>16)return false;
 const double sx=double(width)/v.width,sy=double(height)/v.height;
 const double px=(double(m.x)-m.y)*30/256.-v.cameraX;
 const double py=(double(m.x)+m.y)*15/256.-screenHeight(m.z,zscale)-v.cameraY+camera::anchorY(m.kind)+drop;
 if(image.width>0&&image.height>0){
  if(image.width>32768||image.height>32768)return false;
  const double scale=image.rasterScale;
  int left=int(std::floor((px+image.x/scale-2)*sx)),top=int(std::floor((py+image.y/scale-2)*sy));
  int right=int(std::ceil((px+(image.x+image.width)/scale+2)*sx)),bottom=int(std::ceil((py+(image.y+image.height)/scale+2)*sy));
  auto rect=overlaycomposition::intersection({left,top,right-left,bottom-top},{0,0,width,height});
  if(rect.w&&rect.h){if(out.size()>=2048)return false;out.push_back(rect);}
 }
 for(const auto&part:image.attachments){
  const double shadow=m.pose.shadow.known?m.pose.shadow.drop:0;
  if(part.shadow&&part.airborneShadow&&shadow<=0)continue;
  if(!rectangles(part,m,v,width,height,zscale,out,nesting+1,drop+(part.shadow?shadow:0)))return false;
 }
 return true;
}
inline bool stillPresent(const radar::Frame&current,const std::vector<uint64_t>&keys){
 if(keys.empty())return false;
 std::unordered_map<uint64_t,bool> present;for(const auto&r:current.revealed)present.emplace(identity(r.shown),true);
 for(auto key:keys)if(!present.count(key))return false;
 return true;
}
}}

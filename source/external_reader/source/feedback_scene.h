#pragma once
#include "feedback_selection.h"
#include "main_viewport.h"
#include "camera_raster.h"
namespace external { namespace nativefeedback {
// Use immutable bounds from the current rendered sprites; never decode twice.
template<class Renderer,class Frame>std::vector<Object> objectGeometry(const Renderer&renderer,const Frame&frame,const mainview::View&v,int w,int h,double zscale,int marginX=0,int marginY=0){
 std::vector<Object> out;if(!v.valid()||w<1||h<1||w>8192||h>8192||!std::isfinite(zscale)||zscale<=0||zscale>1)return out;
 double sx=double(w)/v.width,sy=double(h)/v.height;if(sx>16||sy>16)return out;
 for(const auto&m:frame.markers){if(m.kind<0||m.kind>3||m.x<0||m.y<0||m.x>=512*256||m.y>=512*256||m.z<-16384||m.z>100000)continue;
  int x=0,y=0,width=0,height=0;if(!renderer.feedbackBounds(m,x,y,width,height)||width<1||height<1||width>2048||height>2048)continue;
  double px=(double(m.x)-m.y)*30/256.-v.cameraX;
  double py=(double(m.x)+m.y)*15/256.-screenHeight(m.z,zscale)-v.cameraY+camera::anchorY(m.kind);
  int left=int(std::floor((px+x-2)*sx)),top=int(std::floor((py+y-2)*sy));
  int right=int(std::ceil((px+x+width+2)*sx)),bottom=int(std::ceil((py+y+height+2)*sy));
  if(right<-32-marginX||bottom<-32-marginY||left>w+32+marginX||top>h+32+marginY)continue;
  out.push_back({m.object,m.id,m.type,m.owner,{left,top,right-left,bottom-top},m.kind});
 }
 return out;
}
struct EffectStatus {bool complete=false;unsigned bolts=0;};
// Whole-map previews have a virtual source viewport larger than a physical
// display. Only projection fields are needed by the passive bolt rasterizer.
struct WorldView {int cameraX=0,cameraY=0,width=0,height=0,frame=-1;bool valid()const{return frame>=0&&width>0&&height>0&&width<=122880&&height<=122880&&std::abs(int64_t(cameraX))<1000000&&std::abs(int64_t(cameraY))<1000000;}};
template<class Scene,class Projection>EffectStatus effects(const std::vector<Bolt>&list,Scene&scene,const Projection&v,const std::vector<uint8_t>*mask,double zscale,int viewportWidth=0,int viewportHeight=0,int offsetX=0,int offsetY=0){
 EffectStatus status;
 if(!v.valid()||scene.width<1||scene.height<1||scene.pixels.size()!=size_t(scene.width)*scene.height||
    !std::isfinite(zscale)||zscale<=0||zscale>1||list.size()>MaxBolts)return status;
 status.complete=true;if(list.empty())return status;
 Canvas layer;if(!layer.reset(scene.width,scene.height))return {};
 double sx=double(viewportWidth>0?viewportWidth:scene.width)/v.width,sy=double(viewportHeight>0?viewportHeight:scene.height)/v.height;if(sx<=0||sy<=0||sx>16||sy>16)return {};
 for(const auto&e:list){
  auto point=[&](Point3 p){return std::array<double,2>{{((double(p.x)-p.y)*30/256.-v.cameraX)*sx+offsetX,
    ((double(p.x)+p.y)*15/256.-screenHeight(p.z,zscale)-v.cameraY)*sy+offsetY}};};
  auto a=point(e.source),b=point(e.target);
  // Cull before strand construction: large battles can contain thousands of
  // offscreen bolts. The margin includes all jitter and the glow footprint.
  const double scale=std::min(sx,sy),margin=8*scale+8;
  if(std::max(a[0],b[0])<-margin||std::min(a[0],b[0])>scene.width+margin||std::max(a[1],b[1])<-margin||std::min(a[1],b[1])>scene.height+margin){++status.bolts;continue;}
  // Deterministic visual jitter only; no native RNG/FLH/lifetime updates.
  bolt(layer,a[0],a[1],b[0],b[1],e.seed^uint32_t(v.frame),e.alternate,std::min(sx,sy));++status.bolts;
 }
 if(!applyVisibility(layer,mask))return {};
 for(size_t i=0;i<scene.pixels.size();++i)if(layer.pixels[i])scene.pixels[i]=over(layer.pixels[i],scene.pixels[i]);
 return status;
}
}}

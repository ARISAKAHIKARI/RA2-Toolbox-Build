#pragma once
#include "scene_extra_layers.h"
namespace external { namespace sceneextras {
inline bool hasRadar(const radar::Frame&frame){return frame.extras&&(!frame.extras->radar.empty()||!frame.extras->beacons.empty());}
// RadarClass660000 and BeaconManager431700 order. No event admission, EVA,
// beacon placement, message editing or Spacebar-history writes occur here.
template<class Read,class Assets,class Sink>void radarDecorations(const radar::Frame&frame,int width,int height,Read read,Assets&assets,Layers&layers,Sink&sink){
 if(!frame.extras||!frame.terrain)return;const auto&geo=frame.terrain->geo;const double sx=double(width)/geo.rw,sy=double(height)/geo.rh;
 for(const auto&e:frame.extras->radar){uint32_t bright=e.type==0||e.type==3||e.type==4?0xffffff:e.type==5?0x00ffff:e.type==1||e.type==2||e.type==11||e.type==12?0xffff00:0;if(!bright)continue;
  uint32_t color=0;for(int sh:{0,8,16})if((bright>>sh)&255)color|=unsigned(128+127*std::clamp(e.fade,0.f,1.f))<<sh;
  int dx=int(e.radius*std::cos(e.rotation)),dy=int(e.radius*std::sin(e.rotation));int points[4][2]={{e.x+dx,e.y+dy},{e.x-dy,e.y+dx},{e.x-dx,e.y-dy},{e.x+dy,e.y-dx}};
  for(int n=0;n<4;++n)sink.uiLine({points[n][0]*sx,points[n][1]*sy},{points[(n+1)&3][0]*sx,points[(n+1)&3][1]*sy},color);
 }
 for(const auto&b:frame.extras->beacons)if(auto image=layers.radarBeacon(frame,read,assets,b.owner)){double u=0,v=0;geo.project(double(b.point.x)/256,double(b.point.y)/256,u,v);double x=std::clamp(u,0.,1.)*width,y=std::clamp(v,0.,1.)*height;sink.uiImage(image,layers.imageBytes(image),x,y,sx,sy);}
}
class RadarCPU {
 radar::Scene&scene_;
public:
 explicit RadarCPU(radar::Scene&scene):scene_(scene){}
 void uiLine(std::array<double,2>a,std::array<double,2>b,uint32_t color){
  double dx=b[0]-a[0],dy=b[1]-a[1],lo=0,hi=1;auto clip=[&](double p,double q){if(p==0)return q>=0;double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};if(!clip(-dx,a[0])||!clip(dx,scene_.width-1-a[0])||!clip(-dy,a[1])||!clip(dy,scene_.height-1-a[1]))return;
  int x=int(a[0]+lo*dx),y=int(a[1]+lo*dy),xx=int(a[0]+hi*dx),yy=int(a[1]+hi*dy);if(x>xx){std::swap(x,xx);std::swap(y,yy);}int lx=xx-x,ly=std::abs(yy-y),direction=y>yy?-1:1,count=std::max(lx,ly)+int(!lx||!ly),error=2*std::min(lx,ly)-std::max(lx,ly);
  for(int n=0;n<count;++n){if(x>=0&&x<scene_.width&&y>=0&&y<scene_.height)scene_.pixels[size_t(y)*scene_.width+x]=0xff000000u|color;if(lx>ly){if(error>0){y+=direction;error-=2*lx;}++x;error+=2*ly;}else{if(error>0){++x;error-=2*ly;}y+=direction;error+=2*lx;}}
 }
 void uiImage(const std::shared_ptr<const voxel::Image>&i,const std::shared_ptr<const std::vector<uint8_t>>&,double x,double y,double sx,double sy){
  double left=x+i->x*sx,top=y+i->y*sy;int l=std::clamp(int(std::floor(left)),0,scene_.width),t=std::clamp(int(std::floor(top)),0,scene_.height),r=std::clamp(int(std::ceil(left+i->width*sx)),0,scene_.width),b=std::clamp(int(std::ceil(top+i->height*sy)),0,scene_.height);
  for(int yy=t;yy<b;++yy)for(int xx=l;xx<r;++xx){int ix=int(std::floor((xx+.5-left)/sx)),iy=int(std::floor((yy+.5-top)/sy));if(ix<0||iy<0||ix>=i->width||iy>=i->height)continue;uint32_t p=i->pixels[size_t(iy)*i->width+ix];if(p>>24)scene_.pixels[size_t(yy)*scene_.width+xx]=p;}
 }
};
// Compose schematic markers on the worker as well: objects precede events
// and beacons in both panels, including while a panel scales an older frame.
template<class Read,class Assets>std::shared_ptr<const radar::Scene> decorateRadar(std::shared_ptr<const radar::Scene>scene,const radar::Frame&frame,Read read,Assets&assets,Layers&layers){
 if(!scene||scene->pixels.size()!=size_t(scene->width)*scene->height)return scene;if(frame.markers.empty()&&!hasRadar(frame))return scene;
 auto next=std::make_shared<radar::Scene>(*scene);for(const auto&m:frame.markers){if(!std::isfinite(m.u)||!std::isfinite(m.v)||m.u<0||m.u>=1||m.v<0||m.v>=1)continue;
  int x=int(m.u*next->width),y=int(m.v*next->height),radius=m.building?2:1;for(int yy=std::max(0,y-radius);yy<std::min(next->height,y+radius+1);++yy)for(int xx=std::max(0,x-radius);xx<std::min(next->width,x+radius+1);++xx)next->pixels[size_t(yy)*next->width+xx]=0xff000000u|m.color;
 }RadarCPU sink(*next);radarDecorations(frame,next->width,next->height,read,assets,layers,sink);return next;
}
}}

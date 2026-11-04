#pragma once
#include "native_viewport_host.h"
#include "radar_paint_gate.h"
#include "../../external_reader/source/radar_navigation.h"

namespace assist { namespace minimapview {
// The Tactical viewport is R while zoomed, otherwise the live native map
// rectangle. Screen dimensions include UI and cannot stand in for this span.
template<class Shared,class Read>bool sample(const Shared*shared,Read read,const external::radar::Geometry&geometry,external::mainview::View&out){
 external::mainview::View view;
 if(!geometry.valid()||!nativeviewhost::sample(shared,read,view)||view.house!=geometry.house||external::mapbounds::sample(read)!=geometry.bounds)return false;
 out=view;return true;
}
inline bool rectangle(const external::radar::Geometry&geometry,const external::mainview::View&view,int width,int height,radargate::Mark&out){
 if(!geometry.valid()||!view.valid()||width<3||height<3)return false;
 const auto crop=external::mainview::crop(view,geometry.offsetX,geometry.offsetY);
 const double worldWidth=geometry.rawWidth*30.,worldHeight=geometry.rawHeight*15.;
 const int mapWidth=width-2,mapHeight=height-2;
 // FrameRect and the GPU frame both use exclusive right/bottom endpoints.
 // A normalized endpoint of one must reach the interior's exclusive edge.
 auto edge=[](double value,int span){return 1+int(std::clamp(value,0.,1.)*span);};
 out={edge(crop.x/worldWidth,mapWidth),edge(crop.y/worldHeight,mapHeight),
      edge((crop.x+crop.width)/worldWidth,mapWidth),edge((crop.y+crop.height)/worldHeight,mapHeight),0};return true;
}
inline bool navigate(const external::radar::Geometry&geometry,const external::mainview::View&view,int px,int py,int width,int height,int&x,int&y){
 return view.valid()&&external::radar::navigate(geometry,px,py,width,height,view.width,view.height,x,y);
}
}}

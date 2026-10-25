#include "../../external_reader/source/map_bounds.h"
#include "../source/small_map_camera.h"
#include <cassert>
#include <cstring>
#include <iostream>
using external::mapbounds::Bounds;
int main(int argc,char**){
 if(argc>1){Bounds b;int x,y,level,slope;while(std::cin>>b.width>>b.height>>b.left>>b.top>>b.columns>>b.rows>>x>>y>>level>>slope)std::cout<<b.legal(x,y)<<' '<<b.usable(x,y,level,slope!=0)<<'\n';return 0;}
 Bounds b{131,146,3,5,124,135};assert(b.valid());
 assert(!b.legal(65,66)&&b.legal(66,66));
 assert(!b.usable(68,73)&&b.usable(69,73));
 assert(!b.usable(71,73,2,true)&&b.usable(71,73,2,false));
 assert(!b.usable(8,133)&&b.usable(9,133));
 assert(!b.usable(135,12)&&b.usable(134,12));
 // Native camera limits for odd/even viewports, plus the explicit midpoint
 // remedy when the viewport is larger than the whole available camera area.
 for(auto map:{b,Bounds{32,64,5,8,12,16}})for(int w:{319,320,639,640,1599,1600})for(int h:{255,256,511,512,999,1000})for(int x:{-10000,0,10000})for(int y:{-10000,0,10000}){
  assist::smallcamera::Geometry g{map.width,map.left,map.top,map.columns,map.rows,w,h};assist::smallcamera::Point p{x,y};assist::smallcamera::clamp(g,p);
  double xx=x,yy=y;assert(map.clampCamera(xx,yy,w,h)&&xx==p.x&&yy==p.y);
 }
 Bounds small{32,64,5,8,12,16};double x=100000,y=-100000;assert(small.clampCamera(x,y,1600,1000));
 auto area=small.cameraExtent();assert(x==std::trunc((area.left+area.right)/2)&&y==std::trunc((area.top+area.bottom)/2));
 // Camera extents are scroll limits; legal non-usable cells remain drawable.
 assert(b.legal(68,73)&&!b.usable(68,73));
 // Bounds change independently of match/cell identity. A torn/failed read
 // must not construct a valid hybrid rectangle or silently widen the map.
 std::array<int32_t,6> first{{131,146,3,5,124,135}},next=first;int reads=0;
 auto read=[&](uint32_t at,void*out,size_t n){assert(at==external::mapbounds::Map+0xF4&&n==24);std::memcpy(out,(++reads==1?first:next).data(),n);return true;};
 assert(external::mapbounds::sample(read)==b);reads=0;next[2]=4;assert(!external::mapbounds::sample(read).valid());
 auto fail=[](uint32_t,void*,size_t){return false;};assert(!external::mapbounds::sample(fail).valid());
 assert(!(Bounds{131,146,3,5,130,135}).valid());
 std::cout<<"PASS live map bounds, height/slope edge, camera clamp, small-map camera midpoint and torn rectangle rejection\n";
}

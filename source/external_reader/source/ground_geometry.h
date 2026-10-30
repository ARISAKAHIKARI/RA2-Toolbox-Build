#pragma once
#include "voxel_shadow.h"
#include "native_feedback.h"
#include "building_animations.h"
#include "map_bounds.h"
namespace external {namespace ground {
template<class Read>bool at(Read read,nativefeedback::Point3 point,int&height,uint32_t&cell,bool bridge=true){
 GameView game(read);if(!nativefeedback::worldPoint(point)||point.x<0||point.y<0)return false;int x=point.x/256,y=point.y/256,index=y*512+x,count=game.template get<int>(mapbounds::Map+0x140);uint32_t items=game.template get<uint32_t>(mapbounds::Map+0x13C);if(items<0x10000||index<0||index>=count||count>512*512||!read(items+index*4,&cell,4)||cell<0x10000)return false;int16_t xy[2]{};int8_t level=0;uint8_t ramp=0;int unit=0;if(!read(cell+0x24,xy,4)||xy[0]!=x||xy[1]!=y||!read(cell+0x11B,&level,1)||!read(cell+0x11C,&ramp,1)||ramp>20||!read(0x89E7C0,&unit,4))return false;
 std::array<double,5>terms{};double scale=1;if(ramp&&(!read(0x81C900+(ramp-1)*40,terms.data(),40)||!read(0x89E740,&scale,8)))return false;if(!voxelshadow::floorHeight(level,unit,point.x,point.y,ramp,scale,terms,height))return false;if(bridge&&(game.template get<uint32_t>(cell+0x140)&0x100)){int deck=game.template get<int>(0xAC13BC);if(deck<1||deck>8192)return false;height+=deck;}return true;
}
template<class Read>bool target(Read read,uint32_t pointer,nativefeedback::Point3&out){
 if(pointer<0x10000)return false;GameView game(read);int kind=game.kind(pointer);if(kind>=0){if(!game.live(pointer)||!read(pointer+0x9C,&out,12))return false;if(kind==3){int w=0,h=0;if(!animations::foundation(read,game.type(pointer,kind),w,h))return false;out.x+=(w-1)*128;out.y+=(h-1)*128;}}
 else{int16_t xy[2]{};if(!read(pointer+0x24,xy,4)||!mapbounds::sample(read).legal(xy[0],xy[1]))return false;out={xy[0]*256+128,xy[1]*256+128,0};}
 uint32_t cell=0;int height=0;if(!at(read,out,height,cell)||kind<0&&cell!=pointer)return false;out.z=height;return true;
}
}}

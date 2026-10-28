#pragma once
#include "../../external_reader/source/gpu_shroud.h"
#include <functional>

namespace shroudcases {
using namespace external;
inline radar::Geometry geometry(){return {128,128,128,128,256,256,1,0x100000,0x300000,512*512,{128,128,0,0,128,128},256,256};}
inline std::vector<radar::shroudmask::Cell> cells(const radar::Geometry&g){
 std::vector<radar::shroudmask::Cell> result;constexpr int8_t frames[]={-2,-1,0,1,7,15,31,46,127};
 for(int y=1;y<512;++y)for(int x=1;x<512;++x)if(g.legal(x,y))result.push_back({uint32_t(y*512+x),{frames[(x*13+y*7)%9],frames[(x*5+y*11)%8]}});
 // Stable sorting must preserve conflicting equal-YSort and duplicate stamps.
 if(!result.empty()){auto copy=result[result.size()/2];copy.shade={-1,46};result.insert(result.begin()+7,copy);}
 return result;
}
inline bool shape(bool fog,int f,voxel::Image&i,int version=0){
 i={};i.canvasWidth=60;i.canvasHeight=30;i.width=60-(f%4)*7;i.height=30-(f%3)*5;i.x=-30+(f%4)*3;i.y=-15+(f%3)*2;
 i.indices.resize(size_t(i.width)*i.height);
 for(int y=0;y<i.height;++y)for(int x=0;x<i.width;++x){unsigned v=unsigned(x*17+y*31+f*19+(fog?43:0)+version*7);
  i.indices[size_t(y)*i.width+x]=(v%11==0)?254:uint8_t(v%128);
 }return true;
}
struct Case {int w,h;double spanX,spanY,x,y;bool fog;};
inline std::vector<Case> cases(){return {
 {240,120,240,120,0,0,false},{240,120,240,120,0,0,true},
 {900,450,900,450,3300,500,false},{900,450,900,450,3300,500,true},
 {1350,675,900,450,3300.25,500.75,true},{960,540,640,360,3000.125,400.625,true},
 {4,3,2048,1600,2100.125,100.75,true},{31,17,2048,2048,2200.25,-2.125,true},
 {97,41,97,41,3659.125,719.5,true},{171,93,114,62,7000.5,3800.25,true},
 {129,87,258,174,-11.75,-7.5,true},{513,257,513,257,7620.5,3810.25,false},
 {3840,2160,1920,1080,3000.125,400.25,true}};}
}

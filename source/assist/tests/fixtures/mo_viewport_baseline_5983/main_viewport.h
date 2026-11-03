#pragma once
#include <cstdint>
#include <cmath>
#include "display_state.h"
#include "projection.h"
namespace external { namespace mainview {
struct View {
 uint32_t scenario=0,house=0,tactical=0;int frame=-1;
 int cameraX=0,cameraY=0,screenW=0,screenH=0,x=0,y=0,width=0,height=0;
 bool valid()const{
  return scenario>=0x10000&&house>=0x10000&&tactical>=0x10000&&frame>=0&&
   screenW>=320&&screenH>=200&&screenW<=8192&&screenH<=8192&&
   x>=0&&y>=0&&width>0&&height>0&&int64_t(x)+width<=screenW&&int64_t(y)+height<=screenH&&
   uint64_t(screenW)*screenH<=16*1024*1024&&std::abs(int64_t(cameraX))<1000000&&std::abs(int64_t(cameraY))<1000000;
 }
};
inline bool sameViewport(const View&a,const View&b){
 return a.scenario==b.scenario&&a.house==b.house&&a.tactical==b.tactical&&
  a.cameraX==b.cameraX&&a.cameraY==b.cameraY&&a.screenW==b.screenW&&a.screenH==b.screenH&&
  a.x==b.x&&a.y==b.y&&a.width==b.width&&a.height==b.height;
}
template<class Read>bool sample(Read read,View&out){
 View a;int rect[4]{};uint8_t running=0;
 if(!read(0xA8B230,&a.scenario,4)||!read(0xA83D4C,&a.house,4)||!read(0x887324,&a.tactical,4)||a.tactical<0x10000||
  !read(0xA8ED84,&a.frame,4)||!read(0xA8E9A0,&running,1)||running!=1||!display::inspect(read).visible()||
  !read(0x8A00A4,&a.screenW,4)||!read(0x8A00A8,&a.screenH,4)||
  !read(a.tactical+0xB0,&a.cameraX,4)||!read(a.tactical+0xB4,&a.cameraY,4)||
  !read(0x886FA0,rect,sizeof rect))return false;
 a.x=rect[0];a.y=rect[1];a.width=rect[2];a.height=rect[3];
 if(!a.valid())return false;
 int verify[4]{},camera[2]{};uint32_t tactical=0,scenario=0,house=0;
 if(!read(0x887324,&tactical,4)||tactical!=a.tactical||!read(a.tactical+0xB0,camera,sizeof camera)||
  camera[0]!=a.cameraX||camera[1]!=a.cameraY||!read(0x886FA0,verify,sizeof verify)||
  !read(0xA8B230,&scenario,4)||scenario!=a.scenario||!read(0xA83D4C,&house,4)||house!=a.house)return false;
 for(int i=0;i<4;++i)if(verify[i]!=rect[i])return false;
 out=a;return true;
}
struct Crop {double x=0,y=0,width=0,height=0;};
// Camera is the native projected origin, not unit/world coordinates and not
// a radar percentage. Destination viewport origin is kept separate.
inline Crop crop(const View&v,int radarOffsetX,int radarOffsetY){
 return {double(v.cameraX)+radarOffsetX*30.,double(v.cameraY)-radarOffsetY*15.,double(v.width),double(v.height)};
}
}}

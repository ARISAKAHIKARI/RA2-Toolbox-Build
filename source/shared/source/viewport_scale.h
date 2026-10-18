#pragma once
#include <cmath>
namespace viewport {
struct Scale {
 int gameWidth=0,gameHeight=0,clientWidth=0,clientHeight=0;
 float x=1,y=1;
 bool set(int gw,int gh,int cw,int ch){
  if(gw<320||gh<200||gw>16384||gh>16384||cw<100||ch<100||cw>16384||ch>16384)return false;
  gameWidth=gw;gameHeight=gh;clientWidth=cw;clientHeight=ch;x=float(cw)/gw;y=float(ch)/gh;return true;
 }
 int width(float n)const{return int(std::ceil(n*x));}
 int height(float n)const{return int(std::ceil(n*y));}
 int left(float n)const{return int(std::floor(n*x));}
 int top(float n)const{return int(std::floor(n*y));}
};
}

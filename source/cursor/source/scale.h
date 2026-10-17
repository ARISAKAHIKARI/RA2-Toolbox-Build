#pragma once
#include <cmath>
#include <algorithm>
struct Scale {double x,y;};
inline Scale cursorScale(bool automatic,bool fit,int percent,int cw,int ch,int gw,int gh){
 double x=1,y=1;
 if(automatic&&cw>0&&ch>0&&gw>=320&&gh>=200&&gw<=16384&&gh<=16384){x=double(cw)/gw;y=double(ch)/gh;if(fit)x=y=std::min(x,y);}
 double factor=std::max(25,std::min(400,percent))/100.0;
 return {std::max(0.125,std::min(8.0,x*factor)),std::max(0.125,std::min(8.0,y*factor))};
}
inline int scaledSize(int n,double factor){return std::max(1,(int)std::lround(n*factor));}
inline int scaledHotspot(int n,int src,int dst,int offset){return (int)std::lround(double(n)*dst/src)+offset;}

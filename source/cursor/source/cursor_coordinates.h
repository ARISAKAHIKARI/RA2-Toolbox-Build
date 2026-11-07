#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cursorcoordinates {
struct Point {int x=0,y=0;};
struct Rect {
 int left=0,top=0,right=0,bottom=0;
 int width()const{return int(int64_t(right)-left);}
 int height()const{return int(int64_t(bottom)-top);}
 bool valid()const{return int64_t(right)>left&&int64_t(bottom)>top&&int64_t(right)-left<=131072&&int64_t(bottom)-top<=131072;}
 bool contains(Point point)const{return valid()&&point.x>=left&&point.y>=top&&point.x<right&&point.y<bottom;}
};
inline int pixel(double value){return int(std::clamp(std::round(value),double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max())));}
// A cursor position read in screen pixels already belongs to the output.
// Image/DPI/viewport scaling changes its raster and anchor, never this position.
inline Point spriteOrigin(Point screen,int anchorX,int anchorY){return {pixel(double(screen.x)-anchorX),pixel(double(screen.y)-anchorY)};}
struct Viewport {
 Rect client{};double sx=0,sy=0,offsetX=0,offsetY=0;
 bool set(Rect bounds,int gameWidth,int gameHeight,bool aspectFit){
  client=bounds;sx=sy=offsetX=offsetY=0;
  if(!client.valid()||gameWidth<320||gameHeight<200||gameWidth>16384||gameHeight>16384)return false;
  sx=double(client.width())/gameWidth;sy=double(client.height())/gameHeight;
  // Letterboxing is an explicit existing preference, not inferred from the
  // monitor/game resolution ratio or the platform's executable name.
  if(aspectFit){sx=sy=std::min(sx,sy);offsetX=(client.width()-gameWidth*sx)/2;offsetY=(client.height()-gameHeight*sy)/2;}
  return true;
 }
 Point project(double x,double y)const{return {pixel(client.left+offsetX+x*sx),pixel(client.top+offsetY+y*sy)};}
 Rect project(Rect gameRect)const{const auto a=project(gameRect.left,gameRect.top),b=project(gameRect.right,gameRect.bottom);return {a.x,a.y,b.x,b.y};}
};
}

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace external { namespace mapbounds {
constexpr uint32_t Map=0x87F7E8;
// The native radar stores a flat projection span. A backing bitmap rounds up
// to integer pixels; presentation must retain the exact span so the rounded
// last row/column does not become an extra strip of map background.
struct NativeViewport {
 double width=0,height=0;
 bool valid()const{return std::isfinite(width)&&std::isfinite(height)&&width>0&&height>0&&width<=32768&&height<=32768;}
 int backingWidth()const{return valid()?int(std::ceil(width)):0;}
 int backingHeight()const{return valid()?int(std::ceil(height)):0;}
};
inline NativeViewport nativeViewport(int radarWidth,int radarHeight,double factor){
 if(radarWidth<=0||radarHeight<=0||!std::isfinite(factor)||factor<=0)return {};
 NativeViewport out{radarWidth*30./factor,radarHeight*15./factor};return out.valid()?out:NativeViewport{};
}
struct TileOrigin {int x=0,y=0;};
// MapRect dimensions and the LIVE VisibleRect. Scenario triggers change the
// latter without replacing the cell array or advancing to a new match.
struct Bounds {
 int width=0,height=0,left=0,top=0,columns=0,rows=0;
 bool valid()const{return width>0&&width<=256&&height>0&&height<=256&&left>=0&&top>=0&&columns>0&&rows>0&&left<=width-columns&&top<=height-rows;}
 bool operator==(const Bounds&o)const{return width==o.width&&height==o.height&&left==o.left&&top==o.top&&columns==o.columns&&rows==o.rows;}
 bool operator!=(const Bounds&o)const{return !(*this==o);}
 // MapClass::CoordinatesLegal (568300), plus safe external-array limits.
 bool legal(int x,int y)const{return valid()&&x>0&&y>0&&x<512&&y<512&&x+y>width&&x-y<width&&y-x<width&&x+y<=width+2*height;}
 // Native 6D51F6/6D5215 supplies the TMP's left/top, not its center;
 // 480416 sign-extends Level and 48042E subtracts Level*15 from Y.
 bool tileOrigin(int x,int y,int level,TileOrigin&out)const{
  if(!legal(x,y)||level<-128||level>127)return false;out={(x-y)*30-30,(x+y-level)*15};return true;
 }
 // MapClass::IsWithinUsableArea(CellClass*,true), 578540. This is a
 // gameplay-area predicate, NOT the tactical renderer's cell/pixel mask.
 // Raised cells and the top-edge slope byte follow the native predicate.
 bool usable(int x,int y,int level=0,bool slope=false)const{
  if(!legal(x,y))return false;
  if(slope&&x+y<width+2*top+4+level)++level;
  return x+y>width+2*top+level&&x+y<=width+2*(top+rows)+2+level&&x-y<2*(left+columns)-width&&y-x<width-2*left;
 }
 struct CameraExtent {double left,right,top,bottom;};
 // Native tactical-camera limits (6D8640), before adding/subtracting the
 // viewport half size. Preserve its space above tall edge buildings.
 CameraExtent cameraExtent()const{return {30.*(2*left-width),30.*(2*(left+columns)-width),15.*(2*top+width-5),15.*(2*(top+rows)+width+4)};}
 bool clampCamera(double&x,double&y,double viewWidth,double viewHeight)const{
  if(!valid()||!std::isfinite(x)||!std::isfinite(y)||viewWidth<=0||viewHeight<=0)return false;
  auto b=cameraExtent();auto bound=[](double p,double lo,double hi){return hi<lo?std::trunc((lo+hi)/2):std::clamp(p,lo,hi);};
  // Native pixel viewports divide by two with integer truncation. For odd
  // sizes the right/bottom margin receives the remaining pixel.
  double halfWidth=std::trunc(viewWidth/2),halfHeight=std::trunc(viewHeight/2);
  x=bound(x,b.left+halfWidth,b.right-viewWidth+halfWidth);y=bound(y,b.top+halfHeight,b.bottom-viewHeight+halfHeight);return true;
 }

};
template<class Read>Bounds sample(Read read){
 std::array<int32_t,6> first{},again{};
 if(!read(Map+0xF4,first.data(),sizeof first)||!read(Map+0xF4,again.data(),sizeof again)||first!=again)return {};
 Bounds b{first[0],first[1],first[2],first[3],first[4],first[5]};return b.valid()?b:Bounds{};
}
}}

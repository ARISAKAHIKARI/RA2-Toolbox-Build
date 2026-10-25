#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace external { namespace mapbounds {
constexpr uint32_t Map=0x87F7E8;
// MapRect dimensions and the LIVE VisibleRect. Scenario triggers change the
// latter without replacing the cell array or advancing to a new match.
struct Bounds {
 int width=0,height=0,left=0,top=0,columns=0,rows=0;
 bool valid()const{return width>0&&width<=256&&height>0&&height<=256&&left>=0&&top>=0&&columns>0&&rows>0&&left<=width-columns&&top<=height-rows;}
 bool operator==(const Bounds&o)const{return width==o.width&&height==o.height&&left==o.left&&top==o.top&&columns==o.columns&&rows==o.rows;}
 bool operator!=(const Bounds&o)const{return !(*this==o);}
 // MapClass::CoordinatesLegal (568300), plus safe external-array limits.
 bool legal(int x,int y)const{return valid()&&x>0&&y>0&&x<512&&y<512&&x+y>width&&x-y<width&&y-x<width&&x+y<=width+2*height;}
 // MapClass::IsWithinUsableArea(CellClass*,true), 578540. Raised cells
 // and the top-edge slope byte use the same projected boundary as native.
 bool usable(int x,int y,int level=0,bool slope=false)const{
  if(!legal(x,y))return false;
  if(slope&&x+y<width+2*top+4+level)++level;
  return x+y>width+2*top+level&&x+y<=width+2*(top+rows)+2+level&&x-y<2*(left+columns)-width&&y-x<width-2*left;
 }
 struct Screen {double left,right,top,bottom;};
 // Native tactical-camera limits (6D8640), before adding/subtracting the
 // viewport half size. Preserve its space above tall edge buildings.
 Screen screen()const{return {30.*(2*left-width),30.*(2*(left+columns)-width),15.*(2*top+width-5),15.*(2*(top+rows)+width+4)};}
 bool clampCamera(double&x,double&y,double viewWidth,double viewHeight)const{
  if(!valid()||!std::isfinite(x)||!std::isfinite(y)||viewWidth<=0||viewHeight<=0)return false;
  auto b=screen();auto bound=[](double p,double lo,double hi){return hi<lo?std::trunc((lo+hi)/2):std::clamp(p,lo,hi);};
  x=bound(x,b.left+viewWidth/2,b.right-viewWidth/2);y=bound(y,b.top+viewHeight/2,b.bottom-viewHeight/2);return true;
 }
 // Canvas positions are origin + native screen coordinate * scale. This
 // also clears margins when the entire visible map is smaller than a view.
 void clip(std::vector<uint32_t>&pixels,int w,int h,double ox,double oy,double sx,double sy,uint32_t outside=0xff080c08u)const{
  if(!valid()||w<1||h<1||pixels.size()!=size_t(w)*h||!std::isfinite(ox)||!std::isfinite(oy)||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0)return;
  auto b=screen();auto edge=[](double p,int size){return int(std::clamp(std::ceil(p-.5),0.,double(size)));};
  int x0=edge(ox+b.left*sx,w),x1=edge(ox+b.right*sx,w),y0=edge(oy+b.top*sy,h),y1=edge(oy+b.bottom*sy,h);
  for(int y=0;y<h;++y){auto row=pixels.begin()+size_t(y)*w;if(y<y0||y>=y1)std::fill(row,row+w,outside);else{std::fill(row,row+x0,outside);std::fill(row+x1,row+w,outside);}}
 }
};
template<class Read>Bounds sample(Read read){
 std::array<int32_t,6> first{},again{};
 if(!read(Map+0xF4,first.data(),sizeof first)||!read(Map+0xF4,again.data(),sizeof again)||first!=again)return {};
 Bounds b{first[0],first[1],first[2],first[3],first[4],first[5]};return b.valid()?b:Bounds{};
}
}}

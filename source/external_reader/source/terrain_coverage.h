#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
// Coverage of a sparse native terrain bank. Drawing-source reach is separate
// from output coverage: raised/extra TMPs may originate well outside the crop.
namespace external { namespace terraincoverage {
struct Region {
 double left=0,top=0,right=0,bottom=0;int canvasWidth=0,canvasHeight=0;bool ready=false;
 static bool input(double x,double y,double w,double h,int cw,int ch){
  return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(w)&&std::isfinite(h)&&
   std::isfinite(x+w)&&std::isfinite(y+h)&&w>0&&h>0&&cw>0&&ch>0&&cw<=32768&&ch<=32768;
 }
 static Region plan(double x,double y,double w,double h,int cw,int ch,int padding=256){
  Region out;if(!input(x,y,w,h,cw,ch)||padding<4||padding>2048)return out;
  constexpr double quantum=256.;
  auto clip=[](double v,int maximum){return std::clamp(v,0.,double(maximum));};
  out.left=clip(std::floor((x-padding)/quantum)*quantum,cw);
  out.top=clip(std::floor((y-padding)/quantum)*quantum,ch);
  out.right=clip(std::ceil((x+w+padding)/quantum)*quantum,cw);
  out.bottom=clip(std::ceil((y+h+padding)/quantum)*quantum,ch);
  out.canvasWidth=cw;out.canvasHeight=ch;out.ready=true;return out;
 }
 bool contains(double x,double y,double w,double h,int cw,int ch)const{
  if(!ready||cw!=canvasWidth||ch!=canvasHeight||!input(x,y,w,h,cw,ch))return false;
  // Source filtering reads a few pixels outside the requested output. Areas
  // beyond the actual native backing are transparent and require no pages.
  const double l=std::clamp(x-4.,0.,double(cw)),t=std::clamp(y-4.,0.,double(ch));
  const double r=std::clamp(x+w+4.,0.,double(cw)),b=std::clamp(y+h+4.,0.,double(ch));
  return r<=l||b<=t||(l>=left&&t>=top&&r<=right&&b<=bottom);
 }
 bool intersects(double x,double y,double w,double h)const{
  return ready&&right>left&&bottom>top&&x<right&&x+w>left&&y<bottom&&y+h>top;
 }
 bool tile(int tileLeft,int tileTop)const{
  // terrain::tile bounds the union of diamond and extra image to 512x512.
  // The union can extend to either side of its nominal tile left/top.
  return intersects(double(tileLeft)-512.,double(tileTop)-512.,1024.,1024.);
 }
 bool rgb(int flatLeft,int flatTop)const{return intersects(flatLeft,flatTop,60.,15.);}
};
}}

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
namespace rotation {
using Image=std::vector<uint32_t>;
constexpr double pi=3.14159265358979323846;
inline uint32_t nearest(const Image& a,int w,int h,double x,double y){
 int xx=(int)std::lround(x),yy=(int)std::lround(y);
 return (xx>=0&&xx<w&&yy>=0&&yy<h)?a[yy*w+xx]:0;
}
inline uint32_t bilinear(const Image& a,int w,int h,double x,double y){
 int ix=(int)std::floor(x),iy=(int)std::floor(y);double fx=x-ix,fy=y-iy;
 double totals[4]={};for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
  int xx=ix+dx,yy=iy+dy;if(xx<0||yy<0||xx>=w||yy>=h)continue;
  double weight=(dx?fx:1-fx)*(dy?fy:1-fy);uint32_t c=a[yy*w+xx];
  for(int k=0;k<4;k++)totals[k]+=weight*double((c>>(k*8))&255);
 }
 unsigned alpha=(unsigned)std::clamp((int)std::lround(totals[3]),0,255);
 uint32_t out=alpha<<24;for(int k=0;k<3;k++)out|=uint32_t(std::clamp((int)std::lround(totals[k]),0,(int)alpha))<<(k*8);
 return out;
}
struct Fit {bool valid=false;double angle=0,dx=0,dy=0,score=0,baseScore=0,scale=1;};
// Angle is rotation of A toward B, with the visual canvas center as pivot.
inline double score(const Image& a,const Image& b,int w,int h,double angle,double dx,double dy,int stride,bool linear=false,double scale=1.0){
 double cx=(w-1)*.5,cy=(h-1)*.5,cs=std::cos(angle*pi/180),sn=std::sin(angle*pi/180);
 double error=0,weight=0;
 for(int y=0;y<h;y+=stride)for(int x=0;x<w;x+=stride){
  double px=(x-cx-dx)/scale,py=(y-cy-dy)/scale;
  uint32_t av=linear?bilinear(a,w,h,cx+cs*px+sn*py,cy-sn*px+cs*py):nearest(a,w,h,cx+cs*px+sn*py,cy-sn*px+cs*py),bv=b[y*w+x];
  int aa=av>>24,ba=bv>>24;if(aa==0&&ba==0)continue;
  double aw=double(aa)/255,bw=double(ba)/255;
  error+=180*std::abs(aw-bw);weight+=180*std::max(aw,bw);
  if(aa>16&&ba>16){double common=std::min(aw,bw);for(int s=0;s<24;s+=8){error+=common*std::abs(int((av>>s)&255)-int((bv>>s)&255))/3.;weight+=common*255./3.;}}
 }
 return weight>1?error/weight:0;
}
// Rigid + modest uniform scale. Every candidate is scored over alpha and color;
// local deformation is expected and is handled later in the residual field.
inline Fit detect(const Image& a,const Image& b,int w,int h){
 Fit result;if(a==b||w<8||h<8)return result;
 const int stride=(w*h<=2304)?1:2;
 double transl=std::numeric_limits<double>::infinity();
 for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++)
  transl=std::min(transl,score(a,b,w,h,0,dx,dy,stride));
 double best=std::numeric_limits<double>::infinity(),angle=0,tx=0,ty=0,scaleValue=1;
 for(double sc:{0.90,1.0,1.10})for(int degree=-135;degree<=135;degree+=5){
  double cand=score(a,b,w,h,double(degree),0,0,stride,false,sc);
  if(cand<best){best=cand;angle=degree;scaleValue=sc;}
 }
 if(std::abs(angle)<3 && std::abs(scaleValue-1)<0.05)return result;
 double coarseAngle=angle,coarseScale=scaleValue;
 best=std::numeric_limits<double>::infinity();
 for(int di=-7;di<=7;di++)for(int oy=-4;oy<=4;oy++)for(int ox=-4;ox<=4;ox++)for(int si=-1;si<=1;si++){
  double degree=coarseAngle+di*.5,dx=ox*.5,dy=oy*.5;
  double sc=std::clamp(coarseScale+si*.025,0.78,1.22);
  double cand=score(a,b,w,h,degree,dx,dy,stride,true,sc);
  if(cand<best){best=cand;angle=degree;tx=dx;ty=dy;scaleValue=sc;}
 }
 // Avoid claiming that unrelated frames are a rotation: improvement threshold
 // admits a modest residual displacement but rejects weak global fits.
 if(best>0.58||best>=transl*.90||
    (std::abs(angle)<2.5 && std::abs(scaleValue-1)<0.035))return result;
 result={true,angle,tx,ty,best,transl,scaleValue};return result;
}
inline void forward(const Fit& fit,int w,int h,double x,double y,double t,double& ox,double& oy){
 const double cx=(w-1)*.5,cy=(h-1)*.5;
 const double a=fit.angle*(pi/180)*t,sc=1+(fit.scale-1)*t;
 const double cs=std::cos(a),sn=std::sin(a);
 const double px=x-cx,py=y-cy;
 ox=cx+sc*(cs*px-sn*py)+t*fit.dx;
 oy=cy+sc*(sn*px+cs*py)+t*fit.dy;
}
inline void inverse(const Fit& fit,int w,int h,double x,double y,double& ox,double& oy){
 const double cx=(w-1)*.5,cy=(h-1)*.5;
 const double a=fit.angle*pi/180,cs=std::cos(a),sn=std::sin(a);
 const double px=(x-cx-fit.dx)/fit.scale,py=(y-cy-fit.dy)/fit.scale;
 ox=cx+cs*px+sn*py;
 oy=cy-sn*px+cs*py;
}
inline Image warp(const Image& a,int w,int h,double angle,double dx=0,double dy=0,double scale=1){
 Image out(size_t(w)*h);double cx=(w-1)*.5,cy=(h-1)*.5,cs=std::cos(angle*pi/180),sn=std::sin(angle*pi/180);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  double px=(x-cx-dx)/scale,py=(y-cy-dy)/scale;
  out[y*w+x]=bilinear(a,w,h,cx+cs*px+sn*py,cy-sn*px+cs*py);
 }
 return out;
}
// Warps endpoint B backwards from global-time 1 to time t using the exact
// inverse composite transform, not a negated-angle approximation.
inline Image reverseWarp(const Image& b,int w,int h,const Fit& fit,double t){
 Image out(size_t(w)*h);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  Fit partial=fit;partial.angle*=t;partial.dx*=t;partial.dy*=t;
  partial.scale=1+(fit.scale-1)*t;
  double ax,ay,fromX,fromY;
  inverse(partial,w,h,x,y,ax,ay);
  forward(fit,w,h,ax,ay,1,fromX,fromY);
  out[size_t(y)*w+x]=bilinear(b,w,h,fromX,fromY);
 }
 return out;
}
inline Image render(const Image& a,const Image& b,int w,int h,const Fit& fit,double t){
 if(t<=0)return a;if(t>=1)return b;
 // Motion-compensated source selection keeps sharp rotating edges instead
 // of double-exposing two slightly misaligned color masks for the whole span.
 if(t<.48)return warp(a,w,h,fit.angle*t,fit.dx*t,fit.dy*t,1+(fit.scale-1)*t);
 if(t>.52)return reverseWarp(b,w,h,fit,t);
 double mix=(t-.48)/.04;
 Image aw=warp(a,w,h,fit.angle*t,fit.dx*t,fit.dy*t,1+(fit.scale-1)*t);
 Image bw=reverseWarp(b,w,h,fit,t);
 Image out(aw.size());for(size_t i=0;i<out.size();i++){
  double A=1-mix,B=mix;uint32_t color=0;int alpha=std::clamp((int)std::lround(A*(aw[i]>>24)+B*(bw[i]>>24)),0,255);
  for(int k=0;k<3;k++){
   int channel=std::clamp((int)std::lround(A*((aw[i]>>(k*8))&255)+B*((bw[i]>>(k*8))&255)),0,alpha);
   color|=uint32_t(channel)<<(k*8);
  }out[i]=color|(uint32_t(alpha)<<24);
 }return out;
}
}

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <array>
#include <climits>
#include "rotation.h"
namespace motion {
using Image=std::vector<uint32_t>;
struct Flow {int x=0,y=0;bool valid=false;};
struct Pair {int w=0,h=0;Image a,b;std::vector<Flow> ab,ba;rotation::Fit rigid;bool compatible=true;std::vector<uint32_t> sourceColors;};
inline uint32_t sample(const Image& im,int w,int h,int x,int y){return x>=0&&y>=0&&x<w&&y<h?im[size_t(y)*w+x]:0;}
inline int difference(uint32_t a,uint32_t b){int d=0;for(int s=0;s<32;s+=8)d+=std::abs(int((a>>s)&255)-int((b>>s)&255));return d;}
inline int alpha(uint32_t c){return int((c>>24)&255);} 
inline bool opaqueEnough(uint32_t c,int a=1){return alpha(c)>=a;}
inline uint32_t rgbOf(uint32_t c){return c&0x00ffffffu;}
inline uint32_t premultiplyRGB(uint32_t rgb,int a){
 a=std::clamp(a,0,255); uint32_t out=uint32_t(a)<<24;
 for(int ch=0;ch<3;ch++){
  int v=int((rgb>>(8*ch))&255);
  out|=uint32_t(std::clamp((v*a+127)/255,0,a))<<(8*ch);
 }
 return out;
}
inline uint32_t unpremultipliedRGB(uint32_t c){
 int a=alpha(c); if(a<=0) return 0;
 uint32_t rgb=0;
 for(int ch=0;ch<3;ch++){
  int v=int((c>>(8*ch))&255);
  rgb|=uint32_t(std::clamp((v*255+a/2)/a,0,255))<<(8*ch);
 }
 return rgb;
}
// Bidirectional local patch matching. For a rotating cursor these fields are
// residuals AFTER rigid A->B alignment, not two competing motion hypotheses.
inline std::vector<Flow> estimate(const Image& a,const Image& b,int w,int h){
 std::vector<Flow> out(size_t(w)*h);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  int i=y*w+x;if(!(a[i]>>24))continue;
  double best=1e20;int bx=x,by=y,rawBest=100000;
  for(int yy=std::max(0,y-12);yy<=std::min(h-1,y+12);yy++)for(int xx=std::max(0,x-12);xx<=std::min(w-1,x+12);xx++){
   if(!(b[yy*w+xx]>>24))continue;
   int raw=0;for(int v=-1;v<=1;v++)for(int u=-1;u<=1;u++)raw+=difference(sample(a,w,h,x+u,y+v),sample(b,w,h,xx+u,yy+v));
   double score=raw+0.25*((xx-x)*(xx-x)+(yy-y)*(yy-y));
   if(score<best){best=score;bx=xx;by=yy;rawBest=raw;}
  }
  out[i]={bx-x,by-y,rawBest<=9*4*48};
 }
 return out;
}
inline Pair prepare(Image a,Image b,int w,int h){
 if(w<1||h<1||w>128||h>128||a.size()!=size_t(w*h)||b.size()!=a.size())throw std::runtime_error("Motion sprite dimensions unsupported");
 Pair p;p.w=w;p.h=h;p.a=std::move(a);p.b=std::move(b);
 // Exact endpoint RGB set from correctly palette-decoded images.
 for(const Image* src : {&p.a,&p.b})for(uint32_t c:*src){
  if(alpha(c)<1)continue;
  uint32_t rgb=unpremultipliedRGB(c);
  if(std::find(p.sourceColors.begin(),p.sourceColors.end(),rgb)==p.sourceColors.end())p.sourceColors.push_back(rgb);
 }
 if(p.a==p.b)return p;
 p.rigid=rotation::detect(p.a,p.b,w,h);
 int areaA=0,areaB=0,shared=0;
 for(size_t i=0;i<p.a.size();i++){
  bool x=(p.a[i]>>24)>128,y=(p.b[i]>>24)>128;
  areaA+=x;areaB+=y;shared+=x&&y;
 }
 const int total=areaA+areaB-shared;
 if(p.rigid.valid&&total>0&&shared*100<total*32&&p.rigid.score>.35)
  p.rigid.valid=false;
 if(!p.rigid.valid&&std::min(areaA,areaB)>80&&total>0&&shared*100<total*32){
  p.compatible=false;return p;
 }
 Image aligned=p.rigid.valid?rotation::warp(p.a,w,h,p.rigid.angle,p.rigid.dx,p.rigid.dy,p.rigid.scale):p.a;
 p.ab=estimate(aligned,p.b,w,h);p.ba=estimate(p.b,aligned,w,h);
 const auto forward=p.ab,back=p.ba;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  int i=y*w+x;
  auto check=[&](Flow& f,const std::vector<Flow>& reverse){
   if(!f.valid)return;
   int xx=x+f.x,yy=y+f.y;
   if(xx<0||yy<0||xx>=w||yy>=h){f.valid=false;return;}
   const auto& r=reverse[yy*w+xx];
   f.valid=r.valid&&std::abs(f.x+r.x)<=1&&std::abs(f.y+r.y)<=1;
   if(p.rigid.valid&&f.x*f.x+f.y*f.y>36)f.valid=false;
  };
  check(p.ab[i],back);check(p.ba[i],forward);
 }
 return p;
}
inline Flow nearestFlow(const std::vector<Flow>& f,int w,int h,double x,double y){
 int xx=int(std::floor(x+0.5)),yy=int(std::floor(y+0.5));if(xx<0||xx>=w||yy<0||yy>=h||f.empty())return {};
 return f[size_t(yy)*w+xx];
}
struct VoteSlot { uint32_t rgb=0; double weight=0; int alpha=0; bool used=false; };
struct VoteCell { VoteSlot a,b; };
inline void addVote(VoteCell& cell,uint32_t rgb,int a,double weight){
 if(a<=0||weight<=0) return;
 auto merge=[&](VoteSlot& s)->bool{
  if(s.used && s.rgb==rgb){ s.weight+=weight; s.alpha=std::max(s.alpha,a); return true; }
  return false;
 };
 if(merge(cell.a) || merge(cell.b)) return;
 auto init=[&](VoteSlot& s)->bool{
  if(!s.used){ s={rgb,weight,a,true}; return true; }
  return false;
 };
 if(init(cell.a) || init(cell.b)) return;
 if(cell.a.weight<cell.b.weight){ if(weight>cell.a.weight) cell.a={rgb,weight,a,true}; }
 else { if(weight>cell.b.weight) cell.b={rgb,weight,a,true}; }
}
inline uint32_t finalizeCell(const VoteCell& c){
 if(!c.a.used && !c.b.used) return 0;
 auto best = (!c.b.used || c.a.weight>c.b.weight || (std::abs(c.a.weight-c.b.weight)<1e-9 && c.a.alpha>=c.b.alpha)) ? c.a : c.b;
 return premultiplyRGB(best.rgb,best.alpha);
}
inline void hardSplat(std::vector<VoteCell>& grid,int w,int h,uint32_t c,double x,double y,double endpointWeight){
 int a=alpha(c); if(a<=0) return;
 // Pixel-art rebuild: no bilinear antialiasing. We quantize to a single target pixel.
 int xi=int(std::floor(x+0.5)); int yi=int(std::floor(y+0.5));
 if(xi<0||yi<0||xi>=w||yi>=h) return;
 addVote(grid[size_t(yi)*w+xi], unpremultipliedRGB(c), a, endpointWeight);
}
inline Image rasterEndpoint(const Pair& p,double t,bool fromA){
 const int w=p.w,h=p.h; std::vector<VoteCell> grid(size_t(w)*h);
 const Image& src=fromA?p.a:p.b;
 const auto& flows=fromA?p.ab:p.ba;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  uint32_t c=src[size_t(y)*w+x]; if(!opaqueEnough(c,1)) continue;
  double px=x,py=y;
  if(p.rigid.valid){
   if(fromA){
    double gx,gy; rotation::forward(p.rigid,w,h,x,y,1,gx,gy);
    Flow f=nearestFlow(p.ab,w,h,gx,gy);
    rotation::forward(p.rigid,w,h,x,y,t,px,py);
    if(f.valid){px+=t*f.x; py+=t*f.y;}
   }else{
    Flow f=nearestFlow(p.ba,w,h,x,y);
    double alignedX=x+(f.valid?f.x:0), alignedY=y+(f.valid?f.y:0);
    double origX,origY; rotation::inverse(p.rigid,w,h,alignedX,alignedY,origX,origY);
    rotation::forward(p.rigid,w,h,origX,origY,t,px,py);
    if(f.valid){px-=t*f.x; py-=t*f.y;}
   }
  }else{
    Flow f=nearestFlow(flows,w,h,x,y);
    if(f.valid){px+=(fromA?t:1-t)*f.x; py+=(fromA?t:1-t)*f.y;}
  }
  hardSplat(grid,w,h,c,px,py, fromA?(1.0-t):t);
 }
 Image out(size_t(w)*h,0);
 for(size_t i=0;i<out.size();i++) out[i]=finalizeCell(grid[i]);
 return out;
}
inline uint32_t choosePixel(uint32_t a,uint32_t b,double t){
 int aa=alpha(a), bb=alpha(b);
 if(aa<=0) return b;
 if(bb<=0) return a;
 uint32_t ar=rgbOf(a), br=rgbOf(b);
 if(ar==br) return premultiplyRGB(ar,std::max(aa,bb));
 if(t<=0.45) return a;
 if(t>=0.55) return b;
 // Preserve crispness: no color averaging in the core body. In the narrow
 // handover zone, pick one source color deterministically.
 if((aa>bb?aa-bb:bb-aa)>24) return aa>bb?a:b;
 return t<0.5?a:b;
}
inline void stabilizePaletteFrame(Image& image,const Pair& p){
 // Keep colors inside the exact endpoint palette; do not invent blended colors.
 for(uint32_t& c:image){
  int a=alpha(c); if(a<=0) continue;
  uint32_t src=unpremultipliedRGB(c);
  uint32_t best=p.sourceColors.empty()?src:p.sourceColors.front();
  int bestDist=INT32_MAX;
  for(uint32_t candidate:p.sourceColors){
   int d0=int(src&255)-int(candidate&255), d1=int((src>>8)&255)-int((candidate>>8)&255), d2=int((src>>16)&255)-int((candidate>>16)&255);
   int dist=2*d0*d0+3*d1*d1+2*d2*d2;
   if(dist<bestDist){ bestDist=dist; best=candidate; if(!dist) break; }
  }
  c=premultiplyRGB(best,a);
 }
}
struct Timeline {
 int w=0,h=0,referenceMs=0,stepMs=1;
 bool rotated=false;double degrees=0;
 std::vector<Image> images;
 size_t bytes()const{return images.size()*size_t(w)*h*sizeof(uint32_t);}
 const Image& at(uint32_t elapsedMs,uint32_t periodMs)const{
  static const Image empty;if(images.empty())return empty;if(!periodMs)periodMs=1;
  size_t n=images.size()-1;
  size_t index=std::min(n,(size_t)((uint64_t(elapsedMs)*n+periodMs/2)/periodMs));
  return images[index];
 }
};
inline void stabilizeTimelineColors(Timeline& timeline){
 const int n=int(timeline.images.size()); if(n<3) return;
 const size_t pixels=size_t(timeline.w)*timeline.h;
 // Pass 1: remove one-frame opaque speckles / one-frame holes.
 for(size_t pos=0; pos<pixels; ++pos){
  for(int i=1; i<n-1; ++i){
   uint32_t prev=timeline.images[i-1][pos], cur=timeline.images[i][pos], next=timeline.images[i+1][pos];
   bool p=opaqueEnough(prev,200), c=opaqueEnough(cur,200), nx=opaqueEnough(next,200);
   if(!p && c && !nx) timeline.images[i][pos]=0;
   else if(p && !c && nx && rgbOf(prev)==rgbOf(next)) timeline.images[i][pos]=premultiplyRGB(rgbOf(prev), std::max(alpha(prev),alpha(next)));
   else if(p && nx && rgbOf(prev)==rgbOf(next) && c && rgbOf(cur)!=rgbOf(prev)) timeline.images[i][pos]=premultiplyRGB(rgbOf(prev), std::max({alpha(prev),alpha(cur),alpha(next)}));
  }
 }
 // Pass 2: keep stable core colors stable when endpoints agree.
 const Image first=timeline.images.front(), last=timeline.images.back();
 for(size_t pos=0; pos<pixels; ++pos){
  if(opaqueEnough(first[pos],224) && opaqueEnough(last[pos],224) && rgbOf(first[pos])==rgbOf(last[pos])){
   uint32_t base=rgbOf(first[pos]);
   for(int i=1; i<n-1; ++i){
    if(opaqueEnough(timeline.images[i][pos],224)) timeline.images[i][pos]=premultiplyRGB(base, alpha(timeline.images[i][pos]));
   }
  }
 }
}
inline Image renderAt(const Pair& p,double t){
 if(t<=0) return p.a; if(t>=1) return p.b; if(p.a==p.b) return p.a;
 if(!p.compatible) return t<.5?p.a:p.b;
 Image a=rasterEndpoint(p,t,true), b=rasterEndpoint(p,t,false);
 if(t<=.40){ stabilizePaletteFrame(a,p); return a; }
 if(t>=.60){ stabilizePaletteFrame(b,p); return b; }
 Image out(a.size(),0);
 for(size_t i=0;i<out.size();i++) out[i]=choosePixel(a[i],b[i],t);
 stabilizePaletteFrame(out,p);
 return out;
}
inline Image render(const Pair& p,unsigned phase){return renderAt(p,double(std::min(255u,phase))/255.0);} 
inline Timeline precompute(const Pair& p,unsigned periodMs,unsigned hz=1000,size_t budget=16u*1024*1024){
 if(!periodMs||hz<1||p.w<1||p.h<1)throw std::runtime_error("Invalid timeline period");
 periodMs=std::clamp(periodMs,3u,500u);hz=std::clamp(hz,30u,1000u);
 size_t stride=sizeof(uint32_t)*size_t(p.w)*p.h;
 size_t slots=std::min(size_t((uint64_t(periodMs)*hz+999)/1000),std::max(size_t(1),budget/stride-1));
 Timeline timeline;timeline.w=p.w;timeline.h=p.h;timeline.referenceMs=periodMs;
 timeline.stepMs=(periodMs+(int)slots-1)/(int)slots;
 timeline.rotated=p.rigid.valid;timeline.degrees=p.rigid.angle;timeline.images.reserve(slots+1);
 for(size_t i=0;i<=slots;i++) timeline.images.push_back(renderAt(p,double(i)/slots));
 stabilizeTimelineColors(timeline);
 return timeline;
}
} // namespace motion

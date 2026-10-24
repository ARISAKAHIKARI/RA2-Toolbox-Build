#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
namespace superhot {
struct Range { int id, first, count; };
// Verified vanilla YR table. Paradrop (47) deliberately keeps its X anchor.
inline const std::vector<Range>& ranges(){
 static const std::vector<Range> r={{50,279,20},{53,319,10},{57,346,5},{58,357,12},
  {79,450,10},{80,460,10},{81,470,10},{82,480,8},{83,488,8},{84,496,8},{85,504,8},
  {50,514,1},{53,513,1},{83,516,1},{84,515,1},{85,512,1}};
 return r;
}
struct Point { double x=0,y=0; bool valid=false; };
using Centers=std::map<int,Point>;
// Iron Curtain is a target made of nested rings. Its inner mark is the
// aiming anchor; the canvas, shadow and outer animation bounds are not.
// Work once at resource load, on color structure rather than alpha bounds.
template<class Frame>
Point ringCore(const Frame& frame,int w,int h){
 if(w<9||h<9||w>512||h>512||frame.size()!=size_t(w)*h)return {};
 int peakColor=0,peakLight=0;
 auto light=[](uint32_t p){return std::max({int((p>>16)&255),int((p>>8)&255),int(p&255)});};
 auto color=[&](uint32_t p){return light(p)-std::min({int((p>>16)&255),int((p>>8)&255),int(p&255)});};
 for(auto p:frame)if((p>>24)>=128){peakColor=std::max(peakColor,color(p));peakLight=std::max(peakLight,light(p));}
 bool colored=peakColor>=32;int threshold=std::max(16,(colored?peakColor:peakLight)/8);
 std::vector<uint8_t> ink(size_t(w)*h);
 for(size_t i=0;i<ink.size();++i)ink[i]=(frame[i]>>24)>=128&&(colored?color(frame[i]):light(frame[i]))>=threshold;
 struct Part {int l,t,r,b,n;};std::vector<Part> parts;std::vector<int> queue;
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){
  int at=y*w+x;if(!ink[at])continue;ink[at]=0;queue.clear();queue.push_back(at);Part p{x,y,x,y,0};
  for(size_t i=0;i<queue.size();++i){int u=queue[i]%w,v=queue[i]/w;++p.n;
   p.l=std::min(p.l,u);p.r=std::max(p.r,u);p.t=std::min(p.t,v);p.b=std::max(p.b,v);
   auto visit=[&](int a,int b){if(a<0||b<0||a>=w||b>=h)return;int j=b*w+a;if(ink[j]){ink[j]=0;queue.push_back(j);}};
   visit(u-1,v);visit(u+1,v);visit(u,v-1);visit(u,v+1);
  }if(p.n>=4)parts.push_back(p);
 }
 Point best{};int depth=1,area=w*h;bool ambiguous=false;
 for(const auto&p:parts){
  if(p.r-p.l+1>w/3||p.b-p.t+1>h/3)continue;
  int enclosing=0;
  for(const auto&r:parts)if(r.l<p.l&&r.r>p.r&&r.t<p.t&&r.b>p.b&&r.n>p.n)++enclosing;
  if(enclosing>depth||(enclosing==depth&&enclosing>=2&&p.n<area)){
   best={(p.l+p.r+1)*.5,(p.t+p.b+1)*.5,true};depth=enclosing;area=p.n;ambiguous=false;
  }else if(enclosing==depth&&enclosing>=2&&p.n==area)ambiguous=true;
 }
 return ambiguous?Point{}:best;
}
// Calculate once per source/style, using the union across the animation.
template<class Frames>
Centers prepare(const Frames& frames,int w,int h){
 Centers out;
 for(const auto& r:ranges()){
  if(r.first<0||r.count<1||r.first+r.count>int(frames.size()))continue;
  if(r.id==57){
   std::vector<double> xs,ys;
   for(int f=r.first;f<r.first+r.count;++f){auto p=ringCore(frames[f],w,h);if(p.valid){xs.push_back(p.x);ys.push_back(p.y);}}
   if(int(xs.size())*2>r.count){
    std::sort(xs.begin(),xs.end());std::sort(ys.begin(),ys.end());double x=xs[xs.size()/2],y=ys[ys.size()/2];
    // A stable core across the whole animation prevents pulsing/jumping.
    if(xs.back()-xs.front()<=1&&ys.back()-ys.front()<=1)out[r.first]={x,y,true};
   }
   continue; // Unrecognized custom art keeps the native hotspot, never a guessed box center.
  }
  int left=w,top=h,right=-1,bottom=-1;
  for(int f=r.first;f<r.first+r.count;++f)for(int y=0;y<h;++y)for(int x=0;x<w;++x){
   if((frames[f][size_t(y)*w+x]>>24)==0)continue;
   if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
  }
  if(right>=left&&bottom>=top)out[r.first]={double((left+right+1)/2),double((top+bottom+1)/2),true};
 }
 return out;
}
inline Point resolve(const Centers& centers,int id,int frame,int first,int count){
 for(const auto& r:ranges())if(frame>=r.first&&frame<r.first+r.count&&
  ((r.id==id&&r.first==first&&r.count==count)||
   (r.id==57&&(id==57||id<0)))){
  auto i=centers.find(r.first);if(i!=centers.end())return i->second;
 }
 return {};
}
// A visual anchor owns its placement. Generic manual hotspot calibration
// applies only when visual centering is not active, otherwise it shifts the
// aiming ring away from the click point again.
inline int scaledAnchor(double native,const Point&visual,bool vertical,int src,int dst,int offset){
 double anchor=visual.valid?(vertical?visual.y:visual.x):native;
 return int(std::lround(anchor*dst/src))+(visual.valid?0:offset);
}
}

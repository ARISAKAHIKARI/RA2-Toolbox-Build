#pragma once
#include "native_feedback.h"
namespace external { namespace boltgeometry {
// GPL-3.0: Vera20K3d42426 render/electric_bolt.rs, original4C1F20.
// Native geometry/phase, three strands, depth interpolation and palette words.
// Vera owns its process Main RNG. This passive renderer uses a private visual
// stream: no native RNG consumption or claim of identical random vertices.
struct Random {
 uint32_t state;
 uint32_t next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
 int ranged(int lo,int hi){const uint32_t span=uint32_t(hi-lo+1),threshold=uint32_t(0u-span)%span;uint32_t value;do{value=next();}while(value<threshold);return lo+int(value%span);}
};
template<class Sink,class View>void draw(const nativefeedback::Bolt&e,Sink&sink,const View&view){
 if(!e.math)return;
 auto a=view.project(e.source),b=view.project(e.target);
 double lo=0,hi=1,dx=b[0]-a[0],dy=b[1]-a[1];auto clip=[&](double p,double q){if(p==0)return q>=0;const double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
 if(!clip(-dx,a[0])||!clip(dx,view.pixelsW-1-a[0])||!clip(-dy,a[1])||!clip(dy,view.pixelsH-1-a[1]))return;
 using Point=std::array<int32_t,3>;using Triplet=std::array<Point,3>;
 const Point from{{e.source.x,e.source.y,e.source.z}},to{{e.target.x,e.target.y,e.target.z}};
 const double x=double(from[0])-to[0],y=double(from[1])-to[1],z=double(from[2])-to[2];
 const int length=nativefx::chop(e.math->rootWide(x*x+z*z+y*y));if(length<=0||length>262144)return;
 struct Piece {Triplet from,to;int length,displacement;std::array<int,2>z;};
 Piece current{{from,from,from},{to,to,to},length,(length*23)>>8,{{e.zAdjust,0}}};std::array<Piece,8>pending{};unsigned depth=0;bool first=true;const int sideThreshold=(length*102)>>8;
 Random rng{nativefeedback::hash(e.seed^uint32_t(from[0])^uint32_t(from[1])*131u^uint32_t(to[0])*7919u^uint32_t(to[1]))|1u};
 auto add=[](int a,int b){return int32_t(uint32_t(a)+uint32_t(b));};
 while(true){
  while(current.length>64&&depth<pending.size()){
   Triplet middle{};for(unsigned strand=0;strand<3;++strand)for(unsigned axis=0;axis<3;++axis)middle[strand][axis]=add(current.from[strand][axis],current.to[strand][axis])>>1;
   if(first){first=false;int wave[6]{};for(int n=0;n<6;++n)wave[n]=nativefx::chop(e.math->sine(double(int32_t(e.seed))*3.14159265358979323846/(n+7))*current.displacement);
    const int shift[3]={add(wave[3],wave[0]),add(wave[5],wave[1]),add(add(wave[4],current.displacement*2),wave[2])>>1};
    for(auto&point:middle)for(unsigned axis=0;axis<3;++axis)point[axis]=add(point[axis],shift[axis]);
   }
   for(auto&coord:middle[0])coord=add(coord,current.length<=128?rng.ranged(-1,1)*current.displacement*2:rng.ranged(-current.displacement,current.displacement));
   for(unsigned strand=1;strand<3;++strand)for(unsigned axis=0;axis<3;++axis){const int shift=rng.ranged(-current.displacement,current.displacement);middle[strand][axis]=current.length>sideThreshold?add(middle[0][axis],shift>>1):add(middle[strand][axis],shift);}
   const int midZ=add(current.z[0],current.z[1])>>1;current.length>>=1;current.displacement>>=1;pending[depth]=current;pending[depth].from=middle;pending[depth].z[0]=midZ;++depth;current.to=middle;current.z[1]=midZ;
  }
  for(unsigned strand:{1u,2u,0u}){const auto&p=current.from[strand];const auto&q=current.to[strand];nativefeedback::Point3 fromPoint{p[0],p[1],p[2]},toPoint{q[0],q[1],q[2]};
   sink.nativeLine(view.project(fromPoint),view.project(toPoint),current.z[0]-screenHeight(p[2],view.zscale)-2,current.z[1]-screenHeight(q[2],view.zscale)-2,e.colors[strand==0?2:e.alternate?1:0],2,255,e.rgb555,e.math.get());
  }
  if(!depth)break;current=pending[--depth];
 }
}
template<class Sink,class View>void paint(const std::vector<nativefeedback::Bolt>&list,Sink&sink,const View&view){for(auto i=list.rbegin();i!=list.rend();++i)draw(*i,sink,view);}
}}

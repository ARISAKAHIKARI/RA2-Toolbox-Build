#include "../source/formation.h"
#include <cassert>
#include <iostream>
using namespace assist::formation;
int main(){Point p[Maximum];uint32_t keys[]={90,90,67,90,90,90};assert(cycle(0,90,keys)==1);assert(cycle(1,90,keys)==3);assert(cycle(3,90,keys)==4);assert(cycle(4,90,keys)==5);assert(cycle(5,90,keys)==0);assert(cycle(0,67,keys)==2);assert(cycle(2,88,keys)==2);
 for(int n=0;n<360;++n){double a=n*Pi/180,x=std::cos(a),y=std::sin(a),f=dragFacing(x,y,0);double sx=std::cos(f)-std::sin(f),sy=(std::cos(f)+std::sin(f))*.5;assert(std::abs(sx*y-sy*x)<1e-9&&sx*x+sy*y>0);}assert(dragFacing(0,0,.7)==.7);
 for(int n=1;n<=Maximum;++n)for(int shape=0;shape<Shapes;++shape)for(double t:{0.,.1,.5,1.}){layout(n,shape,t,false,p);double x=0,y=0;for(int i=0;i<n;++i){assert(std::isfinite(p[i].x)&&std::isfinite(p[i].y));x+=p[i].x;y+=p[i].y;for(int j=0;j<i;++j)assert(std::hypot(p[i].x-p[j].x,p[i].y-p[j].y)>.001);if(t==1&&shape!=5)assert(std::abs(p[i].y)<1e-9);}assert(std::abs(x)<1e-7&&std::abs(y)<1e-7);}
 layout(100,0,0,false,p);double x=0,y=0;for(auto q:p){(void)q;}for(int i=0;i<100;++i){x=std::max(x,std::abs(p[i].x));y=std::max(y,std::abs(p[i].y));}assert(x/y>1.2&&x/y<1.6);
 Point a[Maximum],b[Maximum];layout(25,0,0,false,a);layout(25,1,0,false,b);for(int n=0;n<25;++n){assert(a[n].x*2==b[n].x&&a[n].y*2==b[n].y);}
 // No drag means no stretch, not a replacement of the selected shape.
 // Compare geometry as a set so reordering the same points cannot pass.
 Point resting[Shapes][25];auto same=[](Point a,Point b){return a.x==b.x&&a.y==b.y;};
 for(int shape=0;shape<Shapes;++shape){
  layout(25,shape,0,false,a);
  for(double t:{0.,.1,.5,1.}){layout(25,shape,t,true,b);assert(std::equal(a,a+25,b,same)&&"zero-drag preview must preserve the selected shape");}
  std::copy(b,b+25,resting[shape]);std::sort(resting[shape],resting[shape]+25,[](Point a,Point b){return a.x<b.x||(a.x==b.x&&a.y<b.y);});
  for(int previous=0;previous<shape;++previous)assert(!std::equal(resting[shape],resting[shape]+25,resting[previous],same)&&"25-unit shapes must have distinct geometry without dragging");
 }
 for(int count:{16,25,64,100,256}){double last=0;for(double t:{0.,.25,.5,.75,1.}){layout(count,5,t,false,p);double lo=1e9,hi=-1e9;for(int i=0;i<count;++i){lo=std::min(lo,p[i].x);hi=std::max(hi,p[i].x);}assert(hi-lo>=last);last=hi-lo;if(t==1){double minY=1e9,maxY=-1e9;for(int i=0;i<count;++i){minY=std::min(minY,p[i].y);maxY=std::max(maxY,p[i].y);}for(int i=0;i<count;++i)assert(p[i].x==lo||p[i].x==hi||p[i].y==minY||p[i].y==maxY);}}}
 layout(100,4,0,false,p);double maxX=0,maxY=0;for(int i=0;i<100;++i){maxX=std::max(maxX,std::abs(p[i].x));maxY=std::max(maxY,std::abs(p[i].y));}assert(maxX==maxY);
 layout(100,5,0,true,a);layout(100,5,0,false,b);for(int n=0;n<100;++n)assert(a[n].x==b[n].x&&a[n].y==b[n].y);assert(std::none_of(a,a+100,[](auto p){return std::abs(p.x)<2&&std::abs(p.y)<2;}));
 Unit units[2]{};units[0].x=1000;Slot slots[2]={{0,0,0,1},{1000,0,0,1}};assign(units,2,slots);assert(slots[0].x==1000&&slots[1].x==0);
 std::cout<<"PASS shape cycling, 6144 layouts, square and hollow frame, spacing, line limit, six distinct zero-drag shapes, stable assignment\n";
}

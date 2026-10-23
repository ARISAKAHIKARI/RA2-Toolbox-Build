#include "../source/formation.h"
#include <cassert>
#include <iostream>
using namespace assist::formation;
int main(){Point p[Maximum];uint32_t keys[]={90,90,67,90};assert(cycle(0,90,keys)==1);assert(cycle(1,90,keys)==3);assert(cycle(3,90,keys)==0);assert(cycle(0,67,keys)==2);assert(cycle(2,88,keys)==2);
 for(int n=0;n<360;++n){double a=n*Pi/180,x=std::cos(a),y=std::sin(a),f=dragFacing(x,y,0);double sx=std::cos(f)-std::sin(f),sy=(std::cos(f)+std::sin(f))*.5;assert(std::abs(sx*y-sy*x)<1e-9&&sx*x+sy*y>0);}assert(dragFacing(0,0,.7)==.7);
 for(int n=1;n<=Maximum;++n)for(int shape=0;shape<4;++shape)for(double t:{0.,.1,.5,1.}){layout(n,shape,t,false,p);double x=0,y=0;for(int i=0;i<n;++i){assert(std::isfinite(p[i].x)&&std::isfinite(p[i].y));x+=p[i].x;y+=p[i].y;for(int j=0;j<i;++j)assert(std::hypot(p[i].x-p[j].x,p[i].y-p[j].y)>.001);if(t==1)assert(std::abs(p[i].y)<1e-9);}assert(std::abs(x)<1e-7&&std::abs(y)<1e-7);}
 layout(100,0,0,false,p);double x=0,y=0;for(auto q:p){(void)q;}for(int i=0;i<100;++i){x=std::max(x,std::abs(p[i].x));y=std::max(y,std::abs(p[i].y));}assert(x/y>1.2&&x/y<1.6);
 Point a[Maximum],b[Maximum];layout(25,0,0,false,a);layout(25,1,0,false,b);for(int n=0;n<25;++n){assert(a[n].x*2==b[n].x&&a[n].y*2==b[n].y);}for(int s=1;s<4;++s){layout(25,0,0,true,a);layout(25,s,1,true,b);for(int n=0;n<25;++n)assert(a[n].x==b[n].x&&a[n].y==b[n].y);}
 Unit units[2]{};units[0].x=1000;Slot slots[2]={{0,0,0,1},{1000,0,0,1}};assign(units,2,slots);assert(slots[0].x==1000&&slots[1].x==0);
 std::cout<<"PASS shape cycling, 4096 layouts, spacing, line limit, click cluster, stable assignment\n";
}

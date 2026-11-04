#include "../../shared/source/native_viewport_projection.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#ifdef NDEBUG
#error Native viewport verification requires assertions.
#endif
using namespace nativeviewportprojection;
struct Shared {volatile long viewportSerial=0;volatile uint32_t viewportActive=0,viewportGeneration=0,viewportHostCapabilities=0;int32_t viewportPhysical[4]{},viewportLogical[4]{};};
struct ChangingSerial {mutable unsigned reads=0;operator uint32_t()const volatile{return ++const_cast<ChangingSerial*>(this)->reads==1?2u:4u;}};
struct ChangingShared {ChangingSerial viewportSerial;uint32_t viewportActive=1,viewportGeneration=1;int32_t viewportPhysical[4]{0,0,800,568},viewportLogical[4]{0,0,1600,1136};};
int main(){
 unsigned checks=0;auto check=[&](bool value){++checks;assert(value);};
 Transform t;check(sample(static_cast<Shared*>(nullptr),t)&&!t.active);int x=-22,y=33;check(screenToView(t,x,y)&&x==-22&&y==33);
 Shared s;check(sample(&s,t)&&!t.active);s.viewportSerial=1;check(!sample(&s,t));s.viewportSerial=2;s.viewportActive=1;s.viewportGeneration=7;
 int a[4]={168,0,632,568},r[4]={0,0,1264,1136};for(unsigned i=0;i<4;++i){s.viewportPhysical[i]=a[i];s.viewportLogical[i]=r[i];}
 check(sample(&s,t)&&t.active&&t.generation==7);auto same=t;check(t==same);++same.generation;check(t!=same);
 x=168;y=0;check(screenToView(t,x,y)&&x==0&&y==0);x=799;y=567;check(screenToView(t,x,y)&&x==1262&&y==1134);
 check(viewToScreen(t,x,y)&&x==799&&y==567);x=167;y=10;check(!screenToView(t,x,y)&&x==167&&y==10);x=800;y=10;check(!screenToView(t,x,y));x=200;y=568;check(!screenToView(t,x,y));
 x=-60;y=80;check(deltaToScreen(t,x,y)&&x==-30&&y==40);x=100;y=200;check(viewToScreen(t,x,y)&&x==218&&y==100);
 s.viewportLogical[2]=0;check(!sample(&s,t));s.viewportLogical[2]=9000;check(!sample(&s,t));s.viewportLogical[2]=1264;s.viewportActive=2;check(!sample(&s,t));
 s.viewportActive=0;s.viewportLogical[2]=0;check(sample(&s,t)&&!t.active);ChangingShared changing;check(!sample(&changing,t));
 for(int offset:{0,168})for(int percent:{50,75,100,125,150}){
  Transform q{{offset,0,632,568},{0,0,(63200+percent/2)/percent,(56800+percent/2)/percent},8,true};
  x=offset+316;y=284;check(screenToView(q,x,y));const int rx=x,ry=y;check(viewToScreen(q,x,y)&&x>=offset+315&&x<=offset+316&&y>=283&&y<=284);
  check(rx>=0&&rx<q.logical.w&&ry>=0&&ry<q.logical.h);
 }
 Transform extreme{{0,0,8192,4000},{0,0,16,16},9,true};x=INT32_MAX;y=2;check(!viewToScreen(extreme,x,y)&&x==INT32_MAX&&y==2);
 std::printf("Native viewport projection checks=%u; assertions enabled; Windows ABI; no game\n",checks);
}

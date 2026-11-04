#pragma once
#include <stdint.h>
#include <limits.h>
#include <atomic>
// One transform for Runtime and every Host projection/query route. A is the
// physical map area, R the native raster. Odd/changed publication is invalid,
// never an instruction to interpret a live scaled view as identity.
namespace nativeviewportprojection {
constexpr uint32_t RenderCapability=1,InputCapability=2,RequiredCapabilities=3;
constexpr uint32_t HostHandshake=0x80000000u;
struct Rect {int32_t x=0,y=0,w=0,h=0;};
struct Transform {Rect physical,logical;uint32_t generation=0;bool active=false;};
inline bool operator==(Rect a,Rect b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
inline bool operator!=(Rect a,Rect b){return !(a==b);}
inline bool operator==(const Transform&a,const Transform&b){return a.active==b.active&&a.generation==b.generation&&a.physical==b.physical&&a.logical==b.logical;}
inline bool operator!=(const Transform&a,const Transform&b){return !(a==b);}
inline bool valid(Rect r){return r.x>=0&&r.y>=0&&r.w>=16&&r.h>=16&&r.w<=8192&&r.h<=8192&&int64_t(r.x)+r.w<=32768&&int64_t(r.y)+r.h<=32768&&uint64_t(r.w)*r.h<=40000000;}
inline bool contains(Rect r,int x,int y){return x>=r.x&&y>=r.y&&int64_t(x)<int64_t(r.x)+r.w&&int64_t(y)<int64_t(r.y)+r.h;}
template<class Shared>bool sample(const Shared*s,Transform&out){
 if(!s){out={};return true;}
 const uint32_t before=uint32_t(s->viewportSerial);if(before&1)return false;
 std::atomic_thread_fence(std::memory_order_acquire);Transform t;
 const uint32_t active=s->viewportActive;t.active=active!=0;t.generation=s->viewportGeneration;
 t.physical={s->viewportPhysical[0],s->viewportPhysical[1],s->viewportPhysical[2],s->viewportPhysical[3]};
 t.logical={s->viewportLogical[0],s->viewportLogical[1],s->viewportLogical[2],s->viewportLogical[3]};
 std::atomic_thread_fence(std::memory_order_acquire);
 if(before!=uint32_t(s->viewportSerial)||(uint32_t(s->viewportSerial)&1)||active!=s->viewportActive||active>1||
    (t.active&&(!valid(t.physical)||!valid(t.logical))))return false;
 out=t;return true;
}
inline bool coordinate(int value,int from,int span,int to,int target,int&out){
 if(span<=0)return false;const int64_t n=int64_t(to)+(int64_t(value)-from)*target/span;
 if(n<INT32_MIN||n>INT32_MAX)return false;out=int(n);return true;
}
inline bool screenToView(const Transform&t,int&x,int&y){
 if(!t.active)return true;if(!valid(t.physical)||!valid(t.logical)||!contains(t.physical,x,y))return false;
 int a=0,b=0;if(!coordinate(x,t.physical.x,t.physical.w,t.logical.x,t.logical.w,a)||!coordinate(y,t.physical.y,t.physical.h,t.logical.y,t.logical.h,b))return false;
 x=a;y=b;return true;
}
inline bool viewToScreen(const Transform&t,int&x,int&y){
 if(!t.active)return true;if(!valid(t.physical)||!valid(t.logical))return false;
 int a=0,b=0;if(!coordinate(x,t.logical.x,t.logical.w,t.physical.x,t.physical.w,a)||!coordinate(y,t.logical.y,t.logical.h,t.physical.y,t.physical.h,b))return false;
 x=a;y=b;return true;
}
inline bool deltaToScreen(const Transform&t,int&x,int&y){
 if(!t.active)return true;if(!valid(t.physical)||!valid(t.logical))return false;
 int a=0,b=0;if(!coordinate(x,0,t.logical.w,0,t.physical.w,a)||!coordinate(y,0,t.logical.h,0,t.physical.h,b))return false;
 x=a;y=b;return true;
}
}

#pragma once
#include <stdint.h>
#include <atomic>
namespace nativeviewportprojection {
struct Rect {int x=0,y=0,w=0,h=0;};
struct Transform {Rect physical,logical;bool active=false;};
inline bool valid(Rect r){return r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.w<=8192&&r.h<=8192;}
inline bool contains(Rect r,int x,int y){return x>=r.x&&y>=r.y&&int64_t(x)<int64_t(r.x)+r.w&&int64_t(y)<int64_t(r.y)+r.h;}
template<class Shared>Transform sample(const Shared*s){
 Transform t;if(!s||!s->viewportActive)return t;
 const auto before=s->viewportSerial;if(before&1)return t;
 std::atomic_thread_fence(std::memory_order_acquire);
 t.physical={s->viewportPhysical[0],s->viewportPhysical[1],s->viewportPhysical[2],s->viewportPhysical[3]};
 t.logical={s->viewportLogical[0],s->viewportLogical[1],s->viewportLogical[2],s->viewportLogical[3]};
 std::atomic_thread_fence(std::memory_order_acquire);
 const auto after=s->viewportSerial;t.active=before==after&&!(after&1)&&s->viewportActive&&valid(t.physical)&&valid(t.logical);return t;
}
inline bool screenToView(const Transform&t,int&x,int&y){
 if(!t.active)return true;if(!contains(t.physical,x,y))return false;
 x=t.logical.x+int(int64_t(x-t.physical.x)*t.logical.w/t.physical.w);
 y=t.logical.y+int(int64_t(y-t.physical.y)*t.logical.h/t.physical.h);return true;
}
inline void viewToScreen(const Transform&t,int&x,int&y){
 if(!t.active)return;x=t.physical.x+int(int64_t(x-t.logical.x)*t.physical.w/t.logical.w);
 y=t.physical.y+int(int64_t(y-t.logical.y)*t.physical.h/t.logical.h);
}
inline void deltaToScreen(const Transform&t,int&x,int&y){if(t.active){x=int(int64_t(x)*t.physical.w/t.logical.w);y=int(int64_t(y)*t.physical.h/t.logical.h);}}
}

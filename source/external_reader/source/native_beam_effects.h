#pragma once
#include "native_feedback.h"
#include "main_viewport.h"

namespace external { namespace nativeeffects {
constexpr uint32_t LaserArray=0xABC878u;
inline void unionBeamRuns(std::vector<nativefeedback::Rect>&runs){
 using nativefeedback::Rect;
 std::sort(runs.begin(),runs.end(),[](Rect a,Rect b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
 size_t count=0;
 for(auto r:runs){
  if(count&&runs[count-1].y==r.y&&runs[count-1].x+runs[count-1].w>=r.x){auto&last=runs[count-1];last.w=std::max(last.x+last.w,r.x+r.w)-last.x;}
  else runs[count++]=r;
 }
 runs.resize(count);
}
// Native LaserDraw and HouseColor/Prism beams remain in the game's final
// frame. Open only a thin corridor around their verified native draw calls;
// do not replace their colors, lifetime, flicker, depth or blending with an
// invented external line. Any nonempty result requires exact camera coherence.
inline bool beamCorridor(int x0,int y0,int x1,int y1,int radius,const mainview::View&view,
                         int width,int height,std::vector<nativefeedback::Rect>&out){
 const double sx=double(width)/view.width,sy=double(height)/view.height;
 if(width<1||height<1||width>8192||height>8192||!view.valid()||sx<=0||sy<=0||sx>16||sy>16||radius<1||radius>7)return false;
 const double ax=x0*sx,ay=y0*sy,bx=x1*sx,by=y1*sy;
 const double rx=radius*sx,ry=radius*sy,dx=bx-ax,dy=by-ay;
 const int top=std::max(0,int(std::floor(std::min(ay,by)-ry)));
 const int bottom=std::min(height,int(std::ceil(std::max(ay,by)+ry))+1);
 for(int y=top;y<bottom;++y){
  double lo=0,hi=1;
  if(dy){double a=(y-ry-ay)/dy,b=(y+1+ry-ay)/dy;if(a>b)std::swap(a,b);lo=std::max(0.,a);hi=std::min(1.,b);if(lo>hi)continue;}
  const double xa=ax+lo*dx,xb=ax+hi*dx;
  const int l=std::max(0,int(std::floor(std::min(xa,xb)-rx)));
  const int r=std::min(width,int(std::ceil(std::max(xa,xb)+rx))+1);
  if(l>=r)continue;
  out.push_back({l,y,r-l,1});
 }
 return true;
}
template<class Read>bool beams(Read read,const mainview::View&view,int width,int height,std::vector<nativefeedback::Rect>&out){
 using namespace nativefeedback;
 if(!view.valid()||width<1||height<1||width>8192||height>8192||double(width)/view.width>16||double(height)/view.height>16)return false;
 std::array<uint32_t,6> head{},after{};
 if(!read(LaserArray,head.data(),sizeof head)||head[2]>8192||head[4]>head[2]||
    (head[4]&&!range(head[1],size_t(head[4])*4)))return false;
 std::vector<uint32_t> pointers(head[4]);
 if(!pointers.empty()&&!read(head[1],pointers.data(),pointers.size()*4))return false;
 auto sorted=pointers;std::sort(sorted.begin(),sorted.end());
 if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())return false;
 double zscale=0;
 if(!pointers.empty()&&(!read(0xB0CD48,&zscale,8)||!std::isfinite(zscale)||zscale<=0||zscale>1))return false;
 std::vector<Rect> pending;
 for(uint32_t p:pointers){
  std::array<uint8_t,0x5C> state{},verify{};
  if(!range(p,state.size())||!read(p,state.data(),state.size())||!read(p,verify.data(),verify.size())||state!=verify)return false;
  for(unsigned offset:{0x20u,0x21u,0x50u,0x51u,0x52u})if(state[offset]>1)return false;
  const int duration=int32_t(u32(state.data()+0x4C));
  if(duration<=0||state[0x51])continue; // The native Draw_It gate, not a second clock.
  Point3 source{},target{};std::memcpy(&source,state.data()+0x24,sizeof source);std::memcpy(&target,state.data()+0x30,sizeof target);
  if(!worldPoint(source)||!worldPoint(target))return false;
  const int thickness=int32_t(u32(state.data()+0x1C));
  if(state[0x20]&&(thickness<0||thickness>1024))return false;
  // Native HouseColor loops halve byte RGB, with an optional first-layer
  // reset. At most six offset steps survive the lowest threshold (8), so
  // either coordinate is at most 6 pixels away, regardless of larger width.
  // Axis-aligned projections can advance that coordinate on every step.
  const int radius=state[0x20]?std::min(6,thickness)+1:2;
  auto project=[&](Point3 q){return std::array<int,2>{{int((int64_t(q.x)-q.y)*30/256)-view.cameraX,
    int((int64_t(q.x)+q.y)*15/256)-screenHeight(q.z,zscale)-view.cameraY}};};
  const auto a=project(source),b=project(target);
  if(!beamCorridor(a[0],a[1],b[0],b[1],radius,view,width,height,pending))return false;
  // Bound temporary storage by spatial complexity, not the number of live
  // lasers. Many coincident/offscreen effects must not cause a count cutoff.
  if(pending.size()>65536){unionBeamRuns(pending);if(pending.size()>65536)return false;}
 }
 if(!read(LaserArray,after.data(),sizeof after)||head!=after)return false;
 if(!pointers.empty()){std::vector<uint32_t> verify(pointers.size());if(!read(head[1],verify.data(),verify.size()*4)||pointers!=verify)return false;}
 // Multiple coincident beams must not exhaust the compositor span budget.
 // Keep unit-height runs: the host unions these with placement/mining runs
 // before applying the final 16384-span compositor limit.
 unionBeamRuns(pending);
 out.insert(out.end(),pending.begin(),pending.end());return true;
}
}}

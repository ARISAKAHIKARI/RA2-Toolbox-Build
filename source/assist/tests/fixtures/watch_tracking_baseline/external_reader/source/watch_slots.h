#pragma once
#include "watch_view.h"
namespace external { namespace watch {
constexpr int MaximumWindows=3;
struct Track {
 Target target;std::vector<Target> members;int x=0,y=0,z=0;bool located=false;Target spatial;int spatialKind=-1,discoveryFrame=-1;bool searched=false;uint32_t discoveryCursor=0;
 explicit operator bool()const{return bool(target);}
};
template<class Read> bool locate(GameView<Read>&g,Target t,int&x,int&y,int&z){
 if(!t||g.template get<uint32_t>(t.pointer+0x10)!=t.id)return false;
 int xx=g.template get<int>(t.pointer+0x9C,-1),yy=g.template get<int>(t.pointer+0xA0,-1),zz=g.template get<int>(t.pointer+0xA4,-1);
 if(xx<0||yy<0||xx>=512*256||yy>=512*256||zz<0||zz>100000)return false;x=xx;y=yy;z=zz;return true;
}
template<class Read> Track capture(Read read){
 GameView<Read> g(read);Track out;Target hover;
 for(auto p:g.vec(0xA8ECB8)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t)&&std::find(out.members.begin(),out.members.end(),t)==out.members.end())out.members.push_back(t);}
 for(auto p:g.vec(0xA8EC78))if(g.template get<uint8_t>(p+0x431)){Target t{p,g.template get<uint32_t>(p+0x10)};if(valid(g,t)){hover=t;break;}}
 if(!out.members.empty())out.target=hover&&std::find(out.members.begin(),out.members.end(),hover)!=out.members.end()?hover:out.members.front();
 else if(hover){out.target=hover;out.members.push_back(hover);}
 out.located=locate(g,out.target,out.x,out.y,out.z);out.spatial=out.target;return out;
}
// Only the original group can inherit a dead focus. Transport/limbo is not
// death, and pointer reuse must never follow the unrelated replacement object.
template<class Read> void follow(Read read,Track&track){
 if(!track)return;GameView<Read> g(read);Target t=track.target;
 auto observed=life(read,t);if(observed.state==Life::Unknown)return;
 if(observed.state==Life::Present){auto center=anchor(read,t,{track.spatial,track.x,track.y,track.z,track.located,track.spatialKind,track.discoveryFrame,track.searched,track.discoveryCursor});if(center.located){track.x=center.x;track.y=center.y;track.z=center.z;track.located=true;track.spatial=center.target;track.spatialKind=center.kind;track.discoveryFrame=center.discoveryFrame;track.searched=center.searched;track.discoveryCursor=center.discoveryCursor;}return;}
 // Permanent removal/succession needs a second independent, agreeing sample.
 // Production callers use readFresh here, not the renderer's 4 KiB cache.
 if(!(life(read,t)==observed))return;bool identity=observed.state==Life::Dead;
 // Prefer the final same-object coordinates; use the last sample after free.
 if(identity&&!observed.limbo&&!observed.transport&&observed.onMap)track.located=locate(g,t,track.x,track.y,track.z)||track.located;
 Target nearest;int64_t best=int64_t(5*256)*(5*256);int nx=0,ny=0,nz=0;
 if(track.located)for(auto candidate:track.members){if(candidate==t)continue;auto state=life(read,candidate);if(state.state==Life::Unknown)return;if(state.state!=Life::Present)continue;if(!state.drawable())return;int x,y,z;if(!locate(g,candidate,x,y,z))return;
  int64_t dx=int64_t(x)-track.x,dy=int64_t(y)-track.y,d=dx*dx+dy*dy;
  if(d<=best&&(!nearest||d<best||candidate.id<nearest.id)){nearest=candidate;best=d;nx=x;ny=y;nz=z;}
 }
 if(!nearest){track={};return;}track.target=nearest;track.x=nx;track.y=ny;track.z=nz;track.located=true;track.spatial=nearest;track.spatialKind=g.kind(nearest.pointer);track.searched=false;
}
// Same-key slots are one group action, independent of global hotkey fan-out.
inline bool firstBinding(const uint32_t*keys,int slot){if(slot<0||slot>=MaximumWindows||!(keys[slot]&255))return false;for(int i=0;i<slot;++i)if(keys[i]==keys[slot])return false;return true;}
enum class AddResult {Added,AlreadyTracked,Full,NoTarget};
inline AddResult add(Track*tracks,const uint32_t*keys,uint32_t key,Track next,int&slot){
 slot=-1;if(!next)return AddResult::NoTarget;
 for(int i=0;i<MaximumWindows;++i)if(keys[i]==key&&tracks[i]&&tracks[i].target==next.target){slot=i;return AddResult::AlreadyTracked;}
 for(int i=0;i<MaximumWindows;++i)if(keys[i]==key&&!tracks[i]){tracks[i]=std::move(next);slot=i;return AddResult::Added;}return AddResult::Full;
}
struct Box {int x=0,y=0,w=0,h=0;};
inline void arrange(const bool*visible,int width,int height,Box*out){
 int count=0;for(int i=0;i<MaximumWindows;++i){out[i]={};count+=visible[i];}if(!count||width<1||height<1)return;
 int gap=8,margin=12,available=std::max(count,height-2*margin-(count-1)*gap);
 int h=std::min(256,available/count),w=std::min(320,std::max(1,width-2*margin));
 h=std::min(h,w*256/320);w=std::max(1,h*320/256);int y=(height-(count*h+(count-1)*gap))/2;
 for(int i=0;i<MaximumWindows;++i)if(visible[i]){out[i]={margin,y,w,h};y+=h+gap;}
}
template<class Read> bool camera(Read read,Target target,int width,int height,int&x,int&y,const Anchor&prior={}){
 if(width<=160||height<=0)return false;auto center=anchor(read,target,prior);if(!center.located)return false;int wx=center.x,wy=center.y,wz=center.z;
 // SetTacticalPosition accepts the center; native 6D8B30 subtracts half of
 // B0CE30/B0CE34 when deriving the visible origin at Tactical+B0/B4.
 x=int((int64_t(wx)-wy)*30/256);
 y=int((int64_t(wx)+wy)*15/256)-screenHeight(wz,projectionZScale(read));return true;
}
}}

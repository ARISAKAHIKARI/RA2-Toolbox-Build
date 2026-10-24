#pragma once
#include "watch_view.h"
namespace external { namespace watch {
constexpr int MaximumWindows=3;
struct Track {
 Target target;std::vector<Target> members;int x=0,y=0,z=0;bool located=false;
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
 out.located=locate(g,out.target,out.x,out.y,out.z);return out;
}
// Only the original group can inherit a dead focus. Transport/limbo is not
// death, and pointer reuse must never follow the unrelated replacement object.
template<class Read> void follow(Read read,Track&track){
 if(!track)return;GameView<Read> g(read);Target t=track.target;
 uint32_t identityRead=0;int healthRead=0;
 if(!read(t.pointer+0x10,&identityRead,4)||!read(t.pointer+0x6C,&healthRead,4))return;
 bool identity=g.kind(t.pointer)>=0&&g.template get<uint32_t>(t.pointer+0x10)==t.id;
 if(identity&&g.template get<int>(t.pointer+0x6C)>0&&(g.template get<uint8_t>(t.pointer+0x81)||g.template get<uint32_t>(t.pointer+0x11C)||g.template get<int>(t.pointer+0x220)!=0))return;
 bool alive=identity&&g.template get<int>(t.pointer+0x6C)>0&&g.template get<uint8_t>(t.pointer+0x74)&&g.template get<uint8_t>(t.pointer+0x90);
 if(alive){if(valid(g,t))track.located=locate(g,t,track.x,track.y,track.z)||track.located;return;}
 // Prefer the final same-object coordinates; use the last sample after free.
 if(identity)track.located=locate(g,t,track.x,track.y,track.z)||track.located;
 Target nearest;int64_t best=int64_t(5*256)*(5*256);int nx=0,ny=0,nz=0;
 if(track.located)for(auto candidate:track.members){if(candidate==t||!valid(g,candidate))continue;int x,y,z;if(!locate(g,candidate,x,y,z))continue;
  int64_t dx=int64_t(x)-track.x,dy=int64_t(y)-track.y,d=dx*dx+dy*dy;
  if(d<=best&&(!nearest||d<best||candidate.id<nearest.id)){nearest=candidate;best=d;nx=x;ny=y;nz=z;}
 }
 if(!nearest){track={};return;}track.target=nearest;track.x=nx;track.y=ny;track.z=nz;track.located=true;
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
template<class Read> bool camera(Read read,Target target,int width,int height,int&x,int&y){
 GameView<Read> g(read);if(!valid(g,target)||width<=160||height<=0)return false;int wx,wy,wz;if(!locate(g,target,wx,wy,wz))return false;
 // SetTacticalPosition accepts the center; native 6D8B30 subtracts half of
 // B0CE30/B0CE34 when deriving the visible origin at Tactical+B0/B4.
 x=int((int64_t(wx)-wy)*30/256);
 y=int((int64_t(wx)+wy)*15/256)-screenHeight(wz,projectionZScale(read));return true;
}
}}

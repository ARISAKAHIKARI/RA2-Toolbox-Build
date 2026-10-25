#pragma once
#include "game_view.h"
namespace external { namespace watch {
struct Target {uint32_t pointer=0,id=0;bool operator==(const Target&o)const{return pointer==o.pointer&&id==o.id;}explicit operator bool()const{return pointer&&id;}};
enum class Life {Unknown,Present,Dead,Reused};
struct LifeSample {
 Life state=Life::Unknown;uint32_t vtable=0,id=0,transport=0;int health=0,cloak=0;uint8_t alive=0,limbo=0,onMap=0;
 bool drawable()const{return state==Life::Present&&alive&&!limbo&&onMap&&!transport&&!cloak;}
 bool terminal()const{return state==Life::Dead||state==Life::Reused;}
 bool operator==(const LifeSample&o)const{return state==o.state&&vtable==o.vtable&&id==o.id&&transport==o.transport&&health==o.health&&cloak==o.cloak&&alive==o.alive&&limbo==o.limbo&&onMap==o.onMap;}
};
template<class Read> LifeSample life(Read read,Target t){
 LifeSample s;if(!t||t.pointer<0x10000)return s;
 if(!read(t.pointer,&s.vtable,4)||!read(t.pointer+0x10,&s.id,4))return s;
 if(s.id!=t.id){s.state=Life::Reused;return s;}
 // An unsupported vtable is not proof that the tracked object died.
 if(s.vtable!=0x7F5C70&&s.vtable!=0x7EB058&&s.vtable!=0x7E3EBC&&s.vtable!=0x7E22A4)return s;
 if(!read(t.pointer+0x6C,&s.health,4)||!read(t.pointer+0x74,&s.alive,1)||!read(t.pointer+0x81,&s.limbo,1)||!read(t.pointer+0x90,&s.onMap,1)||!read(t.pointer+0x11C,&s.transport,4)||!read(t.pointer+0x220,&s.cloak,4))return s;
 s.state=s.health>0&&(s.alive||s.limbo||s.transport||s.cloak)?Life::Present:Life::Dead;return s;
}
struct Anchor {Target target;int x=0,y=0,z=0;bool located=false;int kind=-1,discoveryFrame=-1;bool searched=false;uint32_t discoveryCursor=0;};
template<class Read> bool position(Read read,Target t,Anchor&out){
 int xyz[3]{};uint32_t id=0;if(!t||!read(t.pointer+0x9C,xyz,sizeof xyz)||!read(t.pointer+0x10,&id,4)||id!=t.id||xyz[0]<0||xyz[1]<0||xyz[0]>=512*256||xyz[1]>=512*256||xyz[2]<0||xyz[2]>100000)return false;
 out={t,xyz[0],xyz[1],xyz[2],true,GameView<Read>(read).kind(t.pointer)};return true;
}
// The tracked identity and the object supplying the camera position differ
// while a passenger is inside a carrier. Validate both links, never substitute
// an arbitrary nearby object for a missing spatial host.
template<class Read> bool carries(Read read,Target carrier,Target passenger){
 auto host=life(read,carrier);if(!host.drawable())return false;
 int count=0;uint32_t q=0;if(!read(carrier.pointer+0x114,&count,4)||!read(carrier.pointer+0x118,&q,4)||count<1||count>256)return false;
 uint32_t seen[256]{};bool found=false;
 for(int n=0;n<count;++n){if(q<0x10000)return false;for(int j=0;j<n;++j)if(seen[j]==q)return false;seen[n]=q;
  uint32_t transport=0,id=0,next=0;if(!read(q+0x11C,&transport,4)||transport!=carrier.pointer||!read(q+0x10,&id,4)||!read(q+0x30,&next,4))return false;
  if(q==passenger.pointer&&id==passenger.id)found=true;q=next;
 }return found&&!q;
}
// BuildingClass::Occupants is DynamicVectorClass<InfantryClass*> at +684:
// Items +688, Capacity +68C, Count +694. Read the bounded header and member
// identity on both sides; a nearby building is never sufficient evidence.
template<class Read> bool occupies(Read read,Target building,Target infantry){
 GameView<Read> g(read);if(g.kind(building.pointer)!=3||!life(read,building).drawable())return false;
 uint32_t header[4]{},after[4]{},id=0;if(!read(building.pointer+0x688,header,sizeof header))return false;
 uint32_t items=header[0],capacity=header[1],count=header[3];if(items<0x10000||count<1||count>128||capacity<count||capacity>65536)return false;
 std::vector<uint32_t> occupants(count),again(count);if(!read(items,occupants.data(),size_t(count)*4)||std::find(occupants.begin(),occupants.end(),infantry.pointer)==occupants.end())return false;
 return read(infantry.pointer+0x10,&id,4)&&id==infantry.id&&read(items,again.data(),size_t(count)*4)&&occupants==again&&read(building.pointer+0x688,after,sizeof after)&&std::equal(std::begin(header),std::end(header),std::begin(after));
}
template<class Read> Anchor anchor(Read read,Target focus,const Anchor&prior={}){
 auto s=life(read,focus);if(s.state!=Life::Present)return {};
 Anchor out;int discoveryFrame=prior.discoveryFrame;bool searched=prior.searched;uint32_t cursor=prior.discoveryCursor;if(s.drawable()&&position(read,focus,out))return out;
 if(s.transport>=0x10000){uint32_t id=0;if(read(s.transport+0x10,&id,4)){Target carrier{s.transport,id};if(carries(read,carrier,focus)&&position(read,carrier,out))return out;}}
 if(s.vtable==0x7EB058&&(s.limbo||!s.onMap)){
  if(prior.target&&occupies(read,prior.target,focus)&&position(read,prior.target,out))return out;
  int frame=-1;read(0xA8ED84,&frame,4);
  // Only discovery is bounded to once per simulation frame. Known-host
  // membership/position and the surrounding scene still refresh every tick.
  if(!searched||frame!=discoveryFrame){searched=true;discoveryFrame=frame;GameView<Read> g(read);auto buildings=g.vec(0xA8EB40);size_t count=std::min<size_t>(64,buildings.size()),start=buildings.empty()?0:cursor%buildings.size();
   for(size_t n=0;n<count;++n){auto p=buildings[(start+n)%buildings.size()];uint32_t header[4]{},id=0;
    // Empty buildings require one bounded header read, not a full set of
    // uncached life fields. Spread large building arrays over later frames.
    if(!read(p+0x688,header,sizeof header)||!header[3]||header[3]>128||!read(p+0x10,&id,4))continue;
    Target building{p,id};if(occupies(read,building,focus)&&position(read,building,out))return out;
   }cursor=buildings.empty()?0:uint32_t((start+count)%buildings.size());
  }
 }
 // A hidden unit without a verified host keeps its last valid camera center.
 // The scene at that center is still sampled; this does not reuse a frozen
 // framebuffer or draw the hidden passenger at stale world coordinates.
 if(prior.located){out=prior;out.target={};out.searched=searched;out.discoveryFrame=discoveryFrame;out.discoveryCursor=cursor;return out;}return {};
}
}}

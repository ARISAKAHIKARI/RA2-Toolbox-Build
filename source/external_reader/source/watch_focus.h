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
 // ObjectClass +74 is IsOnMap; +90 is IsAlive. An alive unit can be
 // temporarily off-map without being in Limbo or inside a transport.
 if(!read(t.pointer+0x6C,&s.health,4)||!read(t.pointer+0x74,&s.onMap,1)||!read(t.pointer+0x81,&s.limbo,1)||!read(t.pointer+0x90,&s.alive,1)||!read(t.pointer+0x11C,&s.transport,4)||!read(t.pointer+0x220,&s.cloak,4))return s;
 s.state=s.health>0&&(s.alive||s.limbo||s.transport||s.cloak)?Life::Present:Life::Dead;return s;
}
struct Anchor {Target target;int x=0,y=0,z=0;bool located=false;int kind=-1,discoveryFrame=-1;bool searched=false;uint32_t discoveryCursor=0;Target garrison;};
template<class Read> bool position(Read read,Target t,Anchor&out){
 int xyz[3]{};uint32_t id=0;if(!t||!read(t.pointer+0x9C,xyz,sizeof xyz)||!read(t.pointer+0x10,&id,4)||id!=t.id||xyz[0]<0||xyz[1]<0||xyz[0]>=512*256||xyz[1]>=512*256||xyz[2]<0||xyz[2]>100000)return false;
 out={t,xyz[0],xyz[1],xyz[2],true,GameView<Read>(read).kind(t.pointer)};return true;
}
// The tracked identity and the object supplying the camera position differ
// while a passenger is inside a carrier. Validate both links, never substitute
// an arbitrary nearby object for a missing spatial host.
template<class Read> bool carries(Read read,Target carrier,Target passenger){
 auto host=life(read,carrier);if(host.state!=Life::Present)return false;
 int count=0;uint32_t q=0;if(!read(carrier.pointer+0x114,&count,4)||!read(carrier.pointer+0x118,&q,4)||count<1||count>256)return false;
 uint32_t seen[256]{};bool found=false;
 for(int n=0;n<count;++n){if(q<0x10000)return false;for(int j=0;j<n;++j)if(seen[j]==q)return false;seen[n]=q;
  uint32_t transport=0,id=0,next=0;if(!read(q+0x11C,&transport,4)||transport!=carrier.pointer||!read(q+0x10,&id,4)||!read(q+0x30,&next,4))return false;
  if(q==passenger.pointer&&id==passenger.id)found=true;q=next;
 }int afterCount=0;uint32_t afterHead=0,transport=0,id=0;
 return found&&!q&&read(carrier.pointer+0x114,&afterCount,4)&&afterCount==count&&read(carrier.pointer+0x118,&afterHead,4)&&afterHead==seen[0]&&
  read(passenger.pointer+0x11C,&transport,4)&&transport==carrier.pointer&&read(passenger.pointer+0x10,&id,4)&&id==passenger.id&&life(read,carrier)==host;
}
// BuildingClass::Occupants is DynamicVectorClass<InfantryClass*> at +684:
// Items +688, Capacity +68C, Count +694. Read the bounded header and member
// identity on both sides; a nearby building is never sufficient evidence.
template<class Read> bool occupies(Read read,Target building,Target infantry){
 GameView<Read> g(read);auto host=life(read,building);if(g.kind(building.pointer)!=3||host.state!=Life::Present)return false;
 uint32_t header[4]{},after[4]{},id=0;if(!read(building.pointer+0x688,header,sizeof header))return false;
 uint32_t items=header[0],capacity=header[1],count=header[3];if(items<0x10000||count<1||count>128||capacity<count||capacity>65536)return false;
 std::vector<uint32_t> occupants(count),again(count);if(!read(items,occupants.data(),size_t(count)*4)||std::find(occupants.begin(),occupants.end(),infantry.pointer)==occupants.end())return false;
 return read(infantry.pointer+0x10,&id,4)&&id==infantry.id&&read(items,again.data(),size_t(count)*4)&&occupants==again&&read(building.pointer+0x688,after,sizeof after)&&std::equal(std::begin(header),std::end(header),std::begin(after))&&life(read,building)==host;
}
// Tank bunkers use BunkerLinkedItem in BOTH directions, not Transporter or a
// nearby building/radio reservation. The original vehicle identity survives.
template<class Read>bool bunkers(Read read,Target building,Target vehicle){
 GameView<Read> g(read);auto host=life(read,building);if(g.kind(building.pointer)!=3||g.kind(vehicle.pointer)!=2||host.state!=Life::Present)return false;
 uint32_t type=0,back=0,forward=0,id=0;uint8_t bunker=0;
 return read(building.pointer+0x520,&type,4)&&type>=0x10000&&read(type+0x16AB,&bunker,1)&&bunker&&
  read(building.pointer+0x2E4,&back,4)&&back==vehicle.pointer&&read(vehicle.pointer+0x2E4,&forward,4)&&forward==building.pointer&&
  read(vehicle.pointer+0x10,&id,4)&&id==vehicle.id&&life(read,building)==host;
}
// A newly resolved spatial host is not permission to reveal a hidden enemy's
// current position, including when a user clicks the watch to navigate. Use
// the same native Cell exploration bits as radar_shroud_selector; uncertain
// or edge-masked cells conservatively keep the last verified camera point.
template<class Read>bool visibleHost(Read read,const Anchor&host){
 uint32_t observer=0,owner=0;if(!read(0xA83D4C,&observer,4)||observer<0x10000||!read(host.target.pointer+0x21C,&owner,4))return false;if(owner==observer)return true;
 uint32_t options=0,items=0,cell=0,again=0;int count=0;uint8_t option=0,spy=0,shroud=0,fog=0;
 if(!read(0xA8B230,&options,4)||options<0x10000||!read(options+1,&option,1)||!read(observer+0x1F5,&spy,1)||
    !read(0x87F7E8+0x13C,&items,4)||!read(0x87F7E8+0x140,&count,4)||items<0x10000||count<1||count>512*512)return false;
 const int x=host.x/256,y=host.y/256,index=y*512+x;if(x<0||y<0||x>=512||y>=512||index>=count)return false;
 int16_t xy[2]{};const uint32_t at=items+uint32_t(index)*4;
 return read(at,&cell,4)&&cell>=0x10000&&read(cell+0x24,xy,sizeof xy)&&xy[0]==x&&xy[1]==y&&
  read(cell+0x12C,&shroud,1)&&(shroud&0x18)==0x10&&(!(option&0x10)||spy||
  (read(cell+0x140,&fog,1)&&(fog&0x03)==0x01))&&read(at,&again,4)&&again==cell;
}
template<class Read> Anchor anchor(Read read,Target focus,const Anchor&prior={}){
 const auto original=life(read,focus);if(original.state!=Life::Present)return {};
 Anchor out;int discoveryFrame=prior.discoveryFrame;bool searched=prior.searched;uint32_t cursor=prior.discoveryCursor;Target garrison=prior.garrison;
 Target chain[8]{};unsigned edges[7]{},count=0;Target current=focus;bool complete=false;
 for(;count<8;){bool cycle=false;for(unsigned i=0;i<count;++i)cycle|=chain[i]==current;if(cycle)break;chain[count++]=current;
  auto s=life(read,current);if(s.state!=Life::Present)break;Target host;unsigned edge=0;
  auto identity=[&](uint32_t p){uint32_t id=0;return p>=0x10000&&read(p+0x10,&id,4)?Target{p,id}:Target{};};
  if(s.transport>=0x10000){host=identity(s.transport);if(!carries(read,host,current))break;edge=1;}
  else if(s.vtable==0x7F5C70){uint32_t p=0;if(!read(current.pointer+0x2E4,&p,4))break;if(p){host=identity(p);if(!bunkers(read,host,current))break;edge=2;}}
  if(!host&&s.drawable()){if(position(read,current,out))complete=true;break;}
  if(!host&&s.vtable==0x7EB058&&(s.limbo||!s.onMap)){
   if(garrison&&occupies(read,garrison,current))host=garrison;
   else if(prior.target&&occupies(read,prior.target,current))host=prior.target;
   int frame=-1;if(!host&&read(0xA8ED84,&frame,4)&&(!searched||frame!=discoveryFrame)){
    searched=true;discoveryFrame=frame;GameView<Read> g(read);auto buildings=g.vec(0xA8EB40);size_t nmax=std::min<size_t>(64,buildings.size()),start=buildings.empty()?0:cursor%buildings.size();
    for(size_t n=0;n<nmax;++n){auto p=buildings[(start+n)%buildings.size()];uint32_t header[4]{};
     if(!read(p+0x688,header,sizeof header)||!header[3]||header[3]>128)continue;
     auto candidate=identity(p);if(candidate&&occupies(read,candidate,current)){host=candidate;break;}
    }cursor=buildings.empty()?0:uint32_t((start+nmax)%buildings.size());
   }
   if(host){garrison=host;edge=3;}
  }
  if(!host||count>=8)break;edges[count-1]=edge;current=host;
 }
 // Recheck every relation after locating the final visible host. This also
 // rejects cycles, changed membership/UIDs and hosts deleted during the walk.
 if(complete&&life(read,focus)==original){
  for(unsigned i=0;i+1<count&&complete;++i)complete=edges[i]==1?carries(read,chain[i+1],chain[i]):edges[i]==2?bunkers(read,chain[i+1],chain[i]):occupies(read,chain[i+1],chain[i]);
  if(complete&&life(read,out.target).drawable()&&(count==1||visibleHost(read,out))){out.discoveryFrame=discoveryFrame;out.searched=searched;out.discoveryCursor=cursor;out.garrison=garrison;return out;}
 }
 // A hidden unit without a verified host keeps its last valid camera center.
 // The scene at that center is still sampled; this does not reuse a frozen
 // framebuffer or draw the hidden passenger at stale world coordinates.
 if(prior.located){out=prior;out.target={};out.searched=searched;out.discoveryFrame=discoveryFrame;out.discoveryCursor=cursor;out.garrison=garrison;return out;}return {};
}
}}

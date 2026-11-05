#pragma once
#include "watch_focus.h"
namespace external { namespace watch {
// Fixed YRpp d979554 GeneralDefinitions::Mission; corroborated by the
// existing native building state/unload readers. These are decimal values.
constexpr int TransitionUnloadMission=16,TransitionSellingMission=19;
// A native deploy/undeploy allocates a new object; it does not transfer UID.
// This is a conservative, short-frame birth inference, not a native backlink.
// Without every witness below the caller retains a live fixed camera instead.
struct Transition {
 bool armed=false,inventoryValid=false,fallbackEligible=false;int frame=-1,kind=-1,x=0,y=0,z=0;
 uint32_t type=0,owner=0;std::vector<Target> existing;
};
struct TransitionState {
 uint32_t id=0,type=0,owner=0,destination=0;int frame=-1,kind=-1,x=0,y=0,z=0;bool active=false,fallbackEligible=false;
 bool operator==(const TransitionState&o)const{return id==o.id&&type==o.type&&owner==o.owner&&destination==o.destination&&frame==o.frame&&kind==o.kind&&x==o.x&&y==o.y&&z==o.z&&active==o.active&&fallbackEligible==o.fallbackEligible;}
};
template<class Read>bool transitionState(Read read,Target focus,TransitionState&out){
 GameView<Read> g(read);const auto live=life(read,focus);if(!live.drawable())return false;
 TransitionState s;s.id=focus.id;s.kind=g.kind(focus.pointer);uint32_t observer=0;int mission=-1;
 if(!read(0xA8ED84,&s.frame,4)||s.frame<0||!read(0xA83D4C,&observer,4)||!read(focus.pointer+0x21C,&s.owner,4)||
    !read(focus.pointer+0xAC,&mission,4)||!read(focus.pointer+0x9C,&s.x,4)||!read(focus.pointer+0xA0,&s.y,4)||!read(focus.pointer+0xA4,&s.z,4))return false;
 if(s.x<0||s.y<0||s.x>=512*256||s.y>=512*256||s.z<0||s.z>100000)return false;
 // Only an owned conversion can create a new tracking identity. Visibility
 // of all rendered pixels still uses the unchanged shared scene/shroud path.
 if(s.owner<0x10000||s.owner!=observer||(s.kind!=2&&s.kind!=3)){out=s;return true;}
 uint32_t sourceType=0;if(!read(focus.pointer+(s.kind==3?0x520:0x6C4),&sourceType,4)||sourceType<0x10000||
    !read(sourceType+(s.kind==3?0x408:0x404),&s.type,4))return false;
 if(s.type<0x10000){out=s;return true;}uint32_t vt=0;if(!read(s.type,&vt,4))return false;
 if(vt!=(s.kind==2?0x7E4570u:0x7F6218u)){out=s;return true;}
 int width=0,height=0,index=-1;const uint32_t buildingType=s.kind==2?s.type:sourceType;
 if(!read(buildingType+0xEF0,&index,4)||index<0||index>=21||!read(0x8192B8+index*4,&width,4)||!read(0x819310+index*4,&height,4)||width<1||height<1||width>32||height>32)return false;
 if(s.kind==2){
  uint8_t yard=0;if(!read(s.type+0x16B9,&yard,1))return false;s.fallbackEligible=yard!=0;
  uint8_t deploying=0;if(!read(focus.pointer+0x68C,&deploying,1))return false;s.active=mission==TransitionUnloadMission||deploying!=0;
  // TryToDeploy 739460..7394BE: foundations larger than 2x2 use
  // the native cell delta, then Unlimbo receives that cell's exact center.
  int cx=s.x/256,cy=s.y/256;if(width>2||height>2){int16_t delta[2]{};if(!read(0x89F6A4,delta,sizeof delta))return false;cx+=delta[0];cy+=delta[1];}
  s.x=cx*256+128;s.y=cy*256+128;s.kind=3;
 }else{
  int status=0;uint8_t yard=0;if(!read(focus.pointer+0xBC,&status,4)||!read(sourceType+0x16B9,&yard,1)||!read(focus.pointer+0x218,&s.destination,4))return false;
  // A yard's ordinary Sell is not an undeploy witness. Native 449CEA only
  // creates the MCV with ArchiveTarget and a non-campaign session enabled.
  int session=0;uint8_t redeploy=0;if(yard&&(!read(0xA8B238,&session,4)||!read(0xA8B320,&redeploy,1)))return false;
  s.active=mission==TransitionSellingMission&&status>=1&&(!yard||(s.destination>=0x10000&&session!=0&&redeploy));
  if(width>2||height>2){int delta[2]{};if(!read(0x89F6F0,delta,sizeof delta))return false;s.x=((s.x+delta[0])/256)*256+128;s.y=((s.y+delta[1])/256)*256+128;}
  s.kind=2;
 }
 if(s.x<0||s.y<0||s.x>=512*256||s.y>=512*256)return false;
 if(!(life(read,focus)==live))return false;out=s;return true;
}
// The complete stable inventory is necessary to exclude pre-existing objects;
// an unreadable array/object cannot be interpreted as an absent candidate.
template<class Read>bool transitionCandidates(Read read,const Transition&t,std::vector<Target>&out){
 GameView<Read> g(read);std::vector<uint32_t> objects,again;out.clear();if(!g.vecChecked(0xA8EC78,objects,8192))return false;
 for(uint32_t p:objects){uint32_t vt=0;if(p<0x10000||!read(p,&vt,4))return false;
  if(vt!=(t.kind==3?0x7E3EBCu:0x7F5C70u))continue;
  uint32_t type=0,owner=0,id=0;int xyz[3]{};
  if(!read(p+(t.kind==3?0x520:0x6C4),&type,4)||!read(p+0x21C,&owner,4)||!read(p+0x10,&id,4)||!read(p+0x9C,xyz,sizeof xyz))return false;
  if(type!=t.type||owner!=t.owner||xyz[0]!=t.x||xyz[1]!=t.y||xyz[2]!=t.z)continue;
  Target candidate{p,id};if(!candidate)return false;if(std::find(out.begin(),out.end(),candidate)==out.end())out.push_back(candidate);
 }
 return g.vecChecked(0xA8EC78,again,8192)&&objects==again;
}
template<class Read>bool transitionInventory(Read read,std::vector<Target>&out){
 GameView<Read> g(read);std::vector<uint32_t> objects,again;out.clear();if(!g.vecChecked(0xA8EC78,objects,8192))return false;
 for(auto p:objects){uint32_t id=0;if(p<0x10000||!read(p+0x10,&id,4)||!id)return false;out.push_back({p,id});}
 return g.vecChecked(0xA8EC78,again,8192)&&objects==again;
}
template<class Read>void observeTransition(Read read,Target focus,Transition&out){
 TransitionState a,b;if(!transitionState(read,focus,a))return;if(!a.active){if(transitionState(read,focus,b)&&a==b){out={};out.fallbackEligible=a.fallbackEligible;}return;}
 // UI navigation and the render worker may observe one simulation frame
 // many times. Its verified before-inventory is shared by that Track copy.
 if(out.armed&&out.frame==a.frame&&out.kind==a.kind&&out.type==a.type&&out.owner==a.owner&&out.x==a.x&&out.y==a.y&&out.z==a.z&&out.fallbackEligible==a.fallbackEligible)return;
 Transition next;next.armed=true;next.fallbackEligible=a.fallbackEligible;next.frame=a.frame;next.kind=a.kind;next.type=a.type;next.owner=a.owner;next.x=a.x;next.y=a.y;next.z=a.z;
 // Exclude every existing address, including other types and objects outside
 // the destination. Moving an old MCV into the cell is not a new birth.
 std::vector<Target> again;next.inventoryValid=transitionInventory(read,next.existing)&&transitionInventory(read,again)&&next.existing==again;
 if(!transitionState(read,focus,b)||!(a==b))return;
 out=std::move(next);
}
template<class Read>Target transitionSuccessor(Read read,Target old,const Transition&t){
 int frame=-1;if(!t.armed||!t.inventoryValid||!read(0xA8ED84,&frame,4)||frame<t.frame||frame-t.frame>2)return {};
 uint32_t observer=0;if(!read(0xA83D4C,&observer,4)||observer!=t.owner)return {};
 std::vector<Target> a,b;if(!transitionCandidates(read,t,a)||!transitionCandidates(read,t,b)||a!=b)return {};
 Target found;for(auto candidate:a){if(candidate.pointer==old.pointer)continue;bool existed=false;for(auto before:t.existing)if(before.pointer==candidate.pointer){existed=true;break;}if(existed)continue;
  const auto live=life(read,candidate);if(!live.drawable())return {};if(found)return {};found=candidate;
  if(!(life(read,candidate)==live))return {};
 }
 std::vector<Target> final;if(!transitionCandidates(read,t,final)||final!=a)return {};
 int after=-1;return read(0xA8ED84,&after,4)&&after==frame?found:Target{};
}
}}

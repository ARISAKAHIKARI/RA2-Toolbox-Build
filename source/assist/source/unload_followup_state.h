#pragma once
#include <cstdint>
#include <cstring>

// Checked *live* observation for an already admitted Ctrl+D passenger. This
// does not create a world snapshot or change the Host/Runtime compute split.
// Unknown native memory is not equivalent to Stop, carrier=0, death or Move.
namespace unloadfollowup {
enum class Knowledge : uint8_t { Unknown, Invalid, Current };
struct Observation {
 uint32_t uid=0, owner=0, carrier=0, type=0, target=0, destination=0;
 int hp=0,mission=0,queued=0,x=0,y=0;
 bool onMap=false,alive=false,limbo=false;
 bool deployer=false,engineer=false,spy=false;
 bool live()const{return hp>0&&onMap&&!limbo&&alive;}
};
// Reader: bool read(uint32_t at, void* out, size_t bytes).
// Two small identity/status spans and a type-flag span are observed together;
// failure of any required read leaves the caller's observation untouched.
template<class Reader>
Knowledge observe(uint32_t p,uint32_t expectedID,uint32_t house,Reader read,Observation&output){
 if(p<0x10000||!expectedID)return Knowledge::Invalid;
 constexpr size_t Span=0x2B8u-0x10u;
 uint8_t raw[Span]{},flags[0xEC9-0xEBE]{};
 uint32_t vt=0,uid=0,owner=0,type=0,destination=0;
 if(!read(p,&vt,sizeof vt)||!read(p+0x10,raw,sizeof raw)||
    !read(p+0x5A4,&destination,sizeof destination)||
    !read(p+0x6C0,&type,sizeof type))return Knowledge::Unknown;
 auto get=[&](size_t offset,auto&value){std::memcpy(&value,raw+offset-0x10,sizeof value);};
 get(0x10,uid);get(0x21C,owner);
 if(vt!=0x7EB058u||uid!=expectedID||owner!=house)return Knowledge::Invalid;
 Observation next{};next.uid=uid;next.owner=owner;next.type=type;next.destination=destination;
 get(0x6C,next.hp);get(0x74,next.onMap);get(0x90,next.alive);get(0x81,next.limbo);
 get(0x11C,next.carrier);get(0xAC,next.mission);get(0xB4,next.queued);
 get(0x2B4,next.target);get(0x9C,next.x);get(0xA0,next.y);
 if(next.hp<=0)return Knowledge::Invalid;
 if(type<0x10000)return Knowledge::Invalid;
 if(!read(type+0xEBE,flags,sizeof flags))return Knowledge::Unknown;
 next.spy=flags[0];next.engineer=flags[0xEC3-0xEBE];next.deployer=flags[0xEC8-0xEBE];
 output=next;
 return Knowledge::Current;
}
inline bool unloadExit(const Observation&view,uint32_t formerCarrier,int mode,int landedFrame,int frame){
 if(view.mission!=7)return false;
 return view.destination==formerCarrier||
    (mode==0&&!view.destination&&!view.target&&frame-landedFrame<=180);
}
} // namespace unloadfollowup

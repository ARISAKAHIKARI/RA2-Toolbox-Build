#pragma once
#include "game_view.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace external { namespace buildings {
// These SHPs are drawn directly by Building::Draw_It (43D290). They are not
// AnimClass objects and must not be added to, or advanced like, Anims[21].
enum Part { Bib, Deploy, RoofDeploy, UnderDoor, UnderRoofDoor, Door, PartCount };
inline constexpr uint32_t partOffsets[PartCount]={0x1518,0x14E4,0x14FC,0x14EC,0x1504,0x150C};
enum Flag { Constructed=1, Unloading=2, Roof=4, Damaged=8 };
struct State {int flags=0,doorFrame=-1;};
template<class Read>bool buildup(const GameView<Read>&g,uint32_t object){
 // Building::GetImage (4513D0) keeps make art until ActuallyPlacedOnMap.
 return g.template get<int>(object+0x534)==0||!g.template get<uint8_t>(object+0x6E4);
}

// TransitionTimer::PercentageDone (4A52F0). A stopped timer preserves TimeLeft;
// an inactive transition is complete. The game clock is the simulation frame.
inline double progress(int start,int left,int duration,bool active,int now){
 if(!active||duration<=0)return 1.;
 int64_t remaining=start==-1?left:std::max<int64_t>(0,int64_t(left)-(int64_t(now)-start));
 return std::clamp((double(duration)-double(remaining))/duration,0.,1.);
}
inline int doorFrame(int stages,int start,int left,int duration,bool active,bool direction,int now,bool damaged){
 if(stages<=0||stages>4096)return -1;
 int frame=int(progress(start,left,duration,active,now)*stages);
 if(!direction)frame=active?stages-frame:0;
 frame=std::clamp(frame,0,stages-1);
 return frame+(damaged?stages:0);
}
template<class Read>State state(Read read,uint32_t object,uint32_t type,double conditionYellow){
 GameView<Read> g(read);State out;
 if(g.template get<int>(object+0x534)!=0)out.flags|=Constructed;
 int mission=g.template get<int>(object+0xAC,-1);if(mission==-1)mission=g.template get<int>(object+0xB4,-1);
 if(mission!=16)return out;
 out.flags|=Unloading;
 // Native 43D33A/65AD40 chooses the roof from radio link zero, not the
 // factory's queue item or a building-type guess about the unit being made.
 uint32_t links=g.template get<uint32_t>(object+0xE4);int count=g.template get<int>(object+0xE8);
 if(links>=0x10000&&count>0&&count<=64){uint32_t unit=g.template get<uint32_t>(links),unitType=g.type(unit,g.kind(unit));
  if(unitType>=0x10000&&(g.template get<uint8_t>(unitType+0xD94)||g.template get<uint8_t>(unitType+0xD6A)))out.flags|=Roof;
 }
 int health=g.template get<int>(object+0x6C),strength=g.template get<int>(type+0xA0);
 bool damaged=health>0&&strength>0&&double(health)/strength<=conditionYellow;
 if(damaged)out.flags|=Damaged;
 if(g.template get<uint32_t>(type+0x150C)>=0x10000)
  out.doorFrame=doorFrame(g.template get<int>(type+0xF00),g.template get<int>(object+0x358,-1),
   g.template get<int>(object+0x360),g.template get<int>(object+0x364),
   g.template get<uint8_t>(object+0x368)!=0,g.template get<uint8_t>(object+0x369)!=0,
   g.template get<int>(0xA8ED84),damaged&&g.template get<uint8_t>(type+0x1700));
 return out;
}
}}

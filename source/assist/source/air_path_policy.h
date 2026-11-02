#pragma once
#include "../../shared/source/game_entry_api.h"
namespace assist { namespace airpathpolicy {
// Native UI gate matches MO AirWay: leave route execution to the engine.
// Only explicit mode 2 also enables the historical local arrival experiment.
// The default native mode never changes arrival/return-to-base behavior.
inline bool multiplayer(int session){return session==3||session==4;}
inline int mode(bool regular,bool requested,unsigned networkMode){return requested?2+(networkMode<=2?int(networkMode):0):regular?1:0;}
inline bool native(int session,bool requested,unsigned networkMode=0){
 if constexpr(!gameentry::CodeWritesAllowed)return false;
 return requested&&(session==0||session==5||(multiplayer(session)&&(networkMode==1||networkMode==2)));
}
inline bool arrival(int session,bool requested,unsigned networkMode=0){
 return native(session,requested,networkMode)&&networkMode==2;
}
inline bool events(int session,bool regular,bool requested,unsigned networkMode=0){
 return (session==0||session==5||multiplayer(session))&&!native(session,requested,networkMode)&&(regular||requested);
}
}}


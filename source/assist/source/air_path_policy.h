#pragma once
namespace assist { namespace airpathpolicy {
// UI mode 1 uses a custom key; mode 2 uses the game's planning key but
// routes aircraft through the same bounded sequential Move continuation.
// Only explicit networkMode 1/2 installs UI/arrival hooks.
// Neither native experiment is netplay-certified.
constexpr bool multiplayer(int session){return session==3||session==4;}
inline int mode(bool regular,bool requested,unsigned networkMode){return requested?2+(networkMode<=2?int(networkMode):0):regular?1:0;}
constexpr bool native(int session,bool requested,unsigned networkMode=0){
 return requested&&(networkMode==1||networkMode==2)&&(session==0||session==5||multiplayer(session));
}
constexpr bool arrival(int session,bool requested,unsigned networkMode=0){
 return native(session,requested,networkMode)&&networkMode==2;
}
// Native PlanningToken append cannot reliably materialize aircraft nodes.
// The normal aircraft waypoint mode uses sequential Move orders instead;
// the explicit higher-risk hook modes remain separate.
constexpr bool direct(int, bool, unsigned){return false;}
// The current mode is mandatory at every admission point. Omitting it would
// admit a stale sampled request after switching to either native experiment.
constexpr bool events(int session,bool regular,bool requested,unsigned networkMode){
 return (session==0||session==5||multiplayer(session))&&!native(session,requested,networkMode)&&
  !direct(session,requested,networkMode)&&(regular||(requested&&networkMode==0));
}
}}


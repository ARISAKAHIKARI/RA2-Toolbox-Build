#pragma once
namespace assist { namespace airpathpolicy {
// Compatibility mode 0 uses ordinary synchronized moves in every supported
// session. Only explicit high-risk modes 1/2 install native UI/arrival hooks.
// Neither native experiment is netplay-certified.
constexpr bool multiplayer(int session){return session==3||session==4;}
inline int mode(bool regular,bool requested,unsigned networkMode){return requested?2+(networkMode<=2?int(networkMode):0):regular?1:0;}
constexpr bool native(int session,bool requested,unsigned networkMode=0){
 return requested&&(networkMode==1||networkMode==2)&&(session==0||session==5||multiplayer(session));
}
constexpr bool arrival(int session,bool requested,unsigned networkMode=0){
 return native(session,requested,networkMode)&&networkMode==2;
}
// The current mode is mandatory at every admission point. Omitting it would
// admit a stale sampled request after switching to either native experiment.
constexpr bool events(int session,bool regular,bool requested,unsigned networkMode){
 return (session==0||session==5||multiplayer(session))&&!native(session,requested,networkMode)&&(regular||requested);
}
}}


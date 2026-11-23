#pragma once
namespace assist { namespace airpathpolicy {
// UI mode 1 retains host arrival continuation; UI mode 2 appends native
// planning events. Only explicit networkMode 1/2 installs UI/arrival hooks.
// Neither native experiment is netplay-certified.
constexpr bool multiplayer(int session){return session==3||session==4;}
inline int mode(bool regular,bool requested,unsigned networkMode){return requested?2+(networkMode<=2?int(networkMode):0):regular?1:0;}
constexpr bool native(int session,bool requested,unsigned networkMode=0){
 return requested&&(networkMode==1||networkMode==2)&&(session==0||session==5||multiplayer(session));
}
constexpr bool arrival(int session,bool requested,unsigned networkMode=0){
 return native(session,requested,networkMode)&&networkMode==2;
}
constexpr bool direct(int session,bool requested,unsigned networkMode){
 return requested&&networkMode==0&&(session==0||session==5||multiplayer(session));
}
// The current mode is mandatory at every admission point. Omitting it would
// admit a stale sampled request after switching to either native experiment.
constexpr bool events(int session,bool regular,bool requested,unsigned networkMode){
 return (session==0||session==5||multiplayer(session))&&!native(session,requested,networkMode)&&!direct(session,requested,networkMode)&&regular;
}
}}


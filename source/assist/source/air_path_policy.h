#pragma once
namespace assist { namespace airpathpolicy {
// Network opt-in: 0=ordinary synchronized moves, 1=native UI gate only,
// 2=native UI and local arrival advancement. Neither experiment is netplay-certified.
inline bool multiplayer(int session){return session==3||session==4;}
inline int mode(bool regular,bool requested,unsigned networkMode){return requested?2+(networkMode<=2?int(networkMode):0):regular?1:0;}
inline bool native(int session,bool requested,unsigned networkMode=0){
 return requested&&(session==0||session==5||(multiplayer(session)&&(networkMode==1||networkMode==2)));
}
inline bool arrival(int session,bool requested,unsigned networkMode=0){
 return native(session,requested,networkMode)&&(!multiplayer(session)||networkMode==2);
}
inline bool events(int session,bool regular,bool requested,unsigned networkMode=0){
 return (session==0||session==5||multiplayer(session))&&!native(session,requested,networkMode)&&(regular||requested);
}
}}


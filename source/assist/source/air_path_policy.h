#pragma once
namespace assist { namespace airpathpolicy {
// Native arrival advances planning tokens by changing the local mission first.
// Only single-player may use that path. Network games append ordinary events.
inline bool native(int session,bool requested){return requested&&(session==0||session==5);}
inline bool events(int session,bool regular,bool requested){
 return (session==0||session==5||session==3||session==4)&&!native(session,requested)&&(regular||requested);
}
}}

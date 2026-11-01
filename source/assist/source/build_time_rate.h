#pragma once
namespace assist { namespace buildtime {
// GameOptions.GameSpeed is 6 minus the visible speed selector. The native
// multiplayer setup has a distinct cap for internal speed 1 (45, not 60).
// Never use FPSCounter.CurrentFrameRate or an adaptive FrameInfo rate here.
inline unsigned allowedRate(int session,int speed){
 if(speed<0||speed>5)return 0;
 if(session==3||session==4){constexpr unsigned rates[]={60,45,30,20,15,12};return rates[speed];}
 if(session==0||session==5){constexpr unsigned rates[]={60,60,30,20,15,12};return rates[speed];}
 return 0;
}
}}

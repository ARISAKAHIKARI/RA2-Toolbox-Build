#pragma once
namespace assist {namespace shroudpolicy {
constexpr int MaxStrategy=6;
constexpr int runtimeMode(int strategy){return strategy==1?2:strategy==2?1:strategy==4?3:strategy==5?4:strategy==6?5:0;}
constexpr bool materialMode(unsigned mode){return mode==4||mode==5;}
}}

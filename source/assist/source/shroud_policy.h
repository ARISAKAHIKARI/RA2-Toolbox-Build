#pragma once
namespace assist {namespace shroudpolicy {
constexpr int MaxStrategy=8;
// The material prototype did not produce a usable display in the user's game.
// Preserve explicit half/clear choices as their original high-risk detours.
// Old material preferences 5/6 become off; never silently upgrade them to hooks.
constexpr int savedStrategy(int strategy){return strategy>=0&&strategy<=MaxStrategy&&strategy!=5&&strategy!=6?strategy:0;}
constexpr int legacyStrategy(int mode){return mode==1?2:mode==2?1:mode==3?4:0;}
constexpr int uiIndex(int strategy){return strategy==7?1:strategy==8?2:savedStrategy(strategy);}
constexpr int strategyFromUI(int index){return index>=0&&index<=4?index:-1;}
constexpr int runtimeMode(int strategy){return strategy==1||strategy==7?2:strategy==2||strategy==8?1:strategy==4?3:0;}
constexpr bool materialMode(unsigned mode){return mode==4||mode==5;}
constexpr bool fullPreview(int strategy){return strategy==1||strategy==2||strategy==3||strategy==7||strategy==8;}
constexpr unsigned overlayOpacity(int strategy){return strategy==1||strategy==7?128u:strategy==2||strategy==3||strategy==4||strategy==8?0u:255u;}
constexpr bool automaticReveal(int strategy){return strategy==3;}
constexpr bool revealEnabled(bool manual,int strategy){return manual||automaticReveal(strategy);}
}}

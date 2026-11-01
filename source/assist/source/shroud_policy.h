#pragma once
namespace assist {namespace shroudpolicy {
constexpr int MaxStrategy=8;
// Normal half/transparent display uses the existing verified SHP alpha data
// replacement. Legacy instruction detours require explicit high-risk 7/8.
// Keep 5/6 as persisted aliases for their existing material modes.
constexpr int runtimeMode(int strategy){return strategy==1?5:strategy==2?4:strategy==4?3:strategy==5?4:strategy==6?5:strategy==7?2:strategy==8?1:0;}
constexpr bool materialMode(unsigned mode){return mode==4||mode==5;}
constexpr bool fullPreview(int strategy){return strategy==1||strategy==2||strategy==3||strategy==5||strategy==6;}
}}

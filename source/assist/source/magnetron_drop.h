#pragma once
#include <cstdint>
// Read-only drop prediction for native YR ground vehicles. Buildings trigger
// Jumpjet's fatal collision branch; Rock land fails the original landing test.
// Terrain objects (trees etc.) can be crushed without killing the dropped unit.
namespace assist { namespace magnetdrop {
struct Ground {bool lifted=false,enemy=false,bridge=false,building=false;int height=0,land=-1;};
inline bool lethal(const Ground&g){return g.lifted&&g.enemy&&g.height>0&&!g.bridge&&(g.building||g.land==7);}
}}

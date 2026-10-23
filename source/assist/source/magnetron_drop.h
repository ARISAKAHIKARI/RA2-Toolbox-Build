#pragma once
#include <cstdint>
// Read-only landing opportunity. Terrain can be destroyed without killing the
// dropped unit, so opportunity() must not be described as guaranteed lethal.
namespace assist { namespace magnetdrop {
struct Ground {bool lifted=false,enemy=false,bridge=false,building=false;int height=0,land=-1;bool terrain=false,groundVehicle=false;};
inline bool opportunity(const Ground&g){return g.lifted&&g.enemy&&g.height>0&&!g.bridge&&(g.building||g.land==7||g.terrain||g.groundVehicle);}
inline bool lethal(const Ground&g){return g.lifted&&g.enemy&&g.height>0&&!g.bridge&&(g.building||g.land==7);}
}}

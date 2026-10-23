#pragma once
#include <cstdint>
// Read-only landing opportunity. Terrain can be destroyed without killing the
// dropped unit, so opportunity() must not be described as guaranteed lethal.
namespace assist { namespace magnetdrop {
struct Ground {
 bool lifted=false,bridge=false,building=false;int height=0,land=-1;bool terrain=false,groundVehicle=false,cliff=false;
 int speed=-1,zone=-1;bool mobilityKnown=false,passable=true;
};
inline bool amphibious(const Ground&g){return g.speed==3||g.speed==6||g.zone==3||g.zone==4||g.zone==5;}
inline bool incompatibleGround(const Ground&g){
 if(g.land<0||g.land>11||g.speed<0||g.speed>7||g.speed==4)return false;
 // Water/land alone never justifies dropping hover or amphibious vehicles.
 // Other obstacles below them still do. FloatBeach may land on beach cells.
 if(g.land==2)return !amphibious(g)&&g.speed!=5&&g.speed!=7;
 if(!amphibious(g)&&(g.speed==5||g.speed==7))return !(g.speed==7&&g.land==6);
 // Native Unit::CanEnterCell reads this same LandType x SpeedType table.
 // Rock is 3, Rough is 7; a neighboring height change is not a cliff landing.
 return g.cliff||(g.mobilityKnown&&!g.passable);
}
inline bool opportunity(const Ground&g){return g.lifted&&g.height>0&&!g.bridge&&(g.building||incompatibleGround(g)||g.terrain||g.groundVehicle);}
inline bool lethal(const Ground&g){return g.lifted&&g.height>0&&!g.bridge&&(g.building||incompatibleGround(g));}
}}

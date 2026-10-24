#pragma once
#include <cstdint>
#include <tuple>
namespace external { namespace camera {
// Building::GetRenderCoords (459EF0) subtracts half a cell from both
// horizontal coordinates. Keep sampling/lighting in the original cell.
inline int anchorY(int kind){return kind==3?-15:0;}
inline auto orderKey(int layer,int kind,int x,int y,int adjustment=0){
 // GetYSort uses leptons, not screen pixels. Anim +104 is added verbatim.
 return std::make_tuple(layer,int64_t(x)+y+(kind==3?-256:0)+adjustment,kind==5?1:0);
}
}}

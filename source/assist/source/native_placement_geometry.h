#pragma once
#include <stdint.h>
namespace assist { namespace nativeplacement {
// Native SetActiveFoundation(4A8BF0): center cell to top-left placement cell.
// Even dimensions deliberately use -(dimension-1)/2, not -dimension/2.
struct Origin {int x=0,y=0;};
inline Origin origin(int x,int y,int width,int height){
 return {x-(width>0&&width<=32?(width-1)/2:0),y-(height>0&&height<=32?(height-1)/2:0)};
}
}}

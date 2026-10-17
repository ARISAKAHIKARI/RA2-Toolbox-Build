#pragma once
namespace nativehot {
struct Point {int x=0,y=0;bool usedGame=false;};
// WWMouse +0x3C/+0x40 are full SHA canvas coordinates, independent of crop.
// A hotspot in transparent pixels is valid. Never add frame offsets or guess
// the visible center: either would move the apparent click location.
template<class IsVisible>
Point resolve(int canvasW,int canvasH,int,int,int,int,
              int sampledX,int sampledY,IsVisible){
 bool valid=sampledX>=0&&sampledY>=0&&sampledX<canvasW&&sampledY<canvasH;
 return valid?Point{sampledX,sampledY,true}:Point{0,0,false};
}
}

#pragma once
#include "cursor_coordinates.h"

namespace cursorpresentation {
using Point=cursorcoordinates::Point;
using Rect=cursorcoordinates::Rect;
enum Mode {Automatic=0,Screen=1,CanvasFullscreen=2};
struct Input {
 Point point{};
 Rect client{},monitor{},clip{};
 bool physical=false,clipValid=false;
 int gameW=0,gameH=0,overrideW=0,overrideH=0;
 bool aspectFit=false;
 int mode=Automatic;
};
struct Result {
 Point point{};
 Rect input{},output{};
 bool mapped=false;
 const wchar_t* route=L"screen-identity";
};
inline bool validCanvas(int width,int height){return width>=320&&height>=200&&width<=16384&&height<=16384;}
inline bool sameRect(const Rect&a,const Rect&b){return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;}
// The clip rectangle can identify a compatibility input canvas, but it cannot
// prove a renderer's semantics. Automatic mapping deliberately requires the
// exact fullscreen monitor, verified game dimensions and a uniform upscale.
// Callers must not substitute fallback or configured dimensions for gameW/H.
inline Result resolve(const Input&in){
 Result result{in.point,in.client,in.client,false,L"screen-identity"};
 auto identity=[&](const wchar_t*route){result.route=route;return result;};
 if(in.mode==Screen)return identity(L"screen-selected");
 if(in.mode!=Automatic&&in.mode!=CanvasFullscreen)return identity(L"unknown-mode");
 if(!in.physical)return identity(L"physical-domain-unavailable");
 if(!in.monitor.valid())return identity(L"monitor-unavailable");
 int width=in.gameW,height=in.gameH;
 if(in.mode==CanvasFullscreen&&(in.overrideW!=0||in.overrideH!=0)){
  if(!validCanvas(in.overrideW,in.overrideH))return identity(L"explicit-size-override-invalid");
  width=in.overrideW;height=in.overrideH;
 }
 if(!validCanvas(width,height))return identity(L"game-size-unavailable");
 Rect source{},target=in.mode==Automatic?in.client:in.monitor;
 cursorcoordinates::Viewport viewport;
 if(in.mode==Automatic){
  if(!in.client.valid()||!sameRect(in.client,in.monitor))return identity(L"automatic-not-fullscreen");
  if(!in.clipValid||!in.clip.valid())return identity(L"automatic-clip-unavailable");
  if(in.clip.width()!=width||in.clip.height()!=height)return identity(L"automatic-clip-size-mismatch");
 }
 if(!viewport.set(target,width,height,in.aspectFit))return identity(L"viewport-unavailable");
 const Rect output=viewport.project({0,0,width,height});
 if(!output.valid())return identity(L"viewport-unavailable");
 if(in.mode==Automatic){
  if(viewport.sx<=1.0||viewport.sy<=1.0||
     std::abs(viewport.sx-viewport.sy)>1e-9*std::max(viewport.sx,viewport.sy))
   return identity(L"automatic-not-uniform-upscale");
  if(in.clip.left!=output.left||in.clip.top!=output.top)return identity(L"automatic-clip-origin-mismatch");
  source=in.clip;
 }else if(in.clipValid&&in.clip.valid()&&in.clip.width()==width&&in.clip.height()==height){
  source=in.clip;
 }else{
  if(!in.client.valid())return identity(L"explicit-source-origin-unavailable");
  const int64_t right=int64_t(in.client.left)+width,bottom=int64_t(in.client.top)+height;
  if(right>std::numeric_limits<int>::max()||bottom>std::numeric_limits<int>::max())return identity(L"explicit-source-overflow");
  source={in.client.left,in.client.top,int(right),int(bottom)};
 }
 if(!source.valid()||!source.contains(in.point))return identity(L"point-outside-input");
 // Project half-open pixel cells with integer arithmetic. A valid source point
// remains within the target even for downscaling; no out-of-range point is
// clamped into a plausible-looking cursor position. Hotspots are applied later.
 const int64_t x=int64_t(in.point.x)-source.left,y=int64_t(in.point.y)-source.top;
 result.point={int(int64_t(output.left)+x*output.width()/source.width()),
               int(int64_t(output.top)+y*output.height()/source.height())};
 result.input=source;result.output=output;
 result.mapped=!sameRect(source,output);
 result.route=in.mode==Automatic?L"automatic-canvas-clip-compat":
  result.mapped?L"explicit-canvas-fullscreen":L"explicit-canvas-identity";
 return result;
}
}

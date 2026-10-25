#pragma once
#include <memory>

// The map worker publishes immutable frames. Repaint only when a window's
// frame, camera rectangle, mode, or presentation state actually changes.
// Weak ownership avoids retaining old map frames across worker updates.
namespace radargate {
template<class Frame>class PaintGate {
 std::weak_ptr<const Frame> last_;
 int cameraX_=0,cameraY_=0,viewWidth_=0,viewHeight_=0,mode_=0;
 bool known_=false;
public:
 void invalidate(){last_.reset();known_=false;}
 bool needs(const std::shared_ptr<const Frame>&frame,int cameraX,int cameraY,int viewWidth,int viewHeight,int mode,bool forced)const{
  return frame&&(forced||!known_||last_.lock()!=frame||cameraX_!=cameraX||cameraY_!=cameraY||
                 viewWidth_!=viewWidth||viewHeight_!=viewHeight||mode_!=mode);
 }
 void committed(const std::shared_ptr<const Frame>&frame,int cameraX,int cameraY,int viewWidth,int viewHeight,int mode){
  last_=frame;cameraX_=cameraX;cameraY_=cameraY;viewWidth_=viewWidth;viewHeight_=viewHeight;mode_=mode;known_=true;
 }
};
}

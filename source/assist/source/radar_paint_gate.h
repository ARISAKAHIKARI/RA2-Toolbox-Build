#pragma once
#include <memory>
#include <vector>
#include <cstdint>

// The map worker publishes immutable frames. Repaint only when a window's
// frame, camera rectangle, mode, or presentation state actually changes.
// Weak ownership avoids retaining old map frames across worker updates.
namespace radargate {
struct Mark {
 int left=0,top=0,right=0,bottom=0;uint32_t color=0;
 bool operator==(const Mark&o)const{return left==o.left&&top==o.top&&right==o.right&&bottom==o.bottom&&color==o.color;}
};
struct Visual {
 std::shared_ptr<const void> image;std::vector<Mark> marks;Mark view;
 int width=0,height=0;bool real=false;
};
template<class Frame>class PaintGate {
 std::weak_ptr<const Frame> last_;
 int cameraX_=0,cameraY_=0,viewWidth_=0,viewHeight_=0,mode_=0;
 bool known_=false;
 std::weak_ptr<const void> image_;std::vector<Mark> marks_;Mark view_;
 int width_=0,height_=0;bool real_=false,visual_=false;
public:
 void invalidate(){last_.reset();image_.reset();marks_.clear();known_=visual_=false;}
 bool needsVisual(const Visual&v,bool forced=false)const{
  return v.image&&(forced||!known_||!visual_||image_.lock()!=v.image||marks_!=v.marks||!(view_==v.view)||width_!=v.width||height_!=v.height||real_!=v.real);
 }
 void committedVisual(const Visual&v){image_=v.image;marks_=v.marks;view_=v.view;width_=v.width;height_=v.height;real_=v.real;known_=visual_=true;last_.reset();}
 bool needs(const std::shared_ptr<const Frame>&frame,int cameraX,int cameraY,int viewWidth,int viewHeight,int mode,bool forced)const{
  return frame&&(forced||!known_||visual_||last_.lock()!=frame||cameraX_!=cameraX||cameraY_!=cameraY||
                 viewWidth_!=viewWidth||viewHeight_!=viewHeight||mode_!=mode);
 }
 void committed(const std::shared_ptr<const Frame>&frame,int cameraX,int cameraY,int viewWidth,int viewHeight,int mode){
  last_=frame;cameraX_=cameraX;cameraY_=cameraY;viewWidth_=viewWidth;viewHeight_=viewHeight;mode_=mode;known_=true;visual_=false;image_.reset();marks_.clear();
 }
};
}

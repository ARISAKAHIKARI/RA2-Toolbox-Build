#include "../source/radar_paint_gate.h"
#include <cassert>
#include <iostream>
struct Frame {int value=0;};
int main(){
 using Gate=radargate::PaintGate<Frame>;
 Gate primary,secondary;
 std::shared_ptr<const Frame> first=std::make_shared<Frame>(Frame{1});
 auto needs=[&](Gate&g,const std::shared_ptr<const Frame>&f,int x=0,int y=0,int w=640,int h=480,int mode=1,bool force=false){return g.needs(f,x,y,w,h,mode,force);};
 assert(needs(primary,first)&&needs(secondary,first));
 primary.committed(first,0,0,640,480,1);
 assert(!needs(primary,first)&&needs(secondary,first)); // independent panels
 assert(needs(primary,first,1)&&needs(primary,first,0,1)); // camera rectangle
 assert(needs(primary,first,0,0,800)&&needs(primary,first,0,0,640,600));
 assert(needs(primary,first,0,0,640,480,0)&&needs(primary,first,0,0,640,480,1,true));
 std::shared_ptr<const Frame> second=std::make_shared<Frame>(Frame{1});
 assert(needs(primary,second)); // same pixels in a newly published frame
 primary.committed(second,0,0,640,480,1);
 assert(!needs(primary,second));primary.invalidate();assert(needs(primary,second)); // hide/hotplug
 primary.committed(second,0,0,640,480,1);
 std::weak_ptr<const Frame> old=second;second.reset();assert(old.expired());
 auto replacement=std::make_shared<const Frame>(Frame{1});assert(needs(primary,replacement)); // stale pointer cannot be reused
 assert(!needs(primary,{})); // no frame is never marked as presented
 radargate::Visual visual;auto image=std::make_shared<const int>(5);visual.image=image;visual.width=320;visual.height=160;visual.marks.push_back({1,2,4,5,0xff1200});visual.view={4,8,20,30,0};
 assert(primary.needsVisual(visual));primary.committedVisual(visual);assert(!primary.needsVisual(visual));
 // New observation wrappers may share identical completed pixels/markers.
 assert(primary.needs(replacement,0,0,640,480,1,false));assert(!primary.needsVisual(visual));
 auto moved=visual;++moved.view.left;assert(primary.needsVisual(moved));moved=visual;++moved.marks[0].color;assert(primary.needsVisual(moved));
 moved=visual;++moved.marks[0].right;assert(primary.needsVisual(moved));moved=visual;moved.real=true;assert(primary.needsVisual(moved));
 moved=visual;++moved.width;assert(primary.needsVisual(moved));assert(primary.needsVisual(visual,true));
 visual.image=std::make_shared<const int>(5);assert(primary.needsVisual(visual));primary.committedVisual(visual);assert(!primary.needsVisual(visual));
 primary.invalidate();assert(primary.needsVisual(visual));assert(!primary.needsVisual({}));
 std::cout<<"PASS per-window radar repaint gate: frame, camera, viewport, mode, recovery, hide, no frame, expiry\n";
}

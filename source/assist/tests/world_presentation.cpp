#include "../source/world_presentation.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../shared/source/viewport_scale.h"
int main(){
 for(float scale:{0.75f,1.f,1.25f,1.28f,1.3f,1.5f,2.f})for(int left=0;left<150;++left){
  // A label at fixed client position must keep its raster phase when a
  // neighbouring moving object shifts the tight bitmap's bounding rectangle.
  viewport::Scale viewport;viewport.x=scale;
  double device=viewport.left(float(left))+(200.25-worldpresentation::rasterOrigin(left,scale))*scale;
  assert(std::abs(device-200.25*scale)<0.0002);
 }
 assert(worldpresentation::cameraFrameCurrent(10000,1,10,20,10,20));
 assert(worldpresentation::cameraFrameCurrent(1119,1000,10,20,11,20));
 assert(!worldpresentation::cameraFrameCurrent(1120,1000,10,20,11,20));
 assert(worldpresentation::cameraFrameCurrent(0x30,0xfffffff0u,0,0,1,1));

 overlayui::Context context{101,7,3,3840,2160};overlayui::Recovery recovery;
 worldpresentation::ContextRevision revision;
 assert(!revision.changed(0,0,0,0,0));
 assert(!revision.changed(0,10,20,10,20)); // Legacy camera behavior is unchanged.
 assert(revision.changed(1,10,20,10,20)); // Unified activation tags a new job.
 assert(!revision.changed(1,10,20,10,20));
 // A fixed toast/summary has unchanged pixels, but its old camera-bound
 // surface becomes invalid. It must submit a newly tagged job after scrolling.
 assert(revision.changed(1,11,21,10,20));
 assert(!revision.changed(1,11,21,10,20));
 assert(revision.changed(1,11,21,11,21)); // A lagging sample has now caught up.
 assert(!revision.changed(1,11,21,11,21));
 assert(revision.changed(2,11,21,11,21)); // New compositor identity.
 unsigned forced=0;
 for(uint32_t now=1000;now<21000;now+=16){
  forced+=worldpresentation::recoveryDue(recovery,now,context,true,false);
  assert(!revision.changed(2,11,21,11,21));
 }
 assert(forced==0); // Hidden legacy HWND is normal in the unified renderer.
 assert(worldpresentation::recoveryDue(recovery,21000,context,false,false));
 recovery.submitted(true);
 assert(!worldpresentation::recoveryDue(recovery,21249,context,false,false));
 assert(worldpresentation::recoveryDue(recovery,21250,context,false,false));
 assert(!worldpresentation::recoveryDue(recovery,22000,context,false,true));
 assert(worldpresentation::recoveryDue(recovery,22250,context,false,true));
 recovery.submitted(false);
 assert(worldpresentation::recoveryDue(recovery,22500,context,false,true));
 assert(!worldpresentation::recoveryDue(recovery,22501,context,true,false));
 assert(worldpresentation::recoveryDue(recovery,22502,context,false,true));
 assert(revision.changed(0,11,21,11,21));
 assert(!revision.changed(0,12,22,12,22));
 std::puts("PASS: static World content resubmits on unified camera/sample changes; 20s stable presentation avoids periodic rasterization; legacy focus/hidden/error recovery remains bounded");
}

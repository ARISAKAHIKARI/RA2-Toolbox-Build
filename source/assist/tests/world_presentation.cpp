#include "../source/world_presentation.h"
#include <cassert>
#include <cstdio>
int main(){
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

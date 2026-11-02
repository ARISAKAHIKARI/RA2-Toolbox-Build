#include "small_map_camera.h"
#ifdef RA2_HOOK_DIAGNOSTICS
#include "../../shared/source/hook_diagnostic_gate.h"
#endif
// Uses the existing game-thread patch manager, never a second hook owner.
static smallcamera::Geometry cameraGeometry(){return {get<int>(0x87F8DC),get<int>(0x87F8E4),get<int>(0x87F8E8),get<int>(0x87F8EC),get<int>(0x87F8F0),get<int>(0x886FA8),get<int>(0x886FAC)};}
static bool smallMapCameraRequested(){
 if(!sh||sh->stop||fault)return false;
#ifdef RA2_HOOK_DIAGNOSTICS
 // The diagnostic tester owns its zero-patch baseline and explicit group.
 return hookdiagnostic::camera(sh->owner);
#else
 return sh->smallMapClamp!=0;
#endif
}
// Original method does not use ECX; both ABIs pop its one Point* argument.
static int smallMapCameraAction(uint32_t point){
 const auto geometry=cameraGeometry();if(!smallMapCameraRequested()||!smallcamera::valid(geometry))return 0;
 auto*p=reinterpret_cast<smallcamera::Point*>(uintptr_t(point));return p?(smallcamera::clamp(geometry,*p)?2:1):1;
}
static bool __stdcall smallMapClamp(smallcamera::Point*p){
 if(!p)return false;
 const int result=GuardedRiskAction(7,uint32_t(uintptr_t(p)),0);
 // Preserve original clamp semantics if the common Capture lease is denied.
 return result?result==2:smallcamera::clampNative(cameraGeometry(),*p);
}
static RiskSite cameraSite{experimentalpatch::CameraClamp,5,{0xA1,0xE4,0xF8,0x87,0},{}};
static bool releaseSmallMapCamera(){
 const bool restored=restoreRiskSite(cameraSite);if(sh)sh->smallMapClampStatus=restored?smallcamera::Off:smallcamera::Conflict;return restored;
}
static void restoreSmallMapCamera(){releaseSmallMapCamera();}
static void smallMapCameraTick(){
 if(!smallMapCameraRequested()){restoreSmallMapCamera();return;}
 const auto geometry=cameraGeometry();
 if(!smallcamera::valid(geometry)){if(releaseSmallMapCamera())sh->smallMapClampStatus=smallcamera::Waiting;return;}
 // One inverted axis is sufficient. Its partner keeps the native scroll
 // interval, including when zoom/sidebar changes only one viewport dimension.
 if(!smallcamera::needed(geometry)){if(releaseSmallMapCamera())sh->smallMapClampStatus=smallcamera::NotNeeded;return;}
 if(!cameraSite.after[0]){cameraSite.after[0]=0xE9;uint32_t offset=uint32_t(uintptr_t(&smallMapClamp))-cameraSite.address-5;memcpy(cameraSite.after+1,&offset,4);}
 // Existing CnCNet/Phobos/platform replacements keep ownership of the site.
 sh->smallMapClampStatus=installRiskSite(cameraSite)?smallcamera::Active:smallcamera::Conflict;
}

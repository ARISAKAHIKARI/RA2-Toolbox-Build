#include "small_map_camera.h"
// Uses the existing game-thread patch manager, never a second hook owner.
static smallcamera::Geometry cameraGeometry(){return {get<int>(0x87F8DC),get<int>(0x87F8E4),get<int>(0x87F8E8),get<int>(0x87F8EC),get<int>(0x87F8F0),get<int>(0x886FA8),get<int>(0x886FAC)};}
// Original method does not use ECX; both ABIs pop its one Point* argument.
static int smallMapCameraAction(uint32_t point){
 auto*p=reinterpret_cast<smallcamera::Point*>(uintptr_t(point));return p?(smallcamera::clamp(cameraGeometry(),*p)?2:1):1;
}
static bool __stdcall smallMapClamp(smallcamera::Point*p){
 if(!p)return false;
 const int result=GuardedRiskAction(7,uint32_t(uintptr_t(p)),0);
 // Preserve original clamp semantics if the common Capture lease is denied.
 return result?result==2:smallcamera::clampNative(cameraGeometry(),*p);
}
static RiskSite cameraSite{experimentalpatch::CameraClamp,5,{0xA1,0xE4,0xF8,0x87,0},{}};
static void restoreSmallMapCamera(){restoreRiskSite(cameraSite);}
static void smallMapCameraTick(){
 if(!smallcamera::needed(cameraGeometry())){restoreSmallMapCamera();return;}
 if(!cameraSite.after[0]){cameraSite.after[0]=0xE9;uint32_t offset=uint32_t(uintptr_t(&smallMapClamp))-cameraSite.address-5;memcpy(cameraSite.after+1,&offset,4);}
 // Existing CnCNet/Phobos/platform replacements keep ownership of the site.
 installRiskSite(cameraSite);
}

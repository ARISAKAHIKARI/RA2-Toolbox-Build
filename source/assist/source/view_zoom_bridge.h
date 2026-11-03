#pragma once
#include "view_zoom_legacy_bridge.h"
#include "view_zoom_native_bridge.h"
// The original whole-screen renderer path remains available while an older
// or partially integrated Host advertises no complete A/R capabilities.
static bool zoomClientPoint(HWND window,POINT&point){
 if(nativeViewNeedsTracking())return GetCursorPos(&point)&&ScreenToClient(window,&point);
 return legacyZoomClientPoint(window,point);
}
static void restoreViewZoom(){
 nativeViewPublishWire();
 if(!nativeViewRetired()&&!nativeViewRestore())return;
 restoreLegacyViewZoom();
 nativeViewPublishWire();
}
static void viewZoomTick(){
 nativeViewPublishWire();
 if(sh&&(sh->viewportHostCapabilities&nativeviewportprojection::HostHandshake)&&
    (sh->viewportHostCapabilities&nativeviewportprojection::RequiredCapabilities)!=nativeviewportprojection::RequiredCapabilities){
  if(!nativeViewRetired()&&!nativeViewRestore())return;
  if(zoomBase.width){restoreLegacyViewZoom();if(zoomBase.width)return;}
  nativeViewPublishWire();nativeViewPublish(featureavailability::zoomPercent(int(sh->viewZoomPercent))==100?viewzoom::Off:viewzoom::Waiting);return;
 }
 if(nativeViewDemand()||!nativeViewRetired()){
  if(zoomBase.width){restoreLegacyViewZoom();if(zoomBase.width){nativeViewPublish(viewzoom::Waiting);return;}}
  nativeViewTick();if(nativeViewDemand()||!nativeViewRetired())return;
 }
 legacyViewZoomTick();
}

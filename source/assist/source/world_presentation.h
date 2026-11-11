#pragma once
#include <cmath>
#include "../../shared/source/overlay_recovery.h"
namespace worldpresentation {
// Keep the raster phase in client pixels even when tight bitmap bounds move.
inline float rasterOrigin(int logical,float scale){return float(std::floor(double(logical)*scale)/scale);}
// Moving cameras may use a just-completed frame while the next one renders.
// Frozen content at the current camera is valid without periodic re-rasterizing.
inline bool cameraFrameCurrent(uint32_t now,uint32_t tick,int x,int y,int wantedX,int wantedY){
 return (x==wantedX&&y==wantedY)||uint32_t(now-tick)<120;
}
// World content needs a newly tagged job when the camera changes. Observe
// the source sample too: a job rejected while that sample lagged the committed
// camera must be retried once the source catches up.
class ContextRevision {
 uint64_t token_=0;int cameraX_=0,cameraY_=0,sampleX_=0,sampleY_=0;
public:
 bool changed(uint64_t token,int cameraX,int cameraY,int sampleX,int sampleY){
  const bool changed=token!=token_||(token&&(cameraX!=cameraX_||cameraY!=cameraY_||sampleX!=sampleX_||sampleY!=sampleY_));
  token_=token;cameraX_=cameraX;cameraY_=cameraY;sampleX_=sampleX;sampleY_=sampleY;return changed;
 }
};
// Comparison caches describe the last submitted content, not whether those
// pixels still exist. Invalidation must force an identical next frame to draw.
class Submission {
 bool active_=false;
public:
 bool required(bool changed)const{return !active_||changed;}
 void submitted(){active_=true;}
 bool invalidate(){bool prior=active_;active_=false;return prior;}
};
// A unified surface is kept alive by Host::publish/touch. Its deliberately
// hidden legacy HWND must not trigger a new GDI+ rasterization every 250 ms.
inline bool recoveryDue(overlayui::Recovery&recovery,uint32_t now,const overlayui::Context&context,
                        bool unified,bool legacyVisible){
 if(unified){recovery.suspend();return false;}
 return recovery.due(now,true,context,legacyVisible);
}
}

#pragma once
#include "../../shared/source/overlay_recovery.h"
namespace worldpresentation {
// Even fixed screen content belongs to a camera-bound World surface. A scroll
// needs a newly tagged job when its pixels do not otherwise change. Observe
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
// A unified surface is kept alive by Host::publish/touch. Its deliberately
// hidden legacy HWND must not trigger a new GDI+ rasterization every 250 ms.
inline bool recoveryDue(overlayui::Recovery&recovery,uint32_t now,const overlayui::Context&context,
                        bool unified,bool legacyVisible){
 if(unified){recovery.suspend();return false;}
 return recovery.due(now,true,context,legacyVisible);
}
}

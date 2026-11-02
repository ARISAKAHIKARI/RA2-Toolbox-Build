#pragma once
#include "middle_input_owner.h"
// Both Runtime MSG filtering and the host's GetAsyncKeyState sampler observe
// the same physical click. Swallowing the MSG alone cannot prevent a second
// host mailbox request, so reserve these gestures in both host input routes.
static bool runtimeMiddleReserved(const uint8_t*keys){return middleinput::reserved(extra.moFeatures,extra.formationEnabled,keys);}
static bool runtimeMiddleOwnsNow(){
 if(shared&&shared->moOrdersPreview.active&&shared->moOrdersPreview.match==commandMatch())return true;
 uint8_t keys[256]{};for(unsigned key:{unsigned(VK_MBUTTON),unsigned(VK_SHIFT),unsigned(VK_CONTROL),unsigned(VK_MENU),unsigned('Z')})keys[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;
 return keys[VK_MBUTTON]&&runtimeMiddleReserved(keys);
}

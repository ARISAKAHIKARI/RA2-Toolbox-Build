#pragma once
#include "mo_orders.h"
static moorders::GesturePolicy moRuntimeGestures;
static moorders::Options moRuntimeOptions;
static bool moReadGestures(bool captured=false){
 static const void*mapping=nullptr;static uint32_t epoch=0,house=0,at=0;static bool valid=false;
 if(!sh)return false;
 if(mapping!=sh||epoch!=match||house!=lastHouse){valid=false;mapping=sh;epoch=match;house=lastHouse;}
 auto collision=[&](){return captured&&valid&&DWORD(sampleClock(sh)-at)<=1000;};
 long seq=sh->moGesturePolicySeq;if(seq&1)return collision();
 MemoryBarrier();auto next=sh->moGestures;auto options=sh->moOptions;MemoryBarrier();
 if(seq!=sh->moGesturePolicySeq)return collision();
 if(!moorders::validOptions(options)){valid=false;return false;}
 moRuntimeGestures=next;moRuntimeOptions=options;at=sampleClock(sh);valid=true;return true;
}
static void moGestureKeyboard(uint8_t(&keys)[256]){for(unsigned key=0;key<256;++key)keys[key]=(GetKeyState(int(key))&0x8000)!=0;runtimeKeys.decorate(keys);}
static uint32_t moGestureEnabled(){return sh?moorders::gestureEnabled(sh->moFeatures,sh->formationPolicy.enabled!=0):0;}
static moorders::GestureResolution moGestureResolve(uint32_t button,const uint8_t*keys){return moorders::resolveGesture(moRuntimeGestures,moGestureEnabled(),button,keys);}

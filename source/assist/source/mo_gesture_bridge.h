#pragma once
#include "mo_orders.h"
static moorders::GesturePolicy moRuntimeGestures;
static bool moReadGestures(){
 if(!sh)return false;long seq=sh->moGesturePolicySeq;if(seq&1)return false;MemoryBarrier();auto next=sh->moGestures;MemoryBarrier();
 if(seq!=sh->moGesturePolicySeq)return false;moRuntimeGestures=next;return true;
}
static void moGestureKeyboard(uint8_t(&keys)[256]){for(unsigned key=0;key<256;++key)keys[key]=(GetKeyState(int(key))&0x8000)!=0;runtimeKeys.decorate(keys);}
static uint32_t moGestureEnabled(){return sh?moorders::gestureEnabled(sh->moFeatures,sh->formationPolicy.enabled!=0):0;}
static moorders::GestureResolution moGestureResolve(uint32_t button,const uint8_t*keys){return moorders::resolveGesture(moRuntimeGestures,moGestureEnabled(),button,keys);}

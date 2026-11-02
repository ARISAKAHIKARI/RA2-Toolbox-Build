#pragma once
#include "middle_input_owner.h"
// Both Runtime MSG filtering and the host's GetAsyncKeyState sampler observe
// the same physical click. Swallowing the MSG alone cannot prevent a second
// host mailbox request, so reserve these gestures in both host input routes.
static const moorders::GesturePolicy& runtimeGesturePolicy(){
 static moorders::GesturePolicy cached;if(!shared)return extra.moGestures;
 long seq=shared->moGesturePolicySeq;if(!(seq&1)){MemoryBarrier();auto next=shared->moGestures;MemoryBarrier();if(seq==shared->moGesturePolicySeq)cached=next;}return cached;
}
static uint32_t runtimeGestureEnabled(){return shared?moorders::gestureEnabled(shared->moFeatures,shared->formationPolicy.enabled!=0):moorders::gestureEnabled(extra.moFeatures,extra.formationEnabled);}
static bool runtimeGestureReserved(uint32_t button,const uint8_t*keys){return middleinput::ownsPointer(int(button),runtimeGesturePolicy(),runtimeGestureEnabled(),keys);}
static const uint8_t* runtimeGestureKeyboard(){
 static uint8_t keys[256]{};static keygesture::Tracker tracker;static uint32_t sampled=0,epoch=0;const uint32_t now=GetTickCount(),current=commandMatch();
 if(epoch!=current){tracker.clear();epoch=current;sampled=0;}
 if(!sampled||sampled!=now){sampled=now;for(unsigned key=0;key<256;++key)keys[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;tracker.sample(keys,now);tracker.decorate(keys);
  uint8_t recorded[256]{};gameinput::keySnapshot(hook,recorded);for(unsigned key=0;key<256;++key)if(keys[key]&&(recorded[key]&2))keys[key]|=2;
 }return keys;
}
static bool runtimeMiddleOwnsNow(){
 if(shared&&shared->moOrdersPreview.active&&shared->moOrdersPreview.match==commandMatch())return true;
 if(shared&&shared->formationPreview.active&&shared->formationPreview.match==commandMatch())return true;
 const auto*keys=runtimeGestureKeyboard();for(uint32_t button:{1u,2u,3u,4u,6u})if(keys[moorders::gestureVK(button)]&&runtimeGestureReserved(button,keys))return true;return false;
}
static std::wstring runtimeGestureDescription(uint32_t mode){
 const int index=moorders::gestureIndex(mode);if(index<0)return L"未绑定";const auto&policy=runtimeGesturePolicy();const uint32_t button=policy.buttons[index],key=policy.keys[index];
 if(!moorders::gestureButton(button)||!moorders::gestureKey(key))return L"未绑定";
 const wchar_t*mouse=button==1?L"中键":button==2?L"侧键1":button==3?L"侧键2":button==4?L"右键":L"左键";
 return (key?keygesture::describe(key,modernui::hotkeyName(WORD(key)),true)+L" + ":L"")+mouse;
}

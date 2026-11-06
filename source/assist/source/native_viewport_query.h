#include "../../shared/source/debug_entry_checks.h"
#pragma once
#include "../../shared/source/native_viewport_projection.h"
// Programmatic queries retain their existing Assist/Capture lease. A separate
// MSVC SEH finally owns this tiny depth counter, so an engine AV cannot leave
// a permanent already-mapped classification behind.
static unsigned nativeViewQueryDepth=0;
static bool nativeViewQueryBlocked=false;
static bool(*nativeViewQueryOwner)()=nullptr;
extern "C" int __cdecl GuardedNativeViewQuery(void*,void*,void*,void*,void*,void*);
static bool nativeViewQueryCompatible(){
 static constexpr uint8_t original[]={0x8B,0x44,0x24,0x10,0x83,0xEC,0x18,0x8B};uint8_t bytes[sizeof original]{};
 const bool readable=read(0x692300,bytes,sizeof bytes);
 const bool ours=readable&&nativeViewQueryOwner&&nativeViewQueryOwner();
 return debugentries::allow(debugentries::Kind::Call,0x692300,readable,!memcmp(bytes,original,sizeof bytes)||ours,ours?debugentries::Signature::Platform:debugentries::Signature::Match);
}
static bool nativeViewTransform(nativeviewportprojection::Transform&out){
 if(nativeViewQueryBlocked||(sh&&(sh->stop||sh->viewZoomBusy))||!nativeviewportprojection::sample(sh,out))return false;
 NativeViewRect view{};if(!read(0x886FA0,&view,sizeof view))return false;
 if(out.active){if(view.x!=out.logical.x||view.y!=out.logical.y||view.w!=out.logical.w||view.h!=out.logical.h)return false;}
 else out.physical=out.logical={view.x,view.y,view.w,view.h};
 return true;
}
static bool nativeViewTransformUnchanged(const nativeviewportprojection::Transform&before){
 nativeviewportprojection::Transform after;return nativeViewTransform(after)&&after==before;
}
static bool nativeViewMapPoint(NativeViewPoint&point){
 nativeviewportprojection::Transform t;if(!nativeViewTransform(t))return false;int x=point.x,y=point.y;
 if(!nativeviewportprojection::screenToView(t,x,y)||!nativeViewTransformUnchanged(t))return false;point={x,y};return true;
}
static bool nativeViewScreenPoint(NativeViewPoint&point){
 nativeviewportprojection::Transform t;if(!nativeViewTransform(t))return false;int x=point.x,y=point.y;
 if(!nativeviewportprojection::viewToScreen(t,x,y)||!nativeViewTransformUnchanged(t))return false;point={x,y};return true;
}
extern "C" unsigned __cdecl BeginNativeViewQuery(){
 if(!gameentry::active(gameentry::Client::Assist)||nativeViewQueryBlocked||(sh&&(sh->stop||sh->viewZoomBusy))||
    nativeViewQueryDepth>=16||!nativeViewQueryCompatible())return 0;
 return ++nativeViewQueryDepth;
}
extern "C" void __cdecl EndNativeViewQuery(unsigned token){
 if(token&&nativeViewQueryDepth==token)--nativeViewQueryDepth;
 else nativeViewQueryDepth=0;
}
static bool nativeViewQuery(void*point,void*cell,void*coord,void*object,void*flagA,void*flagB){
 return GuardedNativeViewQuery(point,cell,coord,object,flagA,flagB)!=0;
}

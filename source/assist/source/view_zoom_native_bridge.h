#pragma once
#include "view_zoom_native_state.h"
static int __cdecl nativeViewGuarded(unsigned action,uint32_t argument){RuntimeFPScope fp;int result=0;runtimeunwind::run([&]{result=GuardedNativeViewAction(action,argument);},[&]{fp.restore();});return result;}
static __declspec(naked) void nativeViewMapEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 2
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,dword ptr ds:[88731Ch]
 push 4F44D0h
 ret
}}
static __declspec(naked) void nativeViewChromePreEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 3
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov edx,[esi]
 push eax
 mov ecx,esi
 push 4F4502h
 ret
}}
static __declspec(naked) void nativeViewChromePostEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 4
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov ecx,dword ptr ds:[88731Ch]
 push 4F450Bh
 ret
}}
static __declspec(naked) void nativeViewAfterEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 5
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr ds:[0B0B519h]
 push 4F4520h
 ret
}}
static __declspec(naked) void nativeViewShroudEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 12
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,48h
 push esi
 push edi
 push 6D7845h
 ret
}}
static __declspec(naked) void nativeViewCursorEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+28]
 push 1
 call nativeViewGuarded
 add esp,8
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov dword ptr ds:[887314h],eax
 push 4F4497h
 ret
}}
static __declspec(naked) void nativeViewEdgeEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+40]
 push 7
 call nativeViewGuarded
 add esp,8
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,18h
 push ebx
 push ebp
 push esi
 push edi
 push 692B67h
 ret
}}
static __declspec(naked) void nativeViewClickEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+40]
 push 9
 call nativeViewGuarded
 add esp,8
 cmp eax,2
 je denied
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov eax,[esp+10h]
 sub esp,18h
 push 692307h
 ret
denied:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 xor eax,eax
 ret 18h
}}
static __declspec(naked) void nativeViewTipEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push 0
 push 10
 call nativeViewGuarded
 add esp,8
 test eax,eax
 jnz skipped
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov edx,[ecx]
 push 1
 call dword ptr [edx+0Ch]
 push 6A7090h
 ret
skipped:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 6A7090h
 ret
}}
static __declspec(naked) void nativeViewGetMouse(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+40]
 call GuardedNativeViewMousePoint
 add esp,4
 mov eax,[ebx+40]
 mov [ebx+28],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret 4
}}
static __declspec(naked) void nativeViewWndReturnEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 call GuardedNativeViewWndLeave
 mov [ebx-4],eax
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push dword ptr [esp-40]
 ret
}}
static __declspec(naked) void nativeViewWndEntry(){__asm {
 pushfd
 pushad
 mov ebx,esp
 sub esp,528
 and esp,-16
 fxsave [esp]
 cld
 fninit
 fldcw [esp]
 mov dword ptr [esp+512],1F80h
 ldmxcsr [esp+512]
 push dword ptr [ebx+36]
 push dword ptr [ebx+52]
 call GuardedNativeViewWndEnter
 add esp,8
 cmp eax,-1
 je denied
 test eax,eax
 jz original
 mov [ebx+52],eax
 mov eax,dword ptr [nativeViewWndReturnTarget]
 mov [ebx+36],eax
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 mov al,byte ptr ds:[0A8E378h]
 sub esp,1Ch
 push 6930A8h
 ret
denied:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 xor eax,eax
 ret 10h
}}
static RiskSite nativeViewSites[sizeof(nativeviewportpolicy::Sites)/sizeof(nativeviewportpolicy::Sites[0])]{};
static bool nativeViewClickQueryOwned(){
 if(!nativeViewHooks||!nativeViewActive||nativeViewHold||nativeViewMutation||nativeViewError)return false;
 const auto&s=nativeViewSites[7];return codeHookRegistry.active(s.address)&&!codeHookRegistry.conflict(s.address)&&riskSiteCurrent(s)&&
  patchcontext::compatible(s.address,[](uint32_t at,void*out,size_t n){return read(at,out,n);});
}
static void nativeViewMakeSites(){
 static bool made=false;if(made)return;made=true;
 nativeViewWndReturnTarget=uint32_t(uintptr_t(&nativeViewWndReturnEntry));
 void*entries[]={(void*)&sidebarViewRenderEnter,(void*)&nativeViewCursorEntry,(void*)&nativeViewMapEntry,(void*)&nativeViewChromePreEntry,
  (void*)&nativeViewChromePostEntry,(void*)&nativeViewAfterEntry,(void*)&nativeViewEdgeEntry,(void*)&nativeViewClickEntry,
  (void*)&nativeViewWndEntry,(void*)&nativeViewShroudEntry,(void*)&nativeViewTipEntry,(void*)&nativeViewGetMouse};
 for(unsigned i=0;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i){auto&s=nativeViewSites[i];const auto&p=nativeviewportpolicy::Sites[i];s.address=p.at;s.size=p.n;memcpy(s.before,p.before,p.n);
  if(p.kind==nativeviewportpolicy::Kind::Pointer){const uint32_t target=uint32_t(uintptr_t(entries[i]));memcpy(s.after,&target,4);}
  else if(p.kind==nativeviewportpolicy::Kind::DataOperand){memcpy(s.after,s.before,s.size);const uint32_t target=uint32_t(uintptr_t(&nativeViewA))+(p.at==0x478BD7||p.at==0x478E50?8u:0u);memcpy(s.after+p.imm,&target,4);}
  else{memset(s.after,0x90,s.size);s.after[0]=0xE9;const uint32_t rel=uint32_t(uintptr_t(entries[i]))-s.address-5;memcpy(s.after+1,&rel,4);}
 }
 nativeViewQueryOwner=nativeViewClickQueryOwned;
}
static bool nativeViewAllSites(){
 if(!nativeViewHooks||!nativeSidebarInstalled||!sidebarViewTrackingIntact())return false;
 bool ok=true;for(unsigned i=0;i<11;++i)ok=sidebarViewOwnsSite(i)&&ok;
 for(unsigned i=1;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i){const auto&s=nativeViewSites[i];
  if(!codeHookRegistry.active(s.address)||codeHookRegistry.conflict(s.address)||!riskSiteCurrent(s)){codeHookRegistry.markConflict(s.address);ok=false;}
 }return ok;
}
static bool nativeViewRemoveSites(){
 nativeViewMakeSites();bool ok=true;
 for(unsigned i=unsigned(sizeof(nativeViewSites)/sizeof(nativeViewSites[0]));i>1;--i)ok=restoreRiskSite(nativeViewSites[i-1])&&ok;
 if(ok)nativeViewHooks=false;return ok;
}
static bool nativeViewInstall(){
 nativeViewMakeSites();if(nativeViewHooks)return nativeViewAllSites();
 if(!nativeSidebarInstalled||!sidebarViewTrackingIntact()||!nativeViewIdle())return false;
 // Check the complete group before installing the first additional site.
 for(unsigned i=1;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i){const auto&s=nativeViewSites[i];uint8_t bytes[16]{};
  if(codeHookRegistry.active(s.address)||codeHookRegistry.conflict(s.address)||!read(s.address,bytes,s.size)||!hookregistry::admissibleOriginal(s.address,bytes,s.size)||
     !patchcontext::compatible(s.address,[](uint32_t at,void*out,size_t n){return read(at,out,n);}))return false;
 }
 for(unsigned i=1;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i)if(!installRiskSite(nativeViewSites[i])){nativeViewRemoveSites();return false;}
 nativeViewHooks=true;return nativeViewAllSites();
}
static bool nativeViewRetired(){
 if(nativeViewActive||nativeViewHooks||nativeViewSwapped||nativeViewMutation||nativeViewHold||nativeViewWorldWait||nativeViewInRender||nativeViewWndDepth||nativeViewQueryDepth||
    nativeViewRaster[0]||nativeViewRaster[1]||nativeViewBuffersOwned[0]||nativeViewBuffersOwned[1])return false;
 nativeViewMakeSites();for(unsigned i=1;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i)if(codeHookRegistry.active(nativeViewSites[i].address))return false;return true;
}
// Reused singleton/pair addresses after loading are not ownership continuity.
// Recompute only current local presentation with current centres, then retire
// the sidebar journal without deleting or replaying its pre-transition objects.
static bool nativeViewFreshCurrentGeometry(){
 NativeSidebarRect window{},area{};const auto current=get<NativeViewRect>(0x886FA0);
 if(!nativeViewIdle()||nativeViewLoading()||!get<bool>(0xA8E9A0)||!get<uint32_t>(0xA8B230)||get<uint32_t>(0x887324)<0x10000||
  !nativeViewRectEqual(current,nativeViewR)||!nativeViewRectEqual(get<NativeViewRect>(0xB0CE28),nativeViewR)||!nativeViewWindowIntact()||
  !nativeSidebarWindow(window)||!nativeSidebarArea(area)||!nativeViewAllSites()||!sidebarViewTrackingIntact())return false;
 for(auto*p:nativeViewWindow)if(nativeViewSurfaceValue(p,0x7C)<area.x+area.w||nativeViewSurfaceValue(p,0x80)<area.y+area.h||nativeViewSurfaceValue(p,0x70)!=2)return false;
 nativeViewEpoch=get<uint32_t>(0xA8B230);nativeViewTactical=get<uint32_t>(0x887324);nativeViewFrame=get<int>(0xA8ED84);
 nativeViewWindowBounds=window;nativeViewScreen={get<int>(0x8A00A4),get<int>(0x8A00A8)};nativeViewHwnd=get<uint32_t>(0xB73550);
 const NativeViewRect fresh{area.x,area.y,area.w,area.h};
 if(!nativeViewGeometry(fresh,fresh,false)){nativeViewFail(viewzoom::RestoreFailed);return false;}
 NativeViewRetirementProof p;p.valid=p.layout=p.local=true;p.scenario=nativeViewEpoch;p.tactical=nativeViewTactical;p.frame=nativeViewFrame;p.hwnd=nativeViewHwnd;p.area=fresh;p.window=window;
 const uint32_t slots[]={0x88731C,0x8872FC,0x887314,0x887644,0x87E8A4};for(unsigned i=0;i<5;++i)p.pointers[i]=get<void*>(slots[i]);
 nativeViewRetirement=p;nativeViewWorldWait=false;return true;
}
static bool nativeViewRestoreImpl(){
 if(nativeViewRetired()){nativeViewPublish(viewzoom::Off);return true;}
 if(!nativeViewIdle()){nativeViewRestorePending=true;nativeViewPublish(viewzoom::Waiting);return false;}
 if(nativeViewHold||nativeViewGeometryUncertain){nativeViewPublish(nativeViewError?nativeViewError:viewzoom::Fault);return false;}
 if(!sidebarViewTrackingIntact()||(nativeViewHooks&&!nativeViewAllSites())){nativeViewFail(-8);return false;}
 if(nativeViewActive){
  const auto current=get<NativeViewRect>(0x886FA0);
  if(nativeViewWorld()&&nativeViewRectEqual(current,nativeViewR)&&nativeViewWindowIntact()){
   nativeViewWorldWait=false;
   if(!nativeViewGeometry(nativeViewA,nativeViewA)){nativeViewFail(viewzoom::RestoreFailed);return false;}
  }else if(!nativeViewWorld()){
   nativeViewWorldWait=true;nativeViewRestorePending=true;nativeViewQueryBlocked=true;
   if(!nativeViewFreshCurrentGeometry()&&(nativeViewHold||!nativeViewNewGenerationReady())){nativeViewPublish(nativeViewHold?nativeViewError:viewzoom::Waiting);return false;}
   // A new native generation already owns its geometry and all global pairs.
   // Do not replay the old view over it; retain only our private allocations
   // until the complete tracking/phase checks above establish safe idleness.
   nativeViewWorldWait=false;nativeViewMutationBegin();if(!nativeViewRetirement.local)nativeViewA=nativeViewR=current;
  }else{nativeViewFail(-8);return false;}
  nativeViewActive=false;nativeZoomApplied=100;nativeViewMutationEnd();nativeViewPublish(viewzoom::Off,true);
 }
 if(!nativeViewNoOwnedReferences()||!nativeViewRemoveSites()||!nativeViewReleaseRaster()){nativeViewFail(-8);return false;}
 nativeViewRestorePending=nativeViewWorldWait=false;nativeViewThread=0;nativeViewDirty=0;nativeViewPreviousA={};nativeViewLastRequest=100;
 if(!nativeViewRetirement.valid)nativeViewSidebarRetired();
 nativeViewError=0;nativeViewQueryBlocked=false;nativeViewPublish(viewzoom::Off);return nativeViewRetired();
}
static bool nativeViewRestore(){return nativeViewGuarded(21,0)!=0;}
static void nativeViewTickImpl(){
 if(!sh)return;
 const int requested=featureavailability::zoomPercent(int(sh->viewZoomPercent));
 if(!nativeViewDemand()){
  const bool restored=nativeViewRestoreImpl();
  if(restored&&requested==100){nativeViewError=0;nativeViewHold=false;nativeViewQueryBlocked=false;}return;
 }
 if(nativeViewRetirement.valid){restoreSidebarView();nativeViewPublish(viewzoom::Waiting);return;}
 if(nativeViewHold){nativeViewPublish(nativeViewError?nativeViewError:viewzoom::Fault);return;}
 if(nativeViewError&&requested==nativeViewLastRequest){nativeViewPublish(nativeViewError);return;}
 if(!viewzoom::valid(requested)){nativeViewError=viewzoom::Dimensions;nativeViewLastRequest=requested;nativeViewPublish(nativeViewError);return;}
 if(!nativeViewIdle()||!sh->foreground||snap.chat||get<int>(0xA8EDA0)||nativeViewLoading()||
    (GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000)){nativeViewPublish(viewzoom::Waiting);return;}
 NativeSidebarRect area{};if(!nativeSidebarInstalled||nativeSidebarMode!=int(sh->sidebarMode<=2?sh->sidebarMode:0)||!nativeSidebarArea(area)||!sidebarViewRenderSerial){nativeViewPublish(viewzoom::Waiting);return;}
 const NativeViewRect physical{area.x,area.y,area.w,area.h};
 viewzoom::Size map{};
 if(requested==0){const auto geometry=cameraGeometry();if(smallcamera::valid(geometry))map={60*geometry.width,30*geometry.height+135};}
 const auto target=viewzoom::nativeTarget({area.w,area.h},requested,map);
 const NativeViewRect logical{0,0,target.width,target.height};
 if(logical.w<64||logical.h<64||logical.w>8192||logical.h>8192||uint64_t(logical.w)*logical.h>40000000){
  // A rejected new request is not corruption of the currently applied view.
  // Keep its transform, mouse mapping and paired Render fully operational.
  if(!nativeViewActive){nativeViewError=viewzoom::Dimensions;nativeViewLastRequest=requested;}
  nativeViewPublish(viewzoom::Dimensions);return;
 }
 if(nativeViewActive&&requested==nativeZoomApplied&&nativeViewRectEqual(physical,nativeViewA)&&nativeViewRectEqual(logical,nativeViewR)){
  if(!nativeViewWorld()){
   nativeViewWorldWait=true;nativeViewRestorePending=true;nativeViewQueryBlocked=true;
   nativeViewRestoreImpl();nativeViewPublish(viewzoom::Waiting);return;
  }
  if(!nativeViewAllSites()||!nativeViewWindowIntact()||!nativeViewRectEqual(get<NativeViewRect>(0x886FA0),nativeViewR)){nativeViewFail(-8);return;}
  nativeViewWorldWait=false;
  nativeViewPublish(viewzoom::Applied);return;
 }
 if(!nativeViewRetired()&&!nativeViewRestoreImpl())return;
 if(nativeViewRetirement.valid){restoreSidebarView();nativeViewPublish(viewzoom::Waiting);return;}
 if(!nativeViewNativeCompatible()){nativeViewError=viewzoom::Native;nativeViewLastRequest=requested;nativeViewPublish(nativeViewError);return;}
 restoreHighScroll();if(scrollRendering||scrollRefreshActive){nativeViewPublish(viewzoom::Waiting);return;}
 nativeViewError=0;nativeViewQueryBlocked=false;nativeViewLastRequest=requested;nativeViewA=physical;nativeViewR=logical;
 nativeViewEpoch=get<uint32_t>(0xA8B230);nativeViewTactical=get<uint32_t>(0x887324);nativeViewFrame=get<int>(0xA8ED84);nativeViewThread=GetCurrentThreadId();
 nativeSidebarWindow(nativeViewWindowBounds);nativeViewScreen={get<int>(0x8A00A4),get<int>(0x8A00A8)};nativeViewHwnd=get<uint32_t>(0xB73550);
 if(!nativeViewInstall()){
  if(!nativeViewRemoveSites())nativeViewFail(-8);else{nativeViewError=viewzoom::Native;nativeViewPublish(nativeViewError);}return;
 }
 if(!nativeViewPrepareRaster(logical.w,logical.h)){
  if(!nativeViewRemoveSites()||!nativeViewReleaseRaster())nativeViewFail(viewzoom::ResizeFailed);
  else{nativeViewError=viewzoom::ResizeFailed;nativeViewPublish(nativeViewError);}return;
 }
 if(!nativeViewGeometry(physical,logical)){nativeViewFail(viewzoom::ResizeFailed);return;}
 nativeViewActive=true;nativeViewWorldWait=false;nativeZoomApplied=requested;nativeViewDirty=4;nativeViewDirtySerial=UINT32_MAX;nativeViewMutationEnd();
 resetPointerInput();nativeViewPublish(viewzoom::Applied,true);sidebarViewRefresh(true);smallMapCameraTick();
}
static void nativeViewTick(){nativeViewGuarded(20,0);}

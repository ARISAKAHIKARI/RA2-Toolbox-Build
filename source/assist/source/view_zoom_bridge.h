#pragma once
#include "view_zoom.h"
#include "native_viewport_surface.h"
#include "native_sidebar_bridge.h"
#include "../../shared/source/native_viewport_policy.h"
#include "../../shared/source/native_viewport_projection.h"
#include "../../shared/source/runtime_fp_scope.h"
// A is the physical map area; R is the native map raster.  Each zoom uses
// its own R-sized incremental surfaces, so resampled pixels never feed back
// into native scrolling. Chrome and cursor always see the window surfaces.
static NativeViewRect nativeViewA{},nativeViewR{},nativeViewPreviousA{};
static void*nativeViewRaster[2]{},*nativeViewRasterCurrent[2]{},*nativeViewWindow[2]{};
static void*nativeViewBuffersOwned[2]{},*nativeViewBuffersSaved[2]{},*nativeViewOldCurrent=nullptr;
static int nativeViewBufferW=0,nativeViewBufferH=0;
static bool nativeViewRestorePending=false;
static unsigned nativeViewRenderDepth=0;
static unsigned nativeViewQueryDepth=0;
struct NativeViewQueryScope {NativeViewQueryScope(){++nativeViewQueryDepth;}~NativeViewQueryScope(){--nativeViewQueryDepth;}};
static void restoreViewZoom();
static int nativeViewRasterW=0,nativeViewRasterH=0,zoomApplied=100;
static bool nativeViewHooks=false,nativeViewActive=false,nativeViewInRender=false,nativeViewSwapped=false,nativeViewSuspended=false;
static int nativeViewDirty=0,nativeViewDirtyFrame=-1,nativeViewLastRequest=-1;
static uint32_t nativeViewEpoch=0,nativeViewTactical=0,nativeViewWndParam=0;
static bool zoomClientPoint(HWND window,POINT&p){return GetCursorPos(&p)&&ScreenToClient(window,&p);}
static bool nativeViewZoomed(){return nativeViewActive&&(nativeViewA.w!=nativeViewR.w||nativeViewA.h!=nativeViewR.h||nativeViewA.x!=nativeViewR.x);}
static bool nativeViewLoading(){return get<int>(0xA9FAB0)!=0||get<uint8_t>(0xA9FA94)!=0;}
static void nativeViewPublish(int status){
 if(!sh)return;sh->viewZoomStatus=status;sh->viewZoomApplied=zoomApplied;sh->viewZoomWidth=nativeViewR.w;sh->viewZoomHeight=nativeViewR.h;
 const int a[4]={nativeViewA.x,nativeViewA.y,nativeViewA.w,nativeViewA.h},r[4]={nativeViewR.x,nativeViewR.y,nativeViewR.w,nativeViewR.h};
 bool changed=sh->viewportActive!=LONG(nativeViewActive);for(int i=0;i<4;++i)changed|=sh->viewportPhysical[i]!=a[i]||sh->viewportLogical[i]!=r[i];
 if(!changed)return;InterlockedIncrement(&sh->viewportSerial);sh->viewportActive=0;MemoryBarrier();
 for(int i=0;i<4;++i){sh->viewportPhysical[i]=a[i];sh->viewportLogical[i]=r[i];}
 MemoryBarrier();sh->viewportActive=nativeViewActive?1:0;InterlockedIncrement(&sh->viewportSerial);
}
static bool nativeViewMapPoint(NativeViewPoint&p){
 if(!nativeViewActive)return true;const auto a=nativeViewA,r=nativeViewR;if(a.w<=0||a.h<=0||r.w<=0||r.h<=0)return false;
 const bool inside=p.x>=a.x&&p.y>=a.y&&p.x<a.x+a.w&&p.y<a.y+a.h;
 p.x=int(int64_t(p.x-a.x)*r.w/a.w)+r.x;p.y=int(int64_t(p.y-a.y)*r.h/a.h)+r.y;
 if(p.x<0)p.x=0;if(p.y<0)p.y=0;if(p.x>32767)p.x=32767;if(p.y>32767)p.y=32767;return inside;
}
static void nativeViewScreenPoint(NativeViewPoint&p){if(!nativeViewActive||nativeViewR.w<1||nativeViewR.h<1)return;p.x=nativeViewA.x+int(int64_t(p.x-nativeViewR.x)*nativeViewA.w/nativeViewR.w);p.y=nativeViewA.y+int(int64_t(p.y-nativeViewR.y)*nativeViewA.h/nativeViewR.h);}
static void nativeViewDirtyMap(){uint32_t t=get<uint32_t>(0x887324);if(t>=0x10000&&nativeViewR.w>=16&&nativeViewR.h>=16)((void(__fastcall*)(void*,NativeViewRect,int))0x6D2790)((void*)(uintptr_t)t,{0,0,nativeViewR.w,nativeViewR.h},1);}
static void nativeViewSwapOut(){
 if(!nativeViewSwapped)return;if(!nativeViewSuspended){nativeViewRasterCurrent[0]=get<void*>(0x88731C);nativeViewRasterCurrent[1]=get<void*>(0x8872FC);}
 put<void*>(0x88731C,nativeViewWindow[0]);put<void*>(0x8872FC,nativeViewWindow[1]);put<void*>(0x887314,nativeViewOldCurrent);
 put<void*>(0x887644,nativeViewBuffersSaved[0]);put<void*>(0x87E8A4,nativeViewBuffersSaved[1]);nativeViewSwapped=false;nativeViewSuspended=false;
}
static void nativeViewSuspend(){
 if(!nativeViewSwapped||nativeViewSuspended)return;nativeViewRasterCurrent[0]=get<void*>(0x88731C);nativeViewRasterCurrent[1]=get<void*>(0x8872FC);
 put<void*>(0x88731C,nativeViewWindow[0]);put<void*>(0x8872FC,nativeViewWindow[1]);put<void*>(0x887314,nativeViewOldCurrent);
 put<void*>(0x887644,nativeViewBuffersSaved[0]);put<void*>(0x87E8A4,nativeViewBuffersSaved[1]);nativeViewSuspended=true;
}
static void nativeViewResume(){if(nativeViewSwapped&&nativeViewSuspended){put<void*>(0x88731C,nativeViewRasterCurrent[0]);put<void*>(0x8872FC,nativeViewRasterCurrent[1]);put<void*>(0x887314,nativeViewRasterCurrent[0]);put<void*>(0x887644,nativeViewBuffersOwned[0]);put<void*>(0x87E8A4,nativeViewBuffersOwned[1]);nativeViewSuspended=false;}}
static void nativeViewDropRaster(){
 if(nativeViewInRender)return;nativeViewSwapOut();for(auto&p:nativeViewRaster){if(p)nativeViewFreeSurface(p);p=nullptr;}nativeViewRasterCurrent[0]=nativeViewRasterCurrent[1]=nullptr;nativeViewRasterW=nativeViewRasterH=0;
}
static bool nativeViewPrepareRaster(int w,int h){
 if(nativeViewRaster[0]&&nativeViewRaster[1]&&nativeViewRasterW==w&&nativeViewRasterH==h)return true;
 void*c=nativeViewNewSurface(w,h);if(!c)return false;void*m=nativeViewNewSurface(w,h);if(!m){nativeViewFreeSurface(c);return false;}
 if(!nativeViewClearSurface(c)||!nativeViewClearSurface(m)){nativeViewFreeSurface(c);nativeViewFreeSurface(m);return false;}
 nativeViewDropRaster();nativeViewRaster[0]=nativeViewRasterCurrent[0]=c;nativeViewRaster[1]=nativeViewRasterCurrent[1]=m;nativeViewRasterW=w;nativeViewRasterH=h;nativeViewDirty=4;return true;
}
static bool nativeViewBuffers(int w,int h){
 if(nativeViewBuffersOwned[0]&&nativeViewBuffersOwned[1]&&nativeViewBufferW==w&&nativeViewBufferH==h)return true;
 NativeViewRect r{0,0,w,h};void*nz=nativeViewNewBuffer(false,r),*na=nativeViewNewBuffer(true,r);
 if(!nz||!na){nativeViewFreeBuffer(false,nz);nativeViewFreeBuffer(true,na);return false;}
 nativeViewFreeBuffer(false,nativeViewBuffersOwned[0]);nativeViewFreeBuffer(true,nativeViewBuffersOwned[1]);nativeViewBuffersOwned[0]=nz;nativeViewBuffersOwned[1]=na;nativeViewBufferW=w;nativeViewBufferH=h;return true;
}
static bool nativeViewGeometry(NativeViewRect a,NativeViewRect r,bool refresh){
 uint32_t t=get<uint32_t>(0x887324);if(t<0x10000)return false;const auto old=get<NativeViewRect>(0x886FA0);
 auto nativeBuffersFit=[&](){for(uint32_t slot:{0x887644u,0x87E8A4u}){uint32_t p=get<uint32_t>(slot);int w=0,h=0;if(p<0x10000||!read(p+0x28,&w,4)||!read(p+0x2C,&h,4)||w<a.w||h<a.h)return false;}return true;};
 if(!nativeViewRectEqual(old,r)||!nativeViewRectEqual(nativeViewPreviousA,a)||refresh||!nativeBuffersFit()){
  int c[4]={get<int>(t+0xD64),get<int>(t+0xD68),get<int>(t+0xD74),get<int>(t+0xD78)};
  put<NativeViewRect>(0x886FA0,a);call<void>(0x4A8960,0x87F7E8,&a);
  put<int>(t+0xD64,c[0]);put<int>(t+0xD68,c[1]);put<int>(t+0xD74,c[2]);put<int>(t+0xD78,c[3]);nativeViewPreviousA=a;nativeViewDirty=4;put<uint32_t>(0xB0CD44,0);
 }
 if(!nativeBuffersFit())return false;
 if(!nativeViewBuffers(a.w>r.w?a.w:r.w,a.h>r.h?a.h:r.h))return false;
 put<NativeViewRect>(0x886FA0,r);if(!nativeViewRectEqual(get<NativeViewRect>(0xB0CE28),r))call<void>(0x6D5F60,t,(void*)0x886FA0);nativeSidebarColumn();return true;
}
static void nativeViewVoid(void*surface){
 if(!surface)return;auto g=cameraGeometry();if(!smallcamera::valid(g))return;uint32_t t=get<uint32_t>(0x887324);if(t<0x10000)return;
 const int ox=get<int>(t+0xB0),oy=get<int>(t+0xB4),left=int(30LL*(2*g.x-g.mapWidth))-ox+nativeViewR.x,top=int(15LL*(g.mapWidth+2*g.y-5))-oy+nativeViewR.y,right=left+60*g.width,bottom=top+30*g.height+135;
 const int sw=nativeViewSurfaceValue(surface,0x7C),hh=nativeViewSurfaceValue(surface,0x80),pitch=nativeViewSurfaceValue(surface,0x74);if(sw<16||hh<16||pitch<sw*2||nativeViewSurfaceValue(surface,0x70)!=2)return;
 auto*b=nativeViewSurfaceLock(surface);if(!b)return;const auto r=nativeViewR;int x0=r.x<0?0:r.x,y0=r.y<0?0:r.y,x1=r.x+r.w,y1=r.y+r.h;if(x1>sw)x1=sw;if(y1>hh)y1=hh;
 for(int y=y0;y<y1;++y){auto*row=(unsigned short*)(b+size_t(y)*pitch);if(y<top||y>=bottom){for(int x=x0;x<x1;++x)row[x]=0;}else{int l=left<x1?left:x1,rr=right>x0?right:x0;for(int x=x0;x<l;++x)row[x]=0;for(int x=rr;x<x1;++x)row[x]=0;}}nativeViewSurfaceUnlock(surface);
}
static bool nativeViewPresentRaster(void*source,void*target){
 if(!source||!target||source==target)return false;const int sw=nativeViewSurfaceValue(source,0x7C),hh=nativeViewSurfaceValue(source,0x80),sp=nativeViewSurfaceValue(source,0x74),tw=nativeViewSurfaceValue(target,0x7C),th=nativeViewSurfaceValue(target,0x80),tp=nativeViewSurfaceValue(target,0x74);
 if(sw<nativeViewR.w||hh<nativeViewR.h||sp<sw*2||tp<tw*2||nativeViewSurfaceValue(source,0x70)!=2||nativeViewSurfaceValue(target,0x70)!=2)return false;
 const auto a=nativeViewA,r=nativeViewR;if(a.x<0||a.y<0||a.x+a.w>tw||a.y+a.h>th)return false;
 auto*s=nativeViewSurfaceLock(source);if(!s)return false;auto*d=nativeViewSurfaceLock(target);if(!d){nativeViewSurfaceUnlock(source);return false;}
 static int columns[8192];for(int x=0;x<a.w;++x)columns[x]=r.x+int(int64_t(x)*r.w/a.w);
 for(int y=0;y<a.h;++y){const auto*sr=(const unsigned short*)(s+size_t(r.y+int(int64_t(y)*r.h/a.h))*sp);auto*dr=(unsigned short*)(d+size_t(a.y+y)*tp)+a.x;for(int x=0;x<a.w;++x)dr[x]=sr[columns[x]];}
 nativeViewSurfaceUnlock(target);nativeViewSurfaceUnlock(source);return true;
}
static int nativeViewportAction(unsigned action,uint32_t argument){
 if(action>=32)return nativeSidebarAction(action-32,argument);if(!nativeViewHooks)return 0;
 // Teardown can detach the IPC mapping while this Render is still on the
 // stack. Surface restoration at its remaining phase callbacks must run.
 if(!sh&&action>6)return 0;
 if(!sh&&action==0){nativeViewRestorePending=true;return 0;}
 // A recursive display-only Render is redundant: the outer Render will
 // present. Do not run a second native pass on a temporarily suspended pair.
 if(action==0){if(nativeViewRenderDepth)return 2;nativeViewRenderDepth=1;}
 if(nativeViewRenderDepth>1&&action<=6){if(action==5)--nativeViewRenderDepth;return action==1?int(argument):1;}
 if(action==0){nativeViewInRender=true;if(!nativeViewActive||nativeViewLoading())return 1;
  if(nativeViewDirty>0){nativeViewDirtyMap();int f=get<int>(0xA8ED84);if(f!=nativeViewDirtyFrame){nativeViewDirtyFrame=f;--nativeViewDirty;}}
  if(!nativeViewZoomed())return 1;nativeViewWindow[0]=get<void*>(0x88731C);nativeViewWindow[1]=get<void*>(0x8872FC);nativeViewOldCurrent=get<void*>(0x887314);nativeViewBuffersSaved[0]=get<void*>(0x887644);nativeViewBuffersSaved[1]=get<void*>(0x87E8A4);put<void*>(0x88731C,nativeViewRasterCurrent[0]);put<void*>(0x8872FC,nativeViewRasterCurrent[1]);put<void*>(0x887644,nativeViewBuffersOwned[0]);put<void*>(0x87E8A4,nativeViewBuffersOwned[1]);nativeViewSwapped=true;nativeViewSuspended=false;return 1;}
 if(action==1){nativeViewSuspend();return nativeViewSwapped?int(uintptr_t(nativeViewWindow[0])):int(argument);}
 if(action==2||action==4){nativeViewResume();return 1;}if(action==3){nativeViewSuspend();return 1;}
 if(action==5){void*source=get<void*>(0x88731C);const bool raster=nativeViewSwapped;nativeViewSwapOut();nativeViewRenderDepth=0;nativeViewInRender=false;if(nativeViewActive&&!nativeViewLoading()){nativeViewVoid(source);if(raster)nativeViewPresentRaster(source,get<void*>(0x88731C));nativeSidebarPresent(get<void*>(0x88731C));}if(nativeViewRestorePending)restoreViewZoom();return 1;}
 if(action==6){if(nativeViewSwapped&&!nativeViewSuspended)put<void*>(0x887314,get<void*>(0x88731C));return 1;}
 if(action==7){if(nativeViewZoomed()&&argument){uint32_t m=get<uint32_t>(0x887640);if(m>=0x10000){auto*p=(NativeViewPoint*)(uintptr_t)argument;p->x=get<int>(m+0x1C);p->y=get<int>(m+0x20);}}return 1;}
 if(action==8){if(!argument||!nativeViewZoomed())return int(argument);uint32_t lp=*(uint32_t*)(uintptr_t)argument;NativeViewPoint p{short(lp&65535),short(lp>>16)};nativeViewMapPoint(p);nativeViewWndParam=(uint32_t(p.y&65535)<<16)|uint32_t(p.x&65535);return int(uintptr_t(&nativeViewWndParam));}
 if(action==9){if(!nativeViewActive||!argument||nativeViewQueryDepth)return 1;auto p=*(NativeViewPoint*)(uintptr_t)argument;uint32_t m=get<uint32_t>(0x887640);if(m<0x10000)return 1;int x=get<int>(m+0x1C),y=get<int>(m+0x20);if(nativeSidebarTopHit(x,y))return 1;return x>=nativeViewA.x&&y>=nativeViewA.y&&x<nativeViewA.x+nativeViewA.w&&y<nativeViewA.y+nativeViewA.h&&p.x>=nativeViewR.x&&p.y>=nativeViewR.y&&p.x<nativeViewR.x+nativeViewR.w&&p.y<nativeViewR.y+nativeViewR.h?1:2;}
 if(action==10){if(!nativeViewZoomed())return 0;uint32_t self=get<uint32_t>(0x887368);if(self>=0x10000)call<void>(0x724AD0,self);return 1;}
 if(action==11){if(!argument)return 0;auto*p=(NativeViewPoint*)(uintptr_t)argument;uint32_t m=get<uint32_t>(0x887640);if(m<0x10000)return 0;p->x=get<int>(m+0x1C);p->y=get<int>(m+0x20);nativeViewMapPoint(*p);return 1;}return 0;
}
static int __cdecl nativeViewGuarded(unsigned action,uint32_t argument){RuntimeFPScope fp;return GuardedRiskAction(15,action,argument);}
static NativeViewPoint*__stdcall nativeViewGetMouse(NativeViewPoint*out){if(!out)return out;if(!nativeViewGuarded(11,uint32_t(uintptr_t(out)))){uint32_t m=get<uint32_t>(0x887640);if(m>=0x10000){out->x=get<int>(m+0x1C);out->y=get<int>(m+0x20);}}return out;}

// Fixed continuations replay every displaced instruction; no executable
// allocations or private trampoline/patch owner.
static __declspec(naked) void nativeViewRenderEntry(){__asm {
 pushfd
 pushad
 push 0
 push 0
 call nativeViewGuarded
 add esp,8
 cmp eax,2
 je nested
 popad
 popfd
 sub esp,8
 mov eax,dword ptr ds:[88731Ch]
 push 4F4488h
 ret
nested:
 popad
 popfd
 ret
}}
static __declspec(naked) void nativeViewMapEntry(){__asm {
 pushfd
 pushad
 push 0
 push 2
 call nativeViewGuarded
 add esp,8
 popad
 popfd
 mov eax,dword ptr ds:[88731Ch]
 push 4F44D0h
 ret
}}
static __declspec(naked) void nativeViewChromePreEntry(){__asm {
 pushfd
 pushad
 push 0
 push 3
 call nativeViewGuarded
 add esp,8
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
 push 0
 push 4
 call nativeViewGuarded
 add esp,8
 popad
 popfd
 mov ecx,dword ptr ds:[88731Ch]
 push 4F450Bh
 ret
}}
static __declspec(naked) void nativeViewAfterEntry(){__asm {
 pushfd
 pushad
 push 0
 push 5
 call nativeViewGuarded
 add esp,8
 popad
 popfd
 mov al,byte ptr ds:[0B0B519h]
 push 4F4520h
 ret
}}
static __declspec(naked) void nativeViewShroudEntry(){__asm {
 pushfd
 pushad
 push 0
 push 6
 call nativeViewGuarded
 add esp,8
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
 push eax
 push 1
 call nativeViewGuarded
 add esp,8
 test eax,eax
 jz oldeax
 mov [esp+28],eax
oldeax:
 popad
 popfd
 mov dword ptr ds:[887314h],eax
 push 4F4497h
 ret
}}
static __declspec(naked) void nativeViewEdgeEntry(){__asm {
 pushfd
 pushad
 mov eax,[esp+40]
 push eax
 push 7
 call nativeViewGuarded
 add esp,8
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
static __declspec(naked) void nativeViewWndEntry(){__asm {
 pushfd
 pushad
 mov eax,[esp+52]
 push eax
 push 8
 call nativeViewGuarded
 add esp,8
 test eax,eax
 jz original
 mov [esp+52],eax
original:
 popad
 popfd
 mov al,byte ptr ds:[0A8E378h]
 sub esp,1Ch
 push 6930A8h
 ret
}}
static __declspec(naked) void nativeViewClickEntry(){__asm {
 pushfd
 pushad
 mov eax,[esp+40]
 push eax
 push 9
 call nativeViewGuarded
 add esp,8
 cmp eax,2
 je denied
 popad
 popfd
 mov eax,[esp+10h]
 sub esp,18h
 push 692307h
 ret
denied:
 popad
 popfd
 xor eax,eax
 ret 18h
}}
static __declspec(naked) void nativeViewTipEntry(){__asm {
 pushfd
 pushad
 push 0
 push 10
 call nativeViewGuarded
 add esp,8
 test eax,eax
 jnz skipped
 popad
 popfd
 mov edx,[ecx]
 push 1
 call dword ptr [edx+0Ch]
 push 6A7090h
 ret
skipped:
 popad
 popfd
 push 6A7090h
 ret
}}
static RiskSite nativeViewSites[sizeof(nativeviewportpolicy::Sites)/sizeof(nativeviewportpolicy::Sites[0])]{};
static void nativeViewMakeSites(){
 static bool made=false;if(made)return;made=true;
 void*entries[]={(void*)&nativeViewRenderEntry,(void*)&nativeViewCursorEntry,(void*)&nativeViewMapEntry,(void*)&nativeViewChromePreEntry,(void*)&nativeViewChromePostEntry,(void*)&nativeViewAfterEntry,(void*)&nativeViewEdgeEntry,(void*)&nativeViewClickEntry,(void*)&nativeViewWndEntry,(void*)&nativeViewShroudEntry,(void*)&nativeViewTipEntry,(void*)&nativeViewGetMouse};
 for(unsigned i=0;i<sizeof(nativeViewSites)/sizeof(nativeViewSites[0]);++i){auto&s=nativeViewSites[i];const auto&p=nativeviewportpolicy::Sites[i];s.address=p.at;s.size=p.n;memcpy(s.before,p.before,s.size);
  if(p.kind==nativeviewportpolicy::Kind::Pointer){uint32_t target=uint32_t(uintptr_t(entries[i]));memcpy(s.after,&target,4);}
  else if(p.kind==nativeviewportpolicy::Kind::DataOperand){memcpy(s.after,s.before,s.size);uint32_t target=uint32_t(uintptr_t(&nativeViewA))+(p.at==0x478BD7||p.at==0x478E50?8:0);memcpy(s.after+p.imm,&target,4);}
  else{memset(s.after,0x90,s.size);s.after[0]=0xE9;uint32_t rel=uint32_t(uintptr_t(entries[i]))-s.address-5;memcpy(s.after+1,&rel,4);}
 }
}
static bool nativeViewInstall(){
 nativeViewMakeSites();if(nativeViewHooks)return true;
 for(auto&s:nativeViewSites)if(!installRiskSite(s)){for(auto&t:nativeViewSites)restoreRiskSite(t);return false;}
 nativeViewHooks=true;return true;
}
static void restoreViewZoom(){
 if(nativeViewInRender){nativeViewRestorePending=true;return;}nativeViewRestorePending=false;nativeViewSwapOut();
 if(nativeViewActive&&get<uint32_t>(0x887324)==nativeViewTactical&&!nativeViewLoading()){
  NativeSidebarRect area{};nativeSidebarMode=0;if(nativeSidebarArea(area)){NativeViewRect a{area.x,area.y,area.w,area.h};nativeViewGeometry(a,a,true);nativeViewA=nativeViewR=a;nativeViewDirtyMap();}
 }
 nativeViewActive=false;nativeViewDropRaster();nativeSidebarRestore();
 nativeViewFreeBuffer(false,nativeViewBuffersOwned[0]);nativeViewFreeBuffer(true,nativeViewBuffersOwned[1]);nativeViewBuffersOwned[0]=nativeViewBuffersOwned[1]=nullptr;nativeViewBufferW=nativeViewBufferH=0;
 for(auto&s:nativeViewSites)restoreRiskSite(s);
 nativeViewHooks=false;nativeViewEpoch=nativeViewTactical=0;nativeViewLastRequest=-1;zoomApplied=100;nativeViewPublish(viewzoom::Off);
}
static bool nativeViewRetireReady(){restoreViewZoom();return !nativeViewInRender&&!nativeViewRestorePending&&!nativeViewSwapped;}
static void viewZoomTick(){
 if(!sh||nativeViewInRender)return;
 const int requested=featureavailability::zoomPercent(int(sh->viewZoomPercent));
 if(nativeViewLoading()||!get<uint32_t>(0x887640)||!get<uint32_t>(0x88731C)||!get<uint32_t>(0x887300)){nativeViewPublish(viewzoom::Waiting);return;}
 if(!viewzoom::valid(requested)){nativeViewPublish(viewzoom::Dimensions);return;}
 if(!sh->foreground||snap.chat||(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000)){nativeViewPublish(viewzoom::Waiting);return;}
 const uint32_t epoch=get<uint32_t>(0xA8B230),tactical=get<uint32_t>(0x887324);
 if(nativeViewEpoch&&(epoch!=nativeViewEpoch||tactical!=nativeViewTactical)){nativeViewDropRaster();nativeViewPreviousA={};nativeViewActive=false;nativeViewLastRequest=-1;}
 nativeViewEpoch=epoch;nativeViewTactical=tactical;
 nativeSidebarTick(sh->sidebarMode);
 NativeSidebarRect physical{};if(!nativeSidebarArea(physical)){nativeViewPublish(viewzoom::Native);return;}
 NativeViewRect a{physical.x,physical.y,physical.w,physical.h},r=a;
 if(requested!=100){r.x=r.y=0;if(requested==0){auto g=cameraGeometry();if(!smallcamera::valid(g)){nativeViewPublish(viewzoom::Dimensions);return;}double scale=double(60*g.width)/a.w;const double ys=double(30*g.height+135)/a.h;if(ys>scale)scale=ys;r.w=int(a.w*scale+.5);r.h=int(a.h*scale+.5);}else{r.w=int((int64_t(a.w)*100+requested/2)/requested);r.h=int((int64_t(a.h)*100+requested/2)/requested);}
  if(r.w>8192||r.h>8192){double scale=r.w>r.h?8192./r.w:8192./r.h;r.w=int(r.w*scale);r.h=int(r.h*scale);}
 }
 if(r.w<64||r.h<64||uint64_t(r.w)*r.h>40000000){nativeViewPublish(viewzoom::Dimensions);return;}
 if(!nativeViewInstall()){nativeViewPublish(viewzoom::Native);return;}
 const bool changed=!nativeViewRectEqual(nativeViewA,a)||!nativeViewRectEqual(nativeViewR,r)||nativeSidebarSurfaceChanged();
 if(requested!=100&&!nativeViewPrepareRaster(r.w,r.h)){nativeViewPublish(viewzoom::ResizeFailed);return;}
 InterlockedExchange(&sh->viewZoomBusy,1);nativeViewA=a;nativeViewR=r;
 const bool applied=nativeViewGeometry(a,r,changed);
 nativeSidebarAfterLayout();InterlockedExchange(&sh->viewZoomBusy,0);
 if(!applied){nativeViewActive=false;nativeViewGeometry(a,a,true);nativeViewR=a;nativeViewPublish(viewzoom::ResizeFailed);return;}
 nativeViewActive=true;zoomApplied=int((int64_t(a.w)*100+r.w/2)/r.w);nativeViewLastRequest=requested;
 if(requested==100)nativeViewDropRaster();
 if(changed){resetPointerInput();sceneFrame=-2147483647;sceneTick=0;nativeViewDirtyMap();smallMapCameraTick();}
 nativeViewPublish(requested==100?viewzoom::Off:viewzoom::Applied);
}


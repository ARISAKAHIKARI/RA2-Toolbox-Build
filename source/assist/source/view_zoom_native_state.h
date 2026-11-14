#include "../../shared/source/debug_entry_checks.h"
#pragma once
#include "native_viewport_query.h"
#include "../../shared/source/native_viewport_policy.h"
#include "../../shared/source/runtime_fp_scope.h"
// All fields below belong to the existing game thread. This owner borrows the
// sidebar's complete Render lifetime; map completion never makes it idle.
static NativeViewRect nativeViewA{},nativeViewR{},nativeViewPreviousA{};
static void*nativeViewRaster[2]{},*nativeViewRasterCurrent[2]{},*nativeViewBuffersOwned[2]{};
static void*nativeViewWindow[2]{},*nativeViewWindowBuffers[2]{},*nativeViewOldCurrent=nullptr;
static void*nativeViewLocked[2]{};
static bool nativeMapZoomLease=false,nativeMapPipLease=false;
static bool nativeViewHooks=false,nativeViewActive=false,nativeViewInRender=false,nativeViewSwapped=false,nativeViewSuspended=false;
static bool nativeViewMutation=false,nativeViewGeometryUncertain=false,nativeViewHold=false,nativeViewRestorePending=false,nativeViewHadMap=false;
static bool nativeViewWorldWait=false,nativeViewPublished=false,nativeViewPublishedActive=false;
static NativeViewRect nativeViewPublishedA{},nativeViewPublishedR{};
static NativeSidebarRect nativeViewWindowBounds{};static viewzoom::Size nativeViewScreen{};static uint32_t nativeViewHwnd=0;
struct NativeViewRetirementProof {bool valid=false,layout=false,local=false;uint32_t scenario=0,tactical=0,hwnd=0;int frame=0;NativeViewRect area{};NativeSidebarRect window{};void*pointers[5]{};};
static NativeViewRetirementProof nativeViewRetirement;
static int nativeViewError=0,nativeViewRasterW=0,nativeViewRasterH=0,nativeZoomApplied=100;
static uint32_t nativeViewEpoch=0,nativeViewTactical=0,nativeViewGeneration=0,nativeViewWndParam=0;
static int nativeViewFrame=0;
static DWORD nativeViewThread=0;
static unsigned nativeViewDirty=0,nativeViewDirtySerial=UINT32_MAX;
static int nativeViewLastRequest=100;
struct NativeViewWndFrame {uint32_t returnAddress=0,packed=0;};
static NativeViewWndFrame nativeViewWndFrames[16];static unsigned nativeViewWndDepth=0;
static uint32_t nativeViewWndReturnTarget=0;
extern "C" int __cdecl GuardedNativeViewAction(unsigned,unsigned);
extern "C" unsigned __cdecl GuardedNativeViewWndEnter(unsigned,unsigned);
extern "C" unsigned __cdecl GuardedNativeViewWndLeave();
extern "C" int __cdecl GuardedNativeViewMousePoint(void*);
static bool nativeViewRestore();static bool nativeViewAllSites();static void nativeViewMakeSites();
static bool nativeViewInstall(),nativeViewRemoveSites(),nativeViewRetired();
static void nativeViewTickImpl();static bool nativeViewRestoreImpl();
static bool nativeViewLoading(){return get<int>(0xA9FAB0)!=0||get<uint8_t>(0xA9FA94)!=0;}
static bool nativeViewDemand(){return sh&&!sh->stop&&!fault&&
 (sh->viewportHostCapabilities&nativeviewportprojection::RequiredCapabilities)==nativeviewportprojection::RequiredCapabilities&&
 viewzoom::valid(featureavailability::zoomPercent(int(sh->viewZoomPercent)))&&featureavailability::zoomPercent(int(sh->viewZoomPercent))!=100;}
static bool nativeViewNeedsTracking(){return nativeViewDemand()||nativeViewHooks||nativeViewActive||nativeViewSwapped||nativeViewMutation||nativeViewHold;}
static bool nativeViewRenderNeeded(){return nativeViewActive||nativeViewSwapped||nativeViewMutation||nativeViewHold||nativeViewWorldWait;}
static bool nativeViewIdle(){return !sidebarViewRendering()&&!nativeViewInRender&&!nativeViewWndDepth&&!nativeViewQueryDepth&&!nativePipBusy();}
static bool nativeViewPair(void*a,void*b,void*const*pair){return a&&b&&a!=b&&
 (a==pair[0]||a==pair[1])&&(b==pair[0]||b==pair[1]);}
static bool nativeViewRasterPointer(void*p){return p&&(p==nativeViewRaster[0]||p==nativeViewRaster[1]);}
static bool nativeViewBufferPointer(void*p){return p&&(p==nativeViewBuffersOwned[0]||p==nativeViewBuffersOwned[1]);}
static void nativeViewPublishWire(bool changed=false){
 if(!sh)return;
 if(nativeViewMutation||nativeViewGeometryUncertain||nativeViewHold||nativeViewWorldWait){
  if(nativeViewWorldWait&&!nativeViewHold&&!nativeViewGeometryUncertain)InterlockedExchange(&sh->viewZoomBusy,1);
  if(!(uint32_t(sh->viewportSerial)&1))InterlockedIncrement(&sh->viewportSerial);return;
 }
 const NativeViewRect physical=nativeViewActive?nativeViewA:get<NativeViewRect>(0x886FA0),logical=nativeViewActive?nativeViewR:physical;
 const bool geometryChanged=!nativeViewPublished||changed||nativeViewPublishedActive!=nativeViewActive||!nativeViewRectEqual(physical,nativeViewPublishedA)||!nativeViewRectEqual(logical,nativeViewPublishedR);
 bool wireChanged=sh->viewportActive!=(nativeViewActive?1u:0u)||sh->viewportGeneration!=nativeViewGeneration||!uint32_t(sh->viewportSerial);
 const int a[4]={physical.x,physical.y,physical.w,physical.h},r[4]={logical.x,logical.y,logical.w,logical.h};
 for(unsigned i=0;i<4;++i)wireChanged|=sh->viewportPhysical[i]!=a[i]||sh->viewportLogical[i]!=r[i];
 if(!geometryChanged&&!wireChanged&&!(uint32_t(sh->viewportSerial)&1))return;
 InterlockedExchange(&sh->viewZoomBusy,1);if(!(uint32_t(sh->viewportSerial)&1))InterlockedIncrement(&sh->viewportSerial);
 if(geometryChanged||!nativeViewGeneration){if(++nativeViewGeneration==0)++nativeViewGeneration;}
 sh->viewportActive=nativeViewActive?1:0;sh->viewportGeneration=nativeViewGeneration;
 for(unsigned i=0;i<4;++i){sh->viewportPhysical[i]=a[i];sh->viewportLogical[i]=r[i];}
 MemoryBarrier();if(uint32_t(sh->viewportSerial)&1)InterlockedIncrement(&sh->viewportSerial);
 InterlockedExchange(&sh->viewZoomBusy,0);nativeViewQueryBlocked=nativeViewActive&&nativeViewError!=0;
 nativeViewPublished=true;nativeViewPublishedActive=nativeViewActive;nativeViewPublishedA=physical;nativeViewPublishedR=logical;
}
static void nativeViewPublish(int status,bool changed=false){
 if(!sh)return;sh->viewZoomStatus=status;sh->viewZoomApplied=nativeZoomApplied;
 sh->viewZoomWidth=nativeViewActive?nativeViewR.w:get<int>(0x8A00A4);sh->viewZoomHeight=nativeViewActive?nativeViewR.h:get<int>(0x8A00A8);
 nativeViewPublishWire(changed);
}
static void nativeViewMutationBegin(){
 nativeViewMutation=true;nativeViewQueryBlocked=true;
 if(sh){InterlockedExchange(&sh->viewZoomBusy,1);if(!(uint32_t(sh->viewportSerial)&1))InterlockedIncrement(&sh->viewportSerial);}
}
static void nativeViewMutationEnd(){nativeViewMutation=false;nativeViewQueryBlocked=true;}
static bool nativeViewSignature(uint32_t at,unsigned n,uint32_t hash){
 uint8_t bytes[32]{};const bool readable=n<=sizeof bytes&&read(at,bytes,n);uint32_t h=2166136261u;if(readable)for(unsigned i=0;i<n;++i)h=(h^bytes[i])*16777619u;return debugentries::allow(debugentries::Kind::Call,at,readable,h==hash);
}
static bool nativeViewWorld(){
 NativeSidebarRect window{};
 const int frame=get<int>(0xA8ED84);
 const bool same=frame>=nativeViewFrame&&nativeViewTactical&&get<uint32_t>(0x887324)==nativeViewTactical&&get<uint32_t>(0xA8B230)==nativeViewEpoch&&!nativeViewLoading()&&
  nativeSidebarWindow(window)&&!memcmp(&window,&nativeViewWindowBounds,sizeof window)&&
  get<int>(0x8A00A4)==nativeViewScreen.width&&get<int>(0x8A00A8)==nativeViewScreen.height&&get<uint32_t>(0xB73550)==nativeViewHwnd;
 if(same)nativeViewFrame=frame;return same;
}
static bool nativeViewWindowIntact(){return nativeViewPair(get<void*>(0x88731C),get<void*>(0x8872FC),nativeViewWindow)&&
 get<void*>(0x887644)==nativeViewWindowBuffers[0]&&get<void*>(0x87E8A4)==nativeViewWindowBuffers[1]&&!nativeViewRasterPointer(get<void*>(0x887314));}
static bool nativeViewNoOwnedReferences(){
 for(uint32_t at:{0x88731Cu,0x8872FCu,0x887314u})if(nativeViewRasterPointer(get<void*>(at)))return false;
 return !nativeViewBufferPointer(get<void*>(0x887644))&&!nativeViewBufferPointer(get<void*>(0x87E8A4));
}
static bool nativeViewBufferEnough(void*buffer,int w,int h){
 if(!buffer)return false;const uint32_t p=uint32_t(uintptr_t(buffer));int width=0,height=0;uint32_t bits=0,end=0;
 return read(p+0x28,&width,4)&&read(p+0x2C,&height,4)&&width>=w&&height>=h&&width<=8192&&height<=8192&&uint64_t(width)*height<=40000000&&
  read(p+0x18,&bits,4)&&read(p+0x1C,&end,4)&&bits>=0x10000&&uint64_t(bits)+uint64_t(width)*height*2<=end;
}
// A world/window transition is recoverable waiting, not permission to replay
// the previous A. Retirement needs a fully rebuilt, non-borrowed generation.
static bool nativeViewNewGenerationReady(){
 if(!nativeViewIdle()||!nativeViewNoOwnedReferences())return false;
 NativeViewRetirementProof p;p.scenario=get<uint32_t>(0xA8B230);p.tactical=get<uint32_t>(0x887324);p.hwnd=get<uint32_t>(0xB73550);p.area=get<NativeViewRect>(0x886FA0);
 const uint32_t slots[]={0x88731C,0x8872FC,0x887314,0x887644,0x87E8A4};bool empty=true;
 for(unsigned i=0;i<5;++i){p.pointers[i]=get<void*>(slots[i]);empty&=p.pointers[i]==nullptr;}
 if(empty&&!get<bool>(0xA8E9A0)&&!p.scenario){p.valid=true;p.layout=false;nativeViewRetirement=p;return true;}
 if(!p.scenario||p.tactical<0x10000||nativeViewLoading()||!get<bool>(0xA8E9A0)||!nativeSidebarWindow(p.window)||
    p.area.x<0||p.area.y<0||p.area.w<64||p.area.h<64||int64_t(p.area.x)+p.area.w>p.window.w||int64_t(p.area.y)+p.area.h>p.window.h)return false;
 // A partial replacement cannot retire either owner's surfaces or buffers.
 for(unsigned i=0;i<3;++i)if(p.pointers[i]&&(p.pointers[i]==nativeViewWindow[0]||p.pointers[i]==nativeViewWindow[1]||p.pointers[i]==sidebarViewAppliedPair[0]||p.pointers[i]==sidebarViewAppliedPair[1]))return false;
 for(unsigned i=3;i<5;++i)if(!p.pointers[i]||p.pointers[i]==nativeViewWindowBuffers[0]||p.pointers[i]==nativeViewWindowBuffers[1])return false;
 if(!p.pointers[0]||!p.pointers[1]||p.pointers[0]==p.pointers[1])return false;
 for(unsigned i=0;i<2;++i)if(nativeViewSurfaceValue(p.pointers[i],0x7C)<p.area.x+p.area.w||nativeViewSurfaceValue(p.pointers[i],0x80)<p.area.y+p.area.h||nativeViewSurfaceValue(p.pointers[i],0x70)!=2)return false;
 if(!nativeViewBufferEnough(p.pointers[3],p.area.w,p.area.h)||!nativeViewBufferEnough(p.pointers[4],p.area.w,p.area.h))return false;
 p.valid=p.layout=true;nativeViewRetirement=p;return true;
}
static bool nativeViewLocalRetirementReady(){
 const auto&p=nativeViewRetirement;NativeSidebarRect window{};
 if(!p.valid||!p.local||!nativeViewIdle()||!nativeViewNoOwnedReferences()||nativeViewLoading()||!get<bool>(0xA8E9A0)||
  get<uint32_t>(0xA8B230)!=p.scenario||get<uint32_t>(0x887324)!=p.tactical||get<uint32_t>(0xB73550)!=p.hwnd||get<int>(0xA8ED84)<p.frame||
  !nativeSidebarWindow(window)||memcmp(&window,&p.window,sizeof window)||!nativeViewRectEqual(get<NativeViewRect>(0x886FA0),p.area))return false;
 const uint32_t slots[]={0x88731C,0x8872FC,0x887314,0x887644,0x87E8A4};
 for(unsigned i=0;i<5;++i)if(get<void*>(slots[i])!=p.pointers[i])return false;return true;
}
static unsigned nativeViewSidebarRetirement(){
 if(!nativeViewRetirement.valid||!nativeViewRetired()||!sidebarViewTrackingIntact())return 0;
 if(!(nativeViewRetirement.local?nativeViewLocalRetirementReady():nativeViewNewGenerationReady()))return 0;
 return nativeViewRetirement.layout?2u:1u;
}
static void nativeViewSidebarRetired(){
 nativeViewRetirement={};nativeViewWindow[0]=nativeViewWindow[1]=nativeViewWindowBuffers[0]=nativeViewWindowBuffers[1]=nativeViewOldCurrent=nullptr;
 nativeViewEpoch=nativeViewTactical=nativeViewHwnd=0;nativeViewFrame=0;nativeViewWindowBounds={};nativeViewScreen={};
}
static bool nativeViewCurrentGeometry(const NativeSidebarRect&current,const NativeSidebarRect&physical){
 if(!nativeViewActive)return false;
 return current.x==nativeViewR.x&&current.y==nativeViewR.y&&current.w==nativeViewR.w&&current.h==nativeViewR.h&&
 physical.x==nativeViewA.x&&physical.y==nativeViewA.y&&physical.w==nativeViewA.w&&physical.h==nativeViewA.h;
}
static bool nativeViewQuiesce(){if(!nativeViewHooks&&!nativeViewActive&&!nativeViewRaster[0]&&!nativeViewRaster[1])return true;return nativeViewRestore();}
static unsigned char*nativeViewLock(void*surface,unsigned slot){
 if(slot>=2||nativeViewLocked[slot])return nullptr;auto*p=nativeViewSurfaceLock(surface);if(p)nativeViewLocked[slot]=surface;return p;
}
extern "C" void __cdecl AssistNativeViewUnlockOne(){
 for(auto&surface:nativeViewLocked)if(surface){void*p=surface;surface=nullptr;nativeViewSurfaceUnlock(p);return;}
}
static void nativeViewUnlock(unsigned slot){if(slot<2&&nativeViewLocked[slot]){void*p=nativeViewLocked[slot];nativeViewLocked[slot]=nullptr;nativeViewSurfaceUnlock(p);}}
static bool nativeViewClear(void*surface){
 const int w=nativeViewSurfaceValue(surface,0x7C),h=nativeViewSurfaceValue(surface,0x80),pitch=nativeViewSurfaceValue(surface,0x74);
 if(w<16||h<16||w>8192||h>8192||uint64_t(w)*h>40000000||pitch<w*2||pitch>131072||nativeViewSurfaceValue(surface,0x70)!=2)return false;
 auto*bits=nativeViewLock(surface,0);if(!bits)return false;
 for(int y=0;y<h;++y)memset(bits+size_t(y)*pitch,0,size_t(w)*2);nativeViewUnlock(0);return true;
}
static bool nativeViewBufferValid(void*buffer,int w,int h){
 if(!buffer)return false;const uint32_t p=uint32_t(uintptr_t(buffer));uint32_t surface=0,bits=0,end=0;int width=0,height=0;
 return read(p+0x28,&width,4)&&read(p+0x2C,&height,4)&&width==w&&height==h&&read(p+0x14,&surface,4)&&surface>=0x10000&&
  read(p+0x18,&bits,4)&&bits>=0x10000&&read(p+0x1C,&end,4)&&uint64_t(bits)+uint64_t(w)*h*2<=end&&end>bits;
}
static bool nativeViewPrepareRaster(int w,int h){
 if(nativeViewRaster[0]||nativeViewRaster[1]||nativeViewBuffersOwned[0]||nativeViewBuffersOwned[1]||!nativeViewIdle())return false;
 nativeViewRaster[0]=nativeViewNewSurface(w,h);if(!nativeViewRaster[0])return false;
 nativeViewRaster[1]=nativeViewNewSurface(w,h);if(!nativeViewRaster[1])return false;
 if(!nativeViewClear(nativeViewRaster[0])||!nativeViewClear(nativeViewRaster[1]))return false;
 nativeViewBuffersOwned[0]=nativeViewNewBuffer(false,{0,0,w,h});
 nativeViewBuffersOwned[1]=nativeViewNewBuffer(true,{0,0,w,h});
 if(!nativeViewBufferValid(nativeViewBuffersOwned[0],w,h)||!nativeViewBufferValid(nativeViewBuffersOwned[1],w,h))return false;
 nativeViewRasterCurrent[0]=nativeViewRaster[0];nativeViewRasterCurrent[1]=nativeViewRaster[1];nativeViewRasterW=w;nativeViewRasterH=h;return true;
}
static bool nativeViewReleaseRaster(){
 if(!nativeViewIdle()||nativeViewHold||nativeViewGeometryUncertain||!nativeViewNoOwnedReferences())return false;
 for(auto&p:nativeViewRaster){if(p)nativeViewFreeSurface(p);p=nullptr;}
 if(nativeViewBuffersOwned[0])nativeViewFreeBuffer(false,nativeViewBuffersOwned[0]);
 if(nativeViewBuffersOwned[1])nativeViewFreeBuffer(true,nativeViewBuffersOwned[1]);
 nativeViewBuffersOwned[0]=nativeViewBuffersOwned[1]=nativeViewRasterCurrent[0]=nativeViewRasterCurrent[1]=nullptr;nativeViewRasterW=nativeViewRasterH=0;return true;
}
// Restoring a known exact pair is permitted on an error path, but freeing it
// is not. Never overwrite a foreign or partially replaced pair.
static bool nativeViewSwapOut(){
 if(!nativeViewSwapped)return nativeViewNoOwnedReferences();
 if(!nativeViewSuspended){
  if(!nativeViewPair(get<void*>(0x88731C),get<void*>(0x8872FC),nativeViewRaster)||get<void*>(0x887644)!=nativeViewBuffersOwned[0]||get<void*>(0x87E8A4)!=nativeViewBuffersOwned[1])return false;
  nativeViewRasterCurrent[0]=get<void*>(0x88731C);nativeViewRasterCurrent[1]=get<void*>(0x8872FC);
 }else if(!nativeViewWindowIntact())return false;
 put<void*>(0x88731C,nativeViewWindow[0]);put<void*>(0x8872FC,nativeViewWindow[1]);put<void*>(0x887314,nativeViewOldCurrent);
 put<void*>(0x887644,nativeViewWindowBuffers[0]);put<void*>(0x87E8A4,nativeViewWindowBuffers[1]);nativeViewSwapped=nativeViewSuspended=false;return true;
}
static bool nativeViewSuspend(){
 if(!nativeViewSwapped||nativeViewSuspended)return true;
 if(!nativeViewPair(get<void*>(0x88731C),get<void*>(0x8872FC),nativeViewRaster)||get<void*>(0x887644)!=nativeViewBuffersOwned[0]||get<void*>(0x87E8A4)!=nativeViewBuffersOwned[1])return false;
 nativeViewRasterCurrent[0]=get<void*>(0x88731C);nativeViewRasterCurrent[1]=get<void*>(0x8872FC);
 put<void*>(0x88731C,nativeViewWindow[0]);put<void*>(0x8872FC,nativeViewWindow[1]);put<void*>(0x887314,nativeViewOldCurrent);
 put<void*>(0x887644,nativeViewWindowBuffers[0]);put<void*>(0x87E8A4,nativeViewWindowBuffers[1]);nativeViewSuspended=true;return true;
}
static bool nativeViewResume(){
 if(!nativeViewSwapped||!nativeViewSuspended)return true;if(!nativeViewWindowIntact())return false;
 put<void*>(0x88731C,nativeViewRasterCurrent[0]);put<void*>(0x8872FC,nativeViewRasterCurrent[1]);put<void*>(0x887314,nativeViewRasterCurrent[0]);
 put<void*>(0x887644,nativeViewBuffersOwned[0]);put<void*>(0x87E8A4,nativeViewBuffersOwned[1]);nativeViewSuspended=false;nativeViewHadMap=true;return true;
}
static bool nativeViewPresentRaster(void*source,void*target){
 if(!source||!target||source==target||!nativeViewRasterPointer(source)||!nativeViewPair(target,get<void*>(0x8872FC),nativeViewWindow))return false;
 const int sw=nativeViewSurfaceValue(source,0x7C),shh=nativeViewSurfaceValue(source,0x80),sp=nativeViewSurfaceValue(source,0x74),tw=nativeViewSurfaceValue(target,0x7C),th=nativeViewSurfaceValue(target,0x80),tp=nativeViewSurfaceValue(target,0x74);
 if(sw!=nativeViewR.w||shh!=nativeViewR.h||sw>8192||shh>8192||tw<16||th<16||tw>8192||th>8192||uint64_t(tw)*th>40000000||sp<sw*2||tp<tw*2||sp>131072||tp>131072||nativeViewSurfaceValue(source,0x70)!=2||nativeViewSurfaceValue(target,0x70)!=2)return false;
 const auto a=nativeViewA,r=nativeViewR;if(a.x<0||a.y<0||a.x+a.w>tw||a.y+a.h>th||r.x||r.y)return false;
 auto*s=nativeViewLock(source,0);if(!s)return false;auto*d=nativeViewLock(target,1);if(!d){nativeViewUnlock(0);return false;}
 static int columns[8192],lastWidth=0,lastRaster=0;
 if(lastWidth!=a.w||lastRaster!=r.w){for(int x=0;x<a.w;++x)columns[x]=int(int64_t(x)*r.w/a.w);lastWidth=a.w;lastRaster=r.w;}
 for(int y=0;y<a.h;++y){const auto*sr=(const uint16_t*)(s+size_t(int(int64_t(y)*r.h/a.h))*sp);auto*dr=(uint16_t*)(d+size_t(a.y+y)*tp)+a.x;for(int x=0;x<a.w;++x)dr[x]=sr[columns[x]];}
 nativeViewUnlock(1);nativeViewUnlock(0);return true;
}
static void nativeViewFail(int error,bool hold=true){
 nativeViewError=error;nativeViewHold|=hold;nativeViewQueryBlocked=true;nativeViewMutation=false;
 if(sh){MemoryBarrier();InterlockedExchange(&sh->viewZoomBusy,0);}nativeViewPublish(error);
}
static bool nativeViewNativeCompatible(){
 static constexpr struct{uint32_t at,hash;} checks[]={{0x4A8960,0x196B1023u},{0x6D5F60,0xA780188Eu},{0x4BA5A0,0x3B67EC58u},
  {0x7BC970,0x9C62F259u},{0x7BCAE0,0xB03F0A3Au},{0x410CE0,0x9C62F259u},{0x410E50,0xB63993EEu}};
 for(const auto&c:checks)if(!nativeViewSignature(c.at,16,c.hash))return false;return sidebarViewRedrawCompatible();
}
static bool nativeViewGeometry(NativeViewRect physical,NativeViewRect logical,bool maintainSidebar=true){
 if(!nativeViewIdle()||!nativeViewWorld()||!sidebarViewTrackingIntact()||!nativeViewAllSites()||!nativeViewNativeCompatible())return false;
 const uint32_t tactical=get<uint32_t>(0x887324);if(tactical<0x10000||!nativeViewPair(get<void*>(0x88731C),get<void*>(0x8872FC),sidebarViewAppliedPair))return false;
 const auto old=get<NativeViewRect>(0x886FA0);
 if(nativeViewActive){if(!nativeViewRectEqual(old,nativeViewR)||!nativeViewWindowIntact())return false;}
 else if(!nativeViewRectEqual(old,physical))return false;
 void*pair[2]={get<void*>(0x88731C),get<void*>(0x8872FC)};
 const int center[4]={get<int>(tactical+0xD64),get<int>(tactical+0xD68),get<int>(tactical+0xD74),get<int>(tactical+0xD78)};
 nativeViewMutationBegin();nativeViewGeometryUncertain=true;
 // UI layout and its own Z/A pair are always rebuilt with physical A.
 put<NativeViewRect>(0x886FA0,physical);call<void>(0x4A8960,0x87F7E8,&physical);
 for(unsigned i=0;i<4;++i)put<int>(tactical+(i<2?0xD64:0xD74)+(i%2)*4,center[i]);
 if(get<void*>(0x88731C)!=pair[0]||get<void*>(0x8872FC)!=pair[1])return false;
 nativeViewWindow[0]=pair[0];nativeViewWindow[1]=pair[1];nativeViewWindowBuffers[0]=get<void*>(0x887644);nativeViewWindowBuffers[1]=get<void*>(0x87E8A4);
 if(!nativeViewWindowBuffers[0]||!nativeViewWindowBuffers[1]||nativeViewBufferPointer(nativeViewWindowBuffers[0])||nativeViewBufferPointer(nativeViewWindowBuffers[1]))return false;
 put<NativeViewRect>(0x886FA0,logical);call<void>(0x6D5F60,tactical,(void*)0x886FA0);
 if(!nativeViewRectEqual(get<NativeViewRect>(0x886FA0),logical)||!nativeViewRectEqual(get<NativeViewRect>(0xB0CE28),logical))return false;
 if(maintainSidebar)nativeSidebarAfterLayout();put<uint8_t>(0x884B8E,1);put<uint8_t>(0x884B8F,1);put<uint32_t>(0xB0CD44,0);
 nativeViewA=physical;nativeViewR=logical;nativeViewPreviousA=physical;nativeViewTactical=tactical;nativeViewEpoch=get<uint32_t>(0xA8B230);
 nativeViewGeometryUncertain=false;return true;
}
static void nativeViewDirtyMap(){
 if(!nativeViewWorld()||nativeViewR.w<16||nativeViewR.h<16)return;
 call<void>(0x6D2790,nativeViewTactical,NativeViewRect{0,0,nativeViewR.w,nativeViewR.h},true);
 call<void>(0x4F42F0,0x87F7E8,2);sceneFrame=-2147483647;sceneTick=0;
}
// Outside-map pixels are presentation background. This never changes shroud,
// reveal flags, terrain objects or any other synchronized simulation state.
static bool nativeViewVoid(void*surface){
 const auto g=cameraGeometry();if(!smallcamera::valid(g)||!nativeViewWorld()||!nativeViewRasterPointer(surface))return false;
 const int ox=get<int>(nativeViewTactical+0xB0),oy=get<int>(nativeViewTactical+0xB4);
 const int64_t left=30LL*(2*g.x-g.mapWidth)-ox,top=15LL*(g.mapWidth+2*g.y-5)-oy,right=left+60LL*g.width,bottom=top+30LL*g.height+135;
 const int w=nativeViewSurfaceValue(surface,0x7C),h=nativeViewSurfaceValue(surface,0x80),pitch=nativeViewSurfaceValue(surface,0x74);
 if(w!=nativeViewR.w||h!=nativeViewR.h||pitch<w*2||pitch>131072||nativeViewSurfaceValue(surface,0x70)!=2)return false;
 if(left<=0&&top<=0&&right>=w&&bottom>=h)return true;
 auto*bits=nativeViewLock(surface,0);if(!bits)return false;
 for(int y=0;y<h;++y){auto*row=(uint16_t*)(bits+size_t(y)*pitch);
  if(y<top||y>=bottom)memset(row,0,size_t(w)*2);
  else{const int l=left<0?0:left>w?w:int(left),r=right<0?0:right>w?w:int(right);if(l)memset(row,0,size_t(l)*2);if(r<w)memset(row+r,0,size_t(w-r)*2);}
 }nativeViewUnlock(0);return true;
}
extern "C" void __cdecl AssistNativeViewFailed(){
 // An interrupted native mutation cannot prove a safe geometry or lifetime.
 // Keep every potentially referenced allocation/adapter until process exit.
 nativeViewFail(viewzoom::Fault,true);
}
static bool nativeViewMapUnchecked(NativeViewPoint&point){
 if(!nativeViewActive)return true;if(nativeViewMutation||nativeViewHold||nativeViewWorldWait||nativeViewError)return false;
 if(!nativeViewWorld()||!nativeViewRectEqual(get<NativeViewRect>(0x886FA0),nativeViewR)||!nativeViewRectEqual(get<NativeViewRect>(0xB0CE28),nativeViewR))return false;
 if(nativeSidebarUIHit(point.x,point.y))return true; // native gadgets retain physical client pixels
 int x=0,y=0;if(!nativeviewportprojection::coordinate(point.x,nativeViewA.x,nativeViewA.w,0,nativeViewR.w,x)||
  !nativeviewportprojection::coordinate(point.y,nativeViewA.y,nativeViewA.h,0,nativeViewR.h,y))return false;
 // Preserve the native outside-view sentinel instead of mapping a sidebar
 // point onto an in-map cell. The raw physical mouse remains unchanged.
 if(x<0)x=0;if(y<0)y=0;if(x>nativeViewR.w)x=nativeViewR.w;if(y>nativeViewR.h)y=nativeViewR.h;point={x,y};return true;
}
extern "C" unsigned __cdecl AssistNativeViewWndEnter(unsigned argument,unsigned returnAddress){
 if(!nativeViewActive&&!nativeViewMutation&&!nativeViewHold)return 0;
 if(nativeViewMutation||nativeViewHold||nativeViewWorldWait||nativeViewError||GetCurrentThreadId()!=nativeViewThread||nativeViewWndDepth>=16||!argument)return UINT32_MAX;
 const uint32_t packed=*(const uint32_t*)(uintptr_t)argument;NativeViewPoint point{short(packed),short(packed>>16)};
 if(!nativeViewMapUnchecked(point))return UINT32_MAX;
 auto&frame=nativeViewWndFrames[nativeViewWndDepth++];frame.returnAddress=returnAddress;frame.packed=(uint32_t(point.y&65535)<<16)|uint32_t(point.x&65535);
 return uint32_t(uintptr_t(&frame.packed));
}
extern "C" unsigned __cdecl AssistNativeViewWndLeave(){
 if(!nativeViewWndDepth)return 0;auto&frame=nativeViewWndFrames[--nativeViewWndDepth];const uint32_t ret=frame.returnAddress;frame={};return ret;
}
extern "C" int __cdecl AssistNativeViewMousePoint(void*raw){
 if(!raw)return 0;auto&point=*static_cast<NativeViewPoint*>(raw);point={-1,-1};
 if((nativeViewActive&&GetCurrentThreadId()!=nativeViewThread)||nativeViewMutation||nativeViewHold||nativeViewWorldWait||nativeViewError)return 1;
 const uint32_t mouse=get<uint32_t>(0x887640);if(mouse<0x10000)return 1;
 point={get<int>(mouse+0x1C),get<int>(mouse+0x20)};if(!nativeViewMapUnchecked(point))point={-1,-1};return 1;
}
extern "C" int __cdecl AssistNativeViewAction(unsigned action,unsigned argument){
 if(action==20){nativeViewTickImpl();return 1;}
 if(action==21)return nativeViewRestoreImpl()?1:0;
 if(action==0){
  if(nativeViewMutation||nativeViewHold||nativeViewError)return nativeViewRenderNeeded()?2:1;
  if(!nativeViewActive)return 1;
  if(!nativeViewWorld()){nativeViewWorldWait=true;nativeViewRestorePending=true;nativeViewQueryBlocked=true;nativeViewPublish(viewzoom::Waiting);return 2;}
  if(nativeViewInRender||!nativeViewAllSites()||!sidebarViewTrackingIntact()||!nativeViewWindowIntact()||
     !nativeViewRectEqual(get<NativeViewRect>(0x886FA0),nativeViewR)||!nativeViewRectEqual(get<NativeViewRect>(0xB0CE28),nativeViewR)){nativeViewFail(-8);return 2;}
  if(nativeViewWorldWait){nativeViewWorldWait=false;nativeViewQueryBlocked=false;nativeViewPublishWire();}
  if(nativeViewDirty&&nativeViewDirtySerial!=sidebarViewRenderSerial){nativeViewDirtyMap();nativeViewDirtySerial=sidebarViewRenderSerial;--nativeViewDirty;}
  nativeViewWindow[0]=get<void*>(0x88731C);nativeViewWindow[1]=get<void*>(0x8872FC);nativeViewOldCurrent=get<void*>(0x887314);
  nativeViewInRender=nativeViewSwapped=true;nativeViewSuspended=false;nativeViewHadMap=false;
  put<void*>(0x88731C,nativeViewRasterCurrent[0]);put<void*>(0x8872FC,nativeViewRasterCurrent[1]);put<void*>(0x887644,nativeViewBuffersOwned[0]);put<void*>(0x87E8A4,nativeViewBuffersOwned[1]);return 1;
 }
 if(action==6){
  if(nativeViewSwapped&&!nativeViewSwapOut())nativeViewFail(-8);
  nativeViewInRender=false;return nativeViewHold?0:1;
 }
 if(action==9&&(nativeViewMutation||nativeViewHold||nativeViewWorldWait||nativeViewError))return 2;
 if(!nativeViewActive)return action==1?int(argument):action==10?0:1;
 if(action==1){if(!nativeViewSuspend()){nativeViewFail(-8);return int(argument);}return int(uintptr_t(nativeViewWindow[0]));}
 if(action==2||action==4){if(!nativeViewResume())nativeViewFail(-8);return 1;}
 if(action==3){if(!nativeViewSuspend())nativeViewFail(-8);return 1;}
 if(action==5){
  void*source=get<void*>(0x88731C);const bool painted=nativeViewHadMap&&!nativeViewSuspended;
  const bool intact=nativeViewAllSites()&&sidebarViewTrackingIntact();
  if(!nativeViewSwapOut()){nativeViewFail(-8);return 0;}
  if(!intact){nativeViewFail(-8);return 0;}
  if(painted&&(!nativeViewVoid(source)||!nativeViewPresentRaster(source,get<void*>(0x88731C)))){nativeViewFail(viewzoom::ResizeFailed);return 0;}
  nativeSidebarPresent(get<void*>(0x88731C));return 1;
 }
 if(action==7){if(argument){const uint32_t mouse=get<uint32_t>(0x887640);if(mouse>=0x10000){auto*p=(NativeViewPoint*)(uintptr_t)argument;p->x=get<int>(mouse+0x1C);p->y=get<int>(mouse+0x20);}}return 1;}
 if(action==9){
  if(nativeViewMutation||nativeViewHold||nativeViewError)return 2;if(nativeViewQueryDepth)return 1;
  const uint32_t mouse=get<uint32_t>(0x887640);if(mouse<0x10000)return 2;
  const int x=get<int>(mouse+0x1C),y=get<int>(mouse+0x20);if(nativeSidebarTopHit(x,y))return 1;
  return nativeviewportprojection::contains({nativeViewA.x,nativeViewA.y,nativeViewA.w,nativeViewA.h},x,y)?1:2;
 }
 if(action==10){const uint32_t tooltip=get<uint32_t>(0x887368);if(tooltip>=0x10000)call<void>(0x724AD0,tooltip);return 1;}
 if(action==11){
  if(!argument)return 0;const uint32_t mouse=get<uint32_t>(0x887640);if(mouse<0x10000)return 0;
  auto*point=(NativeViewPoint*)(uintptr_t)argument;point->x=get<int>(mouse+0x1C);point->y=get<int>(mouse+0x20);return nativeViewMapUnchecked(*point)?1:0;
 }
 if(action==12){if(nativeViewSwapped&&!nativeViewSuspended)put<void*>(0x887314,get<void*>(0x88731C));return 1;}
 return 0;
}

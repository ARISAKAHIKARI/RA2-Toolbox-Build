#pragma once
#include "native_pip_core.h"
#include "../../external_reader/source/projection.h"
#include "../../external_reader/source/map_bounds.h"
// Included after the native viewport/sidebar owner. Runs only under its
// verified Render lifetime; never from a Host worker or a polling callback.
static nativepip::Transaction nativePipTransaction;
static nativepip::Cadence nativePipCadence;
static void*nativePipSurfaces[2]{},*nativePipBuffers[2]{},*nativePipLocks[2]{};
static bool nativePipFailed=false,nativePipRendering=false,nativePipContent=false,nativePipPinned=false;
static uint32_t nativePipScenario=0,nativePipTactical=0,nativePipObject=0,nativePipID=0;
static int nativePipCenterX=0,nativePipCenterY=0;
static uint32_t nativePipAllocatedAt=0;
static uint32_t nativePipMouseOwned=0,nativePipJumpAt=0,nativePipJumpMatch=0;
static smallcamera::Point nativePipJumpPoint{};
static bool nativePipJumpPending=false;
struct NativePipUnderlay {void*surface=nullptr;uint32_t scenario=0,tactical=0;int x=0,y=0,width=0,height=0,pitch=0;std::array<uint16_t,nativepip::DisplayWidth*nativepip::DisplayHeight> pixels{};};
static NativePipUnderlay nativePipUnderlay;
static bool nativePipUnderlayPending(){return nativePipUnderlay.surface!=nullptr;}
static bool nativePipBusy(){return nativePipRendering||nativePipTransaction.borrowed();}
struct NativePipIO {
 bool read(uint32_t p,void*out,size_t n){return ::read(p,out,n);}
 bool write(uint32_t p,const void*data,size_t n){memcpy((void*)(uintptr_t)p,data,n);return true;}
};
static NativePipIO nativePipIO;
static bool nativePipDemand(){return sh&&sh->nativePipEnabled&&!sh->readOnly&&!sh->stop&&!fault;}
extern "C" int __cdecl AssistNativePipRestore(){return nativePipTransaction.restore(nativePipIO)?1:0;}
extern "C" void __cdecl AssistNativePipUnlock(){
 for(auto&p:nativePipLocks)if(p){auto*surface=p;p=nullptr;nativeViewSurfaceUnlock(surface);}
}
extern "C" void __cdecl AssistNativePipFailed(){nativePipFailed=true;nativePipContent=false;if(sh)sh->nativePipStatus=-3;}
static bool nativePipResources(){
 if(nativePipSurfaces[0]&&nativePipSurfaces[1]&&nativePipBuffers[0]&&nativePipBuffers[1])return true;
 if(nativePipTransaction.borrowed()||sidebarViewRendering()||nativeViewInRender)return false;
 const auto now=GetTickCount();if(nativePipAllocatedAt&&uint32_t(now-nativePipAllocatedAt)<1000)return false;nativePipAllocatedAt=now;
 for(unsigned i=0;i<2;++i)if(!nativePipSurfaces[i])nativePipSurfaces[i]=nativeViewNewSurface(nativepip::RasterWidth,nativepip::RasterHeight);
 for(unsigned i=0;i<2;++i)if(!nativePipBuffers[i])nativePipBuffers[i]=nativeViewNewBuffer(i!=0,{0,0,nativepip::RasterWidth,nativepip::RasterHeight});
 if(!nativePipSurfaces[0]||!nativePipSurfaces[1]||!nativePipBuffers[0]||!nativePipBuffers[1])return false;
 for(unsigned i=0;i<2;++i)if(!nativeViewBufferValid(nativePipBuffers[i],nativepip::RasterWidth,nativepip::RasterHeight))return false;
 return true;
}
static bool nativePipRestoreUnderlay(){
 auto&u=nativePipUnderlay;if(!u.surface)return true;void*surface=u.surface;
 // A scene/layout replacement can reuse addresses. Never replay old pixels
 // into an unverified replacement surface or call through its stale pointer.
 if(u.scenario!=get<uint32_t>(0xA8B230)||u.tactical!=get<uint32_t>(0x887324)||
    (surface!=get<void*>(0x88731C)&&surface!=get<void*>(0x8872FC))){u.surface=nullptr;return true;}
 if(nativeViewSurfaceValue(surface,0x7C)!=u.width||nativeViewSurfaceValue(surface,0x80)!=u.height||nativeViewSurfaceValue(surface,0x74)!=u.pitch){u.surface=nullptr;return true;}
 auto*bits=nativeViewSurfaceLock(surface);if(!bits)return false;nativePipLocks[0]=surface;
 for(int row=0;row<nativepip::DisplayHeight;++row)memcpy(bits+size_t(u.y+row)*u.pitch+u.x*2,u.pixels.data()+size_t(row)*nativepip::DisplayWidth,nativepip::DisplayWidth*2);
 AssistNativePipUnlock();u.surface=nullptr;return true;
}
static bool nativePipQuiesceImpl(){
 if(nativePipTransaction.borrowed()||nativePipRendering||sidebarViewRendering()||nativeViewInRender)return false;
 if(!nativePipRestoreUnderlay())return false;
 if(!nativeMapRelease(false))return false;
 for(auto&p:nativePipSurfaces){if(p)nativeViewFreeSurface(p);p=nullptr;}
 for(unsigned i=0;i<2;++i){if(nativePipBuffers[i])nativeViewFreeBuffer(i!=0,nativePipBuffers[i]);nativePipBuffers[i]=nullptr;}
 nativePipCadence.reset();nativePipContent=nativePipPinned=nativePipFailed=false;nativePipJumpPending=false;nativePipScenario=nativePipTactical=nativePipObject=nativePipID=nativePipAllocatedAt=0;
 if(sh){sh->nativePipPinAck=sh->nativePipPinRequest;sh->nativePipStatus=0;}return true;
}
static bool nativePipQuiesce(){
 if(!nativeMapPipLease&&!nativePipSurfaces[0]&&!nativePipSurfaces[1]&&!nativePipBuffers[0]&&!nativePipBuffers[1]&&!nativePipUnderlayPending()&&!nativePipFailed&&!nativePipTransaction.borrowed())return true;
 return GuardedNativePipAction(2,0)!=0;
}
static bool nativePipVisible(){
 return nativePipDemand()&&sh->nativePipVisible&&sh->foreground&&!get<bool>(0xA8BC60+25)&&!get<int>(0xA8EDA0)&&
  !get<int>(0xA9FAB0)&&!get<uint8_t>(0xA9FA94)&&get<bool>(0xA8E9A0);
}
static void nativePipPin(uint32_t tactical){
 const uint32_t request=sh->nativePipPinRequest;const bool requested=request!=sh->nativePipPinAck;
 if(nativePipPinned&&!requested)return;
 if(requested&&(sh->nativePipPinMatch!=match||uint32_t(GetTickCount()-sh->nativePipPinTick)>1500)){sh->nativePipPinAck=request;if(nativePipPinned)return;}
 nativePipCenterX=get<int>(tactical+0xD64);nativePipCenterY=get<int>(tactical+0xD68);
 nativePipObject=nativePipID=0;nativePipPinned=true;nativePipContent=false;nativePipCadence.reset();
 // Opening fixes the current area. An explicit pin request follows the first
 // selected live actor; changing/deselecting the selection does not replace it.
 if(requested&&request!=sh->nativePipPinAck){const Vec selection=vec(0xA8ECB8);
  for(int i=0;i<selection.count;++i){uint32_t p=item(selection,i);if(!p||!live(p)||kind(p)<0)continue;nativePipObject=p;nativePipID=get<uint32_t>(p+0x10);break;}
  sh->nativePipPinAck=request;
 }
}
static void nativePipFollow(){
 if(!nativePipObject)return;
 if(get<uint32_t>(nativePipObject+0x10)!=nativePipID||!live(nativePipObject)||kind(nativePipObject)<0){nativePipObject=nativePipID=0;return;}
 const int x=get<int>(nativePipObject+0x9C),y=get<int>(nativePipObject+0xA0),z=get<int>(nativePipObject+0xA4);
 if(x<0||y<0||x>512*256||y>512*256||z< -65536||z>65536)return;
 const double scale=external::projectionZScale([](uint32_t at,void*out,size_t n){return read(at,out,n);});
 nativePipCenterX=int((int64_t(x)-y)*30/256);nativePipCenterY=int((int64_t(x)+y)*15/256)-external::screenHeight(z,scale);
}
static bool nativePipBlit(){
 void*dst=get<void*>(0x88731C);if(!dst||dst==nativePipSurfaces[0]||dst==nativePipSurfaces[1])return false;
 const int cw=nativeViewSurfaceValue(dst,0x7C),ch=nativeViewSurfaceValue(dst,0x80),dp=nativeViewSurfaceValue(dst,0x74);
 const int sp=nativeViewSurfaceValue(nativePipSurfaces[0],0x74);
 if(cw<320||ch<240||cw>8192||ch>8192||dp<cw*2||dp>131072||sp<nativepip::RasterWidth*2||sp>131072||nativeViewSurfaceValue(dst,0x70)!=2)return false;
 NativeViewRect area=nativeViewActive?nativeViewA:get<NativeViewRect>(0x886FA0);
 const int x=area.x+area.w-nativepip::DisplayWidth-8,y=area.y+8;
 if(x<area.x||y<area.y||x<0||y<0||x+nativepip::DisplayWidth>cw||y+nativepip::DisplayHeight>ch||y+nativepip::DisplayHeight>area.y+area.h)return false;
 auto*source=nativeViewSurfaceLock(nativePipSurfaces[0]);if(!source)return false;nativePipLocks[0]=nativePipSurfaces[0];
 auto*target=nativeViewSurfaceLock(dst);if(!target)return false;nativePipLocks[1]=dst;
 auto&u=nativePipUnderlay;if(u.surface)return false;
 u.scenario=nativePipScenario;u.tactical=nativePipTactical;u.x=x;u.y=y;u.width=cw;u.height=ch;u.pitch=dp;
 for(int row=0;row<nativepip::DisplayHeight;++row)memcpy(u.pixels.data()+size_t(row)*nativepip::DisplayWidth,target+size_t(y+row)*dp+x*2,nativepip::DisplayWidth*2);
 u.surface=dst;
 for(int row=0;row<nativepip::DisplayHeight;++row){auto*s=(const uint16_t*)(source+size_t(row*2)*sp);auto*d=(uint16_t*)(target+size_t(y+row)*dp)+x;
  for(int col=0;col<nativepip::DisplayWidth;++col)d[col]=s[col*2];
 }
 AssistNativePipUnlock();return true;
}
extern "C" int __cdecl AssistNativePipAction(unsigned action,unsigned){
 if(!sh)return 0;
 if(action==2)return nativePipQuiesceImpl()?1:0;
 if(action==3)return nativePipRestoreUnderlay()?1:0;
 if(!nativePipDemand())return 0;
 if(action==4){if(!nativePipJumpPending)return 1;nativePipJumpPending=false;
  if(nativePipJumpMatch==match&&uint32_t(GetTickCount()-nativePipJumpAt)<=200&&nativePipVisible())radarNavigationMove(nativePipJumpPoint);return 1;}
 if(nativePipFailed){sh->nativePipStatus=-3;return 0;}
 if(action==0){
  if(!sidebarViewTracking||!sidebarViewRenderSerial||!sidebarViewTrackingIntact()){sh->nativePipStatus=1;return 0;}
  // Reuse the one map-completion adapter, including when zoom owns it too.
  if(!nativeViewExecutable(0x6D3D10)||!nativeMapAcquire(false)){sh->nativePipStatus=-1;return 0;}
  if(!nativePipResources()){sh->nativePipStatus=-2;return 0;}
  sh->nativePipStatus=nativePipObject?3:2;return 1;
 }
 if(action!=1||!nativePipVisible()||nativePipRendering||sidebarViewRenderDepth!=1||!sidebarViewTrackingIntact()||!nativeMapPipLease||!nativePipSurfaces[0]||!nativePipSurfaces[1]||!nativePipBuffers[0]||!nativePipBuffers[1])return 0;
 const uint32_t scenario=get<uint32_t>(0xA8B230),tactical=get<uint32_t>(0x887324);if(!scenario||tactical<0x10000)return 0;
 if(scenario!=nativePipScenario||tactical!=nativePipTactical){nativePipScenario=scenario;nativePipTactical=tactical;nativePipPinned=nativePipContent=false;nativePipObject=nativePipID=0;nativePipCadence.reset();}
 nativePipPin(tactical);nativePipFollow();
 const uint32_t now=GetTickCount();const int frame=get<int>(0xA8ED84);
 if(nativePipCadence.due(now,frame)&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){
  const auto bounds=external::mapbounds::sample([](uint32_t at,void*out,size_t n){return read(at,out,n);});
  double cx=nativePipCenterX,cy=nativePipCenterY;if(!bounds.clampCamera(cx,cy,nativepip::RasterWidth,nativepip::RasterHeight)){sh->nativePipStatus=1;return 0;}
  if(!nativePipTransaction.capture(nativePipIO,tactical)){sh->nativePipStatus=-1;return 0;}
  uint32_t surfaces[]={uint32_t(uintptr_t(nativePipSurfaces[0])),uint32_t(uintptr_t(nativePipSurfaces[1])),uint32_t(uintptr_t(nativePipSurfaces[0])),uint32_t(uintptr_t(nativePipBuffers[0])),uint32_t(uintptr_t(nativePipBuffers[1]))};
  LARGE_INTEGER start{},end{},frequency{};QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
  nativePipRendering=true;
  if(!nativePipTransaction.enter(nativePipIO,surfaces,int(cx),int(cy)))return 0;
  for(int phase=0;phase<3;++phase)call<void>(0x6D3D10,tactical,nativePipSurfaces[0],1,phase);
  if(!nativePipTransaction.restore(nativePipIO)){sh->nativePipStatus=-4;return 0;}
  nativePipRendering=false;nativePipContent=true;
  QueryPerformanceCounter(&end);const uint32_t us=frequency.QuadPart>0?uint32_t(std::max<LONGLONG>(0,(end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart)):0;
  sh->nativePipMicros=us;sh->nativePipMaxMicros=std::max(uint32_t(sh->nativePipMaxMicros),us);++sh->nativePipFrames;
  nativePipCadence.completed(GetTickCount(),frame,us);
 }
 sh->nativePipStatus=nativePipObject?3:2;
 return nativePipVisible()&&nativePipContent&&nativePipBlit()?1:0;
}
extern "C" void __cdecl AssistNativePipEnd(){nativePipRendering=false;}
// Own the physical button pair through the existing WH_GETMESSAGE filter.
// The dispatcher later applies a local camera intent; no game order is issued
// here and the Runtime never writes the Host-owned radar camera mailbox.
static bool nativePipMessage(MSG*msg){
 const bool leftDown=msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONDBLCLK,leftUp=msg->message==WM_LBUTTONUP;
 const bool rightDown=msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONDBLCLK,rightUp=msg->message==WM_RBUTTONUP;
 const uint32_t bit=(leftDown||leftUp)?1:(rightDown||rightUp)?2:0;const bool owned=bit&&(nativePipMouseOwned&bit);
 if(bit&&(leftUp||rightUp||leftDown||rightDown))nativePipMouseOwned&=~bit;
 bool inside=false;const auto&u=nativePipUnderlay;
 if(nativePipVisible()&&!nativePipRendering&&u.surface&&u.scenario==get<uint32_t>(0xA8B230)&&u.tactical==get<uint32_t>(0x887324)&&
    msg->hwnd==(HWND)(uintptr_t)get<uint32_t>(0xB73550)){
  RECT client{};if(GetClientRect(msg->hwnd,&client)&&client.right>0&&client.bottom>0){
   const int x=int(int64_t(short(LOWORD(msg->lParam)))*u.width/client.right),y=int(int64_t(short(HIWORD(msg->lParam)))*u.height/client.bottom);
   inside=x>=u.x&&y>=u.y&&x<u.x+nativepip::DisplayWidth&&y<u.y+nativepip::DisplayHeight;
  }
 }
 if((leftDown||rightDown)&&inside)nativePipMouseOwned|=bit;
 if(leftUp&&owned&&inside){nativePipJumpPoint={nativePipCenterX,nativePipCenterY};nativePipJumpMatch=match;nativePipJumpAt=GetTickCount();nativePipJumpPending=true;InterlockedExchange(&hostWorkPending,1);}
 if((inside&&(leftDown||rightDown||msg->message==WM_MOUSEMOVE))||((leftUp||rightUp)&&owned)){
  msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;
 }return false;
}

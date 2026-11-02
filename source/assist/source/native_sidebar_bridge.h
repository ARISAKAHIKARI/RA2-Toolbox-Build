#pragma once
#include "../../shared/source/native_sidebar_policy.h"
#include "native_viewport_surface.h"
// Native local-presentation adapter. Geometry R and clipping buffers belong to
// view_zoom_bridge; this module owns physical A, sidebar input, and the columns.
struct NativeSidebarRect {int x=0,y=0,w=0,h=0;};
static void* nativeViewNewSurface(int,int);
static void nativeViewFreeSurface(void*);
static int nativeSidebarMode=0;
static bool nativeSidebarInstalled=false,nativeSidebarGeometryChanged=false;
static void* nativeSidebarOwned[2]{};
static void* nativeSidebarSaved[2]{};
static void* nativeSidebarViewGadget=nullptr;
static uint8_t nativeSidebarGadgetSaved[247]{};
static bool nativeSidebarGadgetHeld[247]{};
static int nativeSidebarLastGadgetFrame=-1;
static int nativeSidebarSurfaceWidth(void* p,unsigned offset){
 if(!p)return 0;uint32_t table=0,fn=0;
 if(!read(uint32_t(uintptr_t(p)),&table,4)||!read(table+offset,&fn,4))return 0;
 return ((int(__thiscall*)(void*))(uintptr_t)fn)(p);
}
static int nativeSidebarWidth(){int w=get<int>(0x7F5BF8);return w>=64&&w<=512?w:168;}
static bool nativeSidebarWindow(NativeSidebarRect&r){
 int w=get<int>(0x886FB8),h=get<int>(0x886FBC);
 if(w<640||w>8192||h<240||h>8192)return false;r={0,0,w,h};return true;
}
static bool nativeSidebarArea(NativeSidebarRect&r){
 if(!nativeSidebarWindow(r))return false;
 r.h-=32;
 if(nativeSidebarMode==0){int sw=nativeSidebarWidth();r.w-=sw;
  r.x=get<uint8_t>(0xA8EB7C)||get<uint8_t>(0xA8ED6B)?0:sw;
 }
 return r.w>64&&r.h>64;
}
static int nativeSidebarColumnX(){NativeSidebarRect w{};return nativeSidebarWindow(w)?(get<uint8_t>(0xA8EB7C)?w.w-nativeSidebarWidth():0):-1;}
static int nativeSidebarMapRight(){NativeSidebarRect a{};return nativeSidebarArea(a)?a.x+a.w:0;}
static int nativeSidebarTopHeight(){int h=get<int>(0x886F94);return h>=32&&h<=512?h:158;}
static bool nativeSidebarKeep(NativeSidebarRect&r,bool topOnly=false){
 if(!nativeSidebarInstalled||nativeSidebarMode==2)return false;
 void* strip=get<void*>(0x887300);int w=nativeSidebarSurfaceWidth(strip,0x7C),h=nativeSidebarSurfaceWidth(strip,0x80);
 if(w<32||w>512||h<32||h>8192)return false;
 if(nativeSidebarMode==1||topOnly){int k=nativeSidebarTopHeight()+(topOnly?0:5);if(k<h)h=k;}
 r={0,0,w,h};return true;
}
static bool nativeSidebarMouse(POINT&p){
 HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));
 return window&&GetCursorPos(&p)&&ScreenToClient(window,&p);
}
static bool nativeSidebarTopHit(int x,int y){
 NativeSidebarRect r{};if(!nativeSidebarKeep(r,true))return false;
 int col=nativeSidebarColumnX();return col>=0&&x>=col&&x<col+r.w&&y>=r.y&&y<r.y+r.h;
}
static bool nativeSidebarTopHit(){POINT p{};return nativeSidebarMouse(p)&&nativeSidebarTopHit(p.x,p.y);}
static void nativeSidebarColumn(bool layout=false){
 if(!nativeSidebarInstalled)return;
 int col=nativeSidebarColumnX();if(col<0)return;
 if(nativeSidebarMode==2&&!layout){NativeSidebarRect w{};if(!nativeSidebarWindow(w))return;col=get<uint8_t>(0xA8EB7C)?w.w:-(nativeSidebarWidth()+8);}
 put<int>(0x886F90,col);
}
static void nativeSidebarCopy(void* target,bool cursor){
 NativeSidebarRect src{},win{};if(!target||!nativeSidebarKeep(src)||!nativeSidebarWindow(win))return;
 void* strip=get<void*>(0x887300);int tw=nativeSidebarSurfaceWidth(target,0x7C),th=nativeSidebarSurfaceWidth(target,0x80);
 if(tw<32||th<32)return;
 int col=nativeSidebarColumnX();if(col<0)return;NativeSidebarRect dst{col,src.y,src.w,src.h};
 if(cursor&&(tw!=win.w||th!=win.h)){
  HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));POINT p{};
  if(window&&ClientToScreen(window,&p)){dst.x+=p.x;dst.y+=p.y;}
 }
 if(dst.x<0||dst.y<0||dst.x>=tw||dst.y>=th)return;
 if(dst.x+src.w>tw)src.w=tw-dst.x;if(dst.y+src.h>th)src.h=th-dst.y;
 if(src.w<=0||src.h<=0)return;dst.w=src.w;dst.h=src.h;
 if(cursor){void* mouse=get<void*>(0x887640);uint32_t table=0,draw=0;
  if(mouse&&read(uint32_t(uintptr_t(mouse)),&table,4)&&read(table+0x3C,&draw,4)){
   int saved=get<int>(0x886FA8);if(get<uint8_t>(0xA8EB7C))put<int>(0x886FA8,col);
   ((void(__thiscall*)(void*,void*,int))(uintptr_t)draw)(mouse,strip,1);put<int>(0x886FA8,saved);
  }
 }
 uint32_t table=0,copy=0;if(!read(uint32_t(uintptr_t(target)),&table,4)||!read(table+0x08,&copy,4))return;
 ((int(__thiscall*)(void*,NativeSidebarRect*,void*,NativeSidebarRect*,int,int))(uintptr_t)copy)(target,&dst,strip,&src,0,1);
 // Only the full sidebar owns the remaining column below the strip.
 if(nativeSidebarMode==0&&dst.y+dst.h<th){uint32_t fill=0;if(read(table+0x14,&fill,4)){
  NativeSidebarRect tail{dst.x,dst.y+dst.h,dst.w,th-dst.y-dst.h};
  ((int(__thiscall*)(void*,NativeSidebarRect*,unsigned))(uintptr_t)fill)(target,&tail,0);
 }}
}
static void nativeSidebarPresent(void* surf){if(nativeSidebarInstalled)nativeSidebarCopy(surf,false);}
static bool nativeSidebarOwnsPair(void* c,void* m){
 return nativeSidebarOwned[0]&&nativeSidebarOwned[1]&&c!=m&&
  (c==nativeSidebarOwned[0]||c==nativeSidebarOwned[1])&&(m==nativeSidebarOwned[0]||m==nativeSidebarOwned[1]);
}
static void nativeSidebarReleaseSurfaces(){
 void* c=get<void*>(0x88731C),*m=get<void*>(0x8872FC);
 if(nativeSidebarOwnsPair(c,m)){
  bool normal=c==nativeSidebarOwned[0];
  void*current=get<void*>(0x887314);if(current==nativeSidebarOwned[0])put<void*>(0x887314,nativeSidebarSaved[0]);else if(current==nativeSidebarOwned[1])put<void*>(0x887314,nativeSidebarSaved[1]);
  put<void*>(0x88731C,nativeSidebarSaved[normal?0:1]);put<void*>(0x8872FC,nativeSidebarSaved[normal?1:0]);
  nativeViewFreeSurface(nativeSidebarOwned[0]);nativeViewFreeSurface(nativeSidebarOwned[1]);
 }
 // A native rebuild may already have destroyed the borrowed surfaces. Never
 // free or restore pointers after another display owner replaces the pair.
 nativeSidebarOwned[0]=nativeSidebarOwned[1]=nativeSidebarSaved[0]=nativeSidebarSaved[1]=nullptr;
}
static bool nativeSidebarExpandSurfaces(){
 NativeSidebarRect w{};if(!nativeSidebarWindow(w))return false;
 void* c=get<void*>(0x88731C),*m=get<void*>(0x8872FC);if(!c||!m||c==m)return false;
 if(nativeSidebarOwned[0]&&!nativeSidebarOwnsPair(c,m)){
  nativeSidebarOwned[0]=nativeSidebarOwned[1]=nativeSidebarSaved[0]=nativeSidebarSaved[1]=nullptr;
 }
 int cw=nativeSidebarSurfaceWidth(c,0x7C),ch=nativeSidebarSurfaceWidth(c,0x80),mw=nativeSidebarSurfaceWidth(m,0x7C),mh=nativeSidebarSurfaceWidth(m,0x80);
 if(cw>=w.w&&ch>=w.h&&mw>=w.w&&mh>=w.h)return true;
 if(nativeSidebarOwned[0]){nativeSidebarReleaseSurfaces();c=get<void*>(0x88731C);m=get<void*>(0x8872FC);}
 void* a=nativeViewNewSurface(w.w,w.h);if(!a)return false;
 void* b=nativeViewNewSurface(w.w,w.h);if(!b){nativeViewFreeSurface(a);return false;}
 if(!nativeViewClearSurface(a)||!nativeViewClearSurface(b)){nativeViewFreeSurface(a);nativeViewFreeSurface(b);return false;}
 nativeSidebarSaved[0]=c;nativeSidebarSaved[1]=m;nativeSidebarOwned[0]=a;nativeSidebarOwned[1]=b;
 put<void*>(0x88731C,a);put<void*>(0x8872FC,b);nativeSidebarGeometryChanged=true;return true;
}
static void nativeSidebarGadgets(){
 const int frame=get<int>(0xA8ED84);if(frame==nativeSidebarLastGadgetFrame)return;nativeSidebarLastGadgetFrame=frame;
 static const uint32_t top[]={0xB04978,0xB04910,0xB04A10};
 for(unsigned i=0;i<247;++i){uint32_t g=i<240?0xB07E80+i*0x38:i<244?0xB07C48+(i-240)*0x60:top[i-244];
  const bool hidden=i<244?nativeSidebarMode!=0:nativeSidebarMode==2;
  if(hidden){if(!nativeSidebarGadgetHeld[i]){nativeSidebarGadgetSaved[i]=get<uint8_t>(g+0x1E);nativeSidebarGadgetHeld[i]=true;}
   put<uint8_t>(g+0x1E,1);put<uint8_t>(g+0x1C,1);
   for(uint32_t capture:{0x8B3E88u,0x8B3E90u,0x8B3E94u})if(get<uint32_t>(capture)==g)put<uint32_t>(capture,0);
  }else if(nativeSidebarGadgetHeld[i]){if(get<uint8_t>(g+0x1E)==1)put<uint8_t>(g+0x1E,nativeSidebarGadgetSaved[i]);put<uint8_t>(g+0x1C,1);nativeSidebarGadgetHeld[i]=false;}
 }
}
static void nativeSidebarAfterLayout(){nativeSidebarLastGadgetFrame=-1;nativeSidebarColumn();nativeSidebarGadgets();}
static void nativeSidebarTrimGadget(uint32_t object){
 if(!nativeSidebarInstalled||object<0x400000||object>0x8A0000)return;
 auto*r=reinterpret_cast<NativeSidebarRect*>(uintptr_t(object+0x0C));NativeSidebarRect a{};if(!nativeSidebarArea(a))return;
 const auto logical=get<NativeSidebarRect>(0x886FA0);
 if(object!=uint32_t(uintptr_t(nativeSidebarViewGadget))&&(r->x!=logical.x||r->w!=logical.w))return;
 nativeSidebarViewGadget=reinterpret_cast<void*>(uintptr_t(object));
 // In radar-only mode the top widget owns its column during dispatch. The
 // lower column still belongs to map input through the native click gate.
 if(nativeSidebarMode==1){int col=nativeSidebarColumnX();if(get<uint8_t>(0xA8EB7C))a.w=col-a.x;else{a.x=col+nativeSidebarWidth();a.w=nativeSidebarMapRight()-a.x;}}
 if(a.w>16&&a.h>16){*r=a;put<uint8_t>(object+0x20,1);}
}
static int nativeSidebarAction(unsigned action,uint32_t argument){
 if(!nativeSidebarInstalled)return 0;
 if(action==1){nativeSidebarColumn(true);return 1;}
 if(action==2){put<uint8_t>(0xB0B519,nativeSidebarMode!=2);put<uint8_t>(0xB0B518,0);put<uint8_t>(0x884B90,0);nativeSidebarCopy(get<void*>(0x887308),true);return 1;}
 if(action==3){nativeSidebarTrimGadget(argument);return 1;}
 if(action==4)return (GetAsyncKeyState(VK_LBUTTON)&0x8000)&&nativeSidebarTopHit()?1:0;
 return 0;
}
static __declspec(naked) void nativeSidebarBlitEntry(){__asm {
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
 push 34
 push 15
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 ret 4
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,38h
 push ebx
 push ebp
 push esi
 push 6A70E6h
 ret
}}
static __declspec(naked) void nativeSidebarInitEntry(){__asm {
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
 push 33
 push 15
 call GuardedRiskAction
 add esp,12
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,24h
 push ebx
 push ebp
 push 6ABD35h
 ret
}}
static __declspec(naked) void nativeSidebarGadgetEntry(){__asm {
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
 push dword ptr [ebx+24]
 push 35
 push 15
 call GuardedRiskAction
 add esp,12
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 sub esp,0Ch
 push ebx
 push ebp
 push esi
 push 4E1646h
 ret
}}
static __declspec(naked) void nativeSidebarClickEntry(){__asm {
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
 push 36
 push 15
 call GuardedRiskAction
 add esp,12
 test eax,eax
 jz original
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 6925FCh
 ret
original:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push ecx
 mov ecx,87F7E8h
 push 69240Ch
 ret
}}
static RiskSite nativeSidebarSites[sizeof(nativesidebarpolicy::Sites)/sizeof(nativesidebarpolicy::Sites[0])]{};
static void nativeSidebarMakeSites(){static bool made=false;if(made)return;made=true;
 for(unsigned i=0;i<sizeof(nativeSidebarSites)/sizeof(nativeSidebarSites[0]);++i){const auto&p=nativesidebarpolicy::Sites[i];auto&s=nativeSidebarSites[i];s.address=p.address;s.size=p.size;memcpy(s.before,p.before,p.size);
  if(p.kind==nativesidebarpolicy::Kind::Bytes)memcpy(s.after,p.after,p.size);
  else {void* entry=p.address==0x6A70E0?(void*)&nativeSidebarBlitEntry:p.address==0x6ABD30?(void*)&nativeSidebarInitEntry:p.address==0x4E1640?(void*)&nativeSidebarGadgetEntry:(void*)&nativeSidebarClickEntry;
   memset(s.after,0x90,s.size);s.after[0]=0xE9;uint32_t rel=uint32_t(uintptr_t(entry))-s.address-5;memcpy(s.after+1,&rel,4);}
 }
}
static bool nativeSidebarPrepare(){
 nativeSidebarMakeSites();
 if(!nativeSidebarInstalled){
  // Width mode windows are installed only for radar/bare; the rest preserve
  // full-mode behavior and supply physical column geometry to map-only zoom.
  bool ok=true;for(unsigned i=2;i<sizeof(nativeSidebarSites)/sizeof(nativeSidebarSites[0]);++i)if(!installRiskSite(nativeSidebarSites[i])){ok=false;break;}
  if(!ok){for(auto&s:nativeSidebarSites)restoreRiskSite(s);return false;}
  nativeSidebarInstalled=true;
 }
 return nativeSidebarExpandSurfaces();
}
static void nativeSidebarTick(int requested){
 if(requested<0||requested>2)requested=0;
 const bool needed=requested!=0||(sh&&sh->viewZoomPercent!=100)||nativeSidebarInstalled;
 if(!needed)return;
 if(!nativeSidebarPrepare()){nativeSidebarMode=0;for(unsigned i=0;i<2;++i)restoreRiskSite(nativeSidebarSites[i]);nativeSidebarColumn();nativeSidebarLastGadgetFrame=-1;nativeSidebarGadgets();nativeSidebarGeometryChanged=true;if(sh)sh->sidebarStatus=-2;return;}
 const int before=nativeSidebarMode;bool ok=true;
 for(unsigned i=0;i<2;++i)if(!(requested?installRiskSite(nativeSidebarSites[i]):restoreRiskSite(nativeSidebarSites[i])))ok=false;
 if(!ok){for(unsigned i=0;i<2;++i)restoreRiskSite(nativeSidebarSites[i]);requested=0;}
 nativeSidebarMode=requested;nativeSidebarColumn();
 if(before!=nativeSidebarMode){nativeSidebarGeometryChanged=true;nativeSidebarLastGadgetFrame=-1;put<uint8_t>(0x884B8E,1);put<uint8_t>(0x884B8F,1);put<uint32_t>(0xB0CD44,0);}
 nativeSidebarGadgets();if(sh)sh->sidebarStatus=ok?1:-2;
}
static bool nativeSidebarSurfaceChanged(){bool changed=nativeSidebarGeometryChanged;nativeSidebarGeometryChanged=false;return changed;}
static void nativeSidebarRestore(){
 nativeSidebarMode=0;nativeSidebarLastGadgetFrame=-1;nativeSidebarGadgets();nativeSidebarColumn(true);
 nativeSidebarReleaseSurfaces();for(auto&s:nativeSidebarSites)restoreRiskSite(s);
 nativeSidebarInstalled=false;nativeSidebarViewGadget=nullptr;nativeSidebarGeometryChanged=true;
 if(sh)sh->sidebarStatus=0;
}

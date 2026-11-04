#pragma once
// Native surface ABI shared by the local viewport and sidebar adapters.
// All callers run under the common game-thread Capture/Assist lease.
struct NativeViewRect {int x=0,y=0,w=0,h=0;};
struct NativeViewPoint {int x=0,y=0;};
static bool nativeViewRectEqual(const NativeViewRect&a,const NativeViewRect&b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
static bool nativeViewExecutable(uint32_t fn){
 MEMORY_BASIC_INFORMATION info{};
 return fn>=0x400000&&VirtualQuery((void*)(uintptr_t)fn,&info,sizeof info)&&
  info.State==MEM_COMMIT&&info.Type==MEM_IMAGE&&info.AllocationBase==GetModuleHandleW(nullptr)&&
  (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))&&
  !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS));
}
static int nativeViewSurfaceValue(void*p,unsigned slot){
 if(!p)return 0;uint32_t vt=0,fn=0;
 if(!read(uint32_t(uintptr_t(p)),&vt,4)||!read(vt+slot,&fn,4)||!nativeViewExecutable(fn))return 0;
 return ((int(__thiscall*)(void*))(uintptr_t)fn)(p);
}
static unsigned char*nativeViewSurfaceLock(void*p){
 uint32_t vt=0,fn=0;if(!p||!read(uint32_t(uintptr_t(p)),&vt,4)||!read(vt+0x5C,&fn,4)||!nativeViewExecutable(fn))return nullptr;
 return ((unsigned char*(__thiscall*)(void*,int,int))(uintptr_t)fn)(p,0,0);
}
static void nativeViewSurfaceUnlock(void*p){uint32_t vt=0,fn=0;if(p&&read(uint32_t(uintptr_t(p)),&vt,4)&&read(vt+0x60,&fn,4)&&nativeViewExecutable(fn))((void(__thiscall*)(void*))(uintptr_t)fn)(p);}
static void nativeViewFreeSurface(void*p){uint32_t vt=0,fn=0;if(p&&read(uint32_t(uintptr_t(p)),&vt,4)&&read(vt,&fn,4)&&nativeViewExecutable(fn))((void(__thiscall*)(void*,int))(uintptr_t)fn)(p,1);}
static void*nativeViewNewSurface(int w,int h){
 if(w<16||h<16||w>8192||h>8192||uint64_t(w)*h>40000000)return nullptr;
 void*p=((void*(__cdecl*)(unsigned))0x7C8E17)(0x24);if(!p)return nullptr;
 ((void*(__thiscall*)(void*,int,int,int,int))0x4BA5A0)(p,w,h,1,1);
 if(nativeViewSurfaceValue(p,0x7C)!=w||nativeViewSurfaceValue(p,0x80)!=h||nativeViewSurfaceValue(p,0x70)!=2){nativeViewFreeSurface(p);return nullptr;}
 return p;
}
static bool nativeViewClearSurface(void*p,unsigned short value=0){
 const int w=nativeViewSurfaceValue(p,0x7C),h=nativeViewSurfaceValue(p,0x80),pitch=nativeViewSurfaceValue(p,0x74);
 if(w<16||h<16||w>8192||h>8192||uint64_t(w)*h>40000000||pitch<w*2||pitch>131072||nativeViewSurfaceValue(p,0x70)!=2)return false;
 auto*bits=nativeViewSurfaceLock(p);if(!bits)return false;
 for(int y=0;y<h;++y){auto*row=(unsigned short*)(bits+size_t(y)*pitch);for(int x=0;x<w;++x)row[x]=value;}
 nativeViewSurfaceUnlock(p);return true;
}
static void*nativeViewNewBuffer(bool alpha,NativeViewRect r){
 void*p=((void*(__cdecl*)(unsigned))0x7C8E17)(0x30);if(!p)return nullptr;
 ((void(__thiscall*)(void*,NativeViewRect))(alpha?0x410CE0:0x7BC970))(p,r);
 if(!alpha)*(unsigned*)((unsigned char*)p+0x24)=0x8000;
 return p;
}
static void nativeViewFreeBuffer(bool alpha,void*p){if(!p)return;((void(__thiscall*)(void*))(alpha?0x410E50:0x7BCAE0))(p);((void(__cdecl*)(void*))0x7C8B3D)(p);}

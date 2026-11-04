#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stddef.h>
#include "bridge.h"
#include "sequence.h"
#include "../../shared/source/game_entry_client.h"
#include "hide_watch.h"
#include "perf_policy.h"
#include "native_cache.h"
#include "resource_capture.h"
// All calls into the game execute on its own hooked GUI thread.  No game
// memory is written directly, no hooks outside the supported game version.
static DWORD ownerId;
static Shared* s;
static HANDLE mapping,owner;
static void* held;
static DWORD heldVtable;

static LONG hiddenRef;
static DWORD lastInspectAt,lastSampleAt;
static bool rightWasDown, rightReleased;
static HCURSOR nativeCursor, oldCursor, blankOld;
static bool blankActive;
static BYTE nativeMaskBits[8192];
static LONG nativeLastSeq=-1, nativeShowBalance, menuShowBalance;
static bool menuCursorSet;
struct CursorEntry {HCURSOR handle;DWORD* pixels;nativecache::Tag tag;};
static CursorEntry cursorCache[nativecache::Slots];
static nativecache::Usage cursorUsage[nativecache::Slots];
static uint32_t nativeScratch[NATIVE_MAX_DIM*NATIVE_MAX_DIM];
static uint32_t cursorClock,cursorBytes;
static DWORD lastOwnerCheck,lastExtentCheck;
static LONG sampledExtent[2];
static int cachedSlot(HCURSOR handle){for(int i=0;i<nativecache::Slots;i++)if(cursorCache[i].handle==handle&&handle)return i;return -1;}
static void evictCursor(int i){
 auto& e=cursorCache[i];if(e.handle)DestroyCursor(e.handle);
 if(e.pixels){cursorBytes-=uint32_t(e.tag.w*e.tag.h*4);HeapFree(GetProcessHeap(),0,e.pixels);}
 e.handle=0;e.pixels=0;cursorUsage[i].occupied=false;
}
static void clearCursorCache(){for(int i=0;i<nativecache::Slots;i++)evictCursor(i);cursorClock=cursorBytes=0;}

static bool read(DWORD p,void* out,SIZE_T n){
 if(!p||p<0x10000||n>8192||p>0x7FFFFFFF-n)return false;
 SIZE_T got=0;
 // ReadProcessMemory on GetCurrentProcess is an in-game read. No external
 // VM_READ privilege is needed and an invalid pointer is rejected safely.
 return ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)p,out,n,&got)&&got==n;
}
static DWORD at32(const BYTE* p){return DWORD(p[0])|(DWORD(p[1])<<8)|(DWORD(p[2])<<16)|(DWORD(p[3])<<24);}
static bool readLarge(DWORD p,void* out,unsigned n){
 if(n>RESOURCE_MAX_BYTES||uint64_t(p)+n>=0x80000000ull)return false;
 auto b=(BYTE*)out;for(unsigned i=0;i<n;){unsigned block=n-i;if(block>8192)block=8192;
  if(!read(p+i,b+i,block))return false;i+=block;}return true;
}
static BYTE resourceHeaders[8+cursorresource::MaxFrames*24];
static BYTE archivePixels[RESOURCE_MAX_BYTES];
static bool valid(void* p);
static bool nativeResource(DWORD& image,unsigned& length,unsigned& frames){
 void* p=0;DWORD ref=0;BYTE h[8];unsigned w=0,height=0;
 if(!read(0x887640,&p,4)||!valid(p)||!read((DWORD)p+4,&ref,4)||!read(ref,h,8))return false;
 if(cursorresource::u16(h)==0xffff){if(!read(ref+12,&ref,4))return false;}
 image=ref;return cursorresource::extent(image,readLarge,resourceHeaders,length,frames,w,height);
}
static bool valid(void* p){DWORD vt=0,slots[5]={};return p&&read((DWORD)p,&vt,4)&&read(vt,slots,sizeof(slots))&&slots[1]==0x7B8A00&&slots[3]==0x7B9930&&slots[4]==0x7B9750;}
// MixFileClass::Offset bypasses loose mouse.sha and searches loaded archives.
// ABI and prologue verified against the supported game EXE / YRpp. Used only
// once, to recover YR art if an old 450-frame/transparent loose file is loaded.
static bool archiveResource(unsigned& length){
 if(!gameentry::active(gameentry::Client::Cursor))return false;
 const BYTE signature[]={0x81,0xEC,0x18,0x01,0x00,0x00,0x53,0x33,0xDB};BYTE actual[9];
 if(!read(0x5B4430,actual,9))return false;for(int i=0;i<9;++i)if(actual[i]!=signature[i])return false;
 void* data=0;DWORD mix=0;int offset=0,bytes=0;
 typedef bool(__fastcall* Offset)(const char*,void*&,DWORD&,int&,int&);
 if(!((Offset)0x5B4430)("mouse.sha",data,mix,offset,bytes)||bytes<8||bytes>int(RESOURCE_MAX_BYTES)||offset<0)return false;
 if(data){if(!readLarge((DWORD)data,archivePixels,bytes))return false;}
 else {
  DWORD filename=0;if(!read(mix+0xC,&filename,4))return false;
  char name[260];unsigned i=0;for(;i<259;++i){if(!read(filename+i,name+i,1))return false;if(!name[i])break;}if(i==259)return false;
  __declspec(align(16)) DWORD object[64]={};
  typedef void(__thiscall* Construct)(void*,const char*);
  ((Construct)0x4739F0)(object,name);
  DWORD* vt=(DWORD*)(uintptr_t)object[0];
  typedef bool(__thiscall* Open)(void*,unsigned);
  typedef int(__thiscall* Seek)(void*,int,unsigned);
  typedef int(__thiscall* Read)(void*,void*,int);
  typedef void(__thiscall* Destruct)(void*,unsigned);
  bool ok=((Open)vt[7])(object,1)&&((Seek)vt[10])(object,offset,0)==offset&&((Read)vt[9])(object,archivePixels,bytes)==bytes;
  ((Destruct)vt[0])(object,0);if(!ok)return false;
 }
 if(cursorresource::u16(archivePixels)!=0||cursorresource::u16(archivePixels+2)!=55||cursorresource::u16(archivePixels+4)!=43||cursorresource::u16(archivePixels+6)<517||cursorresource::u16(archivePixels+6)>4096)return false;
 for(int i=0;i<bytes;++i)s->resourcePixels[i]=archivePixels[i];
 length=unsigned(bytes);return true;
}
extern "C" void CaptureCursorResource(){
 if(!s)return;s->resourceState=1;InterlockedIncrement(&s->resourceSeq);
 DWORD image=0;unsigned length=0,frames=0;bool ok=nativeResource(image,length,frames);
 if(ok)ok=readLarge(image,s->resourcePixels,length);
 // A native YR SHA is authoritative. Older loose files use archive art.
 bool visible=false;
 if(ok)for(unsigned i=0;i<frames&&!visible;++i){auto f=s->resourcePixels+8+i*24;unsigned off=cursorresource::u32(f+20),fmt=cursorresource::u32(f+8)&255,area=cursorresource::u16(f+4)*cursorresource::u16(f+6);
  if(!off||!area)continue;if(fmt>=2){visible=true;break;}
  if(uint64_t(off)+area<=length)for(unsigned p=0;p<area;++p)if(s->resourcePixels[off+p]){visible=true;break;}
 }
 if(!ok||!visible||frames<517){unsigned archived=0;if(archiveResource(archived)){ok=true;length=archived;}}
 DWORD convert=0,bpp=0,colors=0;BYTE rgb[512],shiftBytes[24];
 if(ok)ok=read(0x87F6C8,&convert,4)&&read(convert+4,&bpp,4)&&bpp==2&&read(convert+0x174,&colors,4)&&read(colors,rgb,512)&&read(0x8A0DD0,shiftBytes,24);
 if(ok){int rl=at32(shiftBytes),rr=at32(shiftBytes+4),bl=at32(shiftBytes+8),br=at32(shiftBytes+12),gl=at32(shiftBytes+16),gr=at32(shiftBytes+20);
  ok=(rl==11&&rr==3&&gl==5&&gr==2&&bl==0&&br==3)||(rl==10&&rr==3&&gl==5&&gr==3&&bl==0&&br==3);
  if(ok)for(int i=0;i<256;++i){unsigned v=cursorresource::u16(rgb+i*2);s->resourcePalette[i]=i?0xFF000000|((((v>>rl)<<rr)&255)<<16)|((((v>>gl)<<gr)&255)<<8)|(((v>>bl)<<br)&255):0;}
 }
 s->resourceBytes=ok?length:0;MemoryBarrier();InterlockedIncrement(&s->resourceSeq);s->resourceState=ok?2:-1;
}
extern "C" void CursorResourceFault(){if(s){s->resourceBytes=0;if(s->resourceSeq&1)InterlockedIncrement(&s->resourceSeq);s->resourceState=-2;}}
extern "C" void GuardedCursorResource();
static void invoke(void* p,DWORD fn){if(!gameentry::active(gameentry::Client::Cursor))return;typedef void (__thiscall* F)(void*);((F)fn)(p);}
static bool current(void** p,LONG* ref){
 *p=0;*ref=0;
 return read(0x887640,p,4)&&valid(*p)&&read((DWORD)(*p)+12,ref,4);
}
enum class HeldMouseState { Unavailable, Replaced, Current };
static HeldMouseState heldMouse(void*&p,LONG*ref=nullptr){
 p=nullptr;if(!read(0x887640,&p,4))return HeldMouseState::Unavailable;
 if(p!=held)return HeldMouseState::Replaced;
 DWORD vt=0,slots[5]{};
 if(!read(DWORD(uintptr_t(p)),&vt,sizeof vt))return HeldMouseState::Unavailable;
 if(vt!=heldVtable||vt<0x10000)return HeldMouseState::Replaced;
 if(!read(vt,slots,sizeof slots))return HeldMouseState::Unavailable;
 if(slots[1]!=0x7B8A00||slots[3]!=0x7B9930||slots[4]!=0x7B9750)return HeldMouseState::Replaced;
 if(ref&&!read(DWORD(uintptr_t(p))+12,ref,sizeof *ref))return HeldMouseState::Unavailable;
 return HeldMouseState::Current;
}
extern "C" bool CursorTooltipNeeded(){return held&&s&&s->state==1;}
extern "C" void CursorTooltipStatus(int status){if(s)s->tooltipState=status;}
extern "C" bool CursorTooltipOwnsHiddenMouse(){
 if(!CursorTooltipNeeded())return false;
 gameentry::Request lease{};if(!gameentry::capture(gameentry::Client::Cursor,lease))return false;
 void* p=nullptr;LONG ref=0;DWORD vt=0,getter=0;
 // -1 + our one held HideCursor = native visible count 0. Leave additional
 // game hides (< -1), replacement objects and foreign getters untouched.
 bool ours=false;
 runtimeunwind::run([&]{ours=current(&p,&ref)&&p==held&&ref==-1&&
  read(DWORD(uintptr_t(p)),&vt,4)&&read(vt+0x28,&getter,4)&&getter==0x7B89F0;},[&]{gameentry::leave(lease);});
 return ours;
}
// Return Windows cursor control to its prior owner. Balance only our own
// ShowCursor increments; do not unconditionally toggle visibility on detach.
static void nativeRelease(){
 if(blankActive){if(!GetCursor())SetCursor(blankOld);blankOld=0;blankActive=false;}
 if(nativeCursor){
  if(GetCursor()==nativeCursor)SetCursor(oldCursor);
  nativeCursor=0;
 }
 if(cursorBytes)clearCursorCache();
 for(int n=0;n<nativeShowBalance;n++)ShowCursor(FALSE);
 nativeShowBalance=0;oldCursor=0;nativeLastSeq=-1;
 if(s){s->nativeVisible=0;s->nativeAck=0;}
}
static void menuBalanceRelease(){
 for(int n=0;n<menuShowBalance;n++)ShowCursor(FALSE);
 menuShowBalance=0;menuCursorSet=false;
}
static void nativeMenuRefresh(){
 nativeRelease();
 if(!menuCursorSet){SetCursor(LoadCursorW(NULL,IDC_ARROW));menuCursorSet=true;}
 CURSORINFO ci={sizeof(ci)};
 if(GetCursorInfo(&ci)&&!(ci.flags&CURSOR_SHOWING)){
  for(int n=0;n<8&&menuShowBalance<16;n++){
   ++menuShowBalance;
   if(ShowCursor(TRUE)>=0)break;
  }
 }
 if(s)s->nativeVisible=0;
}
// Create an alpha-aware Windows hardware cursor from a complete, stable frame.
// This is executed in the game GUI thread (the same thread as CursorHook).
static HCURSOR nativeBuild(LONG* accepted){
 if(!s)return 0;
 LONG seq=s->nativeSeq;if(!cursorseq::committed(uint32_t(seq)))return 0;
 int w=s->nativeW,h=s->nativeH,hx=s->nativeHotX,hy=s->nativeHotY;
 if(w<1||h<1||w>NATIVE_MAX_DIM||h>NATIVE_MAX_DIM||hx<0||hy<0||hx>=w||hy>=h)return 0;
 unsigned count=unsigned(w*h);for(unsigned i=0;i<count;i++)nativeScratch[i]=s->nativePixels[i];
 MemoryBarrier();if(s->nativeSeq!=seq)return 0;
 nativecache::Tag tag{w,h,hx,hy,nativecache::hash(nativeScratch,count)};
 ++cursorClock;
 for(int i=0;i<nativecache::Slots;i++)if(cursorCache[i].handle&&nativecache::same(cursorCache[i].tag,tag)){
  bool equal=true;for(unsigned k=0;k<count;k++)if(cursorCache[i].pixels[k]!=nativeScratch[k]){equal=false;break;}
  if(equal){InterlockedIncrement(&s->nativeCacheHits);cursorUsage[i].used=cursorClock;*accepted=seq;return cursorCache[i].handle;}
 }
 int protectedSlot=cachedSlot(nativeCursor);
 unsigned bytes=count*4;
 while(cursorBytes+bytes>nativecache::PixelBudget){
  int slot=nativecache::victim(cursorUsage,protectedSlot,cursorClock);
  // Empty slots are preferred for insertion; for pressure reclamation find
  // the oldest occupied slot instead. Never destroy the current cursor.
  if(slot<0||!cursorUsage[slot].occupied){
   slot=-1;uint32_t age=0;
   for(int i=0;i<nativecache::Slots;i++)if(i!=protectedSlot&&cursorUsage[i].occupied){uint32_t a=cursorClock-cursorUsage[i].used;if(slot<0||a>age){slot=i;age=a;}}
  }
  if(slot<0)return 0;evictCursor(slot);
 }
 int slot=nativecache::victim(cursorUsage,protectedSlot,cursorClock);if(slot<0)return 0;
 evictCursor(slot);
 DWORD* copy=(DWORD*)HeapAlloc(GetProcessHeap(),0,bytes);if(!copy)return 0;
 for(unsigned i=0;i<count;i++)copy[i]=nativeScratch[i];
 BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
 bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void* dest=0;HBITMAP color=CreateDIBSection(0,&bi,DIB_RGB_COLORS,&dest,0,0);
 if(!color||!dest){if(color)DeleteObject(color);HeapFree(GetProcessHeap(),0,copy);return 0;}
 for(unsigned i=0;i<count;i++)((DWORD*)dest)[i]=copy[i];
 HBITMAP mask=CreateBitmap(w,h,1,1,nativeMaskBits);
 if(!mask){DeleteObject(color);HeapFree(GetProcessHeap(),0,copy);return 0;}
 ICONINFO ii={};ii.fIcon=FALSE;ii.xHotspot=hx;ii.yHotspot=hy;ii.hbmColor=color;ii.hbmMask=mask;
 HCURSOR cursor=CreateIconIndirect(&ii);DeleteObject(color);DeleteObject(mask);
 if(!cursor){HeapFree(GetProcessHeap(),0,copy);return 0;}
 InterlockedIncrement(&s->nativeCacheMisses);cursorCache[slot]={cursor,copy,tag};cursorUsage[slot]={true,cursorClock};cursorBytes+=bytes;
 *accepted=seq;return cursor;
}
static void nativeRefresh(){
 if(!s)return;
 if(s->menuSuspend==2){nativeRelease();menuBalanceRelease();return;}
 if(s->nativeBlank&&held&&s->state==1){
  if(!blankActive){nativeRelease();blankOld=GetCursor();blankActive=true;}
  SetCursor(NULL);s->nativeVisible=0;return;
 }
 if(s->menuSuspend&&held&&s->state==1){nativeMenuRefresh();return;}
 if(menuCursorSet)menuBalanceRelease();
 if(!s->nativeEnabled||!held||s->state!=1){nativeRelease();return;}
 LONG seq=s->nativeSeq;
 if(cursorseq::committed(uint32_t(seq))&&seq!=nativeLastSeq){
  LONG accepted=0;HCURSOR c=nativeBuild(&accepted);
  if(c){
   HCURSOR previous=SetCursor(c);
   if(!nativeCursor)oldCursor=previous;
   nativeCursor=c;nativeLastSeq=accepted;
   s->nativeAck=accepted;s->nativeVisible=0;
  }
 }
 if(nativeCursor){
  if(GetCursor()!=nativeCursor)SetCursor(nativeCursor);
  // Some game builds hide the actual Windows pointer too; restore visibility
  // only as needed, and undo exactly our own increments on disable.
  CURSORINFO ci={sizeof(ci)};bool got=GetCursorInfo(&ci)!=FALSE;
  if(got&&!(ci.flags&CURSOR_SHOWING)){
   for(int n=0;n<8&&nativeShowBalance<16;n++){
    ++nativeShowBalance;if(ShowCursor(TRUE)>=0)break;
   }
   got=GetCursorInfo(&ci)!=FALSE;
  }
  s->nativeVisible=got&&(ci.flags&CURSOR_SHOWING)&&ci.hCursor==nativeCursor?1:0;
 }
}
// Read cursor frame in the game process, publish a snapshot to the controller.
// Avoids an external ReadProcessMemory dependency where the platform restricts it.
static void publishSample(){
 if(!s||!held||s->state!=1)return;
 InterlockedIncrement(&s->sampleAttempts);
 void* p=0;BYTE state[0x44];DWORD slots[5];
 if(!read(0x887640,&p,4)||!p||!read((DWORD)p,state,sizeof(state))){s->sampleReadError=1;return;}
 if(p!=held){s->sampleReadError=2;return;}
 DWORD vt=at32(state);
 if(!read(vt,slots,sizeof(slots))||slots[1]!=0x7B8A00||slots[3]!=0x7B9930||slots[4]!=0x7B9750){s->sampleReadError=1;return;}
 DWORD frame=at32(state+8);const int frameLimit=s->cursorFrameCount;
 if(frameLimit<1||frameLimit>4096||frame>=DWORD(frameLimit)){s->sampleReadError=3;return;}
 LONG cursorID=-1;
 LONG first=-1,count=0;DWORD definitionState[2]={};
 if(read(0x87F7E8+0x555C,definitionState,sizeof(definitionState))&&definitionState[1]<86&&(definitionState[0]&255)<=1){
  DWORD definition[7]={};
  if(read(0x82D028+definitionState[1]*28,definition,sizeof(definition))){
   int mini=int(definitionState[0]&255);DWORD f=definition[mini?3:0],n=definition[mini?4:1];
   if(n>0&&n<=DWORD(frameLimit)&&f<DWORD(frameLimit)&&uint64_t(f)+n<=DWORD(frameLimit)&&frame>=f&&frame<f+n){first=(LONG)f;count=(LONG)n;cursorID=LONG(definitionState[1]);}
  }
 }
 // The bulk object read includes the same full-canvas anchor fields.
 LONG hot[2]={LONG(at32(state+0x3C)),LONG(at32(state+0x40))};
 // The legacy game can publish symbolic hotspots. Normalize those in its
 // own thread using canvas size supplied by the original asset.
 const int cw=s->cursorCanvasW,ch=s->cursorCanvasH;
 if(cw>0&&cw<=512&&ch>0&&ch<=512){
  if(hot[0]==12345)hot[0]=cw/2;else if(hot[0]==54321)hot[0]=cw-1;
  if(hot[1]==12345)hot[1]=ch/2;else if(hot[1]==54321)hot[1]=ch-1;
 }
 DWORD now=GetTickCount();
 if(!lastExtentCheck||DWORD(now-lastExtentCheck)>=250){
  lastExtentCheck=now;DWORD surface=0;sampledExtent[0]=sampledExtent[1]=0;
  if(read(0x887308,&surface,4))read(surface+4,sampledExtent,sizeof(sampledExtent));
  if(sampledExtent[0]<320||sampledExtent[1]<200||sampledExtent[0]>16384||sampledExtent[1]>16384)sampledExtent[0]=sampledExtent[1]=0;
 }
 LONG* wh=sampledExtent;
 InterlockedIncrement(&s->sampleSeq); // odd: snapshot in progress
 s->sampledFirst=first;s->sampledCount=count;
 s->sampledCursorID=cursorID;
 s->sampledHotX=hot[0];s->sampledHotY=hot[1];
 s->sampledGameW=wh[0];s->sampledGameH=wh[1];
 s->sampledFrame=(LONG)frame;s->sampledTick=GetTickCount();
 MemoryBarrier();InterlockedIncrement(&s->sampleSeq);
 InterlockedIncrement(&s->sampleSuccess);s->sampleReadError=0;

}
static bool restore(){
 nativeRelease();menuBalanceRelease();
 if(!held)return true;
 void* cur=nullptr;const auto identity=heldMouse(cur);
 // A failed read does not retire our one held Hide. Keep the old identity
 // and mapping until a later game-thread watchdog can prove what happened.
 if(identity==HeldMouseState::Unavailable)return false;
 if(identity==HeldMouseState::Current)invoke(cur,0x7B9750);
 // A confirmed replacement is no longer our mouse; never call its object
 // or an old vtable merely to balance a reference from a retired lifetime.
 held=nullptr;heldVtable=0;
 if(s){s->observedRef=0;s->hiddenRef=0;}
 return true;
}
static void invalidate(LONG error){
 bool restored=restore();
 if(s){s->error=restored?error:4;if(restored){s->state=-1;s->command=0;}else s->command=2;}
}
static bool initialHide(){
 void* p=0;LONG before=0,after=0;
 if(!current(&p,&before)) {s->error=1;return false;}
 if(before<0||before>hidewatch::maxReference){s->error=2;return false;}
 DWORD vt=0;if(!read(DWORD(uintptr_t(p)),&vt,sizeof vt)){s->error=1;return false;}
 held=p;heldVtable=vt;
 invoke(p,0x7B9930);
 if(!read((DWORD)p+12,&after,4)||!hidewatch::oneHideWasPaired(before,after)){
  // If an unexpected implementation is encountered, undo only our call.
  invalidate(6);return false;
 }
 hiddenRef=after;lastInspectAt=0;lastExtentCheck=0;rightWasDown=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;rightReleased=false;
 s->observedRef=after;s->hiddenRef=hiddenRef;s->rehideCount=0;s->lastRehideReason=0;s->sampledFrame=-1;s->sampledFirst=-1;s->sampledCount=0;s->sampledTick=0;
 s->error=0;s->state=1;return true;
}
static void inspect(bool release){
 if(!held||!s||s->state!=1)return;
 void* p=0;LONG ref=0;
 const auto identity=heldMouse(p,&ref);
 if(identity==HeldMouseState::Unavailable)return;
 if(identity==HeldMouseState::Replaced){invalidate(4);return;}
 s->observedRef=ref;
 int32_t pending=hidewatch::required(ref,hiddenRef);
 if(pending<0){invalidate(7);return;}
 if(!pending)return;
 // Recheck on a right release OR after a normal 100ms poll. Do not
 // blindly call HideCursor every tick: that would drift the game's count.
 for(int i=0;i<pending;i++){
  LONG before=ref;
  invoke(p,0x7B9930);
  if(!read((DWORD)p+12,&ref,4)||!hidewatch::oneHideWasPaired(before,ref)){
   invalidate(6);return;
  }
  ++s->rehideCount;
 }
 s->observedRef=ref;
 s->lastRehideReason=release?2:1;
}
static void tick(){
 if(!s)return;
 // Commands restore/hide immediately. Routine sampling is capped at 250 Hz,
 // independently of PeekMessage/GetMessage traffic and timer callbacks.
 DWORD stamp=GetTickCount();
 unsigned interval=s->sampleIntervalMs>=4&&s->sampleIntervalMs<=33?unsigned(s->sampleIntervalMs):4;
 if(!cursorperf::sampleDue(stamp,lastSampleAt,s->command!=0,interval))return;
 lastSampleAt=stamp;
 if(s->owner!=ownerId){
  if(owner)CloseHandle(owner);
  owner=OpenProcess(SYNCHRONIZE,FALSE,s->owner);ownerId=s->owner;lastOwnerCheck=0;
 }
 if(!lastOwnerCheck||DWORD(stamp-lastOwnerCheck)>=100||s->command){
  lastOwnerCheck=stamp;
  if(!owner&&ownerId)owner=OpenProcess(SYNCHRONIZE,FALSE,ownerId);
  // Only the retained process object's exit proves controller death. A
  // stalled heartbeat or failed query pauses work without losing our hide.
  if(owner&&WaitForSingleObject(owner,0)==WAIT_OBJECT_0){
   bool ok=restore();s->error=ok?0:4;if(ok)s->state=0;s->command=ok?0:2;return;
  }
 }
 // Explicit stop must remain serviceable while paused or owner liveness is
 // unknown; other requests stay pending until the live controller resumes.
 if(s->command!=2&&(!owner||WaitForSingleObject(owner,0)!=WAIT_TIMEOUT||DWORD(stamp-s->heartbeat)>5000))return;
 LONG cmd=InterlockedExchange(&s->command,0);
 if(cmd==2){bool ok=restore();s->error=ok?0:4;if(ok)s->state=0;else s->command=2;return;}
 if(InterlockedExchange(&s->resourceRequest,0))GuardedCursorResource();
 if(cmd==3){
  // A resource-only restart may reuse a mapping whose prior hide still
  // awaits restoration. It cannot acknowledge idle while owning that hide.
  bool ok=restore();s->error=ok?0:4;s->state=ok?0:2;if(!ok)s->command=2;return;
 }
 if(cmd==1){if(held){s->state=1;}else if(!initialHide()&&s->state!=-1){
  // The Hide call can succeed before its confirmation read fails. Until
  // the paired restore is resolved, do not publish a detached/error ACK.
  s->state=held?2:-1;
 }return;}
 if(!held)return;
 if(s->menuSuspend==2){nativeRefresh();return;} // retain Hook/hide; suspend presentation and game sampling
 DWORD now=GetTickCount();
 bool pressed=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
 bool released=rightReleased||(rightWasDown&&!pressed);
 rightReleased=false;rightWasDown=pressed;
 if(released||DWORD(now-lastInspectAt)>=100){lastInspectAt=now;inspect(released);}
 // Native presentation and game-thread samples refresh independently of the
 // game's original 16fps software animation cadence.
 if(s->state==1){publishSample();nativeRefresh();}
}
static void disconnectCursor(){
 if(s)UnmapViewOfFile(s);s=nullptr;
 if(mapping)CloseHandle(mapping);mapping=nullptr;
 if(owner)CloseHandle(owner);owner=nullptr;ownerId=0;
}
extern "C" void CursorRuntimeMessage(int code,WPARAM wp,LPARAM lp){
 if(code<0)return;
 gameentry::scoped(gameentry::Client::Cursor,false,[&]{
 if(!s){
  wchar_t name[80];mapName(name,GetCurrentProcessId());
  mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
  if(!mapping)return;
  s=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));
  if(!s||s->magic!=MAGIC||s->pid!=GetCurrentProcessId()){disconnectCursor();return;}
  owner=OpenProcess(SYNCHRONIZE,FALSE,s->owner);ownerId=s->owner;
  if(!owner){s->error=3;s->state=-1;return;}
 }
 if(lp&&wp==PM_REMOVE){const MSG* msg=(const MSG*)lp;
  if(msg->message==WM_RBUTTONUP||msg->message==WM_NCRBUTTONUP)rightReleased=true;
 }
 const bool stopping=s->command==2;
 tick();
 // Stop ACK is published only after restore(). Release this mapping so a
 // subsequent session cannot inherit the old native cursor/cache state.
 if(stopping&&!held&&s->state==0&&!s->error)disconnectCursor();
 });
}
extern "C" void CursorRuntimeWatchdog(){
 if(!s)return;
 const bool dead=owner&&WaitForSingleObject(owner,0)==WAIT_OBJECT_0;
 if(s->command!=2&&!dead)return;
 gameentry::scoped(gameentry::Client::Cursor,false,[&]{
 bool ok=restore();s->error=ok?0:4;if(ok)s->state=0;s->command=ok?0:2;
 if(ok)disconnectCursor();
 });
}
// WH_CALLWNDPROCRET is called AFTER the game has processed WM_SETCURSOR.
// The external scheduling pulse alone cannot prevent Windows' default arrow flashing
// between a game SetCursor call and the next polling tick. This companion
// hook reapplies our existing cursor with no allocation and no game-code patch.
static bool insideGameWindow(HWND w){
 if(!s||!s->gameWindow||!IsWindow(w))return false;
 HWND renderer=(HWND)(uintptr_t)s->gameWindow;
 return IsWindow(renderer)&&GetAncestor(w,GA_ROOT)==GetAncestor(renderer,GA_ROOT);
}
extern "C" bool CursorRuntimeDetached(){return s==nullptr;}
static bool isDialogMessage(HWND w){
 if(!w)return false;
 HWND root=GetAncestor(w,GA_ROOT);
 if(!IsWindowVisible(root))return false;
 wchar_t cls[40];
 // GetClassNameW writes a terminator on success; a failed call must not
 // leave cls uninitialized for the comparison. Avoid array zero-fill.
 if(GetClassNameW(root,cls,40)==0)return false;
 return lstrcmpW(cls,L"#32770")==0;
}
extern "C" void CursorRuntimePost(int code,WPARAM wp,LPARAM lp){
 __declspec(align(16)) BYTE fx[512];
 __asm {fxsave fx}
 runtimeunwind::run([&]{
 if(code>=0&&s&&lp&&s->menuSuspend!=2){
  gameentry::scoped(gameentry::Client::Cursor,false,[&]{
  const CWPRETSTRUCT* ev=(const CWPRETSTRUCT*)lp;
  if(ev->message==WM_SETCURSOR){
   if(s->nativeBlank&&held&&s->state==1&&insideGameWindow(ev->hwnd)){
    SetCursor(NULL);
   }else if(isDialogMessage(ev->hwnd)&&(s->nativeEnabled||s->menuSuspend)){
    // Let the real Windows dialog control its normal pointer.
    s->menuSuspend=1;nativeMenuRefresh();
   }else if(s->nativeEnabled&&!s->menuSuspend&&held&&s->state==1&&
            nativeCursor&&insideGameWindow(ev->hwnd)&&
            LOWORD(ev->lParam)==HTCLIENT){
    if(GetCursor()!=nativeCursor){SetCursor(nativeCursor);InterlockedIncrement(&s->postSetCursorRepairs);}
   }
  }
  });
 }
 },[&]{auto* state=fx;__asm {mov eax,state
 fxrstor [eax]}});

}

// Executes production helpers plus extracted owner-thread coordinate routes.
// Win32 is controlled; no native Windows compositor or game is exercised.
#include "../../cursor/source/cursor_coordinates_windows.h"
#include "../../cursor/source/cursor_presentation.h"
#include "../../cursor/source/cursor_presentation_route.h"
#include "../../cursor/source/sequence.h"
#include "../../cursor/source/scale.h"
#include <array>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <cmath>
using namespace cursorcoordinates;
static intptr_t context=-1;static bool allowPM=true,exportsContext=true,exportsPhysical=true;
static bool cursorOK=true,clientOK=true,secondCornerOK=true,createOK=true,clipOK=true,monitorOK=true,placeOK=true;static double dpiFactor=1.5;
static POINT physicalPoint{3000,1000};static Rect physicalClient{0,0,3840,2160};
static Rect physicalMonitor{0,0,3840,2160},physicalClip{0,0,3840,2160};
static bool surfaceOK=false,observedMapPoint=false;static int surfaceW=2560,surfaceH=1440,observedMapWidth=0,observedMapHeight=0;
static DWORD clockNow=10000,lastError=0;static unsigned submissions=0,placements=0,leaves=0,creations=0,destroyed=0;
static POINT submitted{},placed{};static intptr_t submittedContext=0,placedContext=0;
static std::vector<std::wstring> messages,automaticMessages;
struct Window {RECT rect{};intptr_t context=0;bool visible=false;};static std::map<HWND,Window> windows;
static HWND game=(HWND)1,mainw=(HWND)2,overlay=nullptr,hud=nullptr;static DWORD pid=42;
static bool running=true,nativeGameRefresh=false,noCursorMode=false,aspectFit=true,autoScale=true,mappedCursorPresentation=false,autoPaused=false;
static int scalePercent=100,dpiMode=0,dpiOverride=0,positionMode=0,gameWOverride=0,gameHOverride=0,clickGameWidth=2560,clickGameHeight=1440,sidebarWidth=160,sidebarTop=200;
static int lastFrame=-2;static size_t lastPhase=0;static DWORD hudPaintAt=0,regionTime=0;
static RECT clickRegion{};static std::wstring mode=L"hide";
static Domain overlayCoordinateDomain=Domain::CallerLogical;static HANDLE overlayCoordinateContext=nullptr;
static int shx=15,shy=12,dw=83,dh=65;static HDC memdc=nullptr;
struct ResetProbe{void clear(){}void reset(){}void resetObservation(){}void cancel(){}};
static ResetProbe bridgeFrame,lanboStream,fallbackTracker,tween,timing,activeMotion,burst;
constexpr int NATIVE_MAX_DIM=256;
struct SharedProbe{
 LONG menuSuspend=0,state=1,nativeEnabled=0,nativeBlank=0,nativeVisible=0,nativeSeq=0,nativeAck=0;
 LONG sampleSeq=2,sampledGameW=2560,sampledGameH=1440;
 LONG nativeW=0,nativeH=0,nativeHotX=0,nativeHotY=0,postSetCursorRepairs=0;
 DWORD gameWindow=0,nativePixels[NATIVE_MAX_DIM*NATIVE_MAX_DIM]{};
};static SharedProbe runtimeState;static SharedProbe* shared=&runtimeState;
static HANDLE hook=(HANDLE)9;
static bool pairedHide=true,manualCursorPause=false,wasInside=true,usingHeldSample=false,nativeMenuHold=false;
static DWORD nativeInvisibleAt=0,nativeMenuCandidateAt=0,refreshAt=0,nativeStartAt=0,lastNativePublishAt=0;
static unsigned pollHz=250,wakes=0,clipCalls=0;
static bool dialogActive=false,hardwareCursorPublished=false;
static std::wstring diagAnimation,statusText;
static int previousObservedFrame=-1,refreshTries=0,hudLastFrame=-1;
static unsigned hookStarts=0,hookStops=0,hudUpdates=0,hudCommits=0,hudMotion=0,hudChanges=0;
static uint64_t hudEpoch=0;
static bool hideHeldWhenLeaving=false;
static bool pm(){return context==-3||context==-4;}
static LONG logical(LONG value){return LONG(std::lround(value/(pm()?1:dpiFactor)));}
HMODULE GetModuleHandleW(const wchar_t*){return (HMODULE)7;}
static HANDLE setContext(HANDLE value){const auto next=intptr_t(value);if((next==-3||next==-4)&&!allowPM){lastError=5;return nullptr;}const auto old=context;context=next;return (HANDLE)old;}
static HANDLE getContext(){return (HANDLE)context;}
static int awareness(HANDLE value){return intptr_t(value)==-3||intptr_t(value)==-4?2:intptr_t(value)==-2?1:0;}
static BOOL equalContexts(HANDLE a,HANDLE b){return a==b;}
static BOOL physicalCursor(POINT* out){*out=physicalPoint;if(!cursorOK){lastError=5;return FALSE;}return TRUE;}
void* GetProcAddress(HMODULE,const char* name){
 if(!std::strcmp(name,"GetPhysicalCursorPos"))return exportsPhysical?reinterpret_cast<void*>(physicalCursor):nullptr;
 if(!exportsContext)return nullptr;
 if(!std::strcmp(name,"SetThreadDpiAwarenessContext"))return reinterpret_cast<void*>(setContext);
 if(!std::strcmp(name,"GetThreadDpiAwarenessContext"))return reinterpret_cast<void*>(getContext);
 if(!std::strcmp(name,"GetAwarenessFromDpiAwarenessContext"))return reinterpret_cast<void*>(awareness);
 if(!std::strcmp(name,"AreDpiAwarenessContextsEqual"))return reinterpret_cast<void*>(equalContexts);
 return nullptr;
}
BOOL GetCursorPos(POINT* out){*out={logical(physicalPoint.x),logical(physicalPoint.y)};if(!cursorOK){lastError=5;return FALSE;}return TRUE;}
BOOL GetClientRect(HWND,RECT* out){if(!clientOK){lastError=6;return FALSE;}*out={0,0,logical(physicalClient.width()),logical(physicalClient.height())};return TRUE;}
BOOL ClientToScreen(HWND,POINT* point){if((point->x||point->y)&&!secondCornerOK){lastError=87;return FALSE;}point->x+=logical(physicalClient.left);point->y+=logical(physicalClient.top);return TRUE;}
BOOL GetClipCursor(RECT* out){++clipCalls;if(!clipOK){lastError=5;return FALSE;}*out={logical(physicalClip.left),logical(physicalClip.top),logical(physicalClip.right),logical(physicalClip.bottom)};return TRUE;}
DWORD GetLastError(){return lastError;}void SetLastError(DWORD value){lastError=value;}DWORD GetTickCount(){return clockNow;}
BOOL IsWindow(HWND w){return w!=nullptr;}BOOL IsWindowVisible(HWND w){return w==game||w==mainw||(windows.count(w)&&windows[w].visible);}BOOL IsIconic(HWND){return FALSE;}
BOOL ShowWindow(HWND value,int){if(windows.count(value))windows[value].visible=false;return TRUE;}
void SetRectEmpty(RECT* rect){*rect={};}
HWND CreateWindowExW(DWORD,const wchar_t*,const wchar_t*,DWORD,int x,int y,int w,int h,HWND,HANDLE,HINSTANCE,void*){++creations;if(!createOK){lastError=8;return nullptr;}HWND value=(HWND)uintptr_t(100+creations);windows[value]={{x,y,x+w,y+h},context,false};return value;}
BOOL DestroyWindow(HWND value){++destroyed;windows.erase(value);return TRUE;}BOOL SetLayeredWindowAttributes(HWND,DWORD,unsigned char,DWORD){return TRUE;}
BOOL GetWindowRect(HWND value,RECT* out){if(!windows.count(value))return FALSE;*out=windows[value].rect;return TRUE;}
BOOL UpdateLayeredWindow(HWND,HDC,const POINT* point,const SIZE*,HDC,const POINT*,DWORD,const BLENDFUNCTION*,DWORD){++submissions;submitted=*point;submittedContext=context;return TRUE;}
HMONITOR MonitorFromWindow(HWND,DWORD){return (HMONITOR)1;}BOOL GetMonitorInfoW(HMONITOR,MONITORINFO* out){if(!monitorOK){lastError=6;return FALSE;}out->rcMonitor={logical(physicalMonitor.left),logical(physicalMonitor.top),logical(physicalMonitor.right),logical(physicalMonitor.bottom)};return TRUE;}
namespace overlaystack {
enum class Layer{Cursor};static void forget(HWND){}
static BOOL place(HWND value,Layer,int x,int y,int,int,UINT){++placements;placed={x,y};placedContext=context;if(!placeOK){lastError=5;return FALSE;}windows[value].rect={x,y,x+dw,y+dh};windows[value].visible=true;return TRUE;}
}
namespace autolog {static void write(const std::wstring& text){automaticMessages.push_back(text);}}
namespace suite {static void publishHardwareCursor(bool active){hardwareCursorPublished=active;}}
static void log(const std::wstring& text){messages.push_back(text);}
static bool clickForegroundMatches(){return true;}
namespace gameinput {static void wake(HANDLE){++wakes;}}
static uint64_t inputNow(){return clockNow;}
static std::wstring widen(const char* text){return std::wstring(text,text+std::strlen(text));}
static bool gameMenuDialogActive(){return dialogActive;}
static void status(const std::wstring& text){statusText=text;}
static void fail(const char* text){throw std::runtime_error(text);}
using Bytes=std::vector<uint8_t>;
static uint32_t word(uint32_t address){if(!surfaceOK||address!=0x887308)throw std::runtime_error("surface unavailable");return 0x10000;}
static Bytes read(uint32_t address,size_t count){if(!surfaceOK||address!=0x10004||count!=8)throw std::runtime_error("surface extent unavailable");Bytes value(8);uint32_t width=surfaceW,height=surfaceH;std::memcpy(value.data(),&width,4);std::memcpy(value.data()+4,&height,4);return value;}
static uint32_t u32(const Bytes& bytes,size_t offset){uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);return value;}
// These are subscription boundary substitutes, not Runtime execution.
static void hookStop();static void hookStart();
static void stop(){running=false;}
static void visibilityDiagnostic(const wchar_t*,DWORD){}
#ifdef CURSOR_PRESENTATION
// Win32 cursor visibility and handles are controlled boundary state. The
// generated source below contains the real Runtime ownership/dispatch bodies.
static HCURSOR osCursor=(HCURSOR)501;static int osShowCount=0;
static unsigned cursorWrites=0,nativeBuilds=0;
HCURSOR GetCursor(){return osCursor;}
HCURSOR SetCursor(HCURSOR next){++cursorWrites;const auto previous=osCursor;osCursor=next;return previous;}
HCURSOR LoadCursorW(HINSTANCE,const wchar_t*){return (HCURSOR)601;}
BOOL GetCursorInfo(CURSORINFO* out){out->flags=osCursor&&osShowCount>=0?CURSOR_SHOWING:0;out->hCursor=osCursor;out->ptScreenPos=physicalPoint;return TRUE;}
int ShowCursor(BOOL show){return show?++osShowCount:--osShowCount;}
namespace cursor_runtime_fixture {
static SharedProbe* s=&runtimeState;static bool held=true,blankActive=false,menuCursorSet=false;
static HCURSOR nativeCursor=nullptr,oldCursor=nullptr,blankOld=nullptr;
static int nativeShowBalance=0,menuShowBalance=0;static LONG nativeLastSeq=-1;
static unsigned cursorBytes=0;static bool dialogMessage=false,insideWindow=true;
static void clearCursorCache(){cursorBytes=0;}
static HCURSOR nativeBuild(LONG* accepted){++nativeBuilds;*accepted=s->nativeSeq;return (HCURSOR)uintptr_t(1000+uint32_t(s->nativeSeq));}
static bool isDialogMessage(HWND){return dialogMessage;}
static bool insideGameWindow(HWND){return insideWindow;}
}
#endif
#include CURSOR_ROUTES
static void hookStop(){
 if(!shared&&!hook)return;++hookStops;pairedHide=false;
#ifdef CURSOR_PRESENTATION
 cursor_runtime_fixture::nativeRelease();cursor_runtime_fixture::menuBalanceRelease();
 cursor_runtime_fixture::held=false;cursor_runtime_fixture::s=nullptr;
#endif
 shared=nullptr;hook=nullptr;
}
static void hookStart(){
 ++hookStarts;pairedHide=true;runtimeState={};shared=&runtimeState;hook=(HANDLE)9;
#ifdef CURSOR_PRESENTATION
 productionStartPresentation();cursor_runtime_fixture::s=shared;cursor_runtime_fixture::held=true;
#endif
}
static unsigned checks=0,failures=0,cases=0;
static void check(bool yes,const char* label){++checks;if(!yes){++failures;std::cout<<"FAIL "<<label<<"\n";}}
static void reset(){
 clockNow+=20000;context=-1;allowPM=true;cursorOK=clientOK=secondCornerOK=createOK=clipOK=monitorOK=placeOK=true;dpiFactor=1.5;
 physicalPoint={3000,1000};physicalClient={0,0,3840,2160};submissions=placements=leaves=0;
 physicalMonitor=physicalClip={0,0,3840,2160};clipCalls=0;
 surfaceOK=observedMapPoint=false;surfaceW=2560;surfaceH=1440;observedMapWidth=observedMapHeight=0;
 submitted=placed={};submittedContext=placedContext=0;windows.clear();overlay=hud=nullptr;
 overlayCoordinateDomain=Domain::CallerLogical;overlayCoordinateContext=nullptr;
 nativeGameRefresh=noCursorMode=mappedCursorPresentation=autoPaused=false;mode=L"hide";aspectFit=true;gameWOverride=gameHOverride=positionMode=0;
 clickGameWidth=2560;clickGameHeight=1440;sidebarWidth=160;sidebarTop=200;lastFrame=-2;
 running=true;pairedHide=true;manualCursorPause=false;wasInside=true;runtimeState={};shared=&runtimeState;hook=(HANDLE)9;
 hookStarts=hookStops=0;hideHeldWhenLeaving=false;
 dialogActive=hardwareCursorPublished=false;wakes=0;nativeInvisibleAt=lastNativePublishAt=0;nativeStartAt=clockNow;statusText.clear();
#ifdef CURSOR_PRESENTATION
 osCursor=(HCURSOR)501;osShowCount=0;cursorWrites=nativeBuilds=0;
 cursor_runtime_fixture::s=&runtimeState;cursor_runtime_fixture::held=true;
 cursor_runtime_fixture::blankActive=cursor_runtime_fixture::menuCursorSet=false;
 cursor_runtime_fixture::nativeCursor=cursor_runtime_fixture::oldCursor=cursor_runtime_fixture::blankOld=nullptr;
 cursor_runtime_fixture::nativeShowBalance=cursor_runtime_fixture::menuShowBalance=0;
 cursor_runtime_fixture::nativeLastSeq=-1;cursor_runtime_fixture::cursorBytes=0;
 cursor_runtime_fixture::dialogMessage=false;cursor_runtime_fixture::insideWindow=true;
#endif
}
static void basicCases(){
 reset();++cases;productionDrawRoute();
 check(submissions==1&&submitted.x+shx==3000&&submitted.y+shy==1000,"physical-domain contract is enforced under mocked logical caller");
#ifndef CURSOR_BASELINE
 check(submittedContext==-4&&placedContext==-4&&context==-1,"reads and both submissions share PM context then restore caller");
#endif
 reset();++cases;cursorOK=false;productionDrawRoute();
 check(!submissions&&!placements&&leaves==1,"failed point never submits an image or position");
 reset();++cases;context=-4;secondCornerOK=false;productionDrawRoute();
 check(!submissions&&!placements,"failed second client corner never submits partial geometry");
 reset();++cases;context=-4;physicalClient={0,0,3840,2160};
 check(!productionMapPoint({100,1000},1920,1440),"pillarbox left black bar is not map content");
 check(!productionMapPoint({3200,1000},1920,1440),"pillarbox sidebar uses fitted content origin");
 check(!productionMapPoint({3750,1000},1920,1440),"pillarbox right black bar is not map content");
 check(productionMapPoint({3000,1000},1920,1440),"pillarbox battlefield remains eligible");
 reset();++cases;context=-4;
 check(!productionMapPoint({1000,100},2560,1080),"letterbox top bar is not map content");
 check(!productionMapPoint({1000,2100},2560,1080),"letterbox bottom bar is not map content");
 check(productionMapPoint({1000,1000},2560,1080),"letterbox battlefield remains eligible");
 reset();++cases;context=-4;productionDrawRoute();
 check(submissions==1&&submitted.x+shx==3000,"normal PMv2 point is not multiplied by 1.5");
}
#ifndef CURSOR_BASELINE
static void ownerRecoveryCases(){
 reset();++cases;createOK=false;productionDrawRoute();
 check(!pairedHide&&!hook&&!shared&&hookStops==1&&!hideHeldWhenLeaving&&!wasInside&&running,"failed external window releases paired Hide before leaving");
 clockNow+=1000;productionDrawRoute();
 check(hookStarts==0&&hookStops==1&&!submissions&&running,"persistent window failure never re-arms Hide or stops owner");
 clockNow+=1000;createOK=true;productionDrawRoute();
 check(pairedHide&&hookStarts==1&&submissions==1&&wasInside&&running,"window recovery re-arms Hide through existing re-entry branch");
 productionDrawRoute();check(hookStarts==1,"healthy following frames keep the recovered subscription");
 reset();++cases;productionDrawRoute();allowPM=false;submissions=placements=0;productionDrawRoute();
 check(!pairedHide&&!hook&&!shared&&hookStops==1&&!hideHeldWhenLeaving&&!submissions&&running,"unavailable physical domain releases paired Hide before leaving");
 allowPM=true;productionDrawRoute();
 check(pairedHide&&hookStarts==1&&submissions==1&&wasInside&&running,"domain recovery re-arms Hide through existing re-entry branch");
 reset();++cases;nativeGameRefresh=true;createOK=false;productionDrawRoute();
 check(pairedHide&&hookStops==0&&leaves==0,"native route retains its own cursor when external creation fails");
 reset();++cases;mode=L"keep";pairedHide=false;shared=nullptr;hook=nullptr;createOK=false;productionDrawRoute();
 check(hookStops==0&&!pairedHide&&running,"keep-original mode never releases a Hide subscription it does not own");
 reset();++cases;createOK=false;productionDrawRoute();manualCursorPause=true;clockNow+=1000;createOK=true;productionDrawRoute();
 check(!pairedHide&&hookStarts==0&&running,"recovery preserves explicit manual pause");
}
#endif
static void presentationGeometryCases(){
 reset();++cases;positionMode=2;physicalPoint={1280,720};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1920&&submitted.y+shy==1080,"explicit canvas reaches displayed game midpoint");
 check(physicalPoint.x==1280&&physicalPoint.y==720,"presentation never changes the supplied OS point");
 reset();++cases;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1920&&submitted.y+shy==1080,"automatic canvas clip reaches displayed game midpoint");
 reset();++cases;positionMode=1;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1280&&submitted.y+shy==720,"explicit screen selection prevents a second coordinate scale");
 reset();++cases;physicalPoint={1920,1080};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1920&&submitted.y+shy==1080,"normal full-output clip keeps the already-physical point");
 reset();++cases;physicalClip={0,0,2560,1440};physicalPoint={1280,720};clipOK=false;productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1280&&submitted.y+shy==720,"failed clip query cannot reuse a canvas transform");
 reset();++cases;physicalClient=physicalMonitor={-3840,120,0,2280};physicalClip={-3840,120,-1280,1560};physicalPoint={-2560,840};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==-1920&&submitted.y+shy==1200,"canvas mapping preserves negative and nonzero screen origins");
 reset();++cases;nativeGameRefresh=true;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 check(submissions==1&&submitted.x+shx==1920&&shared&&shared->nativeBlank&&!shared->nativeEnabled&&nativeGameRefresh,
       "mapped native preference uses external submission and preserves preference");
}
#ifdef CURSOR_PRESENTATION
static void presentationOwnerCases(){
 reset();++cases;nativeGameRefresh=true;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 check(mappedCursorPresentation&&!hardwareCursorPublished&&shared->nativeBlank&&!shared->nativeEnabled&&nativeGameRefresh,
       "effective mapped delivery blanks the OS cursor without changing saved native preference");
 cursor_runtime_fixture::nativeRefresh();
 check(!GetCursor()&&cursor_runtime_fixture::blankActive&&nativeBuilds==0,"mapped delivery enters actual Runtime blank ownership");
 const unsigned writes=cursorWrites;clockNow+=3000;productionDrawRoute();
 check(running&&!autoPaused&&nativeInvisibleAt==0&&cursorWrites==writes,"mapped route bypasses native visibility watchdog");
 physicalClip={0,0,3840,2160};lastFrame=31;productionDrawRoute();
 check(!mappedCursorPresentation&&shared->nativeEnabled&&!shared->nativeBlank&&hardwareCursorPublished&&lastFrame==-2,
       "released canvas clip restores native delivery and invalidates the image");
 check(shared->nativeW>0&&shared->nativeH>0&&shared->nativeHotX==shx&&shared->nativeHotY==shy&&cursorseq::committed(shared->nativeSeq),
       "restored native route publishes the production packet and hotspot");
 cursor_runtime_fixture::nativeRefresh();
 check(!cursor_runtime_fixture::blankActive&&GetCursor()==cursor_runtime_fixture::nativeCursor&&cursor_runtime_fixture::oldCursor==(HCURSOR)501&&shared->nativeVisible,
       "blank-to-native handoff releases old blank before owning native cursor");
 SetCursor((HCURSOR)777);CWPRETSTRUCT event{};event.message=WM_SETCURSOR;event.hwnd=game;event.lParam=HTCLIENT;
 cursor_runtime_fixture::post(event);
 check(GetCursor()==cursor_runtime_fixture::nativeCursor&&shared->postSetCursorRepairs==1,"actual Post branch repairs native cursor after game WndProc");
 cursor_runtime_fixture::nativeRelease();cursor_runtime_fixture::menuBalanceRelease();
 check(GetCursor()==(HCURSOR)501&&osShowCount==0,"native handoff releases only its own prior cursor and visibility balance");

 reset();++cases;nativeGameRefresh=true;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 osShowCount=-2;cursor_runtime_fixture::nativeRefresh();
 // Runtime sees a dialog before the host clears nativeBlank. The actual
 // nativeRefresh branch must prioritize menu ownership during this race.
 shared->menuSuspend=1;cursor_runtime_fixture::nativeRefresh();
 check(GetCursor()==(HCURSOR)601&&!cursor_runtime_fixture::blankActive&&cursor_runtime_fixture::menuCursorSet&&osShowCount==0,
       "Runtime menu wins over a pending mapped blank");
 shared->menuSuspend=0;applyCursorPresentation(true);cursor_runtime_fixture::nativeRefresh();
 check(!GetCursor()&&cursor_runtime_fixture::blankActive&&!cursor_runtime_fixture::menuCursorSet&&cursor_runtime_fixture::menuShowBalance==0&&osShowCount==-2,
       "menu-to-mapped handoff promptly returns menu visibility increments");
 dialogActive=true;productionDrawRoute();
 check(nativeMenuHold&&shared->menuSuspend==1&&!shared->nativeBlank&&!shared->nativeEnabled,
       "host menu entry clears mapped blank while retaining paired game Hide");
 cursor_runtime_fixture::nativeRefresh();check(GetCursor()==(HCURSOR)601&&pairedHide,"host menu keeps the controlled Windows dialog cursor visible");
 dialogActive=false;productionDrawRoute();cursor_runtime_fixture::nativeRefresh();
 check(!nativeMenuHold&&!shared->menuSuspend&&shared->nativeBlank&&!shared->nativeEnabled&&!GetCursor()&&nativeGameRefresh,
       "host menu exit resumes mapped external route rather than native preference");
 cursor_runtime_fixture::dialogMessage=true;cursor_runtime_fixture::post(event);
 check(shared->menuSuspend==1&&GetCursor()==(HCURSOR)601,"actual Post dialog branch precedes blank when both window predicates match");
 cursor_runtime_fixture::dialogMessage=false;cursor_runtime_fixture::post(event);
 check(GetCursor()==(HCURSOR)601,"actual Post blank branch respects active menu suspension");
 shared->menuSuspend=2;cursor_runtime_fixture::nativeRefresh();
 check(!cursor_runtime_fixture::blankActive&&!cursor_runtime_fixture::menuCursorSet&&osShowCount==-2,
       "leaving the game releases blank and menu visibility ownership");

 reset();++cases;nativeGameRefresh=true;physicalClip={0,0,2560,1440};physicalPoint={1280,720};createOK=false;productionDrawRoute();
 check(!pairedHide&&!shared&&!hook&&hookStops==1&&!wasInside&&running&&nativeGameRefresh,
       "mapped window failure restores paired Hide while preserving native preference");
 clockNow+=100;productionDrawRoute();
 check(hookStarts==0&&hookStops==1&&!shared&&running,"persistent mapped window failure never re-arms Hide");
 clockNow+=1000;createOK=true;productionDrawRoute();
 check(pairedHide&&shared&&shared->nativeBlank&&!shared->nativeEnabled&&hookStarts==1&&submissions==1,
       "mapped window recovery restores external publication and paired Hide once");
 placeOK=false;physicalPoint.x+=20;productionDrawRoute();
 check(!pairedHide&&!shared&&!wasInside&&running,"mapped placement failure restores paired Hide");
 placeOK=true;productionDrawRoute();
 check(pairedHide&&shared&&shared->nativeBlank&&wasInside&&hookStarts==2,"mapped placement recovers through real re-entry route");

 reset();++cases;nativeGameRefresh=true;mappedCursorPresentation=true;positionMode=1;createOK=false;wasInside=false;
 productionDrawRoute();
 check(!mappedCursorPresentation&&shared&&shared->nativeEnabled&&!shared->nativeBlank&&pairedHide&&hookStops==0&&running,
       "screen selection recovers native despite prior mapped route and persistent overlay failure");
 check(submissions==0&&nativeGameRefresh,"screen recovery does not require an external window or mutate native preference");

 reset();++cases;nativeGameRefresh=true;physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 cursor_runtime_fixture::nativeRefresh();positionMode=1;manualCursorPause=true;applyCursorPresentation(false);
 check(!shared->nativeEnabled&&!shared->nativeBlank&&!hardwareCursorPublished&&nativeGameRefresh,
       "manual pause retires both effective presentations without changing preference");

 reset();++cases;gameWOverride=1920;gameHOverride=1200;physicalClip={0,0,2560,1440};physicalPoint={100,720};productionDrawRoute();
 check(mappedCursorPresentation&&observedMapPoint&&observedMapWidth==2560&&observedMapHeight==1440,
       "mapped command region uses the actual source canvas and final viewport once");
 for(const auto* selected:{L"keep",L"sha"}){
  reset();++cases;mode=selected;shared=nullptr;hook=nullptr;pairedHide=false;surfaceOK=true;surfaceW=1920;surfaceH=1080;
  physicalClip={0,0,1920,1080};physicalPoint={960,540};productionDrawRoute();
  check(clickGameWidth==1920&&clickGameHeight==1080&&submitted.x+shx==1920&&submitted.y+shy==1080,
        "keep/SHA observes actual surface extent instead of retaining an old valid cache");
  surfaceW=1280;surfaceH=720;physicalClip={0,0,1280,720};physicalPoint={640,360};productionDrawRoute();
  check(clickGameWidth==1280&&clickGameHeight==720&&submitted.x+shx==1920&&submitted.y+shy==1080,
        "keep/SHA follows a second runtime resolution change");
  surfaceOK=false;productionDrawRoute();
  check(clickGameWidth==1280&&clickGameHeight==720&&submitted.x+shx==1920&&submitted.y+shy==1080,
        "failed surface read retains only the prior actual extent cache");
 }
 reset();++cases;shared->sampleSeq=1;shared->sampledGameW=1920;shared->sampledGameH=1080;
 physicalClip={0,0,2560,1440};physicalPoint={1280,720};productionDrawRoute();
 check(clickGameWidth==2560&&submitted.x+shx==1920&&submitted.y+shy==1080,
       "uncommitted Runtime extent cannot replace the prior actual canvas");
}
#endif
int main(int argc,char** argv){
 const std::string option=argc>1?argv[1]:"";
 if(option=="--no-context-api")exportsContext=false;
 if(option=="--no-physical-api")exportsPhysical=false;
#ifndef CURSOR_BASELINE
 if(option=="--presentation-geometry-only"){
  presentationGeometryCases();
 }else if(option=="--owner-recovery-only"){
  ownerRecoveryCases();
 }else
#endif
 if(option=="--no-context-api"){
  reset();++cases;productionDrawRoute();
  check(submissions==1&&submitted.x+shx==2000&&submittedContext==-1,"unavailable context APIs use one complete logical domain");
  check(context==-1,"unavailable API fallback leaves caller context unchanged");
 }else{
  basicCases();
#ifndef CURSOR_BASELINE
  for(double dpi:{1.,1.25,1.5,2.})for(int percent:{100,150,200}){
   reset();++cases;dpiFactor=dpi;scalePercent=percent;
   const auto scale=cursorScale(true,false,percent,3840,2160,2560,1440);
   dw=scaledSize(55,scale.x);dh=scaledSize(43,scale.y);shx=scaledHotspot(10,55,dw,0);shy=scaledHotspot(8,43,dh,0);
   productionDrawRoute();
   check(submissions==1&&submitted.x+shx==3000&&submitted.y+shy==1000,"DPI and raster scale never multiply physical pointer");
  }
  reset();++cases;physicalClient={500,300,3060,1740};physicalPoint={2800,1500};productionDrawRoute();
  check(submissions==1&&submitted.x+shx==2800&&submitted.y+shy==1500,"window offset on a 4K monitor uses actual client geometry");
  reset();++cases;physicalClient={-3840,-200,0,1960};physicalPoint={-500,1000};productionDrawRoute();
  check(submissions==1&&submitted.x+shx==-500&&submitted.y+shy==1000,"negative monitor origin is preserved");
  reset();++cases;physicalClient={-3840,0,0,2160};context=-4;
  check(productionRefresh()&&clickRegion.left==-240&&clickRegion.right==0,"sidebar uses physical negative screen origin");
  clickGameWidth=1920;clickGameHeight=1440;
  check(productionRefresh()&&clickRegion.left==-720&&clickRegion.right==-480,"sidebar matches fitted pillarbox viewport");
  reset();++cases;context=-4;aspectFit=false;
  check(productionMapPoint({100,1000},1920,1440),"disabled aspect-fit does not invent platform letterboxing");
  clickGameWidth=1920;clickGameHeight=1440;
  check(productionRefresh()&&clickRegion.left==3520&&clickRegion.right==3840,"stretch mode uses independent configured axes");
  reset();++cases;context=-4;allowPM=false;productionDrawRoute();
  check(submissions==1&&submitted.x+shx==3000&&context==-4,"known per-monitor context remains physical when temporary switch is denied");
  reset();++cases;{CoordinateScope scope;POINT point{91,92};Rect rect{1,2,3,4};cursorOK=false;secondCornerOK=false;
   check(!scope.cursor(point)&&point.x==91&&point.y==92,"failed cursor output is never committed");
   check(!scope.client(game,rect)&&rect.left==1&&rect.top==2&&rect.right==3&&rect.bottom==4,"failed second corner leaves previous output intact");}
  check(context==-1,"failure paths restore original DPI context");
  reset();++cases;allowPM=false;productionDrawRoute();
  check(submissions==1&&overlayCoordinateDomain==Domain::CallerLogical,"initial fallback creates matching logical windows");
  allowPM=true;submissions=placements=0;const HWND old=overlay;productionDrawRoute();
  check(submissions==1&&overlay!=old&&overlayCoordinateDomain==Domain::Physical,"physical recovery promotes windows without restart");
  reset();++cases;productionDrawRoute();const HWND physicalWindow=overlay;allowPM=false;submissions=placements=0;productionDrawRoute();
  check(!submissions&&!placements&&overlay==physicalWindow,"temporary downgrade cannot submit mixed-domain geometry");
  allowPM=true;productionDrawRoute();check(submissions==1&&overlay==physicalWindow,"physical retry restores output automatically");
  reset();++cases;allowPM=false;productionDrawRoute();const HWND unaware=overlay;context=-2;productionDrawRoute();
  check(overlay!=unaware&&overlayCoordinateContext==(HANDLE)-2,"distinct logical DPI contexts rebuild matching windows");
  reset();++cases;createOK=false;productionDrawRoute();const unsigned attempted=creations;
  check(!submissions&&GetLastError()==8,"window creation failure does not submit and preserves error");
  clockNow+=100;productionDrawRoute();check(creations==attempted&&GetLastError()==8,"window creation backoff preserves original error");
  clockNow+=1000;createOK=true;productionDrawRoute();check(submissions==1,"window creation failure can recover");
  reset();++cases;createOK=false;nativeGameRefresh=true;productionDrawRoute();
  check(!leaves&&!submissions,"native route does not depend on external overlay creation");
  nativeGameRefresh=false;createOK=true;productionDrawRoute();check(submissions==1,"native to overlay mode restores same-domain presentation");
  reset();++cases;messages.clear();automaticMessages.clear();
  {CoordinateScope scope(true);coordinateDiagnostic(L"present",scope,physicalPoint,true,physicalClient,2560,1440,83,65,15,12,1.5,1.5,3840,2160);
   for(int i=0;i<200;++i){clockNow+=1;physicalPoint.x+=1;coordinateDiagnostic(L"present",scope,physicalPoint,true,physicalClient,2560,1440,83+i%2,65,15+i%2,12,1.5,1.5,3840,2160);}
   check(messages.size()==1&&automaticMessages.size()==1,"moving cursor and changing rasters cannot flood both logs");
   clockNow+=1000;coordinateDiagnostic(L"present",scope,physicalPoint,true,physicalClient,2560,1440,84,65,16,12,1.5,1.5,3840,2160);
   check(messages.size()==2,"geometry changes publish after minimum interval");
   clockNow+=14000;coordinateDiagnostic(L"present",scope,physicalPoint,true,physicalClient,2560,1440,84,65,16,12,1.5,1.5,3840,2160);
   check(messages.size()==2,"stable diagnostic remains throttled for fifteen seconds");
   clockNow+=1000;coordinateDiagnostic(L"present",scope,physicalPoint,true,physicalClient,2560,1440,84,65,16,12,1.5,1.5,3840,2160);
   check(messages.size()==3,"stable diagnostic publishes after fifteen seconds");}
  ownerRecoveryCases();
#ifdef CURSOR_PRESENTATION
  presentationGeometryCases();presentationOwnerCases();
#endif
#endif
 }
 std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"native_windows_executed\":false,\"game_executed\":false}\n";
 return failures?1:0;
}

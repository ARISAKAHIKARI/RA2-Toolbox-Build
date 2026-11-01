#include "../../external_reader/source/shared_reader.h"
#include "../../external_reader/source/process_discovery.h"
#include "../../shared/source/modern_ui.h"
#include "../../shared/source/ui_message_queue.h"
#include "../../shared/source/settings_dialog.h"
#include "../../shared/source/overlay_stack.h"
#include "../../shared/source/disabled_choices.h"
#include "../../shared/source/release_version.h"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../../shared/source/bounded_shutdown.h"
#include <tlhelp32.h>
#include <shellapi.h>
#include <commdlg.h>
#include "precomputed.h"
#include <shlobj.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include "core.h"

#include "../../shared/source/host_game_input.h"
#include "../../shared/source/command_cursor_hint.h"
#ifdef RA2_UNIFIED
#include "suite_host.h"
#endif
#include "scale.h"
#include "features.h"
#include "motion.h"
#include "quality.h"
#include "persist.h"
#include "adaptive.h"
#include "recognition.h"
#include "ui_dpi.h"
#include <future>
#include <atomic>
#include "perf_policy.h"
#include "playback.h"
#include <map>
#include <deque>
#include <set>
#include <memory>
#include <chrono>
#include "bridge.h"
#include "native_hotspot.h"
#include "super_hotspot.h"
#include "snapshot_latch.h"
#include "stream_inference.h"
static HWND hud,hudBox;static bool manualCursorPause=false,systemCursorGuard=true,nativeMenuHold=false;static DWORD nativeMenuCandidateAt=0,lastObservedFrameAt=0;static int previousObservedFrame=-1;static std::vector<CursorRange> fallbackDefs;static CursorRangeTracker fallbackTracker;static bool hudEnabled=false;static std::wstring hudText;static unsigned hudUpdates=0,hudCommits=0,hudMotion=0,hudChanges=0;static double updateFps=0,commitFps=0,motionFps=0,changeFps=0;static uint32_t hudEpoch=0,hudPaintAt=0;static int hudLastFrame=-1;static uint32_t diagIndex=0xffffffff,diagLegacy=0xffffffff;static int scrollX=0,scrollY=0;static bool uiReady=false;
struct ControlBounds { HWND handle; uidpi::Rect logical; };
static std::vector<ControlBounds> controlOrigins;
static UINT uiDpi=96;
static HFONT uiFont=nullptr, titleFont=nullptr, actionFont=nullptr;
static bool uiLayoutBusy=false;
// Compact first-run panel; legacy controls are shown only in Advanced.
static bool advancedOpen=false;
static constexpr int compactContentW=770;static int compactContentH=394;
static constexpr int advancedContentW=770;static int advancedContentH=1354;
static int uiContentW=compactContentW,uiContentH=compactContentH;
static constexpr int advancedOffsetY=325;
static std::vector<HWND> advancedControls;
static HWND titleLabel=nullptr,subTitleLabel=nullptr,primaryAction=nullptr,advancedAction=nullptr,quickInfo=nullptr,advancedHeader=nullptr;
static HBRUSH panelBrush=nullptr, headerBrush=nullptr, statusBrush=nullptr;
static bool mainWindowReady=false;
static HWND targetBox=nullptr;
static HWND settingsHint=nullptr,detailLabel=nullptr,styleBox=nullptr,prepareButton=nullptr,prepareInfo=nullptr;
static HANDLE prepareProcess=nullptr;static std::wstring prepareStatus,prepareCancel,preparedProfile;
static bool applyingSettings=false;
static Asset displayAsset;static DWORD displayPalette[256];static bool displayLoaded=false;
static std::wstring profilePath,profileId;
static std::vector<std::wstring> profileIds,profileNames;
static std::set<unsigned> allowedPairs;
static void pollPrepare();static void chooseStyle();static void importStyle();static void beginPrepare();
static void readPairs();static void refreshStyleList();static void resetPrepared();
static void rebuildDisplay();
static bool superCenter=true;static HWND superCenterBox;static superhot::Centers superCenters;
static void textIfChanged(HWND w,const std::wstring& text){if(!IsWindow(w))return;int n=GetWindowTextLengthW(w);std::wstring old(size_t(n)+1,L'\0');GetWindowTextW(w,&old[0],n+1);old.resize(n);if(old!=text)SetWindowTextW(w,text.c_str());}

static std::wstring root,gameDir,cacheKey,mode=L"keep";
static external::ReadSession cursorReader;static DWORD pid=0,threadId=0;static HWND game=nullptr,mainw,overlay,statusw;
static int cursorDialog(const wchar_t*text,const wchar_t*title,UINT flags){
#ifdef RA2_UNIFIED
 return settingsdialog::message(mainw,true,text,title,flags);
#else
 return settingsdialog::message(mainw,false,text,title,flags);
#endif
}
static Asset asset;static Bytes original;static DWORD palette[256];static bool loaded=false,running=false,installOwned=false;
static bool hideConfirmed=false;static LONG lastRehideSeen=0;static gameinput::Subscription hook;static HANDLE mapping;static Shared* shared;
// v1.2.7: platform host matching + immediate Hook on start.
// Right-button release and reference count changes trigger conditional re-hide.
static DWORD lastVisibilityLogAt=0;static std::wstring lastVisibilityReason;
static DWORD gameParentPid=0;
static HDC memdc;static HBITMAP dib,oldbm;static void* dibbits;static int dibw,dibh,lastFrame=-2;static DWORD lastColor=0;
static HWND gameWBox,gameHBox,screenWBox,screenHBox,dpiBox,dpiModeBox,readout;static int gameWOverride=0,gameHOverride=0,screenWOverride=0,screenHOverride=0,dpiOverride=0,dpiMode=0;static DWORD lastReadout=0;
static HWND scaleBox,autoBox,fitBox,offsetXBox,offsetYBox;static int scalePercent=100,offsetX=0,offsetY=0;static bool autoScale=false,aspectFit=true;static int lastSystemVisible=-1;static DWORD lastCursorCheck=0;
static AdaptiveTiming timing;static bool adaptive=true;static unsigned transitionMs=60;static HWND adaptiveBox,timingReadout;static DWORD lastTimingReadout=0;
static Tween tween;static Burst burst;static bool smoothing=true,clickEnabled=true,nativeFive=false,aresKnown=false;static int smoothMs=60,clickMode=0,clickGap=0,sidebarWidth=160,sidebarTop=200;static HWND smoothBox,smoothMsBox,clickBox,clickModeBox,clickGapBox;static RECT clickRegion={};static DWORD regionTime=0;static size_t lastPhase=size_t(-1);
static int clickGameWidth=0,clickGameHeight=0;static DWORD clickLoggedAt=0;static LONG clickDenyReason=0;static bool clickNeedsLog=false;static unsigned clickSendOk=0,clickSendFail=0;
static bool physicalLeftDown=false;static bool autoStart=true,autoPaused=false,focusRefresh=true,wasInside=false;static HWND autoStartBox,observedForeground;static DWORD lastAutoCheck=0,lastWindowCheck=0;static int refreshTries=0;static DWORD refreshAt=0;
static LARGE_INTEGER freq;static bool fallback=false;static int modeId=0;static std::wstring lastStatus;
// All timeline decoding completes before game connection. Playback owns a
// fixed resident bank, so no cache eviction, disk reads or decode jobs run in a match.
static std::map<unsigned,std::shared_ptr<playback::Timeline>> motionCache;
static std::vector<motion::Image> displayCanvases;
static std::vector<playback::Frame> displayFrames;
static playback::RasterCache rasterCache;
static std::shared_ptr<playback::Timeline> activeMotion;
struct PlaybackBank {
 std::map<unsigned,std::shared_ptr<playback::Timeline>> timelines;
 unsigned missing=0;size_t bytes=0;
};
static std::future<PlaybackBank> playbackJob;
static std::shared_ptr<std::atomic<bool>> playbackCancel;
static std::shared_ptr<std::atomic<unsigned>> playbackProgress;
static bool playbackReady=false,playbackPending=false;
static unsigned motionEpoch=0,playbackEpoch=0,playbackTotal=0;
static size_t motionBytes=0;static DWORD playbackUiAt=0;
static int previewStart=-1,previewCount=0;
static HWND previewBox, modeBox, applyModeBox, installBox, restoreBox, nativeBox;
static bool nativeGameRefresh=false,readRestricted=false,noCursorMode=false;
static int diagFrame=-1,diagHotX=-1,diagHotY=-1;
static cursorsnap::Latch bridgeFrame;
static cursorstream::Detector lanboStream;
static bool sampleHotspotEstimated=false;
static DWORD lastSampleProbeAt=0;
static bool usingHeldSample=false;
static DWORD lastHostReadLogAt=0;static DWORD resourceRetryAt=0;static unsigned resourceTries=0;

static std::wstring diagAnimation=L"尚未采样";
static DWORD debugHudAt=0;
static unsigned debugHudBeat=0;
static DWORD nativeStartAt=0,lastNativePublishAt=0,nativeInvisibleAt=0,lastHotspotLogAt=0,lastPostRepairLogAt=0;static LONG lastPostRepairSeen=0;
static unsigned detectedRefreshHz=60,pollHz=120;
static double processCpuPercent=0,drawMeanMs=0,drawPeakMs=0;
static LONGLONG drawTotalTicks=0,drawMaxTicks=0;static unsigned drawMeasured=0;
static void performanceSample(){
 static DWORD epoch=0;static ULONGLONG previousCpu=0;
 DWORD now=GetTickCount();if(epoch&&now-epoch<1000)return;
 FILETIME create,exit,kernel,user;
 if(GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel,&user)){
  ULONGLONG cpu=(ULONGLONG(kernel.dwHighDateTime)<<32|kernel.dwLowDateTime)+(ULONGLONG(user.dwHighDateTime)<<32|user.dwLowDateTime);
  static DWORD processors=[](){SYSTEM_INFO info{};GetSystemInfo(&info);return std::max<DWORD>(1,info.dwNumberOfProcessors);}();
  if(epoch)processCpuPercent=double(cpu-previousCpu)/(double(now-epoch)*10000.0)*100.0/processors;
  previousCpu=cpu;
 }
 if(drawMeasured&&freq.QuadPart){drawMeanMs=double(drawTotalTicks)*1000.0/freq.QuadPart/drawMeasured;drawPeakMs=double(drawMaxTicks)*1000.0/freq.QuadPart;}
 drawTotalTicks=drawMaxTicks=0;drawMeasured=0;epoch=now;
}
static DWORD lastRefreshSampleAt=0;
struct CommandDefinition {int first=-1,count=0,interval=4,hotX=12345,hotY=12345;};
static CommandDefinition commandDefs[86];
static commandcursor::Mode shownCommand=commandcursor::None;
static DWORD commandSince=0;
static void resetCommandDefinitions(){
 for(auto&d:commandDefs)d={};
 // Verified stock definitions; live table replaces these when readable.
 // The stock fallback is used only with a standard 55x43 SHA canvas.
 if(asset.w==55&&asset.h==43){
  commandDefs[20]={53,5,4,12345,12345};
  commandDefs[25]={89,10,4,12345,12345};
  commandDefs[71]={404,9,4,12345,12345};
 }
 shownCommand=commandcursor::None;commandSince=0;
}
static motion::Image frameCanvas(int n){
 bool styled=displayLoaded&&n<int(displayAsset.frames.size());
 const Asset& art=styled?displayAsset:asset;const DWORD* colors=styled?displayPalette:palette;
 motion::Image im(art.w*art.h,0);const auto& f=art.frames.at(n);
 for(int y=0;y<f.h;y++)for(int x=0;x<f.w;x++)im[(y+f.y)*art.w+x+f.x]=colors[f.pixels[y*f.w+x]];
 return im;
}
static void rebuildDisplay(){
 rasterCache.clear();activeMotion.reset();lastPhase=size_t(-1);lastFrame=-2;
 displayCanvases.clear();displayFrames.clear();
 int n=std::max(displayLoaded?int(displayAsset.frames.size()):0,loaded?int(asset.frames.size()):0);
 int w=displayLoaded?displayAsset.w:asset.w,h=displayLoaded?displayAsset.h:asset.h;
 for(int i=0;i<n;++i){displayCanvases.push_back(frameCanvas(i));displayFrames.push_back(playback::Frame::pack(displayCanvases.back(),w,h));}
 superCenters=superhot::prepare(displayCanvases,w,h);
}
static unsigned pairKey(int a,int b){return (unsigned(a)<<16)|unsigned(b);}
static void motionPoll(){
 if(playbackJob.valid()&&playbackJob.wait_for(std::chrono::seconds(0))==std::future_status::ready){
  try{auto bank=playbackJob.get();if(playbackEpoch==motionEpoch){
   motionCache=std::move(bank.timelines);motionBytes=bank.bytes;playbackReady=true;
   textIfChanged(prepareInfo,L"播放缓存就绪 · "+std::to_wstring(motionCache.size())+L" 段 · "+std::to_wstring(motionBytes/(1024*1024))+L" MB"+(bank.missing?L" · 缺少 "+std::to_wstring(bank.missing)+L" 段，使用原帧":L""));
  }}catch(...){if(playbackEpoch==motionEpoch){playbackReady=true;textIfChanged(prepareInfo,L"播放缓存不可用，继续显示已验证原帧");}}
 }
 if(playbackPending&&!playbackJob.valid()&&displayLoaded){
  playbackPending=false;playbackEpoch=motionEpoch;playbackTotal=unsigned(allowedPairs.size());
  auto frames=displayCanvases;auto pairs=allowedPairs;
  auto path=std::filesystem::path(profilePath)/L"timelines";
  auto base=std::filesystem::path(root)/L"assets"/L"timelines";
  int w=displayAsset.w,h=displayAsset.h;
  playbackCancel=std::make_shared<std::atomic<bool>>(false);
  playbackProgress=std::make_shared<std::atomic<unsigned>>(0);
  auto cancel=playbackCancel;auto progress=playbackProgress;
  playbackJob=std::async(std::launch::async,[frames=std::move(frames),pairs=std::move(pairs),w,h,path,base,cancel,progress]()mutable{
   SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
   PlaybackBank bank;
   for(unsigned key:pairs){
    if(cancel->load())break;
    unsigned a=key>>16,b=key&65535;
    if(a<frames.size()&&b<frames.size()){
     auto name=std::to_wstring(a)+L"-"+std::to_wstring(b)+L".dis";
     motion::Timeline decoded;
     // 129 display slots: <=1.57ms spacing even for a 200ms animation.
     // Decode and validate every stored sample; retain exact precomputed images.
     if(precomputed::load(path/name,decoded,frames[a],frames[b],w,h,129)||
        (base!=path&&precomputed::load(base/name,decoded,frames[a],frames[b],w,h,129))){
      auto timeline=std::make_shared<playback::Timeline>(playback::Timeline::pack(decoded));
      if(bank.bytes+timeline->bytes()<=512u*1024*1024){bank.bytes+=timeline->bytes();bank.timelines[key]=std::move(timeline);}
      else ++bank.missing;
     }else ++bank.missing;
    }
    ++*progress;
   }
   return bank;
  });
 }
 if(!playbackReady&&playbackProgress&&GetTickCount()-playbackUiAt>=250){
  playbackUiAt=GetTickCount();std::wstring progress=L"准备播放缓存 "+std::to_wstring(playbackProgress->load())+L" / "+std::to_wstring(playbackTotal)+L"，完成后自动连接";
  textIfChanged(statusw,progress);textIfChanged(prepareInfo,progress);
 }
}
static std::wstring widen(const char* x){int n=MultiByteToWideChar(CP_UTF8,0,x,-1,0,0);std::wstring s(n,0);MultiByteToWideChar(CP_UTF8,0,x,-1,&s[0],n);if(n)s.pop_back();return s;}
static void log(const std::wstring& s){FILE* f=_wfopen((root+L"\\cursor.log").c_str(),L"a, ccs=UTF-8");if(f){SYSTEMTIME t;GetLocalTime(&t);fwprintf(f,L"%02u:%02u:%02u %ls\n",t.wHour,t.wMinute,t.wSecond,s.c_str());fclose(f);}}
static void updatePrimaryAction(){
 if(!IsWindow(primaryAction))return;
 bool enabled=autoStart&&!autoPaused;
#ifdef RA2_UNIFIED
 suite::reportEnabled(0,enabled);
#endif
 textIfChanged(primaryAction,mode==L"hide"?(enabled?L"暂停（高危模式）":L"开启（高危模式）"):(enabled?L"暂停":L"开启"));
 textIfChanged(subTitleLabel,mode==L"hide"?L"高危：隐藏原光标需修改原生提示入口":mode==L"sha"?L"透明 SHA · 外置光标":L"保留原光标 · 外置光标");
 std::wstring state=manualCursorPause?L"光标已暂停（F9 恢复）":!autoStart?L"已关闭":autoPaused?L"需要检查 · 展开高级设置":running?L"运行中":L"等待游戏启动";
 textIfChanged(statusw,state);
 static int lastPaint=-1;int paint=running?2:enabled?1:0;
 if(lastPaint!=paint){lastPaint=paint;InvalidateRect(primaryAction,NULL,FALSE);InvalidateRect(mainw,NULL,FALSE);}
}
static void status(const std::wstring& s){
 if(s!=lastStatus){lastStatus=s;log(s);textIfChanged(detailLabel,s);}
 updatePrimaryAction();
}
static void fail(const char* s){throw std::runtime_error(s);}
static Bytes file(const std::wstring& p){FILE* f=_wfopen(p.c_str(),L"rb");if(!f)fail("Cannot open file");fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<0||n>40*1024*1024){fclose(f);fail("File size invalid");}Bytes b(n);bool ok=fread(b.data(),1,n,f)==size_t(n);fclose(f);if(!ok)fail("File read failed");return b;}
static bool exists(const std::wstring& p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
static void save(const std::wstring& p,const Bytes& b){std::wstring tmp=p+L".tmp";HANDLE h=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)fail("Cannot create temporary file (remove stale .tmp only after checking it)");DWORD n=0;bool ok=WriteFile(h,b.data(),(DWORD)b.size(),&n,NULL)&&n==b.size()&&FlushFileBuffers(h);CloseHandle(h);if(!ok){DeleteFileW(tmp.c_str());fail("Write failed");}if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(tmp.c_str());fail("Rename failed");}}
static std::wstring ini(){return root+L"\\settings.ini";}
// Self-contained installed resources: never read a transparent game mouse.sha as source art.
// Existing per-game custom caches are preserved; missing files are seeded from assets/.
static void seedAssets(){
 for(const wchar_t* name:{L"original.sha",L"palette.bgra"}){
  std::wstring dst=cacheKey+L"\\"+name;
  if(exists(dst))continue;
  Bytes content=file(root+L"\\assets\\"+name);
  if(!wcscmp(name,L"original.sha")){auto a=decode(content);if(a.frames.size()!=450||a.w!=55||a.h!=43)fail("Bundled SHA is not the validated 450-frame resource");}
  if(!wcscmp(name,L"palette.bgra")&&content.size()!=1024)fail("Bundled palette damaged");
  save(dst,content);
 }
 if(!exists(cacheKey+L"\\transparent.sha")){
  // SHA is always built from the cached ORIGINAL, not what the game currently displays.
  Asset resource=decode(file(cacheKey+L"\\original.sha"));
  save(cacheKey+L"\\transparent.sha",encode(resource,true));
 }
}
static void setDir(const std::wstring& d){gameDir=d;uint32_t h=2166136261;for(wchar_t c:d){h^=towlower(c);h*=16777619;}wchar_t key[32];wsprintfW(key,L"%08X",h);cacheKey=root+L"\\cache\\"+key;
 CreateDirectoryW((root+L"\\cache").c_str(),NULL);CreateDirectoryW(cacheKey.c_str(),NULL);
 seedAssets();WritePrivateProfileStringW(L"Target",L"Directory",gameDir.c_str(),ini().c_str());}

static bool rpm(uint32_t a,void* out,size_t n){return cursorReader.readFresh(a,out,n);}
static Bytes read(uint32_t a,size_t n){if(n>32*1024*1024)fail("Read bound");Bytes b(n);if(!rpm(a,b.data(),n))fail("Game memory not available");return b;}
static uint32_t word(uint32_t a){return u32(read(a,4),0);}
static bool live(){return cursorReader.alive();}
static uint32_t mouse(){uint32_t p=word(0x887640);Bytes obj=read(p,0x30),vt=read(u32(obj,0),20);if(u32(vt,4)!=0x7B8A00||u32(vt,12)!=0x7B9930||u32(vt,16)!=0x7B9750)fail("Unknown mouse vtable: this game build is not supported");return p;}
static uint32_t imageBase(uint32_t p){uint32_t img=word(p+4);Bytes h=read(img,8);if(u16(h,0)==0xffff){img=word(img+12);h=read(img,8);}validHeader(h);return img;}
static bool accepted(const wchar_t* s){return external::discovery::isGameName(s);}
struct CursorWindowReader {bool read(uint32_t at,void*out,size_t n){return rpm(at,out,n);}};
static bool selectGameWindow(){
 CursorWindowReader reader;DWORD foundThread=0;
 HWND found=external::discovery::resolveYrppWindow(reader,pid,nullptr,&foundThread);
 if(!found){auto scan=external::discovery::scan(pid);found=scan.best.window;foundThread=scan.best.thread;
  // Preserve launcher child-renderer support, using the same shared filter.
  if(!found){std::unordered_set<DWORD> ids{pid};external::discovery::WindowContext context{&scan,&ids,pid};
   HWND foreground=GetForegroundWindow();if(foreground)EnumChildWindows(foreground,external::discovery::checkWindow,(LPARAM)&context);
   found=scan.best.window;foundThread=scan.best.thread;}
 }
 if(!found)found=external::discovery::resolveYrppWindow(reader,pid,game,&foundThread);
 game=found;threadId=foundThread;return game!=nullptr;
}
static DWORD parentProcess(DWORD child){
 HANDLE ss=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(ss==INVALID_HANDLE_VALUE)return 0;
 PROCESSENTRY32W pe={sizeof(pe)};DWORD parent=0;
 if(Process32FirstW(ss,&pe))do{if(pe.th32ProcessID==child){parent=pe.th32ParentProcessID;break;}}while(Process32NextW(ss,&pe));
 CloseHandle(ss);return parent;
}
// Only trust a different-PID platform host when it is the game's actual
// parent process AND its foreground rect nearly matches the renderer rect.
// This avoids drawing the game cursor on top of an unrelated foreground app.
static bool platformHostMatches(HWND fg,DWORD fpid){
 if(!fg||!IsWindow(game))return false;
 // A real cross-process child/root relationship is stronger than PID ancestry.
 if(GetAncestor(fg,GA_ROOT)==GetAncestor(game,GA_ROOT))return true;
 if(!gameParentPid||fpid!=gameParentPid)return false;
 RECT a{},b{};if(!GetWindowRect(fg,&a)||!GetWindowRect(game,&b))return false;
 int aw=a.right-a.left,ah=a.bottom-a.top,bw=b.right-b.left,bh=b.bottom-b.top;
 if(aw<320||ah<200||bw<320||bh<200)return false;
 int ix=std::max(0,static_cast<int>(std::min(a.right,b.right)-std::max(a.left,b.left)));
 int iy=std::max(0,static_cast<int>(std::min(a.bottom,b.bottom)-std::max(a.top,b.top)));
 const double overlap=double(ix)*iy/(double(bw)*bh);
 // The parent can wrap game content with a narrow border, not an arbitrary window.
 return overlap>=.95 && aw<=bw+80 && ah<=bh+100 && aw+80>=bw && ah+100>=bh;
}
// Shared focus policy for the overlay and the click queue. A platform host may
// be the foreground window while the actual gamemd renderer is a child window.
static bool clickForegroundMatches(){
 HWND fg=GetForegroundWindow();DWORD fp=0;GetWindowThreadProcessId(fg,&fp);
 if(fp==pid)return IsWindow(game)&&GetAncestor(fg,GA_ROOT)==GetAncestor(game,GA_ROOT);
 return platformHostMatches(fg,fp);
}
// The click region is calculated without reading any game memory. This must
// work even when a protected launcher prevents the external cursor overlay
// from resolving the current animation frame.
static bool refreshClickRegion(){
 RECT r={};POINT origin={0,0};
 if(!running||!IsWindow(game)||!IsWindowVisible(game)||IsIconic(game)||
    !GetClientRect(game,&r)||r.right<320||r.bottom<200||!ClientToScreen(game,&origin)||!clickForegroundMatches()){
  SetRectEmpty(&clickRegion);regionTime=0;return false;
 }
 int gw=gameWOverride?gameWOverride:clickGameWidth;
 int gh=gameHOverride?gameHOverride:clickGameHeight;
 // The native game size may be unreadable on another launcher. A conservative
 // 1024x768 coordinate model keeps the sidebar input independent of VM_READ;
 // user-configured game dimensions always override this fallback.
 if(gw<320||gh<200){gw=1024;gh=768;}
 if(sidebarWidth<=0||sidebarWidth>=gw||sidebarTop>=gh){SetRectEmpty(&clickRegion);regionTime=0;return false;}
 const double sx=double(r.right)/gw,sy=double(r.bottom)/gh;
 double fx=sx,fy=sy,ox=0,oy=0;
 if(aspectFit){fx=fy=std::min(sx,sy);ox=(r.right-gw*fx)/2;oy=(r.bottom-gh*fy)/2;}
 RECT next={origin.x+(LONG)std::lround(ox+(gw-sidebarWidth)*fx),
            origin.y+(LONG)std::lround(oy+sidebarTop*fy),
            origin.x+(LONG)std::lround(ox+gw*fx),
            origin.y+(LONG)std::lround(oy+gh*fy)};
 if(next.right<=next.left||next.bottom<=next.top){SetRectEmpty(&clickRegion);regionTime=0;return false;}
 clickRegion=next;regionTime=GetTickCount();return true;
}
static const wchar_t* clickReasonText(LONG code){
 switch(code){
 case 1:return L"功能未运行/未启用";
 case 2:return L"自动模式因Ares或模块识别失败跳过补点";
 case 3:return L"前台不是已确认的游戏窗口或宿主窗口";
 case 4:return L"无法计算游戏侧栏区域";
 case 5:return L"鼠标不在推定的侧栏区域";
 case 6:return L"未按Shift或同时按了Ctrl/Alt";
 case 7:return L"鼠标移动/松开状态不满足补点";
 case 8:return L"SendInput调用失败或只发送部分事件";
 default:return L"未知原因";
 }
}
static void visibilityDiagnostic(const wchar_t* reason,DWORD foregroundPid=0){
 DWORD now=GetTickCount();std::wstring why=reason;
 if(why==lastVisibilityReason&&uint32_t(now-lastVisibilityLogAt)<5000)return;
 lastVisibilityReason=why;lastVisibilityLogAt=now;
 std::wstring detail=L"光标显示等待："+why+L"；游戏PID="+std::to_wstring(pid)+L"，前台PID="+std::to_wstring(foregroundPid);
 log(detail);status(detail);
}

static void loadCache(){bridgeFrame.clear();lanboStream.reset();fallbackTracker.reset();fallbackDefs.clear();timing.clear();activeMotion.reset();previewStart=-1;previewCount=0;loaded=false;try{original=file(cacheKey+L"\\original.sha");asset=decode(original);Bytes pal=file(cacheKey+L"\\palette.bgra");if(pal.size()!=sizeof(palette))return;memcpy(palette,pal.data(),sizeof(palette));loaded=true;lastFrame=-2;if(displayLoaded)rebuildDisplay();}catch(...){}}
static void attach(bool quiet=false){bridgeFrame.clear();lanboStream.reset();if(running)fail("Stop the preview before attaching again");auto scan=external::discovery::scan();if(scan.snapshotError&&!scan.processCount)fail("Process enumeration failed");if(scan.processCount!=1||!scan.best.pid)fail("Keep exactly one compatible Yuri game running, then Attach (random process names are supported)");cursorReader.close();pid=scan.best.pid;clickGameWidth=clickGameHeight=0;gameParentPid=parentProcess(pid);// Query-only fallback preserves Runtime cursor snapshots when VM_READ is denied.
 if(!cursorReader.open(pid,mode==L"hide"||nativeGameRefresh||noCursorMode))
  throw std::runtime_error("OpenProcess failed; Win32="+std::to_string(cursorReader.lastOpenError));
 readRestricted=!cursorReader.canRead();
 wchar_t path[32768];DWORD len=32768;if(!cursorReader.imagePath(path,&len))fail("Cannot locate game");std::wstring exe(path);setDir(exe.substr(0,exe.find_last_of(L"\\")));game=0;if(!selectGameWindow())fail("Game window not ready (including fullscreen); waiting for a visible render window");
 DWORD peTimeStamp=0;
 if(!readRestricted){
  try{
   Bytes dos=read(0x400000,64);if(u16(dos,0)!=0x5a4d)fail("Unknown image base");
   Bytes nt=read(0x400000+u32(dos,60),84);
   if(u32(nt,0)!=0x4550||u16(nt,4)!=0x14c||u16(nt,24)!=0x10b||u32(nt,52)!=0x400000)fail("Unsupported PE layout");
   mouse();peTimeStamp=u32(nt,8);
  }catch(const std::exception& e){
   if((mode!=L"hide"&&!nativeGameRefresh&&!noCursorMode)||std::string(e.what())!="Game memory not available")throw;
   // Restricted to own game process/known GUI hook; do not attempt a memory
   // patch or suppress the platform's integrity protection.
   readRestricted=true;log(L"Host VM read denied; game-thread snapshot fallback enabled");
  }
 }
 loadCache();
 resetCommandDefinitions();
 // Read the read-only, 86-entry built-in definition table once per connection.
 // Even when the guessed cursor-type field differs, consecutive frame ranges
 // still provide a conservative way to recognize an animation.
 if(loaded&&!readRestricted){try{Bytes table=read(0x82d028,86*28);
   for(int i=0;i<86;++i){size_t off=size_t(i)*28;int first=int(u32(table,off)),count=int(u32(table,off+4));
    if(cursorRangeMatches(first,count,first,asset.frames.size()))commandDefs[i]={first,count,int(u32(table,off+8)),int(u32(table,off+20)),int(u32(table,off+24))};}
   for(int i=0;i<86;i++)for(int mini=0;mini<2;mini++){
    size_t off=size_t(i)*28+(mini?12:0);int first=(int)u32(table,off),count=(int)u32(table,off+4);
    if(count>1&&cursorRangeMatches(first,count,first,asset.frames.size()))fallbackDefs.push_back({first,count,i*2+mini});
   }
   log(L"Read-only cursor definition scan ready; animated variants="+std::to_wstring(fallbackDefs.size()));
  }catch(const std::exception&){log(L"Definition scan unavailable; raw index matching only");}}
 wchar_t msg[160];wsprintfW(msg,L"已连接 PID %lu；PE 时间戳 %08X；%ls",pid,peTimeStamp,loaded?L"内置原版资源已就绪":L"资源加载失败");if(!quiet)status(msg);}
static void capture(){if(running)fail("Stop preview before capturing");if(!live())fail("Start the game without transparent SHA, enter a skirmish, then Attach");if(exists(gameDir+L"\\mouse.sha.cursor-preview-state"))fail("Restore transparent SHA and restart the game before capturing");uint32_t p=mouse(),initialImage=word(p+4),img=imageBase(p);Bytes hdr=read(img,8);validHeader(hdr);int n=u16(hdr,6);Bytes all=read(img,8+n*24);size_t limit=all.size();bool any=false;
 for(int i=0;i<n;i++){size_t x=8+i*24;uint32_t off=u32(all,x+20);unsigned w=u16(all,x+4),h=u16(all,x+6),fmt=u32(all,x+8)&255;if(w>512||h>512)fail("Invalid frame bounds");if(!off||!w||!h)continue;uint64_t end=off;if(fmt<=1)end+=w*h;else if(fmt==3){for(unsigned y=0;y<h;y++){unsigned len=u16(read(img+(uint32_t)end,2),0);if(len<2||len>2048)fail("RLE bound");end+=len;}}else fail("Cursor format 2 not supported");if(end>32*1024*1024)fail("Image too large");limit=std::max(limit,(size_t)end);}
 Asset a=decode(read(img,limit));for(auto& f:a.frames)for(auto c:f.pixels)if(c)any=true;if(!any)fail("All frames are transparent. Restore originals and restart first");
 uint32_t cv=word(0x87f6c8);if(word(cv+4)!=2)fail("Only 16-bit game palette is supported");uint32_t pp=word(cv+0x174);Bytes colors=read(pp,512),shifts=read(0x8a0dd0,24);int rl=u32(shifts,0),rr=u32(shifts,4),bl=u32(shifts,8),br=u32(shifts,12),gl=u32(shifts,16),gr=u32(shifts,20);if(!((rl==11&&rr==3&&gl==5&&gr==2&&bl==0&&br==3)||(rl==10&&rr==3&&gl==5&&gr==3&&bl==0&&br==3)))fail("Unknown 16-bit palette shifts");DWORD pal[256];for(int i=0;i<256;i++){unsigned v=u16(colors,i*2);unsigned r=((v>>rl)<<rr)&255,g=((v>>gl)<<gr)&255,b=((v>>bl)<<br)&255;pal[i]=i?0xff000000|(r<<16)|(g<<8)|b:0;}
 if(mouse()!=p||word(p+4)!=initialImage)fail("Cursor changed during capture; try again");Bytes canonical=encode(a,false);save(cacheKey+L"\\original.sha",canonical);Bytes palbytes(sizeof(pal));memcpy(palbytes.data(),pal,sizeof(pal));save(cacheKey+L"\\palette.bgra",palbytes);loadCache();status(L"原图已采集。透明版：退出游戏后点“安装透明 SHA”；其他版可直接启动。");}
// A manually copied transparent SHA has no restore marker. Accept it only if
// its bytes exactly match the blank image generated from our original cache.
// Never create a marker/backup for a manual install, and never auto-restore it.
static bool manuallyBlankSHA(){
 if(!loaded||gameDir.empty())return false;
 const std::wstring dst=gameDir+L"\\mouse.sha";
 if(!exists(dst)||exists(dst+L".cursor-preview-state"))return false;
 try{auto bytes=file(dst);if(bytes==encode(asset,true))return true;
  const auto legacy=cacheKey+L"\\legacy-transparent.sha";
  return exists(legacy)&&bytes==file(legacy);
 }catch(...){return false;}
}
static bool anyGame(){auto scan=external::discovery::scan();return scan.processCount||scan.snapshotError;}
static void install(){if(gameDir.empty()||!loaded)fail("Select the game folder first; bundled assets must be readable");if(anyGame())fail("Exit all Yuri game processes before installing");std::wstring dst=gameDir+L"\\mouse.sha",mark=dst+L".cursor-preview-state",bak=dst+L".cursor-preview-backup";if(exists(mark)||exists(bak))fail("Existing preview state/backup found. Restore first; never overwrite backups");if(manuallyBlankSHA()){status(L"检测到手动放入的透明 SHA，已可直接选择替换 SHA 模式；没有本程序的自动恢复备份。");return;}Bytes blank=encode(asset,true);save(cacheKey+L"\\transparent.sha",blank);bool had=exists(dst);if(had&&!CopyFileW(dst.c_str(),bak.c_str(),TRUE))fail("Backup failed");Bytes m={uint8_t(had?1:0)};try{save(mark,m);save(dst,blank);}catch(...){if(had)CopyFileW(bak.c_str(),dst.c_str(),FALSE);else DeleteFileW(dst.c_str());throw;}installOwned=true;status(L"透明 SHA 已安装。重开游戏，连接，再启动外部光标。不要叠加旧 HWCursor。");}
static void restoreFile(){if(gameDir.empty())fail("Attach once to identify the game directory");if(anyGame())fail("Exit all Yuri game processes before restoring SHA");std::wstring dst=gameDir+L"\\mouse.sha",mark=dst+L".cursor-preview-state",bak=dst+L".cursor-preview-backup";if(!exists(mark)){
  if(manuallyBlankSHA()){
   if(cursorDialog(L"检测到此前手动放入的透明 mouse.sha。\n将用本程序内置的已验证的缓存原图恢复；同时保留当前透明文件的唯一备份。确定？",L"恢复手动透明 SHA",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
   const std::wstring manualBackup=dst+L".manual-transparent-backup";
   if(exists(manualBackup))fail("Manual backup already exists; do not overwrite it");
   if(!CopyFileW(dst.c_str(),manualBackup.c_str(),TRUE))fail("Cannot back up manually installed SHA");
   save(dst,original);
   status(L"已从缓存还原原始 SHA。先前手动透明 SHA 已安全备份；请重新启动游戏。");return;
  }
  status(L"没有本工具安装的透明 SHA（或资源与内置缓存不匹配），未更改游戏文件。");return;
 }Bytes m=file(mark);if(m.size()!=1||m[0]>1)fail("Invalid restore marker; use manual backup");if(exists(dst)&&file(dst)!=file(cacheKey+L"\\transparent.sha"))fail("mouse.sha changed after installation; refusing to overwrite. Restore backup manually");if(m[0]){if(!exists(bak)||!CopyFileW(bak.c_str(),dst.c_str(),FALSE))fail("Restore copy failed");}else if(exists(dst)&&!DeleteFileW(dst.c_str()))fail("Cannot remove transparent SHA");if(!DeleteFileW(mark.c_str()))fail("Cannot remove restore marker");if(m[0])DeleteFileW(bak.c_str());installOwned=false;status(L"原文件已恢复；若原先没有散装 mouse.sha，则已删除透明副本。");}
// Detect game-owned Win32 menu dialogs (RA2/YR commonly use #32770 for
// menus). This is limited to the selected game GUI thread, preventing an
// unrelated platform popup from changing the game's cursor mode.
struct MenuScan { DWORD pid; HWND game; bool found; };
static BOOL CALLBACK menuScanWindow(HWND w,LPARAM data){
 auto* p=(MenuScan*)data;
 if(!IsWindowVisible(w))return TRUE;
 DWORD owner=0;GetWindowThreadProcessId(w,&owner);
 if(owner!=p->pid)return TRUE;
 wchar_t cls[40]={};GetClassNameW(w,cls,40);
 if(wcscmp(cls,L"#32770"))return TRUE;
 RECT a{},b{};
 if(!GetWindowRect(w,&a)||!GetWindowRect(p->game,&b))return TRUE;
 int width=a.right-a.left,height=a.bottom-a.top;
 if(width<120||height<70)return TRUE;
 int iw=std::max(0,int(std::min(a.right,b.right)-std::max(a.left,b.left)));
 int ih=std::max(0,int(std::min(a.bottom,b.bottom)-std::max(a.top,b.top)));
 if(iw>60&&ih>45){p->found=true;return FALSE;}
 return TRUE;
}
static bool gameMenuDialogActive(){
 static DWORD checkedAt=0;static HWND checkedWindow=nullptr;static bool cached=false;
 DWORD now=GetTickCount();if(checkedWindow==game&&now-checkedAt<50)return cached;
 checkedAt=now;checkedWindow=game;
 if(!threadId||!IsWindow(game))return false;
 MenuScan scan{pid,game,false};
 EnumThreadWindows(threadId,menuScanWindow,(LPARAM)&scan);
 if(!scan.found)EnumChildWindows(game,menuScanWindow,(LPARAM)&scan);
 cached=scan.found;return cached;
}
static void enterNativeMenu(){
 if(!nativeMenuHold){
  nativeMenuHold=true;lastFrame=-2;lastPhase=size_t(-1);
  ShowWindow(overlay,SW_HIDE);
  if(shared){
   shared->menuSuspend=1;
   shared->nativeEnabled=0;
   // Keep the paired game Hide held throughout the dialog; releasing it
   // here can expose a second game-drawn cursor behind the Windows pointer.
   gameinput::wake(hook);
  }
  log(L"Game Win32 dialog detected; native sprite suspended, Windows menu cursor retained");
 }
}
static void exitNativeMenu(){
 if(!nativeMenuHold)return;
 nativeMenuHold=false;nativeMenuCandidateAt=0;lastFrame=-2;lastPhase=size_t(-1);
 if(shared){
  shared->menuSuspend=0;
  shared->nativeEnabled=1;
  // The original Hide remains paired: only resume native presentation.
  gameinput::wake(hook);
 }
 log(L"Game dialog closed; native display resumes while game Hide remains paired");
}
static void hookStart(bool hide=true){if(!selectGameWindow()||!gameinput::nativeWindow(pid,threadId))fail("Game main window is still initializing; waiting to connect");if(!gameinput::runtimeAvailable())fail("GameRuntime.dll or x86 hook broker missing/incompatible; replace the complete paired package");bridgeFrame.clear();lanboStream.reset();lastSampleProbeAt=0;nativeStartAt=GetTickCount();nativeInvisibleAt=0;hideConfirmed=false;lastRehideSeen=0;wchar_t name[80];mapName(name,pid);mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(Shared),name);if(!mapping)fail("Shared mapping failed");shared=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));if(!shared)fail("Map view failed");memset(shared,0,sizeof(*shared));shared->magic=MAGIC;shared->pid=pid;shared->owner=GetCurrentProcessId();shared->heartbeat=GetTickCount();
 shared->sampledFrame=-1;shared->sampledFirst=-1;
 shared->sampledHotX=-1;shared->sampledHotY=-1;
 shared->cursorCanvasW=asset.w;shared->cursorCanvasH=asset.h;shared->cursorFrameCount=LONG(asset.frames.size());
 shared->sampledCursorID=-1;shared->resourceRequest=1;resourceTries=1;resourceRetryAt=GetTickCount()+5000;
 shared->gameWindow=(DWORD)(uintptr_t)game;
 shared->nativeBlank=noCursorMode?1:0;
 shared->nativeEnabled=(nativeGameRefresh&&!noCursorMode&&mode==L"hide")?1:0;
 shared->sampleIntervalMs=LONG(cursorperf::sampleInterval(pollHz));shared->nativeSeq=0;shared->nativeAck=0;shared->nativeVisible=0;shared->command=hide?1:3;hook=gameinput::install(gameentry::Client::Cursor,threadId,pid,game);
 if(!hook)fail("Unified runtime subscription failed");gameinput::wake(hook);}
static void hookStop(DWORD timeout=1500){
 bridgeFrame.clear();if(!shared&&!hook&&!mapping)return;
 if(shared&&live()){
  shared->heartbeat=GetTickCount();InterlockedExchange(&shared->command,2);DWORD start=GetTickCount();
  while(GetTickCount()-start<timeout&&(shared->command||shared->state!=0)){gameinput::wake(hook);Sleep(10);}
  log((shared->command||shared->state!=0)?L"Hide restore pending; unified watchdog will retry":L"Hide restore acknowledged");
 }
 if(hook){gameinput::release(gameentry::Client::Cursor,hook);hook=0;}
 if(shared){UnmapViewOfFile(shared);shared=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}
}
static bool clickAllowed(POINT p){
 auto deny=[](LONG r){clickDenyReason=r;clickNeedsLog=true;return false;};
 if(!running||!clickEnabled)return deny(1);
 if(clickMode==2||(clickMode==0&&(!aresKnown||nativeFive)))return deny(2);
 if(!clickForegroundMatches())return deny(3);
 if(!refreshClickRegion())return deny(4);
 if(!PtInRect(&clickRegion,p))return deny(5);
 if(!(GetAsyncKeyState(VK_SHIFT)&0x8000)||
    (GetAsyncKeyState(VK_CONTROL)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000))return deny(6);
 clickNeedsLog=false;return true;
}
// Refresh rate of the monitor containing the game's render window, not desktop's primary monitor.
static unsigned monitorRefresh(){
 HWND target=IsWindow(game)?game:mainw;
 MONITORINFOEXW mi={};mi.cbSize=sizeof(mi);
 HMONITOR mon=MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST);
 if(GetMonitorInfoW(mon,reinterpret_cast<LPMONITORINFO>(&mi))){
  DEVMODEW dm={};dm.dmSize=sizeof(dm);
  if(EnumDisplaySettingsExW(mi.szDevice,ENUM_CURRENT_SETTINGS,&dm,0)&&dm.dmDisplayFrequency>=30&&dm.dmDisplayFrequency<=500)return dm.dmDisplayFrequency;
 }
 HDC dc=GetDC(NULL);int f=dc?GetDeviceCaps(dc,VREFRESH):60;if(dc)ReleaseDC(NULL,dc);
 return f>=30&&f<=500?unsigned(f):60;
}
static bool displaySettingsDirty=true;static HMONITOR refreshMonitor=nullptr;static unsigned configuredPollHz=0;
static void updateRefresh(bool force=false){
 DWORD now=GetTickCount();if(!force&&uint32_t(now-lastRefreshSampleAt)<1000)return;
 lastRefreshSampleAt=now;
 HMONITOR mon=MonitorFromWindow(IsWindow(game)?game:mainw,MONITOR_DEFAULTTONEAREST);
 if(!force&&!displaySettingsDirty&&mon==refreshMonitor)return;
 refreshMonitor=mon;displaySettingsDirty=false;detectedRefreshHz=monitorRefresh();
 pollHz=cursorperf::targetRate(configuredPollHz,detectedRefreshHz);
#ifdef RA2_UNIFIED
 suite::publishRefreshRate(pollHz);
#endif
 if(shared)shared->sampleIntervalMs=LONG(cursorperf::sampleInterval(pollHz));
}
static uint32_t inputNow(){LARGE_INTEGER q;if(freq.QuadPart>0&&QueryPerformanceCounter(&q))return uint32_t((q.QuadPart/freq.QuadPart)*1000+(q.QuadPart%freq.QuadPart)*1000/freq.QuadPart);return GetTickCount();}
static void pollPhysicalClick(){
 static bool initialized=false,previous=false;bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
 POINT p{};GetCursorPos(&p);if(!initialized){initialized=true;previous=down;physicalLeftDown=down;return;}
 physicalLeftDown=down;
 if(down!=previous){previous=down;if(down)burst.down(p.x,p.y,(GetAsyncKeyState(VK_SHIFT)&0x8000)&&clickAllowed(p));
  else {bool armed=burst.armed;burst.up(p.x,p.y,armed&&clickAllowed(p),inputNow(),clickGap);}}
 if((GetAsyncKeyState(VK_RBUTTON)|GetAsyncKeyState(VK_MBUTTON)|GetAsyncKeyState(VK_XBUTTON1)|GetAsyncKeyState(VK_XBUTTON2))&0x8000)burst.cancel();
}
static void detectAres(){aresKnown=false;nativeFive=false;HANDLE sn=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(sn==INVALID_HANDLE_VALUE){log(L"Shift5 auto: module detection unavailable, no extra clicks; select manual only if game has no native Shift5");return;}MODULEENTRY32W m={sizeof(m)};if(Module32FirstW(sn,&m)){aresKnown=true;do{std::wstring n=m.szModule;std::transform(n.begin(),n.end(),n.begin(),towlower);if(n.find(L"ares")!=std::wstring::npos)nativeFive=true;}while(Module32NextW(sn,&m));}CloseHandle(sn);log(nativeFive?L"Shift5 auto: Ares module found; use native production, no extra clicks":L"Shift5 auto: no Ares module found; four extra clicks enabled when checked");}
static void clickTick(){
 pollPhysicalClick();
 if(clickNeedsLog){
  DWORD now=GetTickCount();
  if(!clickLoggedAt||uint32_t(now-clickLoggedAt)>=2000){
   clickLoggedAt=now;clickNeedsLog=false;
   DWORD fgPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&fgPid);
   log(L"Shift5 blocked: "+std::wstring(clickReasonText(clickDenyReason))+
       L"; mode="+std::to_wstring(clickMode)+L"; aresKnown="+std::to_wstring(aresKnown)+
       L"; nativeFive="+std::to_wstring(nativeFive)+
       L"; gamePID="+std::to_wstring(pid)+L"; foregroundPID="+std::to_wstring(fgPid)+
       L"; region="+std::to_wstring(clickRegion.right-clickRegion.left)+L"x"+
       std::to_wstring(clickRegion.bottom-clickRegion.top));
  }
 }
 for(int attempt=0;attempt<4&&burst.left;attempt++){
  POINT p;GetCursorPos(&p);
  bool eligible=clickAllowed(p);
  if(!eligible||std::abs(p.x-burst.x)>4||std::abs(p.y-burst.y)>4||physicalLeftDown){
   if(eligible)clickDenyReason=7;
   clickNeedsLog=true;burst.cancel();return;
  }
  if(!burst.ready(inputNow()))return;
  const auto inputResult=gameinput::clickPair(game,pid);UINT sent=inputResult.sent;
  if(sent!=2){
   DWORD err=inputResult.error;++clickSendFail;
   burst.cancel();clickDenyReason=8;clickNeedsLog=true;
   log(L"Shift5 SendInput incomplete: events="+std::to_wstring(sent)+
       L", lastError="+std::to_wstring(err)+L"; cancelled remaining clicks");
  }else{++clickSendOk;burst.sent(inputNow(),clickGap);if(!burst.left)log(L"Shift5 SendInput submitted 8 mouse events for four extra clicks; game acceptance not verified");}
  if(clickGap>0)return;
 }
}
static void stop(){
#ifdef RA2_UNIFIED
 suite::publishHardwareCursor(false);
#endif
bridgeFrame.clear();lanboStream.reset();usingHeldSample=false;lastPhase=size_t(-1);diagFrame=diagHotX=diagHotY=-1;nativeMenuHold=false;nativeMenuCandidateAt=0;previousObservedFrame=-1;fallbackTracker.reset();wasInside=false;refreshTries=0;SetRectEmpty(&clickRegion);burst.cancel();tween.reset();timing.resetObservation();activeMotion.reset();running=false;ShowWindow(overlay,SW_HIDE);if(hud)ShowWindow(hud,SW_HIDE);hookStop();status(mode==L"sha"?L"外部光标已停止。透明版游戏内可能看不到指针；Ctrl+Alt+F11 可重新启动。":L"已停止；如使用 HideCursor，已请求配对恢复操作。");}
static void start(){if(!playbackReady){status(L"准备播放缓存，完成后自动连接游戏");return;}if(running)return;if(!live()||!IsWindow(game))attach();if(!loaded)fail("Bundled original.sha or palette.bgra missing; check assets folder");if(!readRestricted)mouse();
 bool managedBlank=exists(gameDir+L"\\mouse.sha.cursor-preview-state");
 bool manualBlank=!managedBlank && manuallyBlankSHA();
 bool transparent=managedBlank||manualBlank;
 if(mode!=L"sha"&&transparent)fail("Transparent mouse.sha detected. Use mode A, or restore the original SHA and restart the game");
 if(mode==L"sha"&&!transparent)fail("Mode A requires the app-installed SHA or a byte-for-byte matching manual blank mouse.sha in the detected game EXE directory");
 if(manualBlank)log(L"检测到手动安装的透明 SHA（无恢复标记）；不会自动替你备份或恢复，请保留原版文件。");
 // v1.2.7: arm native HideCursor IMMEDIATELY as requested. The bridge
 // conditionally re-hides if game ShowCursor changes its internal ref count.
 if(mode==L"hide"){try{hookStart();}catch(...){hookStop();throw;}}
 else if(asset.frames.size()<517){try{hookStart(false);}catch(const std::exception& e){hookStop();log(L"YR resource probe unavailable: "+widen(e.what()));}}
 nativeStartAt=GetTickCount();lastNativePublishAt=0;nativeInvisibleAt=0;hideConfirmed=false;detectAres();burst.cancel();tween.reset();timing.resetObservation();activeMotion.reset();running=true;lastFrame=-2;lastPhase=size_t(-1);status(L"已连接游戏");}
static void browseGame(){
 BROWSEINFOW bi={};bi.hwndOwner=mainw;bi.lpszTitle=L"选择包含 gamemd.exe、gamemd-spawn.exe 或 gamemd-ares.exe 的游戏文件夹";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
 PIDLIST_ABSOLUTE pidl=SHBrowseForFolderW(&bi);if(!pidl)return;
 wchar_t folder[MAX_PATH]={};BOOL ok=SHGetPathFromIDListW(pidl,folder);CoTaskMemFree(pidl);
 if(!ok)fail("Cannot retrieve selected game directory");
 std::wstring dir(folder);
 if(!exists(dir+L"\\gamemd.exe")&&!exists(dir+L"\\gamemd-spawn.exe")&&!exists(dir+L"\\gamemd-ares.exe"))fail("Selected directory has no supported gamemd EXE");
 if(running)stop();
 setDir(dir);loadCache();status(L"游戏目录已选择；450 帧原版 SHA、调色板及透明资源已就绪，不需要抓取游戏资源。");
}
static void applyHideMode(){
 int choice=(int)SendMessageW(modeBox,CB_GETCURSEL,0,0);if(choice<0||choice>3)fail("Select an original cursor display mode");
 std::wstring selected=(choice==1||choice==3)?L"hide":choice==2?L"sha":L"keep";
 bool blank=choice==3;
 if(selected==mode&&blank==noCursorMode)return;
 if(selected==L"sha"&&anyGame()&&!exists(gameDir+L"\\mouse.sha.cursor-preview-state")&&!manuallyBlankSHA())fail("Exit the game first, then install the transparent SHA; restart the game before choosing this mode");
 bool shouldResume=running;
 if(running)stop();
 if(selected!=L"hide"&&nativeGameRefresh){nativeGameRefresh=false;SendMessageW(nativeBox,CB_SETCURSEL,0,0);WritePrivateProfileStringW(L"Display",L"GameThreadRefresh",L"0",ini().c_str());}
 noCursorMode=blank;
 mode=selected;WritePrivateProfileStringW(L"Startup",L"Mode",mode.c_str(),ini().c_str());
 WritePrivateProfileStringW(L"Compatibility",L"CursorNoCodeWriteDefaultsVersion",L"1",ini().c_str());
 if(noCursorMode)status(L"已选择无光标诊断模式；Ctrl+Alt+F11 停止并恢复，F10 切换覆盖层。");
 else if(mode==L"sha")status(L"已选择替换 SHA：请退出游戏后点击“安装透明 SHA”，再重新启动游戏。安装前会备份现有文件。");
 else if(mode==L"hide")status(L"高危：隐藏原光标会修改游戏内提示入口以保留名称和价格；联机平台可能不兼容。");
 else status(L"已选择不隐藏：不修改游戏文件，原始光标和外部光标可能同时出现。");
 if(shouldResume){try{start();}catch(const std::exception& e){autoPaused=true;status(L"切换后未能启动："+widen(e.what()));}}
}
static void autoTick(){DWORD now=GetTickCount();if(uint32_t(now-lastAutoCheck)<1000)return;lastAutoCheck=now;if(autoPaused&&cursorReader.valid()&&!live())autoPaused=false;if(!autoStart||autoPaused||running||!playbackReady)return;try{attach(true);start();}catch(const std::exception& e){status(L"自动等待："+widen(e.what()));}}
static void leaveGame(){
 bridgeFrame.clear();lanboStream.reset();usingHeldSample=false;
 // Leaving the client changes presentation, not the subscription lifetime.
 // Keep our paired native hide and tooltip slot; Runtime releases only its
 // Windows cursor presentation while the desktop/other app owns the pointer.
 if(mode==L"hide"&&shared){shared->menuSuspend=2;gameinput::wake(hook);}nativeInvisibleAt=0;
 lastPhase=size_t(-1);nativeMenuHold=false;nativeMenuCandidateAt=0;previousObservedFrame=-1;fallbackTracker.reset();if(wasInside){lastFrame=-2;tween.reset();timing.resetObservation();activeMotion.reset();}wasInside=false;refreshTries=0;burst.cancel();SetRectEmpty(&clickRegion);ShowWindow(overlay,SW_HIDE);if(hud)ShowWindow(hud,SW_HIDE);}
static void refreshGameCursor(){if(nativeGameRefresh&&mode==L"hide")return;/* Do not force Windows arrow while native cursor owns it. */if(!focusRefresh||refreshTries<=0||int32_t(GetTickCount()-refreshAt)<0)return;--refreshTries;refreshAt=GetTickCount()+150;gameinput::refreshCursor(game,pid);}
static void bitmap(int w,int h){if(dib&&dibw==w&&dibh==h)return;if(dib){SelectObject(memdc,oldbm);DeleteObject(dib);dib=0;}if(!memdc)memdc=CreateCompatibleDC(NULL);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;dib=CreateDIBSection(memdc,&bi,DIB_RGB_COLORS,&dibbits,NULL,0);if(!dib)fail("Overlay bitmap failed");oldbm=(HBITMAP)SelectObject(memdc,dib);dibw=w;dibh=h;}
static bool hot(int v,int size,int& out){if(v==0){out=0;return true;}if(v==12345){out=size/2;return true;}if(v==54321){out=size-1;return true;}return false;}
static void pollOriginalResource(){
 if(shared&&shared->resourceState==3&&asset.frames.size()<517&&shared->sampleReadError==3&&resourceTries<3&&int32_t(GetTickCount()-resourceRetryAt)>=0){
  shared->resourceState=0;shared->resourceRequest=1;++resourceTries;resourceRetryAt=GetTickCount()+5000;gameinput::wake(hook);
 }
 if(!shared||(shared->resourceState!=2&&shared->resourceState!=-1&&shared->resourceState!=-2))return;
 LONG state=shared->resourceState,seq=shared->resourceSeq;
 if(state<0){shared->resourceState=3;log(L"原始光标资源采集暂不可用；保留已有缓存");return;}
 if(!cursorseq::committed(uint32_t(seq)))return;
 unsigned n=shared->resourceBytes;if(n<8||n>RESOURCE_MAX_BYTES){shared->resourceState=3;return;}
 Bytes bytes(n),colors(1024);memcpy(bytes.data(),shared->resourcePixels,n);memcpy(colors.data(),shared->resourcePalette,1024);
 MemoryBarrier();if(seq!=shared->resourceSeq)return;shared->resourceState=3;
 try{
  Asset captured=decode(bytes);if(captured.w!=55||captured.h!=43||captured.frames.size()<450)fail("Unsupported original YR cursor layout");
  bool visible=false;for(const auto& f:captured.frames)for(auto pixel:f.pixels)if(pixel){visible=true;break;}
  if(!visible)fail("Original cursor resource is entirely transparent");
  Bytes canonical=encode(captured,false);
  if(canonical!=original){
   // Remember an exact manual blank before extending its source to 517 frames.
   // No game file is changed by automatic source discovery.
   if(manuallyBlankSHA())save(cacheKey+L"\\legacy-transparent.sha",file(gameDir+L"\\mouse.sha"));
   save(cacheKey+L"\\original.sha",canonical);save(cacheKey+L"\\palette.bgra",colors);
   original=std::move(canonical);asset=std::move(captured);memcpy(palette,colors.data(),1024);palette[0]=0;loaded=true;
   bridgeFrame.clear();fallbackTracker.reset();rebuildDisplay();
   if(!exists(gameDir+L"\\mouse.sha.cursor-preview-state"))save(cacheKey+L"\\transparent.sha",encode(asset,true));
  }
  shared->cursorFrameCount=LONG(asset.frames.size());shared->cursorCanvasW=asset.w;shared->cursorCanvasH=asset.h;
  log(L"原始光标资源已缓存 · "+std::to_wstring(asset.frames.size())+L" 帧");
 }catch(const std::exception& e){log(L"Original resource rejected: "+widen(e.what()));}
}
static void draw(){
#ifdef RA2_UNIFIED
 suite::publishHardwareCursor(running&&!manualCursorPause&&nativeGameRefresh&&!noCursorMode&&mode==L"hide"&&shared&&shared->nativeEnabled&&!shared->menuSuspend);
#endif
 cursorReader.begin(); // task-local diagnostics; immediate reads never cache old frames
 pollOriginalResource();if(!running)return;if(shared){shared->heartbeat=GetTickCount();
  LONG repairs=shared->postSetCursorRepairs;
  DWORD stamp=GetTickCount();
  if(repairs!=lastPostRepairSeen && uint32_t(stamp-lastPostRepairLogAt)>=2000){
   lastPostRepairSeen=repairs;lastPostRepairLogAt=stamp;
   log(L"Native SetCursor repaired after game WndProc; total="+std::to_wstring(repairs));
  }
  LONG rehide=shared->rehideCount;
  if(rehide!=lastRehideSeen){
   log(L"Game ShowCursor detected; conditional HideCursor reapplied: +"+std::to_wstring(rehide-lastRehideSeen)+L", total="+std::to_wstring(rehide)+L", ref="+std::to_wstring(shared->observedRef));
   lastRehideSeen=rehide;
  }
  if(shared->state==-1){LONG err=shared->error;if(nativeGameRefresh)autoPaused=true;stop();status(L"HideCursor 未执行，错误码 "+std::to_wstring(err)+L"；没有强制绕过校验。");return;}if(shared->state==1&&!hideConfirmed){hideConfirmed=true;log(L"Hide bridge running on game GUI thread; frame sampled="+std::to_wstring(shared->sampledFrame));}if(hideConfirmed&&shared->state==0&&shared->command==0&&!nativeMenuHold&&!shared->menuSuspend){stop();status(L"HideCursor 已恢复或看门狗超时，已停止外部显示。");return;}}
 if(nativeGameRefresh&&!noCursorMode&&mode==L"hide"&&shared&&!nativeMenuHold&&!shared->menuSuspend){
  if(shared->nativeVisible)nativeInvisibleAt=0;
  else if(!nativeInvisibleAt)nativeInvisibleAt=GetTickCount();
  else if(uint32_t(GetTickCount()-nativeInvisibleAt)>2000){
   autoPaused=true;stop();status(L"游戏线程光标未显示，已恢复原光标");return;
  }
 }
 if(!live()){stop();status(L"游戏已退出。透明版可点击恢复 SHA。");return;}
 HWND fg=GetForegroundWindow();DWORD fp=0;GetWindowThreadProcessId(fg,&fp);
 bool compatibleForeground=(fp==pid)||platformHostMatches(fg,fp);
 if(!compatibleForeground){observedForeground=fg;leaveGame();visibilityDiagnostic(L"前台不是游戏或已确认的平台宿主窗口",fp);return;}
 if(nativeGameRefresh&&!noCursorMode&&mode==L"hide"&&shared){
  if(gameMenuDialogActive()){enterNativeMenu();return;}
  exitNativeMenu();
  // A transient modal dialog may have set the bridge flag before the host
  // sees a visible dialog; do not remain stuck in the menu cursor state.
  if(shared->menuSuspend==1&&!nativeMenuHold){shared->menuSuspend=0;shared->nativeEnabled=1;}
  shared->gameWindow=(DWORD)(uintptr_t)game;
 }
 if(!IsWindow(game)||fg!=observedForeground||!IsWindowVisible(game)){observedForeground=fg;lastWindowCheck=GetTickCount();DWORD oldThread=threadId;HWND oldWindow=game;selectGameWindow();if(game!=oldWindow){lastFrame=-2;lastPhase=size_t(-1);wasInside=false;log(L"Render window changed; reacquired without activation");}if(shared&&oldThread!=threadId){stop();status(L"游戏窗口线程变化，等待自动重新连接 HideCursor。");return;}}
 POINT pos;GetCursorPos(&pos);RECT rc={};POINT org={0,0};
 if(!IsWindow(game)||!IsWindowVisible(game)||IsIconic(game)||
    !GetClientRect(game,&rc)||rc.right<=0||rc.bottom<=0||!ClientToScreen(game,&org)||
    (!noCursorMode && (pos.x<org.x||pos.y<org.y||pos.x>=org.x+rc.right||pos.y>=org.y+rc.bottom))){
  leaveGame();visibilityDiagnostic(L"游戏客户区未显示或鼠标不在客户区",fp);return;
 }
 // A different game-owned window may be foreground (e.g. launcher overlay),
 // but a different unrelated desktop app must never receive the cursor.
 if(fp==pid&&fg!=game&&GetAncestor(fg,GA_ROOT)!=GetAncestor(game,GA_ROOT)){
  leaveGame();visibilityDiagnostic(L"游戏前台窗口与渲染窗口不一致",fp);return;
 }
 if(!wasInside){
  // Normally the subscription survived Alt-Tab; resume presentation in place.
  if(shared&&shared->menuSuspend==2){shared->menuSuspend=0;nativeInvisibleAt=0;gameinput::wake(hook);}
  // First attach / explicit pause recovery still needs a subscription.
  if(mode==L"hide"&&!manualCursorPause&&!hook){try{hookStart();}catch(const std::exception& e){hookStop();log(L"Hook re-arm failed: "+widen(e.what()));stop();return;}}
  hudEpoch=inputNow();nativeMenuHold=false;nativeMenuCandidateAt=0;previousObservedFrame=-1;fallbackTracker.reset();hudUpdates=hudCommits=hudMotion=hudChanges=0;hudLastFrame=-1;hudPaintAt=0;wasInside=true;lastFrame=-2;lastPhase=size_t(-1);tween.reset();timing.resetObservation();activeMotion.reset();refreshTries=3;refreshAt=GetTickCount();if(!nativeGameRefresh)overlaystack::place(overlay,overlaystack::Layer::Cursor,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);log(L"Game focus/client re-entered: redraw and cursor refresh, no simulated activation clicks");}
 if(noCursorMode){
  ShowWindow(overlay,SW_HIDE);diagAnimation=L"无光标诊断：不发布鼠标图像";
  if(!manualCursorPause&&shared&&shared->state!=1&&uint32_t(GetTickCount()-nativeStartAt)>2000){
   autoPaused=true;stop();status(L"无光标模式：隐藏未确认，已停止并请求恢复");
  }
  return;
 }
 if(manualCursorPause){ShowWindow(overlay,SW_HIDE);return;}
 refreshGameCursor();
 if(GetTickCount()-lastCursorCheck>(systemCursorGuard?100:1000)){lastCursorCheck=GetTickCount();CURSORINFO ci={sizeof(ci)};if(GetCursorInfo(&ci)){int v=(ci.flags&CURSOR_SHOWING)?1:0;if(v!=lastSystemVisible){// When Hook hides the in-game sprite, an independent Windows cursor
   // visibility change must not incorrectly suppress our overlay.
   if(systemCursorGuard&&mode!=L"hide"&&lastSystemVisible==0&&v==1&&uint32_t(GetTickCount()-lastObservedFrameAt)<1000)nativeMenuCandidateAt=GetTickCount();
 lastSystemVisible=v;if(v==0){nativeMenuHold=false;nativeMenuCandidateAt=0;}
 log(v?L"Windows cursor visible in game; possible extra system-cursor layer":L"Windows cursor not visible in game");}}}
 LONG sampleFrame=-1,sampleFirst=-1,sampleCount=0,sampleHotX=-1,sampleHotY=-1,sampleGW=0,sampleGH=0,sampleCursorID=-1;
 usingHeldSample=false;
 if(mode==L"hide"&&shared&&shared->state==1){
  LONG seq=shared->sampleSeq;MemoryBarrier();
  if(cursorseq::committed(uint32_t(seq))){
   cursorsnap::Sample candidate;
   candidate.seq=seq;
   candidate.frame=shared->sampledFrame;candidate.first=shared->sampledFirst;
   candidate.count=shared->sampledCount;candidate.cursorID=shared->sampledCursorID;
   candidate.hotX=shared->sampledHotX;candidate.hotY=shared->sampledHotY;
   candidate.gameW=shared->sampledGameW;candidate.gameH=shared->sampledGameH;
   candidate.tick=shared->sampledTick;
   MemoryBarrier();
   if(seq==shared->sampleSeq)
    bridgeFrame.observe(candidate,int(asset.frames.size()),asset.w,asset.h);
  }
  // The 10ms game-thread timer may pause during menus/other GUI activity.
  // Keep the last VERIFIED sprite for this connection; no fake new frames/FPS.
  if(bridgeFrame.has()){
   const auto& stable=bridgeFrame.value();
   sampleFrame=stable.frame;sampleFirst=stable.first;sampleCount=stable.count;
   sampleHotX=stable.hotX;sampleHotY=stable.hotY;
   sampleGW=stable.gameW;sampleGH=stable.gameH;sampleCursorID=stable.cursorID;
   usingHeldSample=bridgeFrame.age(GetTickCount())>=400;
  }
 }
 int frame=-1,hx=0,hy=0,w=24,h=32,cursorID=-1,animKey=-1,animStart=0,animCount=0,recognition=0,layout=0;bool inferredRange=false;int candidates=0;bool approx=false;diagIndex=diagLegacy=0xffffffff;
 auto useBridgeSample=[&](){
  if(mode==L"hide"&&shared&&shared->state==1&&
     sampleFrame>=0&&sampleFrame<(LONG)asset.frames.size()){
   frame=sampleFrame;w=asset.w;h=asset.h;cursorID=sampleCursorID;
   animStart=sampleFirst;animCount=sampleCount;
   auto hp=nativehot::resolve(w,h,0,0,0,0,sampleHotX,sampleHotY,[](int,int){return false;});
   {
    if(hp.usedGame){hx=hp.x;hy=hp.y;approx=false;}
    else{hx=0;hy=0;approx=true;} // user-configurable offsets still apply

    // Unknown animation table is NOT evidence that this cursor is static.
    // Keep the tracker across successive valid bridge samples.
    recognition=1;
    if(sampleFirst>=0&&sampleCount>1&&cursorRangeMatches(sampleFirst,sampleCount,frame,asset.frames.size())){
     animStart=sampleFirst;animCount=sampleCount;animKey=900+animStart;recognition=4;
    }else{
     const auto inferred=fallbackTracker.observe(frame,fallbackDefs,asset.frames.size());
     candidates=inferred.choices;
     if(inferred.inferred){animStart=inferred.first;animCount=inferred.count;
      animKey=inferred.key;recognition=4;inferredRange=true;}
    }
   }
  }
 };
 useBridgeSample();
 if(frame<0&&!readRestricted)try{uint32_t p=mouse();Bytes state=read(p,0x58);uint32_t im=imageBase(p);Bytes ih=read(im,8);if(u16(ih,2)!=asset.w||u16(ih,4)!=asset.h||u16(ih,6)>asset.frames.size())fail("Image layout changed; recapture this mod");frame=(int)u32(state,8);if(frame<0||frame>=int(asset.frames.size()))fail("Frame out of range");w=asset.w;h=asset.h;
 const Frame& cf=asset.frames[frame];
 auto exactHot=nativehot::resolve(w,h,cf.x,cf.y,cf.w,cf.h,
   (int)u32(state,0x3c),(int)u32(state,0x40),[](int,int){return false;});
 // ScrollClass ends at 0x555C: mini flag at 0x555C, index at 0x5560.
 // Only accept a candidate whose table range contains the actual WWMouse frame.
 uint32_t idx=0xffffffff;diagIndex=diagLegacy=0xffffffff;recognition=1;
 for(int candidate=0;candidate<2;candidate++){try{uint32_t raw=word(0x87f7e8+(candidate?0x555c:0x5560));if(candidate)diagLegacy=raw;else diagIndex=raw;if(raw>=86)continue;Bytes d=read(0x82d028+raw*28,28);unsigned miniFlag=read(0x87f7e8+(candidate?0x5558:0x555c),1)[0];if(miniFlag>1)continue;bool mini=miniFlag!=0;int a=(int)u32(d,mini?12:0),n=(int)u32(d,mini?16:4);if(!cursorRangeMatches(a,n,frame,asset.frames.size()))continue;idx=raw;cursorID=int(raw);layout=candidate?2:1;recognition=n==1?2:3;animStart=a;animCount=n;if(n>1)animKey=(int)raw*2+(mini?1:0);break;}catch(...){}}

 auto inferred=fallbackTracker.observe(frame,fallbackDefs,asset.frames.size());candidates=inferred.choices;
 if(inferred.inferred&&recognition!=3){animKey=inferred.key;animStart=inferred.first;animCount=inferred.count;recognition=4;inferredRange=true;}
 if(exactHot.usedGame){hx=exactHot.x;hy=exactHot.y;}else if(idx<86){Bytes def=read(0x82d028+idx*28,28);int a=u32(def,0),n=u32(def,4),ma=u32(def,12),mn=u32(def,16);if(!((frame>=a&&int64_t(frame)<int64_t(a)+n)||(ma>=0&&frame>=ma&&int64_t(frame)<int64_t(ma)+mn))||!hot(u32(def,20),w,hx)||!hot(u32(def,24),h,hy))approx=true;}else approx=true;
 if(approx)fail("No verified cursor hotspot");
 }catch(const std::exception& hostError){
  DWORD stamp=GetTickCount();
  if(!lastHostReadLogAt||uint32_t(stamp-lastHostReadLogAt)>=5000){
   lastHostReadLogAt=stamp;
   log(L"Host cursor read unavailable; trying game-thread snapshot: "+widen(hostError.what()));
  }
  frame=-1;hx=hy=0;w=24;h=32;approx=true;recognition=0;
  useBridgeSample();
  // No new reading is NOT a reason to throw away animation history.
  // It is reset explicitly on detach, loss of focus, or a new hook session.

 }
 // Game menus and context switches can leave a pointer to an old last-drawn
 // frame. Never synthesize a potentially misleading arrow on a failed read.
 if(frame<0){
  diagFrame=-1;diagHotX=diagHotY=-1;
  ShowWindow(overlay,SW_HIDE);
  if(mode==L"hide"&&shared&&uint32_t(GetTickCount()-nativeStartAt)<4000){
   // Probe at most ten times/sec. Keep HUD visible for diagnostics.
   DWORD now=GetTickCount();
   if(uint32_t(now-lastSampleProbeAt)>=100){lastSampleProbeAt=now;gameinput::wake(hook);}
   diagAnimation=L"等待 Hook 首次发布有效光标帧（尚非静态帧）";
   return;
  }
  if(mode==L"hide"){
   // Never keep the original pointer hidden if we have NEVER received one
   // usable frame in this connection. This case differs from a stalled frame.
   autoPaused=true;stop();status(L"Hook 已连接但首次光标帧超时；已请求恢复原光标");
  }else visibilityDiagnostic(L"光标帧读取失败",fp);
  lastFrame=-2;tween.reset();activeMotion.reset();
  return;}
 commandcursor::Mode commandMode=commandcursor::None;
 auto hint=commandcursor::read(pid,GetTickCount());
 // Preserve native menu/tools and sidebar/minimap cursors. Release is checked
 // at display cadence, even if the Tactical thread has not run again yet.
 bool mapPoint=pos.x>=org.x&&pos.y>=org.y&&pos.x<org.x+rc.right&&pos.y<org.y+rc.bottom;
 int mapGW=gameWOverride?gameWOverride:sampleGW>0?sampleGW:clickGameWidth>0?clickGameWidth:1024;
 if(mapGW>sidebarWidth&&int64_t(pos.x-org.x)*mapGW>=int64_t(rc.right)*(mapGW-sidebarWidth))mapPoint=false;
 if(hint.mode!=commandcursor::None&&mapPoint&&!nativeMenuHold&&!(shared&&shared->menuSuspend)&&
    (commandcursor::ordinary(cursorID)||(hint.mode==commandcursor::Enter&&cursorID==25))&&gameinput::heldBinding(hint.binding))commandMode=hint.mode;
 if(commandMode!=shownCommand){shownCommand=commandMode;commandSince=GetTickCount();lastFrame=-2;lastPhase=size_t(-1);tween.reset();timing.resetObservation();activeMotion.reset();}
 if(commandMode!=commandcursor::None){
  const auto&d=commandDefs[commandcursor::definition(commandMode)];int chx=0,chy=0;
  if(cursorRangeMatches(d.first,d.count,d.first,asset.frames.size())&&hot(unsigned(d.hotX),w,chx)&&hot(unsigned(d.hotY),h,chy)){
   unsigned duration=unsigned(std::clamp(d.interval,1,60))*1000u/60u;
   frame=d.first+int(uint32_t(GetTickCount()-commandSince)/std::max(1u,duration)%unsigned(d.count));
   animStart=d.first;animCount=d.count;animKey=2000+int(commandMode);hx=chx;hy=chy;approx=false;inferredRange=false;
  }else commandMode=commandcursor::None;
 }
 const auto visualCenter=superCenter?superhot::resolve(superCenters,cursorID,frame,animStart,animCount):superhot::Point{};
 diagFrame=frame;diagHotX=visualCenter.valid?int(std::lround(visualCenter.x)):hx;diagHotY=visualCenter.valid?int(std::lround(visualCenter.y)):hy;
 // Real animation frames can advance without an accessible game definition
 // table on some platform builds; infer only after multiple forward edges.
 if(commandMode==commandcursor::None&&mode==L"hide"&&shared&&shared->state==1){
  lanboStream.observe(frame,GetTickCount());
  if(animKey<0&&lanboStream.predicts(frame,asset.frames.size())){
   const auto& aa=asset.frames[frame];const auto& bb=asset.frames[frame+1];
   // Compare full-canvas support masks, because SHA frames are cropped.
   static std::vector<unsigned char> maskA,maskB;
   maskA.assign(size_t(asset.w)*asset.h,0);maskB.assign(maskA.size(),0);
   for(int y=0;y<aa.h;y++)for(int x=0;x<aa.w;x++)
    maskA[size_t(y+aa.y)*asset.w+x+aa.x]=(aa.pixels[y*aa.w+x]!=0);
   for(int y=0;y<bb.h;y++)for(int x=0;x<bb.w;x++)
    maskB[size_t(y+bb.y)*asset.w+x+bb.x]=(bb.pixels[y*bb.w+x]!=0);
   if(cursorstream::plausible(maskA.data(),maskB.data(),asset.w,asset.h,asset.w,asset.w)){
    animKey=160;animStart=frame;animCount=2;recognition=4;inferredRange=true;
   }
  }
 }
 if(usingHeldSample)diagAnimation=L"Hook 样本暂无更新，保持上次已验证图像";
 DWORD cursorTime=GetTickCount();if(frame!=previousObservedFrame){
  previousObservedFrame=frame;lastObservedFrameAt=cursorTime;
  nativeMenuCandidateAt=0;nativeMenuHold=false;
 }
 bool animationChanged=frame!=tween.frame||animKey!=tween.group;
 uint32_t animationNow=inputNow();timing.observe(frame,animKey,animStart,animCount,animationNow);
 if(animationChanged)transitionMs=timing.duration(animKey,smoothMs,adaptive);
 if(animKey==160)transitionMs=lanboStream.duration(smoothMs);
 tween.update(frame,animKey,animStart,animCount,animationNow,smoothing);
 int predictedNext=(animKey>=0&&animCount>1)?animStart+(frame-animStart+1)%animCount:-1;
 // A guessed fallback range must not synthesize the artificial last->first jump.
 if(inferredRange&&frame==animStart+animCount-1)predictedNext=-1;
 if(animKey>=0&&animCount>1){previewStart=animStart;previewCount=animCount;}
 if(animationChanged){
  activeMotion.reset();lastPhase=size_t(-1);
  // Predict the NEXT real frame immediately after a new boundary, rather than
  // wasting a whole interval interpolating the PREVIOUS-to-current pair.
  if(smoothing&&predictedNext>=0){auto it=motionCache.find(pairKey(frame,predictedNext));if(it!=motionCache.end())activeMotion=it->second;}
 }
 if(predictedNext>=0&&!allowedPairs.count(pairKey(frame,predictedNext)))predictedNext=-1;
 if(smoothing&&predictedNext>=0&&!activeMotion){auto it=motionCache.find(pairKey(frame,predictedNext));if(it!=motionCache.end())activeMotion=it->second;}
 uint32_t animationElapsedMs=animationNow-timing.changedAt;
 bool stalled=animationElapsedMs>transitionMs*2u;
 size_t phaseIndex=size_t(-1);
 if(smoothing&&predictedNext>=0&&activeMotion&&!stalled&&transitionMs){
  size_t samples=activeMotion->images.size()-1;
  // Hold the last predicted sample until an actual frame arrives; no fake
  // 1000Hz display claim. Stalled animations drop to the real frame.
  phaseIndex=std::min(samples-1,size_t((uint64_t(animationElapsedMs)*samples+transitionMs/2)/transitionMs));
 }

 if(uint32_t(GetTickCount()-lastTimingReadout)>=500){
  lastTimingReadout=GetTickCount();
  std::wstring text=!smoothing?L"补帧已关闭":(animKey<0?L"静态/检测中":
    L"补帧 "+std::to_wstring(transitionMs)+L"ms  |  "+(activeMotion?L"时间轴就绪":L"预生成中"));
  if(IsWindowVisible(timingReadout))textIfChanged(timingReadout,text);
 }

 if(approx!=fallback){fallback=approx;log(L"Verified full-canvas hotspot active");}
 int gw=sampleGW,gh=sampleGH;if(gw<320||gh<200){try{uint32_t surface=word(0x887308);Bytes wh=read(surface+4,8);gw=(int)u32(wh,0);gh=(int)u32(wh,4);}catch(...){}}
 if(gw>=320&&gh>=200){clickGameWidth=gw;clickGameHeight=gh;}
 static HMONITOR geometryMonitor=nullptr;static DWORD geometryAt=0;static int mw=0,mh=0;static UINT dx=96,dy=96;
 HMONITOR mon=MonitorFromWindow(game,MONITOR_DEFAULTTONEAREST);
 if(mon!=geometryMonitor||GetTickCount()-geometryAt>=1000){
  geometryMonitor=mon;geometryAt=GetTickCount();MONITORINFO mi={sizeof(mi)};GetMonitorInfoW(mon,&mi);mw=mi.rcMonitor.right-mi.rcMonitor.left;mh=mi.rcMonitor.bottom-mi.rcMonitor.top;
  typedef HRESULT(WINAPI* DpiFn)(HMONITOR,int,UINT*,UINT*);static HMODULE shcore=LoadLibraryW(L"shcore.dll");static DpiFn getDpi=shcore?(DpiFn)GetProcAddress(shcore,"GetDpiForMonitor"):NULL;if(getDpi&&FAILED(getDpi(mon,0,&dx,&dy)))dx=dy=96;
 }
 int useGW=gameWOverride?gameWOverride:gw,useGH=gameHOverride?gameHOverride:gh;int useSW=screenWOverride?screenWOverride:mw,useSH=screenHOverride?screenHOverride:mh;double dp=dpiOverride?dpiOverride:dx*100.0/96;
 int displayW=mw>0?(int)std::lround(double(rc.right)*useSW/mw):rc.right,displayH=mh>0?(int)std::lround(double(rc.bottom)*useSH/mh):rc.bottom;
 refreshClickRegion();
 Scale sc=cursorScale(autoScale,aspectFit,scalePercent,displayW,displayH,useGW,useGH);double wf=dpiMode==1?dp/100:dpiMode==2?100/dp:1;sc.x=std::max(.125,std::min(8.0,sc.x*wf));sc.y=std::max(.125,std::min(8.0,sc.y*wf));int dw=std::min(1024,scaledSize(w,sc.x)),dh=std::min(1024,scaledSize(h,sc.y));int shx=superhot::scaledAnchor(hx,visualCenter,false,w,dw,offsetX),shy=superhot::scaledAnchor(hy,visualCenter,true,h,dh,offsetY);
 if(GetTickCount()-lastReadout>1000){
  lastReadout=GetTickCount();
  wchar_t info[260];
  swprintf(info,260,L"游戏 %d×%d  |  显示器 %d×%d  |  DPI %.0f%%  |  %uHz  |  输出 %d×%d",gw,gh,mw,mh,dx*100.0/96,detectedRefreshHz,dw,dh);
  if(IsWindowVisible(readout))textIfChanged(readout,info);
 }

 static int committedHotX=-9999,committedHotY=-9999;
 static commandcursor::Mode committedCommand=commandcursor::None;
 static const playback::Frame* submittedImage=nullptr;
 if(frame!=lastFrame||phaseIndex!=lastPhase||!dib||dibw!=dw||dibh!=dh||committedHotX!=shx||committedHotY!=shy||committedCommand!=commandMode){
  bool packetSent=true;
  const playback::Frame& source=(phaseIndex!=size_t(-1)&&activeMotion)?*activeMotion->images[phaseIndex]:displayFrames.at(size_t(frame));
  bool changed=&source!=submittedImage||!dib||dibw!=dw||dibh!=dh||committedHotX!=shx||committedHotY!=shy||lastFrame==-2||committedCommand!=commandMode;
  if(changed){
  bitmap(dw,dh);uint32_t* pixels=(uint32_t*)dibbits;
  const auto& raster=rasterCache.get(source,dw,dh);memcpy(pixels,raster.data(),raster.size()*4);
  if(commandMode!=commandcursor::None)for(int y=0;y<dh;++y)for(int x=0;x<dw;++x)
   pixels[size_t(y)*dw+x]=commandcursor::recolor(pixels[size_t(y)*dw+x],commandMode,x*w/dw,y*h/dh,w,h);
  POINT dst={pos.x-shx,pos.y-shy},src={0,0};SIZE sz={dw,dh};BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
  // Optional in-game native hardware cursor. The renderer below stays intact
  // for the regular overlay mode and the existing per-frame timeline.
  if(nativeGameRefresh&&mode==L"hide"&&shared){
   // Avoid creating >refresh-rate GDI cursor handles; if the game GUI thread
   // has not applied the previous frame, wait up to 40ms then retry.
   DWORD stamp=GetTickCount();
   unsigned step=std::max(4u,1000u/pollHz);
   if((lastNativePublishAt&&uint32_t(stamp-lastNativePublishAt)<step)||
      (shared->nativeSeq!=shared->nativeAck&&uint32_t(stamp-lastNativePublishAt)<40))packetSent=false;
   else{
    lastNativePublishAt=stamp;
    // Restrict native Win32 cursor size, preserving the hotspot ratios.
    int nw=std::min(dw,NATIVE_MAX_DIM),nh=std::min(dh,NATIVE_MAX_DIM);
    int nhx=std::clamp(int((int64_t(shx)*nw)/std::max(1,dw)),0,nw-1);
    int nhy=std::clamp(int((int64_t(shy)*nh)/std::max(1,dh)),0,nh-1);
    LONG serial=shared->nativeSeq;
    InterlockedExchange(&shared->nativeSeq,serial|1);
    shared->nativeW=nw;shared->nativeH=nh;
    shared->nativeHotX=nhx;shared->nativeHotY=nhy;
    for(int ny=0;ny<nh;ny++)for(int nx=0;nx<nw;nx++)
      shared->nativePixels[ny*nw+nx]=pixels[(ny*dh/nh)*dw+(nx*dw/nw)];
    MemoryBarrier();InterlockedExchange(&shared->nativeSeq,LONG(cursorseq::next(uint32_t(serial))));
   }
  }else if(!UpdateLayeredWindow(overlay,NULL,&dst,&sz,memdc,&src,0,&bf,ULW_ALPHA))fail("Layered window update failed");
  }
  if(packetSent){committedCommand=commandMode;committedHotX=shx;committedHotY=shy;lastFrame=frame;lastPhase=phaseIndex;if(changed){submittedImage=&source;++hudCommits;if(phaseIndex!=size_t(-1))++hudMotion;}}
 }
 if(mode!=L"hide"&&nativeMenuCandidateAt&&lastSystemVisible==1&&uint32_t(cursorTime-nativeMenuCandidateAt)>=180&&
 uint32_t(cursorTime-lastObservedFrameAt)>=180)nativeMenuHold=true;
 if(!manualCursorPause&&!nativeMenuHold){
  if(nativeGameRefresh&&mode==L"hide"){
   ShowWindow(overlay,SW_HIDE);
   if(shared&&shared->nativeVisible)++hudUpdates;
  }else if([&](){RECT shown{};int x=pos.x-shx,y=pos.y-shy;
    if(IsWindowVisible(overlay)&&GetWindowRect(overlay,&shown)&&shown.left==x&&shown.top==y)return true;
    return overlaystack::place(overlay,overlaystack::Layer::Cursor,x,y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
   }()){
   ++hudUpdates;
  }else{
   // No draw-first gate: however if the overlay actually fails, release the
   // Hook on failure to prevent an indefinitely missing pointer.
   if(mode==L"hide")hookStop();
   wasInside=false;visibilityDiagnostic(L"置顶外部光标窗口失败，暂停隐藏",fp);
  }
}
 else ShowWindow(overlay,SW_HIDE);
 if(!lastVisibilityReason.empty()){
  lastVisibilityReason.clear();lastVisibilityLogAt=0;
  status(L"光标已显示");
 }
 if(frame>=0&&hudLastFrame>=0&&frame!=hudLastFrame)++hudChanges;hudLastFrame=frame;
 uint32_t hn=inputNow(),hudElapsed=hn-hudEpoch;if(hudElapsed>=1000){updateFps=hudUpdates*1000.0/hudElapsed;commitFps=hudCommits*1000.0/hudElapsed;motionFps=hudMotion*1000.0/hudElapsed;changeFps=hudChanges*1000.0/hudElapsed;hudUpdates=hudCommits=hudMotion=hudChanges=0;hudEpoch=hn;}
 std::wstring stateLabel;
 if(manualCursorPause)stateLabel=L"外部光标手动隐藏 (F9恢复)";
 else if(nativeMenuHold)stateLabel=L"检测到系统光标接管，已隐藏残影";
 else if(!smoothing)stateLabel=L"运动补间关闭";else if(!recognition)stateLabel=L"读取失败：原图/布局校验失败";else if(recognition==2)stateLabel=L"类型标记单帧：等待真实换帧验证";else if(recognition==1)stateLabel=L"类型定义未匹配：观察连续换帧中";else if(inferredRange&&phaseIndex!=size_t(-1))stateLabel=L"连续换帧已识别：提交预生成中间帧";else if(inferredRange)stateLabel=L"连续换帧已识别：运动帧准备/等待中";else if(w>128||h>128)stateLabel=L"画布超过128：暂不支持运动补间";else if(phaseIndex!=size_t(-1))stateLabel=L"正在从预生成时间轴取帧";else if(stalled)stateLabel=L"游戏动画停顿：回退真实帧";else if(activeMotion)stateLabel=L"预生成时间轴就绪，等待下一原帧";else if(animCount>1&&motionCache.count(pairKey(frame,animStart+(frame-animStart+1)%animCount)))stateLabel=L"预生成时间轴就绪，等待相邻帧";else stateLabel=L"此段无预生成文件，显示原帧";
 diagAnimation=usingHeldSample?L"等待新光标帧；继续显示已验证原图（不虚报补间 FPS）":stateLabel;

}
static void diagnosticHud(){
 DWORD now=GetTickCount();if(debugHudAt&&now-debugHudAt<250)return;debugHudAt=now;
 if(!hudEnabled||!hud||!IsWindow(game)||!IsWindowVisible(game)||IsIconic(game)){
  if(hud&&IsWindowVisible(hud))ShowWindow(hud,SW_HIDE);return;
 }
 HWND fg=GetForegroundWindow();DWORD fp=0;GetWindowThreadProcessId(fg,&fp);
 if(!((fp==pid&&GetAncestor(fg,GA_ROOT)==GetAncestor(game,GA_ROOT))||platformHostMatches(fg,fp))){if(IsWindowVisible(hud))ShowWindow(hud,SW_HIDE);return;}
 ++debugHudBeat;
 RECT rc{};POINT org{};if(!GetClientRect(game,&rc)||!ClientToScreen(game,&org))return;
 LONG state=shared?shared->state:-9,err=shared?shared->error:0,nativeShown=shared?shared->nativeVisible:0;
 LONG seq=shared?shared->nativeSeq:0,ack=shared?shared->nativeAck:0;
 int frame=diagFrame,hx=diagHotX,hy=diagHotY,gw=clickGameWidth,gh=clickGameHeight;
 DWORD age=0xffffffff;
 if(shared){LONG ss=shared->sampleSeq;MemoryBarrier();
  if(cursorseq::committed(uint32_t(ss))){
   int sf=shared->sampledFrame,sx=shared->sampledHotX,sy=shared->sampledHotY;
   int sw=shared->sampledGameW,sh=shared->sampledGameH;DWORD stamp=shared->sampledTick;MemoryBarrier();
   if(ss==shared->sampleSeq){frame=sf;hx=sx;hy=sy;gw=sw;gh=sh;age=now-stamp;}
  }
 }
 const wchar_t* kind=noCursorMode?L"无光标":nativeGameRefresh&&mode==L"hide"?L"原生光标":L"外部图层";
 const wchar_t* ares=!aresKnown?L"检测未知":nativeFive?L"已检测到":L"未检测到";
 const wchar_t* runText=running?(manualCursorPause?L"手动暂停":L"运行中"):
   (!autoStart?L"已关闭":autoPaused?L"已暂停自动连接":L"等待自动连接");
 const std::wstring frameText=running&&frame>=0?std::to_wstring(frame):L"--";
 const std::wstring hotText=running&&frame>=0?std::to_wstring(hx)+L","+std::to_wstring(hy):L"--";
 const std::wstring ageText=running&&shared&&age!=0xffffffff?std::to_wstring(age)+L" ms":L"--";
 const wchar_t* animationText=!running?L"未运行（无补帧数据）":diagAnimation.c_str();
 const wchar_t* sourceText=!running?L"未启动":usingHeldSample?L"延用最近有效帧":bridgeFrame.has()?L"Hook 帧已验证":L"等待有效帧";
 wchar_t message[1800];
 swprintf(message,1800,L"高刷光标 v" RA2_RELEASE_VERSION_W L" 诊断  |  心跳 #%u  |  %ls / %ls\n"
 L"PID=%lu HWND=%p 前台PID=%lu  客户区=%ld×%ld\n"
 L"Hook=%ld (%ls)  错误=%ld  原生光标确认=%ld\n"
 L"Frame=%ls  热点=%ls  游戏=%d×%d  样本年龄=%ls  %ls\n"
 L"图像序号=%ld 已接收=%ld  提交=%.1f/s  补间=%.1f/s（非游戏FPS）\n"
 L"Hook采样=%ld 有效=%ld 读帧错误=%ld  原帧换帧=%.1f/s\n"
 L"Ares=%ls；补间：%ls\n"
 L"状态：%ls\nCtrl+Alt+F10 面板；Ctrl+Alt+F11 自动开关；Ctrl+Alt+F12 退出",
 debugHudBeat,kind,runText,
 pid,(void*)game,fp,rc.right,rc.bottom,state,
 !running?L"未启动":state==1?L"已连接":state==0?L"等待响应":L"异常或等待",err,nativeShown,
 frameText.c_str(),hotText.c_str(),gw,gh,ageText.c_str(),sourceText,
 seq,ack,running?commitFps:0.0,running?motionFps:0.0,
 shared?shared->sampleAttempts:0,shared?shared->sampleSuccess:0,shared?shared->sampleReadError:0,
 running?changeFps:0.0,ares,animationText,lastStatus.c_str());
 wchar_t perf[300];swprintf(perf,300,L"\n目标 %u FPS · CPU %.2f%% · 刷新 %.3f / 峰值 %.3f ms\n缓存 %u MB · 原生句柄复用 %ld / 创建 %ld",pollHz,processCpuPercent,drawMeanMs,drawPeakMs,unsigned((motionBytes+rasterCache.bytes())/(1024*1024)),shared?shared->nativeCacheHits:0,shared?shared->nativeCacheMisses:0);
 hudText=std::wstring(message)+perf;
 int hudX=org.x+12,hudY=org.y+20,hudW=std::max(280,std::min(1000,(int)rc.right-24));
 RECT oldRect{};
 bool hudIsVisible=IsWindowVisible(hud)!=FALSE;
 bool same=hudIsVisible&&GetWindowRect(hud,&oldRect)&&oldRect.left==hudX&&oldRect.top==hudY&&
    oldRect.right-oldRect.left==hudW&&oldRect.bottom-oldRect.top==230;
 if(!same)overlaystack::place(hud,overlaystack::Layer::Diagnostics,hudX,hudY,hudW,230,SWP_NOACTIVATE|SWP_SHOWWINDOW);
 InvalidateRect(hud,NULL,FALSE); // Never erase between frames; double-buffered paint below.
}
static LRESULT CALLBACK overlayProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_DPICHANGED)return 0;if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;return DefWindowProcW(w,m,a,b);}
static LRESULT CALLBACK hudProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_NCHITTEST)return HTTRANSPARENT;
 if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(m==WM_ERASEBKGND)return 1; // WM_PAINT fills the full backing buffer.
 if(m==WM_PAINT){
  PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);
  RECT rect{};GetClientRect(w,&rect);
  int bw=rect.right-rect.left,bh=rect.bottom-rect.top;
  HDC back=bw>0&&bh>0?CreateCompatibleDC(dc):NULL;
  HBITMAP bitmap=back?CreateCompatibleBitmap(dc,bw,bh):NULL;
  HGDIOBJ oldBitmap=bitmap?SelectObject(back,bitmap):NULL;
  HDC target=bitmap?back:dc;
  HBRUSH hudBackground=CreateSolidBrush(modernui::Background);FillRect(target,&rect,hudBackground);DeleteObject(hudBackground);
  SetBkMode(target,TRANSPARENT);SetTextColor(target,RGB(225,234,245));
  static HFONT font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
   DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,
   DEFAULT_PITCH,L"Microsoft YaHei UI");
  HGDIOBJ oldFont=font?SelectObject(target,font):NULL;
  rect.left+=8;rect.top+=5;
  DrawTextW(target,hudText.c_str(),-1,&rect,DT_LEFT|DT_TOP|DT_NOPREFIX);
  if(oldFont)SelectObject(target,oldFont);
  if(bitmap){BitBlt(dc,0,0,bw,bh,back,0,0,SRCCOPY);SelectObject(back,oldBitmap);DeleteObject(bitmap);}
  if(back)DeleteDC(back);
  EndPaint(w,&ps);return 0;
 }
 return DefWindowProcW(w,m,a,b);
}
// Native Win32 child windows do NOT automatically follow Per-Monitor V2 scaling.
// Retain geometry in 96-DPI design units, never rescale an already-scaled rect.
static UINT windowDpi(HWND w){
 using Fn=UINT(WINAPI*)(HWND);
 static HMODULE user32=GetModuleHandleW(L"user32.dll");
 static Fn fn=user32?reinterpret_cast<Fn>(GetProcAddress(user32,"GetDpiForWindow")):nullptr;
 UINT dpi=fn?fn(w):96;
 #ifdef RA2_UNIFIED
 return modernui::panelDpi(dpi,true);
#else
 return uidpi::validDpi(dpi);
#endif
}
static void setUiFont(){
 HFONT fresh=modernui::makeFont(modernui::BodyPixels,uiDpi);
 if(!fresh)return;
 for(const auto& ctl:controlOrigins)if(IsWindow(ctl.handle))SendMessageW(ctl.handle,WM_SETFONT,(WPARAM)fresh,FALSE);
 HFONT titleNew=modernui::makeFont(modernui::HeadingPixels,uiDpi,FW_SEMIBOLD);
 HFONT actionNew=modernui::makeFont(modernui::BodyPixels,uiDpi);
 if(titleNew&&IsWindow(titleLabel))SendMessageW(titleLabel,WM_SETFONT,(WPARAM)titleNew,FALSE);
 if(actionNew&&IsWindow(primaryAction))SendMessageW(primaryAction,WM_SETFONT,(WPARAM)actionNew,FALSE);
 HFONT old=uiFont;uiFont=fresh;
 if(old)DeleteObject(old);
 if(titleFont)DeleteObject(titleFont);titleFont=titleNew;
 if(actionFont)DeleteObject(actionFont);actionFont=actionNew;
}
static void relayout(HWND w){
 if(!uiReady||uiLayoutBusy)return;
 uiLayoutBusy=true;
 // DeferWindowPos prevents transient overlaps while resizing dozens of controls.
 HDWP batch=BeginDeferWindowPos(int(controlOrigins.size()));
 if(batch){
  for(const auto& ctl:controlOrigins){
   auto b=uidpi::place(ctl.logical,uiDpi,scrollX,scrollY);
   batch=DeferWindowPos(batch,ctl.handle,NULL,b.x,b.y,b.w,b.h,
      SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS|SWP_NOREDRAW);
   if(!batch)break;
  }
  if(batch&&EndDeferWindowPos(batch)){
   RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);uiLayoutBusy=false;return;
  }
 }
 // Fallback also fixes every control if a deferred batch fails halfway.
 for(const auto& ctl:controlOrigins){
  auto b=uidpi::place(ctl.logical,uiDpi,scrollX,scrollY);
  SetWindowPos(ctl.handle,NULL,b.x,b.y,b.w,b.h,
      SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS|SWP_NOREDRAW);
 }
 RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
 uiLayoutBusy=false;
}
static void viewport(HWND w,int sx,int sy){
 if(!uiReady||uiLayoutBusy)return;
 RECT r{};GetClientRect(w,&r);
 auto v=uidpi::scroll(sx,sy,r.right-r.left,r.bottom-r.top,uiContentW,uiContentH,uiDpi);
 SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
 si.nMin=0;si.nMax=std::max(0,v.contentW-1);si.nPage=UINT(std::max(1L,r.right-r.left));si.nPos=v.x;
 SetScrollInfo(w,SB_HORZ,&si,TRUE);
 si.nMax=std::max(0,v.contentH-1);si.nPage=UINT(std::max(1L,r.bottom-r.top));si.nPos=v.y;
 SetScrollInfo(w,SB_VERT,&si,TRUE);
 if(scrollX!=v.x||scrollY!=v.y){scrollX=v.x;scrollY=v.y;relayout(w);}
}
static void applyControlDpi(HWND w,UINT next){
#ifdef RA2_UNIFIED
 next=modernui::panelDpi(next,true);
#else
 next=uidpi::validDpi(next);
#endif
 if(next==uiDpi)return;
 UINT previous=uiDpi;
 scrollX=uidpi::rescale(scrollX,previous,next);
 scrollY=uidpi::rescale(scrollY,previous,next);
 uiDpi=next;
 if(uiReady){setUiFont();viewport(w,scrollX,scrollY);relayout(w);}
}
static void error(const std::exception& e){std::wstring s=widen(e.what());status(L"错误："+s);cursorDialog(s.c_str(),L"高刷光标 v" RA2_RELEASE_VERSION_W,MB_OK|MB_ICONWARNING);}
static int numeric(HWND w,int min,int max){wchar_t b[32];GetWindowTextW(w,b,32);wchar_t* end;long n=wcstol(b,&end,10);if(end==b||*end||n<min||n>max)fail("Invalid numeric setting; check the range in README");return (int)n;}
static void applyTargetRate(){
 wchar_t text[32];GetWindowTextW(targetBox,text,32);
 unsigned value=0;
 if(wcscmp(text,L"自动")&&wcscmp(text,L"0")){
  wchar_t* end=nullptr;long number=wcstol(text,&end,10);
  if(end==text||*end||number<30||number>500){textIfChanged(quickInfo,L"目标帧率：自动或 30–500，保留上次值");return;}
  value=unsigned(number);
 }
 if(value!=configuredPollHz){
  configuredPollHz=value;updateRefresh(true);
 }
 bool saved=WritePrivateProfileStringW(L"Display",L"TargetFPS",std::to_wstring(value).c_str(),ini().c_str())!=FALSE;
 saved=(WritePrivateProfileStringW(L"Display",L"PollHz",std::to_wstring(value).c_str(),ini().c_str())!=FALSE)&&saved;
 textIfChanged(quickInfo,L"刷新目标 "+std::to_wstring(pollHz)+(saved?L" FPS · 已保存":L" FPS · 设置文件写入失败"));
}
static bool liveControl(HWND h){return h==scaleBox||h==offsetXBox||h==offsetYBox||h==gameWBox||h==gameHBox||h==screenWBox||h==screenHBox||h==dpiBox||h==dpiModeBox||h==smoothBox||h==smoothMsBox||h==adaptiveBox||h==clickBox||h==clickModeBox||h==clickGapBox||h==autoBox||h==fitBox;}
#include "live_settings_ui.h"

// Previews are rendered from local game resource cache in a separate process.
// It never injects/reads the game; user can scrub/compare 1000Hz samples offline.
static void previewCurrent(){
 std::wstring page=profilePath+L"\\preview.html";
 if(!exists(page))fail("先完成此光标包的预处理，再打开动画预览");
 ShellExecuteW(mainw,L"open",page.c_str(),NULL,NULL,SW_SHOWNORMAL);
}
// Only the main action changes enablement. When enabled the existing autoTick
// resolves the game in the background; clicking must not block on attach/Hook.
static void togglePrimary(){
 if(autoStart&&!autoPaused){
  autoStart=false;autoPaused=true;
  if(running)stop();
  status(L"已关闭外部光标与自动检测。原游戏光标是否显示由当前隐藏模式决定。");
 }else{
  autoStart=true;autoPaused=false;lastAutoCheck=0;
  status(L"已开启自动检测：等待兼容游戏窗口（支持随机进程名）。");
 }
 WritePrivateProfileStringW(L"Startup",L"AutoStart",autoStart?L"1":L"0",ini().c_str());
 if(IsWindow(autoStartBox))SendMessageW(autoStartBox,BM_SETCHECK,autoStart?BST_CHECKED:BST_UNCHECKED,0);
 updatePrimaryAction();
}
static void resizePrimaryPanel(HWND w){
 if(!IsWindow(w))return;
 RECT wr{},cr{};GetWindowRect(w,&wr);GetClientRect(w,&cr);
 int extraW=(wr.right-wr.left)-(cr.right-cr.left);
 int extraH=(wr.bottom-wr.top)-(cr.bottom-cr.top);
 HMONITOR monitor=MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST);
 MONITORINFO mi{};mi.cbSize=sizeof(mi);
 int nw=uidpi::scale(uiContentW,uiDpi)+extraW;
 int nh=uidpi::scale(uiContentH,uiDpi)+extraH;
 if(GetMonitorInfoW(monitor,&mi)){
  const int workWidth=static_cast<int>(mi.rcWork.right-mi.rcWork.left);
  const int workHeight=static_cast<int>(mi.rcWork.bottom-mi.rcWork.top);
  nw=std::min(nw,workWidth);
  nh=std::min(nh,workHeight);
 }
 scrollX=scrollY=0;
 if(!(GetWindowLongPtrW(w,GWL_STYLE)&WS_CHILD)&&!GetPropW(w,L"RA2.Suite.Managed"))SetWindowPos(w,NULL,0,0,nw,nh,SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
 viewport(w,0,0);relayout(w);
}
static void toggleAdvanced(HWND w){
 advancedOpen=!advancedOpen;
 uiContentW=advancedOpen?advancedContentW:compactContentW;
 uiContentH=advancedOpen?advancedContentH:compactContentH;
 for(HWND h:advancedControls)ShowWindow(h,advancedOpen?SW_SHOW:SW_HIDE);
 if(IsWindow(advancedHeader))ShowWindow(advancedHeader,advancedOpen?SW_SHOW:SW_HIDE);
 SetWindowTextW(advancedAction,advancedOpen?L"收起设置 ▴":L"高级设置 ▾");
 resizePrimaryPanel(w);
 InvalidateRect(w,NULL,TRUE);
}
static void readPairs(){
 allowedPairs.clear();auto p=std::filesystem::path(profilePath)/L"pairs.txt";
 std::ifstream f(p);if(!f)f.open(std::filesystem::path(root)/L"assets"/L"pairs.txt");
 int a,b;while(f>>a>>b)if(a>=0&&b>=0&&a<450&&b<450&&a!=b)allowedPairs.insert(pairKey(a,b));
}
static void resetPrepared(){
 if(running)stop();
 ++motionEpoch;if(playbackCancel)playbackCancel->store(true);
 playbackReady=false;playbackPending=true;rasterCache.clear();motionCache.clear();motionBytes=0;activeMotion.reset();lastPhase=size_t(-1);lastFrame=-2;
 rebuildDisplay();
}
static void refreshStyleList(){
 wchar_t selected[512]={};GetPrivateProfileStringW(L"Cursor",L"Profile",L"",selected,512,ini().c_str());
 profileIds={L""};profileNames={L"红警原版 · DIS 1000 等分"};
 WIN32_FIND_DATAW data{};HANDLE h=FindFirstFileW((root+L"\\profiles\\*").c_str(),&data);
 if(h!=INVALID_HANDLE_VALUE){do{if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&data.cFileName[0]!=L'.'){
  std::wstring id=data.cFileName,folder=root+L"\\profiles\\"+id;
  if(!exists(folder+L"\\original.sha")||!exists(folder+L"\\palette.bgra"))continue;
  wchar_t name[512]={};GetPrivateProfileStringW(L"Profile",L"Name",id.c_str(),name,512,(folder+L"\\profile.ini").c_str());profileIds.push_back(id);profileNames.push_back(name);
 }}while(FindNextFileW(h,&data));FindClose(h);}
 SendMessageW(styleBox,CB_RESETCONTENT,0,0);int pick=0;
 for(size_t i=0;i<profileIds.size();i++){SendMessageW(styleBox,CB_ADDSTRING,0,(LPARAM)profileNames[i].c_str());if(profileIds[i]==selected)pick=int(i);}
 SendMessageW(styleBox,CB_SETCURSEL,pick,0);
}
static void chooseStyle(){
 int selection=(int)SendMessageW(styleBox,CB_GETCURSEL,0,0);if(selection<0||selection>=int(profileIds.size()))return;
 std::wstring id=profileIds[selection],folder=id.empty()?root+L"\\assets":root+L"\\profiles\\"+id;
 Asset candidate=decode(file(folder+L"\\original.sha"));Bytes colors=file(folder+L"\\palette.bgra");
 if(candidate.w!=55||candidate.h!=43||candidate.frames.size()<450||colors.size()!=1024)fail("Style must use the compatible 450+ frame / 55x43 layout and a 1024-byte BGRA palette");
 DWORD cp[256];memcpy(cp,colors.data(),1024);cp[0]=0;
 for(auto& c:cp){int a=(c>>24)&255;c=motion::premultiplyRGB(c&0xffffff,a);}
 displayAsset=std::move(candidate);memcpy(displayPalette,cp,1024);displayLoaded=true;profileId=id;profilePath=folder;
 WritePrivateProfileStringW(L"Cursor",L"Profile",id.c_str(),ini().c_str());readPairs();resetPrepared();
 textIfChanged(settingsHint,L"图案已切换并自动保存");
 textIfChanged(prepareInfo,prepareProcess?L"预处理仍在后台进行；切换图案不影响原任务":L"");
}
static std::wstring pickFile(const wchar_t* title,const wchar_t* filter){
 wchar_t path[32768]={};OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=mainw;o.lpstrTitle=title;o.lpstrFilter=filter;o.lpstrFile=path;o.nMaxFile=32768;o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 return GetOpenFileNameW(&o)?std::wstring(path):L"";
}
static void importStyle(){
 if(prepareProcess)fail("Wait for the current preprocessing task, or cancel it before importing");
 auto selected=pickFile(L"导入红警布局 mouse.sha（450 帧，55×43）",L"SHP / SHA\0*.sha;*.shp\0All files\0*.*\0\0");if(selected.empty())return;
 auto bytes=file(selected);auto art=decode(bytes);if(art.w!=55||art.h!=43||(art.frames.size()!=450&&art.frames.size()!=517))fail("需要兼容红警 450 帧或尤里 517 帧、55x43 画布的 SHA；本版不猜测其他布局的语义映射");
 bool visible=false;for(const auto& fr:art.frames)for(auto c:fr.pixels)if(c){visible=true;break;}if(!visible)fail("文件全部透明，请导入可见的原始 SHA");
 auto parent=std::filesystem::path(selected).parent_path();std::wstring colorPath=(parent/L"palette.bgra").wstring();
 if(!exists(colorPath)){colorPath=(parent/L"mousepal.pal").wstring();if(!exists(colorPath))colorPath=pickFile(L"选择对应色盘（原版可选 assets\\palette.bgra）",L"Palette\0*.bgra;*.pal\0All files\0*.*\0\0");}
 if(colorPath.empty())return;Bytes colors=file(colorPath);
 if(colors.size()==768){Bytes rgba(1024,255);int maxColor=*std::max_element(colors.begin(),colors.end());for(int i=0;i<256;i++)for(int c=0;c<3;c++)rgba[4*i+c]=uint8_t(std::min(255,int(colors[3*i+2-c])*(maxColor<=63?4:1)));colors=std::move(rgba);}
 if(colors.size()!=1024)fail("色盘必须为 768 字节 PAL 或 1024 字节 BGRA");for(int c=0;c<4;c++)colors[c]=0;
 CreateDirectoryW((root+L"\\profiles").c_str(),NULL);std::wstring id=L"user-"+std::to_wstring(GetTickCount64());std::wstring folder=root+L"\\profiles\\"+id;
 if(!CreateDirectoryW(folder.c_str(),NULL))fail("无法创建独立光标包目录");
 save(folder+L"\\original.sha",bytes);save(folder+L"\\palette.bgra",colors);
 auto groups=parent/L"groups.json";save(folder+L"\\groups.json",file(exists(groups.wstring())?groups.wstring():root+L"\\assets\\groups.json"));
 save(folder+L"\\pairs.txt",file(root+L"\\assets\\pairs.txt"));save(folder+L"\\profile.ini",Bytes{0xff,0xfe});
 WritePrivateProfileStringW(L"Profile",L"Name",std::filesystem::path(selected).stem().c_str(),(folder+L"\\profile.ini").c_str());
 WritePrivateProfileStringW(L"Cursor",L"Profile",id.c_str(),ini().c_str());refreshStyleList();chooseStyle();beginPrepare();
}
static void beginPrepare(){
 if(prepareProcess){HANDLE h=CreateFileW(prepareCancel.c_str(),GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);textIfChanged(prepareInfo,L"正在取消，已完成的分段仍会保留");return;}
 const std::wstring python=root+L"\\runtime\\python.exe",script=root+L"\\engine\\precompute.py";
 if(!exists(python)||!exists(script))fail("预处理运行环境缺失，请完整解压含 runtime 的版本");
 preparedProfile=profilePath;prepareStatus=profilePath+L"\\progress.ini";prepareCancel=profilePath+L"\\cancel.flag";DeleteFileW(prepareCancel.c_str());DeleteFileW(prepareStatus.c_str());
 std::wstring command=L"\""+python+L"\" \""+script+L"\" --profile \""+profilePath+L"\" --status \""+prepareStatus+L"\" --cancel \""+prepareCancel+L"\"";
 std::vector<wchar_t> args(command.begin(),command.end());args.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(python.c_str(),args.data(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,root.c_str(),&si,&pi))fail("无法启动内置预处理引擎（需64位 Windows 10/11）");
 CloseHandle(pi.hThread);prepareProcess=pi.hProcess;SetPriorityClass(prepareProcess,BELOW_NORMAL_PRIORITY_CLASS);textIfChanged(prepareButton,L"取消预处理");textIfChanged(prepareInfo,L"正在本机预处理，游戏光标与界面可继续使用…");
}
static void pollPrepare(){
 if(!prepareProcess)return;static DWORD last=0;DWORD now=GetTickCount();if(uint32_t(now-last)<400)return;last=now;
 DWORD code=STILL_ACTIVE;if(!GetExitCodeProcess(prepareProcess,&code))code=1;
 unsigned done=GetPrivateProfileIntW(L"Progress",L"Done",0,prepareStatus.c_str()),total=GetPrivateProfileIntW(L"Progress",L"Total",0,prepareStatus.c_str());
 if(code==STILL_ACTIVE){if(total)textIfChanged(prepareInfo,L"预处理 "+std::to_wstring(done)+L" / "+std::to_wstring(total)+L" 段 · 每段 1000 等分");return;}
 wchar_t state[40]={};GetPrivateProfileStringW(L"Progress",L"State",L"error",state,40,prepareStatus.c_str());
 CloseHandle(prepareProcess);prepareProcess=nullptr;textIfChanged(prepareButton,L"预处理补帧");
 bool success=code==0&&!wcscmp(state,L"done");
 if(success){if(profilePath==preparedProfile){readPairs();resetPrepared();}textIfChanged(prepareInfo,L"预处理完成 · "+std::to_wstring(total)+L" 段已可直接播放");}
 else if(!wcscmp(state,L"cancelled"))textIfChanged(prepareInfo,L"已取消预处理，保留已完成分段");
 else{ textIfChanged(prepareInfo,L"预处理失败：检查 SHA、色盘与动画分组；错误详情见光标包内 progress.ini.error.txt");log(L"Preprocessing failed: "+prepareStatus);}
}

static bool cursorCloseAllowed(){
 if(mode==L"sha"&&live()&&(exists(gameDir+L"\\mouse.sha.cursor-preview-state")||manuallyBlankSHA()))
  return cursorDialog(L"游戏正在使用透明光标。退出工具箱后可能看不到指针。\n建议先退出游戏，再恢复 SHA。仍退出？",L"透明 SHA",MB_YESNO|MB_ICONWARNING)==IDYES;
 return true;
}
#include "ddraw_settings_ui.h"
#include "refresh_settings_ui.h"
static LRESULT CALLBACK wndproc(HWND w,UINT m,WPARAM a,LPARAM b){
#ifdef RA2_UNIFIED
 if(suite::layoutMessage(w,m,b))return 0;
 if(m==suite::DisplaySettingsChanged){syncRefreshControls();return 0;}
 if(m==suite::ModuleEnable){if(bool(a)!=(autoStart&&!autoPaused))togglePrimary();return 0;}
 if(m==suite::CloseCheck){suite::closeDecision(cursorCloseAllowed());return 0;}
#endif
 if(m==modernui::DpiMessage){applyControlDpi(w,unsigned(a));return 0;}
 switch(m){
 case WM_ERASEBKGND:{HDC dc=(HDC)a;RECT r{};GetClientRect(w,&r);FillRect(dc,&r,panelBrush);return TRUE;}
 case WM_CTLCOLORBTN:case WM_CTLCOLORSTATIC:{HDC dc=(HDC)a;SetBkMode(dc,OPAQUE);
  HWND ctl=(HWND)b;
  COLORREF color=(ctl==titleLabel)?modernui::Text:(ctl==subTitleLabel)?modernui::Accent:(ctl==quickInfo)?modernui::Muted:modernui::Text;
  SetTextColor(dc,color);SetBkColor(dc,ctl==statusw?modernui::Surface:modernui::Background);
  return (LRESULT)(ctl==statusw?statusBrush:panelBrush);}
 case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)a;SetTextColor(dc,modernui::Text);SetBkColor(dc,modernui::Field);return (LRESULT)headerBrush;}
 case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
   // Use the same dark palette and type family as the toolbox modules.
   FillRect(dc,&r,panelBrush);
   HPEN line=CreatePen(PS_SOLID,1,RGB(45,60,79));auto oldPen=SelectObject(dc,line);
   MoveToEx(dc,uidpi::scale(24,uiDpi)-scrollX,uidpi::scale(68,uiDpi)-scrollY,nullptr);
   LineTo(dc,uidpi::scale(740,uiDpi)-scrollX,uidpi::scale(68,uiDpi)-scrollY);
   SelectObject(dc,oldPen);DeleteObject(line);
   // Status surface behind a transparent static control; painted in logical DPI units.
   RECT box{uidpi::scale(20,uiDpi)-scrollX,uidpi::scale(74,uiDpi)-scrollY,
            uidpi::scale(740,uiDpi)-scrollX,uidpi::scale(112,uiDpi)-scrollY};
   HPEN outline=CreatePen(PS_SOLID,1,RGB(45,60,79));auto op=SelectObject(dc,outline);
   auto ob=SelectObject(dc,statusBrush);
   RoundRect(dc,box.left,box.top,box.right,box.bottom,uidpi::scale(14,uiDpi),uidpi::scale(14,uiDpi));
   SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(outline);
   HBRUSH dot=CreateSolidBrush(autoStart&&!autoPaused?RGB(22,163,116):RGB(152,163,181));
   RECT state{uidpi::scale(30,uiDpi)-scrollX,uidpi::scale(89,uiDpi)-scrollY,
              uidpi::scale(36,uiDpi)-scrollX,uidpi::scale(95,uiDpi)-scrollY};
   FillRect(dc,&state,dot);DeleteObject(dot);
   if(advancedOpen){HPEN separator=CreatePen(PS_SOLID,1,RGB(45,60,79));auto saved=SelectObject(dc,separator);
     MoveToEx(dc,uidpi::scale(22,uiDpi)-scrollX,uidpi::scale(392,uiDpi)-scrollY,nullptr);
     LineTo(dc,uidpi::scale(758,uiDpi)-scrollX,uidpi::scale(392,uiDpi)-scrollY);
     SelectObject(dc,saved);DeleteObject(separator);}
   EndPaint(w,&ps);return 0;}
case WM_GETMINMAXINFO:{auto p=(MINMAXINFO*)b;p->ptMinTrackSize={uidpi::scale(advancedOpen?510:376,uiDpi),uidpi::scale(240,uiDpi)};return 0;}
 case WM_SIZE:viewport(w,scrollX,scrollY);relayout(w);return 0;
 case WM_MOUSEWHEEL:{int amount=pagewheel::delta(w,a,uidpi::scale(60,uiDpi));if(amount)viewport(w,scrollX,scrollY-amount);return 0;}
 case WM_VSCROLL:case WM_HSCROLL:{int bar=m==WM_VSCROLL?SB_VERT:SB_HORZ;SCROLLINFO si={sizeof(si),SIF_ALL};GetScrollInfo(w,bar,&si);int n=si.nPos;switch(LOWORD(a)){case SB_LINEUP:n-=uidpi::scale(28,uiDpi);break;case SB_LINEDOWN:n+=uidpi::scale(28,uiDpi);break;case SB_PAGEUP:n-=si.nPage;break;case SB_PAGEDOWN:n+=si.nPage;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:n=si.nTrackPos;break;case SB_TOP:n=0;break;case SB_BOTTOM:n=si.nMax;break;}viewport(w,bar==SB_HORZ?n:scrollX,bar==SB_VERT?n:scrollY);return 0;}
 case WM_TIMER:if(a==152){KillTimer(w,152);if(uiReady)applyTargetRate();return 0;}if(a==150){KillTimer(w,150);if(uiReady&&!applyingSettings){applyingSettings=true;applyScale();applyingSettings=false;}return 0;}break;
 case WM_COMMAND:if(!uiReady)return 0;
#ifdef RA2_UNIFIED
 if(refreshSettingCommand(a,b))return 0;
 if(LOWORD(a)==157&&HIWORD(a)==BN_CLICKED){ddrawsettings::configure(false);return 0;}
 if(LOWORD(a)==158&&HIWORD(a)==BN_CLICKED){ddrawsettings::configure(true);return 0;}
#endif
 if((HWND)b==targetBox&&(HIWORD(a)==CBN_SELCHANGE||HIWORD(a)==CBN_EDITCHANGE)){SetTimer(w,152,HIWORD(a)==CBN_SELCHANGE?1:300,NULL);return 0;}
 if((HWND)b==modeBox&&HIWORD(a)==CBN_SELCHANGE){try{applyHideMode();updateRefresh(true);updatePrimaryAction();}catch(const std::exception& e){SendMessageW(modeBox,CB_SETCURSEL,noCursorMode?3:mode==L"hide"?1:mode==L"sha"?2:0,0);error(e);}return 0;}
 if((HWND)b==styleBox&&HIWORD(a)==CBN_SELCHANGE){try{chooseStyle();}catch(const std::exception& e){error(e);}return 0;}
 if(liveControl((HWND)b)&&(HIWORD(a)==EN_CHANGE||HIWORD(a)==CBN_EDITCHANGE||HIWORD(a)==CBN_SELCHANGE||HIWORD(a)==BN_CLICKED)){
  SetTimer(w,150,(HIWORD(a)==EN_CHANGE||HIWORD(a)==CBN_EDITCHANGE)?300:1,NULL);return 0;
 }
 try{switch(LOWORD(a)){case 154:superCenter=SendMessageW(superCenterBox,BM_GETCHECK,0,0)==BST_CHECKED;WritePrivateProfileStringW(L"Cursor",L"SuperWeaponVisualCenter",superCenter?L"1":L"0",ini().c_str());lastFrame=-2;break;case 130:importStyle();break;case 131:beginPrepare();break;case 132:ShellExecuteW(w,L"open",(profilePath+L"\\groups.json").c_str(),NULL,NULL,SW_SHOWNORMAL);break;case 101:togglePrimary();break;case 102:toggleAdvanced(w);break;case 1:attach();break;case 2:browseGame();break;case 3:install();break;case 4:restoreFile();break;case 5:autoStart=true;autoPaused=false;WritePrivateProfileStringW(L"Startup",L"AutoStart",L"1",ini().c_str());start();updatePrimaryAction();break;case 6:autoStart=false;autoPaused=true;WritePrivateProfileStringW(L"Startup",L"AutoStart",L"0",ini().c_str());stop();updatePrimaryAction();break;case 7:applyScale();break;case 10:previewCurrent();break;case 11:applyHideMode();break;
 case 12:{
  if(HIWORD(a)!=CBN_SELCHANGE)break;
  bool selected=SendMessageW(nativeBox,CB_GETCURSEL,0,0)==1;
  if(selected&&mode!=L"hide"){
   SendMessageW(nativeBox,CB_SETCURSEL,0,0);
   fail("游戏内刷新需要 Hook 隐藏模式");
  }
  if(selected!=nativeGameRefresh){
   bool resume=running;
   if(resume)stop();
   cursorReader.close();
   nativeGameRefresh=selected;readRestricted=false;
   WritePrivateProfileStringW(L"Display",L"GameThreadRefresh",selected?L"1":L"0",ini().c_str());
   WritePrivateProfileStringW(L"Compatibility",L"CursorNoCodeWriteDefaultsVersion",L"1",ini().c_str());
   if(resume)start();
  }
  updateRefresh(true);updatePrimaryAction();
  break;
 }
 case 9:hudEnabled=SendMessageW(hudBox,BM_GETCHECK,0,0)==BST_CHECKED;WritePrivateProfileStringW(L"Display",L"HUD",hudEnabled?L"1":L"0",ini().c_str());if(!hudEnabled)ShowWindow(hud,SW_HIDE);hudPaintAt=0;break;case 8:autoStart=SendMessageW(autoStartBox,BM_GETCHECK,0,0)==BST_CHECKED;autoPaused=!autoStart;if(!autoStart&&running)stop();WritePrivateProfileStringW(L"Startup",L"AutoStart",autoStart?L"1":L"0",ini().c_str());updatePrimaryAction();break;}}catch(const std::exception& e){error(e);}return 0;
 case WM_HOTKEY:if(a==4){manualCursorPause=!manualCursorPause;if(manualCursorPause){ShowWindow(overlay,SW_HIDE);if(mode==L"hide")hookStop();}hudPaintAt=0;if(!manualCursorPause)wasInside=false;log(manualCursorPause?L"External pointer paused with F9":L"External pointer resumed with F9");return 0;}if(a==3){hudEnabled=!hudEnabled;SendMessageW(hudBox,BM_SETCHECK,hudEnabled?BST_CHECKED:BST_UNCHECKED,0);WritePrivateProfileStringW(L"Display",L"HUD",hudEnabled?L"1":L"0",ini().c_str());if(!hudEnabled)ShowWindow(hud,SW_HIDE);hudPaintAt=0;return 0;}if(a==2){SendMessageW(w,WM_CLOSE,0,0);return 0;}try{togglePrimary();}catch(const std::exception& e){error(e);}return 0;
 case WM_DPICHANGED:{
  // Use the OS-recommended outer rect and refresh every child and font.
  // Only scale the original 96-DPI layout, never the previous DPI layout.
  UINT next=HIWORD(a);applyControlDpi(w,next);
  const RECT* r=reinterpret_cast<const RECT*>(b);
  if(r&&!(GetWindowLongPtrW(w,GWL_STYLE)&WS_CHILD)&&!GetPropW(w,L"RA2.Suite.Managed"))SetWindowPos(w,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,
                   SWP_NOZORDER|SWP_NOACTIVATE);
  viewport(w,scrollX,scrollY);relayout(w);return 0;
 }
 case WM_DISPLAYCHANGE:case WM_SETTINGCHANGE:{
  displaySettingsDirty=true;
  UINT next=windowDpi(w);
  if(next!=uiDpi)applyControlDpi(w,next);
  return DefWindowProcW(w,m,a,b);
 }
 case WM_CLOSE:
#ifdef RA2_UNIFIED
 if(a!=suite::CloseToken){suite::requestClose();return 0;}
#endif
#ifndef RA2_UNIFIED
 if(!cursorCloseAllowed())return 0;
#endif
 boundedshutdown::begin();
 KillTimer(w,152);if(uiReady)applyTargetRate();KillTimer(w,150);if(uiReady&&!applyingSettings){applyingSettings=true;applyScale();applyingSettings=false;}
 stop();DestroyWindow(w);return 0;
 case WM_DESTROY:if(playbackCancel)playbackCancel->store(true);if(prepareProcess){HANDLE cancel=CreateFileW(prepareCancel.c_str(),GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(cancel!=INVALID_HANDLE_VALUE)CloseHandle(cancel);CloseHandle(prepareProcess);prepareProcess=nullptr;}if(uiFont){DeleteObject(uiFont);uiFont=NULL;}if(titleFont){DeleteObject(titleFont);titleFont=NULL;}if(actionFont){DeleteObject(actionFont);actionFont=NULL;}if(panelBrush){DeleteObject(panelBrush);panelBrush=NULL;}if(headerBrush){DeleteObject(headerBrush);headerBrush=NULL;}if(statusBrush){DeleteObject(statusBrush);statusBrush=NULL;}PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,b);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR cmd,int){
 const bool blankPreset=wcsstr(cmd,L"--no-cursor")!=nullptr;
 const bool lanboPreset=wcsstr(cmd,L"--lanbo")!=nullptr||blankPreset;
 noCursorMode=blankPreset;
 /* DPI awareness is set by the embedded PerMonitorV2 manifest before any window exists. */ wchar_t path[32768];GetModuleFileNameW(NULL,path,32768);root=path;root=root.substr(0,root.find_last_of(L"\\"));if(wcsstr(cmd,L"sha"))mode=L"sha";else if(wcsstr(cmd,L"hide"))mode=L"hide";else if(wcsstr(cmd,L"keep"))mode=L"keep";else{wchar_t remembered[16]={};GetPrivateProfileStringW(L"Startup",L"Mode",L"",remembered,16,ini().c_str());if(!wcscmp(remembered,L"sha")||!wcscmp(remembered,L"hide")||!wcscmp(remembered,L"keep"))mode=remembered;else if(!*remembered){wchar_t previousDir[32768]={};GetPrivateProfileStringW(L"Target",L"Directory",L"",previousDir,32768,ini().c_str());if(*previousDir&&exists(std::wstring(previousDir)+L"\\mouse.sha.cursor-preview-state"))mode=L"sha";}}
 if(lanboPreset)mode=L"hide";
 // Earlier versions saved the implicit "hide" default on every launch, so
 // a stored value alone cannot establish a high-risk opt-in. Migrate once to
 // keep; explicit command-line or newly selected labelled modes remain valid.
 const bool explicitCursorSelection=lanboPreset||wcsstr(cmd,L"hide")||wcsstr(cmd,L"keep")||wcsstr(cmd,L"sha");
 const bool reviewedCursorMode=GetPrivateProfileIntW(L"Compatibility",L"CursorNoCodeWriteDefaultsVersion",0,ini().c_str())>=1;
 if(!reviewedCursorMode&&!explicitCursorSelection&&mode==L"hide")mode=L"keep";
 HANDLE mutex=CreateMutexW(NULL,FALSE,L"Local\\RA2ExternalCursor3Modes");if(GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(NULL,L"程序已在运行。",L"提示",MB_OK);return 0;}
 if(!wcsstr(cmd,L"restore"))WritePrivateProfileStringW(L"Startup",L"Mode",mode.c_str(),ini().c_str());
 if(!wcsstr(cmd,L"restore")&&explicitCursorSelection)WritePrivateProfileStringW(L"Compatibility",L"CursorNoCodeWriteDefaultsVersion",L"1",ini().c_str());
 WNDCLASSW wc={};wc.hInstance=inst;wc.lpfnWndProc=wndproc;wc.lpszClassName=L"RA2CursorControl3";wc.hCursor=LoadCursor(NULL,IDC_ARROW);panelBrush=CreateSolidBrush(modernui::Background);headerBrush=CreateSolidBrush(modernui::Field);statusBrush=CreateSolidBrush(modernui::Surface);wc.hbrBackground=panelBrush;RegisterClassW(&wc);wc.lpfnWndProc=overlayProc;wc.lpszClassName=L"RA2CursorOverlay3";wc.hbrBackground=NULL;wc.hCursor=NULL;RegisterClassW(&wc);
 wc.lpfnWndProc=hudProc;wc.lpszClassName=L"RA2CursorHUD9";RegisterClassW(&wc);
 std::wstring title=L"高刷光标 v" RA2_RELEASE_VERSION_W;
 mainw=CreateWindowExW(0,L"RA2CursorControl3",title.c_str(),WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_HSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,490,405,NULL,NULL,inst,NULL);
 overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"RA2CursorOverlay3",L"",WS_POPUP,0,0,1,1,NULL,NULL,inst,NULL);
 hud=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"RA2CursorHUD9",L"",WS_POPUP,0,0,740,140,NULL,NULL,inst,NULL);SetLayeredWindowAttributes(hud,0,220,LWA_ALPHA);
 const wchar_t* labels[]={L"连接游戏",L"游戏目录",L"安装 SHA",L"恢复 SHA",L"启动",L"停止"};
 for(int i=0;i<6;i++){HWND bt=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,15+(i%3)*245,15+(i/3)*45,226,34,mainw,(HMENU)(INT_PTR)(i+1),inst,NULL);SendMessageW(bt,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);if(i==2)installBox=bt;if(i==3)restoreBox=bt;}
 statusw=CreateWindowW(L"STATIC",L"等待游戏",WS_CHILD|WS_VISIBLE,15,115,735,44,mainw,NULL,inst,NULL);SendMessageW(statusw,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 HWND labelMode=CreateWindowW(L"STATIC",L"原光标",WS_CHILD|WS_VISIBLE,15,172,90,22,mainw,NULL,inst,NULL);SendMessageW(labelMode,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 modeBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,105,168,380,110,mainw,NULL,inst,NULL);
 for(const wchar_t* text:{L"保留原光标",L"高危：隐藏原光标",L"透明 SHA",L"高危：无光标诊断"})SendMessageW(modeBox,CB_ADDSTRING,0,(LPARAM)text);
 SendMessageW(modeBox,CB_SETCURSEL,noCursorMode?3:mode==L"hide"?1:mode==L"sha"?2:0,0);SendMessageW(modeBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);


 HWND note=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,15,212,735,58,mainw,NULL,inst,NULL);SendMessageW(note,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 scalePercent=(int)GetPrivateProfileIntW(L"Cursor",L"ScalePercent",100,ini().c_str());scalePercent=std::max(25,std::min(400,scalePercent));offsetX=(int)GetPrivateProfileIntW(L"Cursor",L"HotspotOffsetX",0,ini().c_str());offsetY=(int)GetPrivateProfileIntW(L"Cursor",L"HotspotOffsetY",0,ini().c_str());offsetX=std::max(-256,std::min(256,offsetX));offsetY=std::max(-256,std::min(256,offsetY));autoScale=GetPrivateProfileIntW(L"Cursor",L"AutoScale",0,ini().c_str())!=0;aspectFit=GetPrivateProfileIntW(L"Cursor",L"AspectFit",0,ini().c_str())!=0;
 if(lanboPreset){offsetX=offsetY=0;}
 auto label=[&](const wchar_t* text,int x,int y,int width){HWND q=CreateWindowW(L"STATIC",text,WS_CHILD|WS_VISIBLE,x,y,width,22,mainw,NULL,inst,NULL);SendMessageW(q,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);};
 label(L"缩放 %",15,280,65);scaleBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWN|WS_VSCROLL,80,276,90,180,mainw,NULL,inst,NULL);for(auto v:{100,125,150,200})SendMessageW(scaleBox,CB_ADDSTRING,0,(LPARAM)std::to_wstring(v).c_str());SetWindowTextW(scaleBox,std::to_wstring(scalePercent).c_str());
 label(L"热点 X",185,280,65);offsetXBox=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",std::to_wstring(offsetX).c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,250,276,65,24,mainw,NULL,inst,NULL);
 label(L"热点 Y",330,280,65);offsetYBox=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",std::to_wstring(offsetY).c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,395,276,65,24,mainw,NULL,inst,NULL);
 autoBox=CreateWindowW(L"BUTTON",L"自动缩放",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,315,220,26,mainw,NULL,inst,NULL);fitBox=CreateWindowW(L"BUTTON",L"保持纵横比",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,245,315,255,26,mainw,NULL,inst,NULL);
 SendMessageW(autoBox,BM_SETCHECK,autoScale?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(fitBox,BM_SETCHECK,aspectFit?BST_CHECKED:BST_UNCHECKED,0);
 for(HWND q:{scaleBox,offsetXBox,offsetYBox,autoBox,fitBox})SendMessageW(q,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 label(L"",15,350,720);
 auto setting=[&](const wchar_t* key,int max){return std::max(0,std::min(max,(int)GetPrivateProfileIntW(L"Cursor",key,0,ini().c_str())));};gameWOverride=setting(L"GameWidth",16384);gameHOverride=setting(L"GameHeight",16384);screenWOverride=setting(L"ScreenWidth",16384);screenHOverride=setting(L"ScreenHeight",16384);dpiOverride=setting(L"WindowsScalePercent",500);if(dpiOverride&&dpiOverride<50)dpiOverride=50;dpiMode=setting(L"WindowsScaleMode",2);
 auto edit=[&](int v,int x,int y,int width){HWND q=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",std::to_wstring(v).c_str(),WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x,y,width,24,mainw,NULL,inst,NULL);SendMessageW(q,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return q;};
 label(L"游戏宽/高",15,390,75);gameWBox=edit(gameWOverride,90,386,70);gameHBox=edit(gameHOverride,170,386,70);label(L"系统宽/高",265,390,80);screenWBox=edit(screenWOverride,350,386,70);screenHBox=edit(screenHOverride,430,386,70);label(L"Windows %",520,390,105);dpiBox=edit(dpiOverride,635,386,90);
 label(L"缩放计算",15,430,345);dpiModeBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,365,426,360,140,mainw,NULL,inst,NULL);for(const wchar_t* text:{L"物理像素",L"系统缩放",L"抵消缩放"})SendMessageW(dpiModeBox,CB_ADDSTRING,0,(LPARAM)text);SendMessageW(dpiModeBox,CB_SETCURSEL,dpiMode,0);SendMessageW(dpiModeBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 readout=CreateWindowW(L"STATIC",L"自动识别分辨率与刷新率",WS_CHILD|WS_VISIBLE,15,472,725,60,mainw,NULL,inst,NULL);SendMessageW(readout,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 adaptive=GetPrivateProfileIntW(L"Animation",L"Adaptive",0,ini().c_str())!=0;
 smoothing=GetPrivateProfileIntW(L"Animation",L"Smooth",0,ini().c_str())!=0;smoothMs=std::max(10,std::min(200,(int)GetPrivateProfileIntW(L"Animation",L"BlendMs",60,ini().c_str())));clickEnabled=GetPrivateProfileIntW(L"Click",L"Enabled",0,ini().c_str())!=0;clickMode=std::min(2u,GetPrivateProfileIntW(L"Click",L"Mode",0,ini().c_str()));clickGap=std::max(0,std::min(250,(int)GetPrivateProfileIntW(L"Click",L"IntervalMs",0,ini().c_str())));sidebarWidth=std::max(32,std::min(600,(int)GetPrivateProfileIntW(L"Click",L"SidebarWidth",160,ini().c_str())));sidebarTop=std::max(0,std::min(2000,(int)GetPrivateProfileIntW(L"Click",L"SidebarTop",200,ini().c_str())));
 smoothBox=CreateWindowW(L"BUTTON",L"平滑补帧",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,545,240,26,mainw,NULL,inst,NULL);SendMessageW(smoothBox,BM_SETCHECK,smoothing?BST_CHECKED:BST_UNCHECKED,0);label(L"备用 ms",265,549,80);smoothMsBox=edit(smoothMs,350,545,70);
 adaptiveBox=CreateWindowW(L"BUTTON",L"自适应",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,445,545,270,26,mainw,NULL,inst,NULL);SendMessageW(adaptiveBox,BM_SETCHECK,adaptive?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(adaptiveBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 timingReadout=CreateWindowW(L"STATIC",L"补帧就绪",WS_CHILD|WS_VISIBLE,15,743,515,26,mainw,NULL,inst,NULL);
 previewBox=CreateWindowW(L"BUTTON",L"预览",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,540,743,210,31,mainw,(HMENU)10,inst,NULL);
 SendMessageW(previewBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);SendMessageW(timingReadout,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 clickBox=CreateWindowW(L"BUTTON",L"Shift 五连点",WS_CHILD|BS_AUTOCHECKBOX,15,584,260,26,mainw,NULL,inst,NULL);SendMessageW(clickBox,BM_SETCHECK,clickEnabled?BST_CHECKED:BST_UNCHECKED,0);clickGapBox=edit(clickGap,415,584,70);ShowWindow(clickGapBox,SW_HIDE);
 clickModeBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|CBS_DROPDOWNLIST,15,623,485,140,mainw,NULL,inst,NULL);for(const wchar_t* t:{L"自动补点",L"强制补点",L"关闭补点"})SendMessageW(clickModeBox,CB_ADDSTRING,0,(LPARAM)t);SendMessageW(clickModeBox,CB_SETCURSEL,clickMode,0);for(HWND q:{smoothBox,clickBox,clickModeBox})SendMessageW(q,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 label(L"",15,665,730);
 systemCursorGuard=GetPrivateProfileIntW(L"Cursor",L"SystemCursorMenuGuard",0,ini().c_str())!=0;autoStart=GetPrivateProfileIntW(L"Startup",L"AutoStart",0,ini().c_str())!=0;
#ifdef RA2_UNIFIED
 autoStart=suite::enabled(0);
#endif
 focusRefresh=GetPrivateProfileIntW(L"Startup",L"FocusRefresh",0,ini().c_str())!=0;
 autoStartBox=CreateWindowW(L"BUTTON",L"自动检测",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,15,705,520,26,mainw,(HMENU)8,inst,NULL);SendMessageW(autoStartBox,BM_SETCHECK,autoStart?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(autoStartBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 nativeGameRefresh=mode==L"hide"&&GetPrivateProfileIntW(L"Display",L"GameThreadRefresh",0,ini().c_str())!=0;
 if(wcsstr(cmd,L"--game-refresh")&&mode==L"hide")nativeGameRefresh=true;
 nativeBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,114,174,226,130,mainw,(HMENU)12,inst,NULL);
 SendMessageW(nativeBox,CB_ADDSTRING,0,(LPARAM)L"外置光标");SendMessageW(nativeBox,CB_ADDSTRING,0,(LPARAM)L"高危：游戏内光标（需隐藏模式）");
 SendMessageW(nativeBox,CB_SETCURSEL,nativeGameRefresh?1:0,0);
 hudEnabled=GetPrivateProfileIntW(L"Display",L"HUD",0,ini().c_str())!=0;hudBox=CreateWindowW(L"BUTTON",L"状态面板",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,540,705,210,26,mainw,(HMENU)9,inst,NULL);SendMessageW(hudBox,BM_SETCHECK,hudEnabled?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(hudBox,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 // Keep the existing settings intact, but move them into an initially hidden
 // scrollable Advanced section.  Top-level actions never require scrolling.
 for(HWND child=GetWindow(mainw,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
  if(child==clickBox||child==clickModeBox||child==clickGapBox)continue;
  wchar_t cls[32]={};GetClassNameW(child,cls,32);
  RECT r{};GetWindowRect(child,&r);
  POINT pt={r.left,r.top};ScreenToClient(mainw,&pt);
  uidpi::Rect logical{pt.x,pt.y,r.right-r.left,r.bottom-r.top};
  if(child==nativeBox)logical.h=130;
  if(child==modeBox)logical.h=110;
  if(child==scaleBox)logical.h=180;
  if(child==dpiModeBox||child==clickModeBox)logical.h=140;
  if(child==statusw){logical={42,81,290,24};}
  else if(child!=nativeBox){logical.y+=advancedOffsetY;advancedControls.push_back(child);ShowWindow(child,SW_HIDE);}
  controlOrigins.push_back({child,logical});
 }
 titleLabel=CreateWindowW(L"STATIC",L"光标与渲染",WS_CHILD|WS_VISIBLE,20,16,220,26,mainw,nullptr,inst,nullptr);
 subTitleLabel=CreateWindowW(L"STATIC",L"光标、场景画质与特效 · 各项独立启用",WS_CHILD|WS_VISIBLE,20,44,320,22,mainw,nullptr,inst,nullptr);
 primaryAction=CreateWindowW(L"BUTTON",L"暂停",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20,128,132,34,mainw,(HMENU)101,inst,nullptr);
 advancedAction=CreateWindowW(L"BUTTON",L"高级设置 ▾",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,164,128,176,34,mainw,(HMENU)102,inst,nullptr);
 quickInfo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,20,250,320,20,mainw,nullptr,inst,nullptr);
 advancedHeader=CreateWindowW(L"STATIC",L"高级设置",WS_CHILD,20,218,150,26,mainw,nullptr,inst,nullptr);
 for(HWND child:{titleLabel,subTitleLabel,primaryAction,advancedAction,quickInfo,advancedHeader}){
  RECT r{};GetWindowRect(child,&r);POINT pt={r.left,r.top};ScreenToClient(mainw,&pt);
  controlOrigins.push_back({child,{pt.x,pt.y,r.right-r.left,r.bottom-r.top}});
 }
 auto extra=[&](HWND h,uidpi::Rect r){controlOrigins.push_back({h,r});advancedControls.push_back(h);};
 settingsHint=CreateWindowW(L"STATIC",L"",WS_CHILD,180,222,555,22,mainw,NULL,inst,NULL);extra(settingsHint,{180,222,555,22});
 styleBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|CBS_DROPDOWNLIST,20,254,320,240,mainw,NULL,inst,NULL);extra(styleBox,{20,254,320,240});
 HWND import=CreateWindowW(L"BUTTON",L"导入 SHA",WS_CHILD|BS_PUSHBUTTON,350,254,108,30,mainw,(HMENU)130,inst,NULL);extra(import,{350,254,108,30});
 prepareButton=CreateWindowW(L"BUTTON",L"预处理补帧",WS_CHILD|BS_PUSHBUTTON,468,254,128,30,mainw,(HMENU)131,inst,NULL);extra(prepareButton,{468,254,128,30});
 HWND groupEdit=CreateWindowW(L"BUTTON",L"动画分组",WS_CHILD|BS_PUSHBUTTON,606,254,128,30,mainw,(HMENU)132,inst,NULL);extra(groupEdit,{606,254,128,30});
 prepareInfo=CreateWindowW(L"STATIC",L"",WS_CHILD,20,290,710,22,mainw,NULL,inst,NULL);extra(prepareInfo,{20,290,710,22});
 superCenter=GetPrivateProfileIntW(L"Cursor",L"SuperWeaponVisualCenter",0,ini().c_str())!=0;
 superCenterBox=CreateWindowW(L"BUTTON",L"超武热点居中",WS_CHILD|BS_AUTOCHECKBOX,20,1095,510,25,mainw,(HMENU)154,inst,NULL);extra(superCenterBox,{20,1095,510,25});
 SendMessageW(superCenterBox,BM_SETCHECK,superCenter?BST_CHECKED:BST_UNCHECKED,0);
 modernui::tip(superCenterBox,L"超武图案的视觉锚点对齐点击位置；铁幕使用中间圈圆心。启用时不叠加普通光标的手动热点偏移。");
 detailLabel=CreateWindowW(L"STATIC",L"",WS_CHILD,20,1122,720,42,mainw,NULL,inst,NULL);extra(detailLabel,{20,1122,720,42});
 HWND displayLabel=CreateWindowW(L"STATIC",L"显示方式",WS_CHILD|WS_VISIBLE,20,178,88,22,mainw,nullptr,inst,nullptr);
 controlOrigins.push_back({displayLabel,{20,178,88,22}});
 for(auto& c:controlOrigins)if(c.handle==advancedHeader||std::find(advancedControls.begin(),advancedControls.end(),c.handle)!=advancedControls.end())c.logical.y+=240;
 configuredPollHz=GetPrivateProfileIntW(L"Display",L"TargetFPS",GetPrivateProfileIntW(L"Display",L"PollHz",0,ini().c_str()),ini().c_str());
 if(configuredPollHz)configuredPollHz=std::clamp(configuredPollHz,30u,500u);
 HWND rateLabel=CreateWindowW(L"STATIC",L"目标刷新率",WS_CHILD|WS_VISIBLE,20,214,88,22,mainw,nullptr,inst,nullptr);
 targetBox=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWN|WS_VSCROLL,114,210,226,180,mainw,(HMENU)153,inst,nullptr);
 for(const wchar_t* rate:{L"自动",L"60",L"120",L"144",L"165",L"240",L"360",L"500"})SendMessageW(targetBox,CB_ADDSTRING,0,(LPARAM)rate);
 SetWindowTextW(targetBox,configuredPollHz?std::to_wstring(configuredPollHz).c_str():L"自动");
 controlOrigins.push_back({rateLabel,{20,214,88,22}});controlOrigins.push_back({targetBox,{114,210,226,180}});
#ifdef RA2_UNIFIED
 auto basic=[&](const wchar_t* cls,const wchar_t* text,int x,int y,int width,int height,int id,DWORD style=0){
  auto h=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,mainw,(HMENU)(INT_PTR)id,inst,nullptr);controlOrigins.push_back({h,{x,y,width,height}});return h;
 };
 basic(L"STATIC",L"高刷范围",20,254,88,22,0);
 refreshModeBox=basic(L"COMBOBOX",L"",114,250,626,150,155,CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP);
 for(const wchar_t* text:{L"仅光标",L"光标＋拖屏（实验性）",L"全部（实验性）",L"强制高刷（实验性 · 联机待验证）"})SendMessageW(refreshModeBox,CB_ADDSTRING,0,(LPARAM)text);
 disabledchoices::set(refreshModeBox,1);
 refreshStateLabel=basic(L"STATIC",L"读取设置…",20,294,720,24,0);
 modernui::tip(refreshModeBox,L"当前仅保留光标高刷；拖屏、全部及强制高刷暂不可用。");
 modernui::tip(targetBox,L"自动跟随显示器；也可输入 30–500 FPS，当前用于光标刷新。");
 EnableWindow(refreshModeBox,FALSE);
 basic(L"BUTTON",L"配置 DDraw 高刷…",20,366,350,32,157,BS_PUSHBUTTON|WS_TABSTOP);
 basic(L"BUTTON",L"恢复 DDraw 配置…",390,366,350,32,158,BS_PUSHBUTTON|WS_TABSTOP);
#endif
#ifdef RA2_UNIFIED
 auto videoControl=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){return basic(cls,text,x,y,width,height,id,style);};
 const int videoEnd=renderVideoSettings.create(videoControl,suite::VideoGroup::Render,468,20,720);
 const int shift=videoEnd-438;for(auto&c:controlOrigins)if(c.handle==advancedHeader||std::find(advancedControls.begin(),advancedControls.end(),c.handle)!=advancedControls.end())c.logical.y+=shift;
 compactContentH=videoEnd+20;advancedContentH+=shift;uiContentH=advancedOpen?advancedContentH:compactContentH;
#endif
 // Main and advanced settings share one width, palette and renderer.
 for(auto& c:controlOrigins){
  if(c.handle==statusw)c.logical.w=690;
  if(c.handle==subTitleLabel||c.handle==quickInfo)c.logical.w=720;
  if(c.handle==quickInfo)c.logical.y=418;
  if(c.handle==primaryAction)c.logical={20,128,354,34};
  if(c.handle==advancedAction)c.logical={386,128,354,34};
  if(c.handle==nativeBox||c.handle==targetBox)c.logical.w=626;
  wchar_t cls[32]{};GetClassNameW(c.handle,cls,32);
  if(!_wcsicmp(cls,L"BUTTON")||!_wcsicmp(cls,L"EDIT")||!_wcsicmp(cls,L"COMBOBOX"))
   SetWindowLongPtrW(c.handle,GWL_STYLE,GetWindowLongPtrW(c.handle,GWL_STYLE)|WS_TABSTOP);
 }
 modernui::tip(primaryAction,L"开关高刷显示；快捷键 Ctrl+Alt+F11。保留原光标使用外部显示；高危隐藏模式修改原生提示入口，可能影响平台兼容。设置自动保存。");
 modernui::tip(styleBox,L"选择独立光标包；不会覆盖游戏目录。");
 modernui::tip(prepareButton,L"在本机生成光标动画补帧；可再次点击取消。已完成分段会保留。");
 modernui::tip(import,L"导入兼容红警或尤里布局的 SHA 文件与色盘。");
 modernui::tip(groupEdit,L"编辑当前光标包的动画分组。");
 modernui::styleChildren(mainw);
 refreshStyleList();
 uiReady=true;
 uiDpi=windowDpi(mainw);
 setUiFont();
 relayout(mainw);
 viewport(mainw,0,0);
 try{chooseStyle();}catch(const std::exception& e){log(widen(e.what()));}
 updatePrimaryAction();
 log(L"Shift5 uses host polling; no mouse interception");
 wchar_t dir[32768]={};GetPrivateProfileStringW(L"Target",L"Directory",L"",dir,32768,ini().c_str());if(*dir){try{setDir(dir);loadCache();}catch(const std::exception& e){log(L"Startup asset seeding failed: "+widen(e.what()));}}
 #ifndef RA2_UNIFIED
 // Fit a DPI-scaled initial client area into the monitor's work region.
 {
  RECT win{},client{};GetWindowRect(mainw,&win);GetClientRect(mainw,&client);
  const int frameW=(win.right-win.left)-(client.right-client.left);
  const int frameH=(win.bottom-win.top)-(client.bottom-client.top);
  const int wantedW=uidpi::scale(uiContentW,uiDpi)+frameW;
  const int wantedH=uidpi::scale(uiContentH,uiDpi)+frameH;
  HMONITOR mon=MonitorFromWindow(mainw,MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{};mi.cbSize=sizeof(mi);
  if(GetMonitorInfoW(mon,&mi)){
   const int availableW=mi.rcWork.right-mi.rcWork.left;
   const int availableH=mi.rcWork.bottom-mi.rcWork.top;
   const int finalW=std::min(wantedW,availableW);
   const int finalH=std::min(wantedH,availableH);
   const int x=mi.rcWork.left+std::max(0,(availableW-finalW)/2);
   const int y=mi.rcWork.top+std::max(0,(availableH-finalH)/2);
   SetWindowPos(mainw,NULL,x,y,finalW,finalH,SWP_NOZORDER|SWP_NOACTIVATE);
  }
 }
 #endif
#ifdef RA2_UNIFIED
 suite::registerPane(0,mainw);
#else
 ShowWindow(mainw,SW_SHOW);
#endif
 updatePrimaryAction();if(wcsstr(cmd,L"restore")){try{restoreFile();cursorDialog(lastStatus.c_str(),L"恢复 SHA",MB_OK);}catch(const std::exception& e){error(e);}DestroyWindow(mainw);return 0;}RegisterHotKey(mainw,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F11);RegisterHotKey(mainw,2,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F12);RegisterHotKey(mainw,3,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F10);RegisterHotKey(mainw,4,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F9);timeBeginPeriod(1);QueryPerformanceFrequency(&freq);LARGE_INTEGER next;QueryPerformanceCounter(&next);updateRefresh(true);MSG msg;bool quit=false;
 HANDLE frameTimer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
 if(!frameTimer)frameTimer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
 LONGLONG armedDeadline=0;uimessages::Queue messages;
 while(!quit){
  unsigned dispatched=0;
  while(dispatched++<32&&messages.peek(msg)){if(msg.message==WM_QUIT){quit=true;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}
  if(quit)break;
  motionPoll();autoTick();updateRefresh();pollPrepare();
  LARGE_INTEGER now;QueryPerformanceCounter(&now);
  if(now.QuadPart>=next.QuadPart){
   LONGLONG drawStarted=now.QuadPart;
   try{draw();}catch(const std::exception& e){autoPaused=true;stop();error(e);}
   QueryPerformanceCounter(&now);LONGLONG cost=now.QuadPart-drawStarted;drawTotalTicks+=cost;drawMaxTicks=std::max(drawMaxTicks,cost);++drawMeasured;
   next.QuadPart=cursorperf::nextFrameTick(next.QuadPart,now.QuadPart,freq.QuadPart,pollHz);
  }
  performanceSample();if(hudEnabled)diagnosticHud();
  QueryPerformanceCounter(&now);
  LONGLONG deadline=running?next.QuadPart:now.QuadPart+freq.QuadPart/20;
  deadline=cursorperf::inputDeadline(deadline,now.QuadPart,freq.QuadPart,running&&clickEnabled&&clickMode!=2);
  if(burst.left>0){
   int32_t ms=int32_t(burst.due-inputNow());
   LONGLONG due=now.QuadPart+std::max<int32_t>(0,ms)*freq.QuadPart/1000;
   deadline=std::min(deadline,due);
  }
  if(frameTimer){
   if(deadline!=armedDeadline){
    LARGE_INTEGER due;due.QuadPart=-std::max<LONGLONG>(1,(deadline-now.QuadPart)*10000000/freq.QuadPart);
    if(SetWaitableTimer(frameTimer,&due,0,nullptr,nullptr,FALSE))armedDeadline=deadline;
    else {CloseHandle(frameTimer);frameTimer=nullptr;}
   }
   if(frameTimer){DWORD result=MsgWaitForMultipleObjectsEx(1,&frameTimer,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);if(result==WAIT_OBJECT_0)armedDeadline=0;}
  }
  if(!frameTimer){DWORD delay=DWORD(std::max<LONGLONG>(1,(deadline-now.QuadPart)*1000/freq.QuadPart));MsgWaitForMultipleObjectsEx(0,nullptr,delay,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}
 }
 if(frameTimer)CloseHandle(frameTimer);
 timeEndPeriod(1);hookStop();cursorReader.close();if(dib){SelectObject(memdc,oldbm);DeleteObject(dib);}if(memdc)DeleteDC(memdc);CloseHandle(mutex);return 0;
}


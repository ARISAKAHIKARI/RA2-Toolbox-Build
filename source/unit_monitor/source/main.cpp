#include "../../shared/source/latest_worker.h"
#include "../../shared/source/modern_ui.h"
#include "../../shared/source/overlay_stack.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../shared/source/release_version.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <tlhelp32.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdint>
#include "core.h"
#include "../../shared/source/viewport_scale.h"
#include "sampler.h"
#include "../../external_reader/source/display_state.h"
#include "../../external_reader/source/shared_reader.h"
#include "../../external_reader/source/process_discovery.h"
#ifdef RA2_UNIFIED
#include "suite_host.h"
#endif
#include "tab_control.h"
#include "../../shared/source/overlay_recovery.h"
#include "../../shared/source/monitor_refresh.h"
#include "../../shared/source/secondary_display.h"
#include "frame_clock.h"
#include "../../external_reader/source/game_asset_files.h"
using namespace Gdiplus;
static HWND ui,overlay,secondaryOverlay,stats,bStart,bDemo,cBuildings,cOthers,eTop,eLeft,cTab,cSecondary,tabModeBox,visibilityLabel;static HFONT font;
static void hidePrimaryStats(){overlaycompositor::host().hide(overlaycompositor::Statistics);if(overlay)ShowWindow(overlay,SW_HIDE);}
static external::ReadSession reader;static um::IconCache& iconCache=um::sharedIconCache();
static std::unique_ptr<um::Sampler<external::SharedReadView,external::NoResolve>> sampler;
static std::unique_ptr<um::Snapshot> snap(new um::Snapshot{}),incoming(new um::Snapshot{});
static HWND game;static DWORD pid,threadId;static bool enabled=true,demo=false,showOthers=true,buildings=false,hidden=false,manualVisible=false;
static int topMargin=20,leftMargin=6,scalePercent=100;static bool controlsReady=false;
static int secondaryX=0,secondaryY=0;
static int page=0;static DWORD lastSearch=0,lastGood=0;static std::wstring root,status;static FILE* logfile;
#include "../../external_reader/source/cameo_catalog.h"
static external::assets::Files assetFiles;static uint32_t assetMatch=0;
static bool loadAsset(const char* name,std::vector<uint8_t>&out){assetFiles.update(reader,pid,assetMatch);return assetFiles.load(name,out);}

static bool readCameoMemory(uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);}
static external::assets::RuntimeCameos<decltype(&readCameoMemory)> cameoCatalog(&readCameoMemory);
static void resolveCameo(um::Type&t,uint32_t match){
 assetMatch=match;
 assetFiles.update(reader,pid,assetMatch);char image[26]{};reader.readFresh(t.ptr+0x1F8,image,25);
 auto file=cameoCatalog.resolve(loadAsset,assetFiles.revision(),assetFiles.revision()^(GetTickCount64()/5000),t.id,image,t.cameo);
 memset(t.cameo,0,sizeof t.cameo);if(file.size()<sizeof t.cameo)memcpy(t.cameo,file.data(),file.size());
}
static uint32_t demoStart,oldMatch=0;static DWORD lastData=0;
static void statsVisible(bool value){
#ifdef RA2_UNIFIED
 suite::setStatsVisible(value);
#endif
}
static bool tabEnabled=true,renderDirty=true,secondaryDirty=true,gameActive=false,secondaryEnabled=false,secondaryActive=false;
static secondarydisplay::Monitor secondaryMonitor;
static void resetSecondary();
static tabui::Mode tabMode=tabui::Mode::TapOrHold;static tabui::Controller tabs;static tabui::Fade fade;
static overlayui::Recovery overlayRecovery,secondaryRecovery;
static constexpr UINT StatsFrameMessage=WM_APP+10;
static umframe::Clock statsFrameClock;
static monitorrefresh::Tracker statsRefreshMonitor;
static int statsEffectiveFPS=60;
static int statsInformationFPS(){return statsEffectiveFPS;}
#ifdef RA2_UNIFIED
static void syncUnitInformationRate();
#endif
static void refreshInformationRate(DWORD now){
 int rate=60;
#ifdef RA2_UNIFIED
 suite::DisplayState state;
 if(suite::readDisplaySettings(state)){
  HWND fallback=suite::parent();if(!fallback)fallback=ui;
  rate=state.values[suite::InfoAdaptive]?statsRefreshMonitor.update(demo?nullptr:game,fallback,now):std::clamp(state.values[suite::InfoFPS],5,500);
 }
#endif
 if(rate!=statsEffectiveFPS){statsEffectiveFPS=rate;
#ifdef RA2_UNIFIED
  syncUnitInformationRate();
#endif
 }
}
struct UnitSurface {
 HDC dc=nullptr;HBITMAP bitmap=nullptr,old=nullptr;void* bits=nullptr;int w=0,h=0;
 void clear(){
  if(dc){if(old&&reinterpret_cast<HGDIOBJ>(old)!=HGDI_ERROR)SelectObject(dc,old);DeleteDC(dc);}
  if(bitmap)DeleteObject(bitmap);
  dc=nullptr;bitmap=old=nullptr;bits=nullptr;w=h=0;
 }
 bool ensure(int width,int height){if(dc&&w==width&&h==height)return true;clear();
  if(width<1||height<1||uint64_t(width)*height>16ull*1024*1024)return false;
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
  HDC screen=GetDC(nullptr);if(!screen)return false;
  dc=CreateCompatibleDC(screen);
  if(dc)bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
  ReleaseDC(nullptr,screen);
  if(!dc||!bitmap||!bits){clear();return false;}
  old=(HBITMAP)SelectObject(dc,bitmap);
  if(!old||reinterpret_cast<HGDIOBJ>(old)==HGDI_ERROR){old=nullptr;clear();return false;}
  w=width;h=height;return true;
 }
} surface,secondarySurface;
static int paintedAlpha=-1;static POINT paintedOrigin{LONG_MIN,LONG_MIN};
static bool foregroundGame(){DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return game&&IsWindow(game)&&!IsIconic(game)&&foreground==pid;}
static bool tabEligible(){return tabEnabled&&enabled&&gameActive&&!demo&&snap->status==1&&!snap->chatActive&&DWORD(GetTickCount()-snap->tick)<2000&&foregroundGame();}
static void pollTab(){
 static bool previous=false;bool down=(GetAsyncKeyState(VK_TAB)&0x8000)!=0;
 bool modified=((GetAsyncKeyState(VK_MENU)|GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_SHIFT)|GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)!=0;
 if(!tabEligible()||modified){tabs.loseFocus();previous=down;return;}
 if(down!=previous){previous=down;if(tabs.event(down,tabEligible(),modified,false,GetTickCount())){if(down){hidden=false;manualVisible=false;}renderDirty=secondaryDirty=true;}}
}
static bool changed(const um::Snapshot& a,const um::Snapshot& b){
 return a.match!=b.match||a.status!=b.status||a.typeCount!=b.typeCount||a.playerCount!=b.playerCount||a.paletteReady!=b.paletteReady||
  memcmp(a.palette,b.palette,sizeof a.palette)||memcmp(a.types,b.types,sizeof(um::Type)*a.typeCount)||memcmp(a.players,b.players,sizeof(um::Player)*a.playerCount);
}
static void log(const wchar_t* msg){if(logfile){fwprintf(logfile,L"[%lu] %ls\n",GetTickCount(),msg);fflush(logfile);}}
static std::wstring path(const wchar_t* f){return root+L"\\"+f;}
static void setStatus(const std::wstring& s){if(status!=s){status=s;SetWindowTextW(stats,s.c_str());log(s.c_str());}}
static void save(){
 const auto ini=path(L"settings.ini");
 WritePrivateProfileStringW(L"overlay",L"secondaryEnabled",secondaryEnabled?L"1":L"0",ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"secondaryX",std::to_wstring(secondaryX).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"secondaryY",std::to_wstring(secondaryY).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"scalePercent",std::to_wstring(scalePercent).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"tabEnabled",tabEnabled?L"1":L"0",ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"tabMode",std::to_wstring(int(tabMode)).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"topMargin",std::to_wstring(topMargin).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"leftMargin",std::to_wstring(leftMargin).c_str(),ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"buildings",buildings?L"1":L"0",ini.c_str());
 WritePrivateProfileStringW(L"overlay",L"otherPlayers",showOthers?L"1":L"0",ini.c_str());
}
static void visibilityStatus(const std::wstring& text){
 static std::wstring previous;if(previous==text)return;previous=text;
 if(visibilityLabel)SetWindowTextW(visibilityLabel,text.c_str());log(text.c_str());
}
static void toggleOverlay(){tabui::toggleDisplay(!tabEnabled,tabs.visible(),hidden,manualVisible);renderDirty=secondaryDirty=true;}
static void detach(){
 statsFrameClock.state(false,statsInformationFPS());
 overlayRecovery.suspend();
 statsVisible(false);resetSecondary();
 sampler.reset();assetMatch=0;assetFiles.clear();cameoCatalog.clear();reader.close();
 game=nullptr;pid=0;threadId=0;lastGood=0;oldMatch=0;page=0;gameActive=false;tabs.reset();fade.hide();renderDirty=secondaryDirty=true;paintedAlpha=-1;surface.clear();memset(snap.get(),0,sizeof(*snap));memset(incoming.get(),0,sizeof(*incoming));hidePrimaryStats();
}
static bool attach(){
 auto scan=external::discovery::scan();
 if(!scan.best.pid){
  if(scan.snapshotError)setStatus(L"游戏进程快照失败，Win32="+std::to_wstring(scan.snapshotError));
  else if(!scan.processCount)setStatus(L"尚未发现兼容游戏进程（支持随机进程名）");
  else setStatus(L"多个无窗口游戏进程，无法确定目标 PID");
  return false;
 }
 const auto& c=scan.best;
 if(!reader.open(c.pid)){
  setStatus(L"发现 PID "+std::to_wstring(c.pid)+L"，但读取权限不足，Win32="+std::to_wstring(reader.lastOpenError));
  return false;
 }
 pid=c.pid;threadId=c.thread;game=external::discovery::resolveYrppWindow(reader,pid,c.window,&threadId);
 wchar_t image[32768]{};DWORD imageSize=32768;reader.imagePath(image,&imageSize);iconCache.open(std::filesystem::path(image),std::filesystem::path(root)/L"cache"/L"icons");
 sampler=std::make_unique<um::Sampler<external::SharedReadView,external::NoResolve>>(external::SharedReadView{&reader},external::NoResolve{},&iconCache,loadAsset,resolveCameo);
 setStatus(L"已连接只读 PID "+std::to_wstring(pid)+(game?L"，验证布局…":L"，等待窗口后再显示覆盖层"));lastGood=GetTickCount();return true;
}
static Color color(uint32_t c){return Color(c);}
static void text(Graphics& g,const std::wstring& s,float x,float y,float size,Color c,bool bold=false){
 FontFamily fam(L"Microsoft YaHei UI");GraphicsPath p;StringFormat format;format.SetFormatFlags(StringFormatFlagsNoWrap);p.AddString(s.c_str(),-1,&fam,bold?FontStyleBold:FontStyleRegular,size,PointF(x,y),&format);
 Pen outline(Color(220,10,15,23),2.5f);outline.SetLineJoin(LineJoinRound);g.DrawPath(&outline,&p);SolidBrush brush(c);g.FillPath(&brush,&p);
}
struct CachedIconBitmap {uint64_t hash=0;uint32_t pixels[um::IW*um::IH]{};std::unique_ptr<Bitmap> image;};
static std::map<std::string,std::unique_ptr<CachedIconBitmap>> iconImages;
static void drawIcon(Graphics& g,const um::Type& t,const um::Snapshot& s,float x,float y,float w,float h,uint32_t col){
 static thread_local std::map<std::string,std::unique_ptr<CachedIconBitmap>> iconImages;
 if(t.iconReady&&s.paletteReady){
  auto&images=iconImages;
  const auto key=um::iconcache::key(t);uint64_t hash=um::iconcache::hash(s.palette,sizeof s.palette,um::iconcache::hash(t.icon,sizeof t.icon));
  if(images.size()>=um::MaxTypes&&!images.count(key))images.clear();auto&entry=images[key];if(!entry)entry=std::make_unique<CachedIconBitmap>();
  if(!entry->image||entry->hash!=hash){entry->image.reset();for(int i=0;i<um::IW*um::IH;i++)entry->pixels[i]=s.palette[t.icon[i]]|0xFF000000;
   entry->image=std::make_unique<Bitmap>(um::IW,um::IH,um::IW*4,PixelFormat32bppARGB,(BYTE*)entry->pixels);entry->hash=hash;}
  g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.DrawImage(entry->image.get(),RectF(x,y,w,h),0,0,um::IW,um::IH,UnitPixel);
 }
 else {Pen p(color(col),1.5f);g.DrawRectangle(&p,x+2,y+2,w-4,h-4);std::wstring id;for(char c:t.id){if(!c)break;id+=wchar_t((unsigned char)c);}text(g,id.substr(0,8),x+4,y+h/3,10,Color::White);}
 Pen border(color(col),1.5f);g.DrawRectangle(&border,x,y,w,h);
}
struct RenderRequest {int width,height,gameWidth,gameHeight,leftMargin,topMargin,scalePercent,page;bool showOthers,buildings,demo;uint32_t pid,match,tick;std::shared_ptr<const um::Snapshot> snap;};
struct RenderFrame {RenderRequest request;int width,height;std::vector<uint32_t> pixels;};
static std::shared_ptr<const RenderFrame> renderPixels(const RenderRequest&r){
 auto snap=r.snap;int width=r.width,height=r.height,gameWidth=r.gameWidth,gameHeight=r.gameHeight,leftMargin=r.leftMargin,topMargin=r.topMargin,scalePercent=r.scalePercent,page=r.page;bool showOthers=r.showOthers,buildings=r.buildings,demo=r.demo;
 if(width<100||height<100||width>16384||height>16384)return {};
 viewport::Scale view;if(!view.set(gameWidth,gameHeight,width,height))return {};
 // Scale content around the existing margin anchor; fit all player columns.
 int players=0;for(int i=0;i<snap->playerCount;++i)if(showOthers||snap->players[i].local)++players;
 float anchorX=float(std::clamp(leftMargin,0,std::max(0,gameWidth-100))),anchorY=float(std::clamp(topMargin,0,std::max(0,gameHeight-100)));
 float zoom=std::min(viewport::factor(scalePercent),std::min((gameWidth-anchorX-8)/(std::max(1,players)*80.f+8),(gameHeight-anchorY-8)/124.f));
 zoom=std::max(.1f,zoom);view.x*=zoom;view.y*=zoom;
 int viewportW=int(gameWidth/zoom),viewportH=int(gameHeight/zoom);
 float top=anchorY/zoom,left=anchorX/zoom;
 const float maxWidth=std::min(112.f,std::max(76.f,(viewportW-left-6.f)/8.f-4.f));
 float croppedW=left+8,croppedH=top+50;
 for(int pi=0;pi<snap->playerCount;++pi){auto& p=snap->players[pi];if(!showOthers&&!p.local)continue;
  int entries=0;for(int i=0;i<snap->typeCount;++i)entries+=um::showCell(snap->types[i].category,p.cells[i],buildings);
  int across=entries>1&&maxWidth>=112?2:1;float panelW=across==2?maxWidth:std::min(84.f,maxWidth),rowH=std::min(44.f,(panelW-12)/across-4)*.8f+18;
  int rows=std::max(1,int((viewportH-top-73)/rowH)),per=rows*across,pages=std::max(1,(entries+per-1)/per),start=(page%pages)*per,shownRows=std::max(1,(std::min(entries,start+per)-start+across-1)/across);
  croppedW+=panelW+4;croppedH=std::max(croppedH,top+60+shownRows*rowH);
 }
 width=std::min(viewportW,std::max(1,int(croppedW+2)));height=std::min(viewportH,std::max(1,int(croppedH+2)));
 if(demo){width=viewportW;height=viewportH;}
 width=view.width(width);height=view.height(height);
 if(width<1||height<1||uint64_t(width)*height>16*1024*1024)return {};
 auto result=std::make_shared<RenderFrame>();result->request=r;result->width=width;result->height=height;result->pixels.resize(size_t(width)*height);void*bits=result->pixels.data();
 {Bitmap bm(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)bits);Graphics g(&bm);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetCompositingMode(CompositingModeSourceOver);
 g.ScaleTransform(view.x,view.y);g.TranslateTransform(left-6,top-6);
 float cursorX=6.f;
 for(int pi=0;pi<snap->playerCount;pi++){
  const auto& p=snap->players[pi];if(!showOthers&&!p.local)continue;
  std::vector<int> entries;
  for(int i=0;i<snap->typeCount;i++)if(um::showCell(snap->types[i].category,p.cells[i],buildings))entries.push_back(i);
  std::stable_sort(entries.begin(),entries.end(),[&](int a,int b){return um::categoryOrder(snap->types[a].category)<um::categoryOrder(snap->types[b].category);});
  int across=entries.size()>1&&maxWidth>=112?2:1;
  float panelW=across==2?maxWidth:std::min(84.f,maxWidth),x=cursorX+6,w=panelW-12;
  cursorX+=panelW+4;
  float tileW=w/across,iconW=std::min(44.f,tileW-4),iconH=iconW*.8f,rowH=iconH+18;
  int rows=std::max(1,int((viewportH-top-73)/rowH)),per=rows*across;
  int pages=std::max(1,(int(entries.size())+per-1)/per),current=page%pages;
  int start=current*per,finish=std::min(int(entries.size()),start+per);
  int shownRows=std::max(1,(finish-start+across-1)/across);
  float panelH=56+shownRows*rowH+4;
  SolidBrush panel(Color(158,12,19,29));g.FillRectangle(&panel,cursorX-panelW-4,6.f,panelW,panelH);
  uint32_t pc=p.color;Pen rule(Color(180,(pc>>16)&255,(pc>>8)&255,pc&255),2.f);g.DrawLine(&rule,x,9.f,x+w,9.f);
  std::wstring name=(const wchar_t*)p.name;if(name.empty())name=L"玩家 "+std::to_wstring(p.houseIndex+1);if(p.local)name=L"我·"+name;
  int maxChars=std::max(3,int(w/11));if(int(name.size())>maxChars)name=name.substr(0,maxChars-1)+L"…";
  text(g,name,x,12,11,color(pc),true);
  const std::wstring money=p.creditsKnown?L"$ "+std::to_wstring(p.credits):L"$ —";
  text(g,money,x,27,std::min(10.f,w/(float(money.size())*.65f)),Color(255,241,215,131),true);
  text(g,L"拥有 / +生产",x,41,9,Color(255,214,225,237));
  if(entries.empty())text(g,L"暂无单位",x,60,10,Color(255,190,204,221));
  if(pages>1)text(g,std::to_wstring(current+1)+L"/"+std::to_wstring(pages),x+w-26,41,9,Color::White);
  for(int j=start;j<finish;j++){
   int ix=entries[j],slot=j-start;const auto& c=p.cells[ix];int category=snap->types[ix].category;
   float tx=x+(slot%across)*tileW,ty=56+(slot/across)*rowH;
   drawIcon(g,snap->types[ix],*snap,tx,ty,iconW,iconH,pc);
   if(category!=3||buildings)text(g,std::to_wstring(um::shownOwned(category,c,buildings)),tx+1,ty+iconH,12,Color::White,true);
   if(c.queued){float qx=category==3&&!buildings?tx+1:tx+iconW/2; text(g,L"+"+std::to_wstring(c.queued),qx,ty+iconH+1,10,color(pc),true);}
   if(c.active){Pen base(Color(255,16,22,30),4),progress(color(pc),3);g.DrawLine(&base,tx,ty+iconH-2,tx+iconW,ty+iconH-2);g.DrawLine(&progress,tx,ty+iconH-2,tx+iconW*c.progress/54.f,ty+iconH-2);if(c.paused)text(g,L"Ⅱ",tx+iconW-17,ty+3,12,Color(255,245,208,106),true);}
  }
 }
 g.ResetTransform();
 if(demo)text(g,L"布局演示 · 模拟数据 · Ctrl+Alt+H 隐藏",8,height-32,14,Color(255,238,215,135));
 }
 return result;
}
static asyncwork::Latest<RenderRequest,RenderFrame> renderWorker(renderPixels);
static std::shared_ptr<const RenderFrame> renderedFrame;
static asyncwork::Latest<RenderRequest,RenderFrame> secondaryRenderWorker(renderPixels);
static std::shared_ptr<const RenderFrame> secondaryRenderedFrame;
static POINT secondaryOrigin{LONG_MIN,LONG_MIN};static int secondaryWidth=0,secondaryHeight=0;
static bool secondaryUploaded=false;static DWORD secondaryLastSubmit=0;
static POINT secondaryPosition(const RECT&work,int width,int height,int x,int y){
 int roomX=std::max(0,int(work.right-work.left)-width),roomY=std::max(0,int(work.bottom-work.top)-height);
 return {LONG(int64_t(work.left)+int64_t(roomX)*std::clamp(x,0,10000)/10000),
         LONG(int64_t(work.top)+int64_t(roomY)*std::clamp(y,0,10000)/10000)};
}
static int secondaryTravelUnit(int desired,int low,int room){
 if(room<=0)return 0;
 int distance=std::clamp(int64_t(desired)-low,int64_t(0),int64_t(room));
 return int((int64_t(distance)*10000+room/2)/room);
}
struct SecondaryDrag {
 bool active=false,changed=false;
 POINT cursor{},origin{};RECT work{};int x=0,y=0,width=0,height=0;
};
struct SecondaryDragSettings {int x=0,y=0;};
static SecondaryDragSettings secondaryDragSettings(const SecondaryDrag&drag,POINT cursor){
 SecondaryDragSettings result{drag.x,drag.y};
 int workWidth=int(drag.work.right-drag.work.left),workHeight=int(drag.work.bottom-drag.work.top);
 if(workWidth<=0||workHeight<=0)return result;
 int targetX=int(std::clamp(int64_t(drag.origin.x)+cursor.x-drag.cursor.x,
                             int64_t(drag.work.left),int64_t(drag.work.right-drag.width)));
 int targetY=int(std::clamp(int64_t(drag.origin.y)+cursor.y-drag.cursor.y,
                             int64_t(drag.work.top),int64_t(drag.work.bottom-drag.height)));
 result.x=secondaryTravelUnit(targetX,drag.work.left,workWidth-drag.width);
 result.y=secondaryTravelUnit(targetY,drag.work.top,workHeight-drag.height);
 return result;
}
static SecondaryDrag secondaryDrag;static bool syncingSecondaryDragControls=false;
static void finishSecondaryDrag();
static void paintSecondaryDragGrip(UnitSurface&surface){
 if(!surface.bits||surface.w<48||surface.h<24)return;
 auto* pixels=static_cast<uint32_t*>(surface.bits);
 for(int y=2;y<7;++y)for(int x=8;x<80&&x<surface.w-4;++x)
  pixels[size_t(y)*surface.w+x]=0xff6488a4u;
}
static void resetSecondary(){
 secondaryRecovery.suspend();secondaryRenderWorker.invalidate();secondaryRenderedFrame.reset();secondarySurface.clear();
 secondaryActive=false;secondaryDirty=true;secondaryUploaded=false;secondaryLastSubmit=0;secondaryWidth=secondaryHeight=0;secondaryOrigin={LONG_MIN,LONG_MIN};
 if(secondaryOverlay)ShowWindow(secondaryOverlay,SW_HIDE);
}
static bool updateSecondary(DWORD now,bool battlefield,bool displayFrame){
 // This display has its own user switch. Foreground, primary overlay pause,
 // Tab and Ctrl+Alt+H only govern the game's copy of the statistics.
 bool valid=secondaryEnabled&&game&&IsWindow(game)&&(demo||(battlefield&&reader.valid()&&snap->status==1&&lastGood&&DWORD(now-lastGood)<3000));
 auto target=valid?secondaryMonitor.get(game,ui,now):secondarydisplay::Target{};
 if(!valid||!target.available()||!secondaryOverlay){if(secondaryActive||secondarySurface.dc)resetSecondary();return false;}
 // Leave the right-hand 40% available for the independent minimap/tracking windows.
 int width=int(target.work.right-target.work.left)*3/5,height=int(target.work.bottom-target.work.top);
 if(width<100||height<100){resetSecondary();return false;}
 if(width!=secondaryWidth||height!=secondaryHeight){
  resetSecondary();secondaryWidth=width;secondaryHeight=height;
 }
 secondaryActive=true;if(!displayFrame)return true;
 if(secondaryDirty||(!secondaryRenderedFrame&&DWORD(now-secondaryLastSubmit)>=250)){
  secondaryRenderWorker.submit({width,height,width,height,leftMargin,topMargin,scalePercent,page,showOthers,buildings,demo,pid,oldMatch,now,std::make_shared<um::Snapshot>(*snap)});secondaryDirty=false;secondaryLastSubmit=now;
 }
 auto completed=secondaryRenderWorker.get();if(completed&&completed!=secondaryRenderedFrame){const auto&r=completed->request;
  if(r.pid==pid&&r.match==oldMatch&&r.width==width&&r.height==height&&r.page==page&&r.scalePercent==scalePercent&&r.leftMargin==leftMargin&&r.topMargin==topMargin&&r.showOthers==showOthers&&r.buildings==buildings&&r.demo==demo&&DWORD(now-r.tick)<1000&&secondarySurface.ensure(completed->width,completed->height)){
   memcpy(secondarySurface.bits,completed->pixels.data(),completed->pixels.size()*4);
   paintSecondaryDragGrip(secondarySurface);
   secondaryRenderedFrame=std::move(completed);secondaryUploaded=false;
  }
 }
 if(secondaryDrag.active&&secondarySurface.dc&&
    (secondarySurface.w!=secondaryDrag.width||secondarySurface.h!=secondaryDrag.height))finishSecondaryDrag();
 // Actual layered pixels are cropped to occupied statistics columns/rows.
 // Anchor that visible surface, not its larger rendering budget, to the work area.
 int shownWidth=secondarySurface.dc?secondarySurface.w:width;
 int shownHeight=secondarySurface.dc?secondarySurface.h:height;
 POINT origin=secondaryPosition(target.work,shownWidth,shownHeight,secondaryX,secondaryY);
 if(origin.x!=secondaryOrigin.x||origin.y!=secondaryOrigin.y){
  secondaryOrigin=origin;secondaryUploaded=false;
 }
 bool repair=secondaryRecovery.due(now,true,{uintptr_t(game),pid,snap->match,width,height},IsWindowVisible(secondaryOverlay)!=0);
 if(secondarySurface.dc&&(!secondaryUploaded||repair||!IsWindowVisible(secondaryOverlay))){
  SIZE size{secondarySurface.w,secondarySurface.h};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
  secondaryUploaded=UpdateLayeredWindow(secondaryOverlay,nullptr,&origin,&size,secondarySurface.dc,&zero,0,&blend,ULW_ALPHA)!=0;
  secondaryRecovery.submitted(secondaryUploaded);
  if(secondaryUploaded)overlaystack::place(secondaryOverlay,overlaystack::Layer::Statistics,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
 }
 return true;
}
static void render(int width,int height,POINT,int gameWidth,int gameHeight){
 renderWorker.submit({width,height,gameWidth,gameHeight,leftMargin,topMargin,scalePercent,page,showOthers,buildings,demo,pid,oldMatch,GetTickCount(),std::make_shared<um::Snapshot>(*snap)});renderDirty=false;
}
static void present(POINT origin,int alpha,int width,int height,int gameWidth,int gameHeight,bool repair=false){
 auto completed=renderWorker.get();if(completed&&completed!=renderedFrame){const auto&r=completed->request;
  if(r.pid==pid&&r.match==oldMatch&&r.width==width&&r.height==height&&r.gameWidth==gameWidth&&r.gameHeight==gameHeight&&r.page==page&&r.scalePercent==scalePercent&&r.leftMargin==leftMargin&&r.topMargin==topMargin&&r.showOthers==showOthers&&r.buildings==buildings&&r.demo==demo&&DWORD(GetTickCount()-r.tick)<1000&&surface.ensure(completed->width,completed->height)){memcpy(surface.bits,completed->pixels.data(),completed->pixels.size()*4);renderedFrame=std::move(completed);paintedAlpha=-1;}}

 if(!surface.dc||alpha<=0){statsVisible(false);hidePrimaryStats();paintedAlpha=0;return;}
 statsVisible(true);
 const auto context=!demo?overlaycompositor::host().capture(uintptr_t(game),pid):overlaycompositor::Context{};
 if(context.token){
  static uint64_t priorToken=0;static DWORD changedAt=0;
  if(priorToken!=context.token){priorToken=context.token;changedAt=GetTickCount();renderDirty=true;}
  // A new match/viewport may not reuse a bitmap sampled before it existed.
  if(renderedFrame&&renderedFrame->request.snap&&int32_t(renderedFrame->request.snap->tick-changedAt)>=0){
   POINT base{};ClientToScreen(game,&base);
   auto pixels=std::shared_ptr<const std::vector<uint32_t>>(renderedFrame,&renderedFrame->pixels);
   overlaycompositor::host().publish({overlaycompositor::Statistics,20,context,{origin.x-base.x,origin.y-base.y,surface.w,surface.h},false,BYTE(alpha),std::move(pixels)});
  }else overlaycompositor::host().hide(overlaycompositor::Statistics);
  if(IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);paintedAlpha=-1;return;
 }
 overlaycompositor::host().hide(overlaycompositor::Statistics);
 if(repair||alpha!=paintedAlpha||origin.x!=paintedOrigin.x||origin.y!=paintedOrigin.y||!IsWindowVisible(overlay)){
  SIZE size{surface.w,surface.h};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,BYTE(alpha),AC_SRC_ALPHA};
  bool uploaded=UpdateLayeredWindow(overlay,nullptr,&origin,&size,surface.dc,&zero,0,&blend,ULW_ALPHA)!=0;
  overlayRecovery.submitted(uploaded);if(uploaded){paintedAlpha=alpha;paintedOrigin=origin;}
 }
 if(repair||!IsWindowVisible(overlay))overlaystack::place(overlay,overlaystack::Layer::Statistics,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

static void demoData(){
 memset(snap.get(),0,sizeof(*snap));snap->status=1;snap->paletteReady=1;snap->typeCount=12;snap->playerCount=3;snap->tick=GetTickCount();
 for(int i=0;i<256;i++)snap->palette[i]=0xFF000000|(i<<16)|(i<<8)|i;snap->palette[0]=0;
 uint32_t colors[]={0xFF55C7FF,0xFFFF6D7E,0xFFFFD66E};const wchar_t* names[]={L"本机玩家",L"玩家 2",L"玩家 3"};
 for(int i=0;i<12;i++){auto& t=snap->types[i];sprintf(t.id,"%s%02d",i%3==0?"AIR":i%3==1?"TANK":"INF",i);t.category=i%4;t.iconReady=1;
  // Symbolic demo silhouettes only; live mode uses the game's actual SHP cameos.
  for(int y=0;y<um::IH;y++)for(int x=0;x<um::IW;x++){bool shape=i%3==0?(abs(x-30)<3&&y>8&&y<40)||(abs(y-24)<4&&abs(x-30)<22):i%3==1?((x>10&&x<49&&y>24&&y<37)||(x>21&&x<40&&y>15&&y<28)||(x>30&&x<54&&y>18&&y<22)):((x-30)*(x-30)+(y-12)*(y-12)<34||(abs(x-30)<6&&y>19&&y<33)||(y>31&&y<42&&(abs(x-25)<3||abs(x-35)<3)));t.icon[y*um::IW+x]=shape?uint8_t(140+i*8):0;}}
 for(int p=0;p<3;p++){auto& v=snap->players[p];v.color=colors[p];v.local=p==0;v.houseIndex=p;v.credits=10000+p*2500;v.creditsKnown=1;memcpy(v.name,names[p],(wcslen(names[p])+1)*2);for(int i=0;i<12-p*3;i++){auto& c=v.cells[i];c.owned=(i*3+p*7)%24+1;if(i%3==0){c.queued=i%5+1;c.active=1;c.progress=((GetTickCount()-demoStart)/150+i*4)%55;c.paused=i==6;}}}
 // Repeat a retirement example every 20 seconds; columns compact automatically.
 if((GetTickCount()-demoStart)/1000%20>=12){snap->players[1]=snap->players[2];snap->playerCount=2;}
}
static void tick(bool displayFrame=false){
 DWORD now=GetTickCount();
 const bool controlStep=!displayFrame||!statsFrameClock.running();
 if(controlStep){pollTab();refreshInformationRate(now);}
 if(now-lastData>=250){lastData=now;
  if((enabled||secondaryEnabled)&&demo){demoData();renderDirty=secondaryDirty=true;setStatus(L"布局演示：模拟数量与玩家退场；点击演示按钮退出");}
  else if(enabled||secondaryEnabled){
   if(pid&&!reader.alive()){
    detach();setStatus(L"游戏进程已退出，等待重新连接…");
   }else if(pid){
    DWORD actualPid=0,actualThread=game?GetWindowThreadProcessId(game,&actualPid):0;
    HWND fallback=actualPid==pid&&actualThread==threadId?game:nullptr;DWORD nextThread=0;
    HWND next=external::discovery::resolveYrppWindow(reader,pid,fallback,&nextThread);
    if(next!=game||nextThread!=threadId){game=next;threadId=nextThread;tabs.loseFocus();fade.hide();surface.clear();renderDirty=secondaryDirty=true;paintedAlpha=-1;if(!game)lastSearch=now-1500;}
   }
   if(pid&&!game&&now-lastSearch>=1500){
    lastSearch=now;auto found=external::discovery::scan(pid);
    if(found.best.window&&found.best.pid==pid){game=found.best.window;threadId=found.best.thread;}
   }
   if(!pid&&now-lastSearch>=1500){lastSearch=now;attach();}
   if(sampler){reader.begin();*incoming=sampler->sample(now,true);
     if(incoming->typeCount>=0&&incoming->typeCount<=um::MaxTypes&&incoming->playerCount>=0&&incoming->playerCount<=um::MaxPlayers){
      const bool dataChanged=changed(*snap,*incoming);renderDirty=renderDirty||dataChanged;secondaryDirty=secondaryDirty||dataChanged;std::swap(snap,incoming);if(snap->status==1&&now-snap->tick<2000)lastGood=now;
      if(snap->status!=1||oldMatch!=snap->match){
       if(snap->status==1){oldMatch=snap->match;
        // Keep the user's latched/manual visibility through loading and rounds.
        // loseFocus below releases a held Tab without changing that choice.
        log((L"单位统计采样代次 "+std::to_wstring(oldMatch)+L"，重建玩家列表与画面").c_str());}
       else lastGood=0;
       page=0;tabs.loseFocus();fade.hide();paintedAlpha=-1;renderDirty=secondaryDirty=true;surface.clear();
       statsVisible(false);hidePrimaryStats();resetSecondary();
      }
     }
    if(snap->status<0)setStatus(L"此 EXE 的数据布局不受支持，已停止采集");
    else if(snap->status==0){
     const auto p=sampler->probe();
     setStatus(std::wstring(L"只读采集：")+external::layout::description(p.stage)+
        L" · 地址="+std::to_wstring(p.address)+
        L" · Win32="+std::to_wstring(reader.lastReadError));
    }
    else if(now-lastGood>3000)setStatus(L"读取暂未更新：检查游戏权限或是否已退出对局");
    else {std::wstring msg=L"已连接 · 真人 "+std::to_wstring(snap->playerCount)+L" · 类型 "+std::to_wstring(snap->typeCount);
     if(!snap->paletteReady)msg+=L" · 等待图标色盘";if(snap->missingIcons)msg+=L" · 图标待载入 "+std::to_wstring(snap->missingIcons);if(iconCache.loaded)msg+=L" · 已载入缓存 "+std::to_wstring(iconCache.loaded.load());if(iconCache.writeFailed)msg+=L" · 图标缓存写入失败";if(snap->readErrors)msg+=L" · 读取异常 "+std::to_wstring(snap->readErrors);setStatus(msg);}
   }
  }
 }
 bool battlefield=false;external::display::State displayState;
 if((enabled||secondaryEnabled)&&!demo&&(secondaryEnabled||foregroundGame())&&reader.valid()){
  displayState=external::display::inspect([&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);});battlefield=displayState.visible();
 }
 const bool secondaryShowing=updateSecondary(now,battlefield,displayFrame);
 bool active=enabled&&(demo||(battlefield&&foregroundGame()&&snap->status==1&&now-lastGood<3000));
 if(!active){
  statsFrameClock.state(secondaryShowing,statsInformationFPS());
  overlayRecovery.suspend();
  std::wstring reason=!enabled?L"覆盖层已暂停":!foregroundGame()?L"覆盖层隐藏：游戏未在前台":snap->status!=1||now-lastGood>=3000?L"覆盖层等待有效游戏数据":!displayState.readable?L"覆盖层隐藏：菜单状态读取失败":L"覆盖层隐藏：菜单/对话框/非战场 · tactical="+std::to_wstring(displayState.tactical)+L" dialog="+std::to_wstring(displayState.specialDialog)+L" windows="+std::to_wstring(displayState.dialogCount);
  if(secondaryShowing)reason+=L" · 副屏仍在显示";
  visibilityStatus(reason);
  statsVisible(false);gameActive=false;tabs.loseFocus();fade.hide();paintedAlpha=-1;hidePrimaryStats();return;}
 if(!gameActive){renderDirty=secondaryDirty=true;paintedAlpha=-1;overlaystack::place(overlay,overlaystack::Layer::Statistics,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
 gameActive=true;
 bool desired=tabui::displayRequested(hidden,demo||!tabEnabled,manualVisible,tabs.visible());fade.target(desired,now);
 visibilityStatus(desired?(manualVisible?L"覆盖层已请求显示：手动开启":!tabEnabled?L"覆盖层已请求显示：常驻模式":L"覆盖层已请求显示：Tab/演示"):hidden?L"覆盖层手动隐藏：按钮或 Ctrl+Alt+H 开启":L"覆盖层等待 Tab：也可用按钮或 Ctrl+Alt+H");
 int alpha=fade.value(now);if(!desired)overlayRecovery.suspend();
 statsFrameClock.state(desired||alpha>0||secondaryShowing,statsInformationFPS());
 if(!desired&&alpha==0){statsVisible(false);hidePrimaryStats();paintedAlpha=0;return;}
 // Controls and sampling remain responsive at 16/250 ms. Only the coalesced
 // presentation message consumes completed pixels and advances visible fades.
 if(!displayFrame)return;
 RECT r{};POINT pt{};
 if(demo){MONITORINFO mi{sizeof mi};GetMonitorInfoW(MonitorFromWindow(ui,MONITOR_DEFAULTTONEAREST),&mi);r.right=mi.rcMonitor.right-mi.rcMonitor.left;r.bottom=mi.rcMonitor.bottom-mi.rcMonitor.top;pt={mi.rcMonitor.left,mi.rcMonitor.top};}
 else {GetClientRect(game,&r);ClientToScreen(game,&pt);}
 int gameWidth=r.right,gameHeight=r.bottom;
 if(!demo&&(!reader.readFresh(0x8A00A4,&gameWidth,4)||!reader.readFresh(0x8A00A8,&gameHeight,4))){statsVisible(false);hidePrimaryStats();return;}
 viewport::Scale view;if(!view.set(gameWidth,gameHeight,r.right,r.bottom)){statsVisible(false);hidePrimaryStats();return;}
 static int previousW=0,previousH=0,previousGW=0,previousGH=0;
 if(previousW!=r.right||previousH!=r.bottom||previousGW!=gameWidth||previousGH!=gameHeight){previousW=r.right;previousH=r.bottom;previousGW=gameWidth;previousGH=gameHeight;renderDirty=secondaryDirty=true;
 log((L"Overlay geometry game="+std::to_wstring(gameWidth)+L"x"+std::to_wstring(gameHeight)+L" client="+std::to_wstring(r.right)+L"x"+std::to_wstring(r.bottom)).c_str());}
 bool repair=overlayRecovery.due(now,desired&&alpha>0,{uintptr_t(game),pid,snap->match,r.right,r.bottom},IsWindowVisible(overlay)!=0);
 if(repair)renderDirty=secondaryDirty=true;
 if(renderDirty||!surface.dc)render(r.right,r.bottom,pt,gameWidth,gameHeight);
 present(pt,alpha,r.right,r.bottom,gameWidth,gameHeight,repair);
}
static void buttonLabel(){
#ifdef RA2_UNIFIED
 suite::reportEnabled(1,enabled);
#endif
 SetWindowTextW(bStart,enabled?L"暂停游戏内":L"继续游戏内");SetWindowTextW(bDemo,demo?L"退出演示":L"布局演示");}
static void beginSecondaryDrag(){
 if(!secondaryEnabled||!secondaryActive||!secondarySurface.dc||!game)return;
 auto target=secondaryMonitor.get(game,ui,GetTickCount());if(!target.available())return;
 POINT cursor{};RECT bounds{};
 if(!GetCursorPos(&cursor)||!GetWindowRect(secondaryOverlay,&bounds))return;
 secondaryDrag={};secondaryDrag.active=true;
 secondaryDrag.cursor=cursor;secondaryDrag.origin={bounds.left,bounds.top};
 secondaryDrag.work=target.work;secondaryDrag.x=secondaryX;secondaryDrag.y=secondaryY;
 secondaryDrag.width=bounds.right-bounds.left;secondaryDrag.height=bounds.bottom-bounds.top;
 SetCapture(secondaryOverlay);
 if(GetCapture()!=secondaryOverlay)secondaryDrag.active=false;
}
static void moveSecondaryDrag(){
 if(!secondaryDrag.active||GetCapture()!=secondaryOverlay)return;
 POINT cursor{};if(!GetCursorPos(&cursor))return;
 auto desired=secondaryDragSettings(secondaryDrag,cursor);
 if(desired.x==secondaryX&&desired.y==secondaryY)return;
 secondaryX=desired.x;secondaryY=desired.y;
 secondaryDrag.changed=true;tick(true);
}
static void finishSecondaryDrag(){
 if(!secondaryDrag.active)return;
 bool changed=secondaryDrag.changed;secondaryDrag.active=false;
 if(GetCapture()==secondaryOverlay)ReleaseCapture();
 if(changed){
  syncingSecondaryDragControls=true;
  SetDlgItemInt(ui,121,secondaryX,FALSE);SetDlgItemInt(ui,122,secondaryY,FALSE);
  syncingSecondaryDragControls=false;
  save();
 }
}
static LRESULT CALLBACK overlayProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_NCHITTEST)return w==secondaryOverlay?HTCLIENT:HTTRANSPARENT;
 if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
 if(w==secondaryOverlay){
  if(m==WM_SETCURSOR){
   SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));return TRUE;
  }
  if(m==WM_LBUTTONDOWN){beginSecondaryDrag();return 0;}
  if(m==WM_MOUSEMOVE){if(a&MK_LBUTTON)moveSecondaryDrag();else finishSecondaryDrag();return 0;}
  if(m==WM_LBUTTONUP){moveSecondaryDrag();finishSecondaryDrag();return 0;}
  if(m==WM_CAPTURECHANGED||m==WM_CANCELMODE){finishSecondaryDrag();return 0;}
 }
 if(m==WM_ERASEBKGND)return 1;
 return DefWindowProcW(w,m,a,b);
}
#include "display_settings_ui.h"
#include "console_layout.h"
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
#ifdef RA2_UNIFIED
 if(suite::layoutMessage(w,m,b))return 0;
 if(m==suite::DisplaySettingsChanged){refreshInformationRate(GetTickCount());syncUnitSettings();tick();return 0;}
 if(m==suite::ModuleEnable){enabled=bool(a);if(!enabled&&!secondaryEnabled)detach();else if(enabled)lastSearch=0;buttonLabel();tick();return 0;}
 if(m==WM_CLOSE&&a!=suite::CloseToken){suite::requestClose();return 0;}
#endif
 if(m==modernui::DpiMessage){consoleScale(unsigned(a));return 0;}
 switch(m){case WM_CREATE:{
  ui=w;consoleDpi=modernui::windowDpi(w);
  font=modernui::makeFont();
  auto ctl=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int x,int y,int cw,int ch,int id){HWND c=CreateWindowW(cls,title,WS_CHILD|WS_VISIBLE|style,x,y,cw,ch,w,(HMENU)(INT_PTR)id,nullptr,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);consoleControls.push_back({c,{x,y,cw,ch}});return c;};
  bStart=ctl(L"BUTTON",L"暂停游戏内",BS_OWNERDRAW,14,54,94,26,100);bDemo=ctl(L"BUTTON",L"布局演示",BS_OWNERDRAW,116,54,94,26,101);ctl(L"BUTTON",L"重新连接",BS_OWNERDRAW,218,54,94,26,102);
  modernui::tip(bStart,L"只暂停游戏内单位统计，副屏统计窗口由下方开关独立控制。");
  cBuildings=ctl(L"BUTTON",L"显示建筑",BS_AUTOCHECKBOX,14,89,144,21,103);cOthers=ctl(L"BUTTON",L"其他玩家",BS_AUTOCHECKBOX,168,89,144,21,104);
  SendMessageW(cBuildings,BM_SETCHECK,buildings?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(cOthers,BM_SETCHECK,showOthers?BST_CHECKED:BST_UNCHECKED,0);
  ctl(L"STATIC",L"上边距",0,14,119,48,20,107);eTop=ctl(L"EDIT",std::to_wstring(topMargin).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,64,116,52,23,108);
  ctl(L"STATIC",L"左边距",0,168,119,48,20,109);eLeft=ctl(L"EDIT",std::to_wstring(leftMargin).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,218,116,52,23,110);
  ctl(L"STATIC",L"px",0,120,119,22,20,111);ctl(L"STATIC",L"px",0,274,119,22,20,112);
  SendMessageW(eTop,EM_SETLIMITTEXT,4,0);SendMessageW(eLeft,EM_SETLIMITTEXT,4,0);
  cTab=ctl(L"BUTTON",L"常驻显示（不依赖 Tab）",BS_AUTOCHECKBOX,14,149,298,22,113);SendMessageW(cTab,BM_SETCHECK,!tabEnabled?BST_CHECKED:BST_UNCHECKED,0);
  tabModeBox=ctl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,14,179,298,110,114);SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"按键切换");SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"按住显示");SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"短按切换 / 长按暂显");SendMessageW(tabModeBox,CB_SETCURSEL,int(tabMode),0);
  stats=ctl(L"STATIC",L"等待游戏…",0,14,216,298,34,105);modernui::tip(tabModeBox,L"使用游戏内 Tab 控制统计覆盖层；不会拦截原版菜单。不兼容时可选择常驻显示。");
  modernui::tip(cTab,L"常驻显示不依赖 Tab；Ctrl+Alt+H 显隐，Ctrl+Alt+←/→ 翻页。");
  ctl(L"BUTTON",L"显示 / 隐藏覆盖层",BS_OWNERDRAW,14,264,298,26,116);
  visibilityLabel=ctl(L"STATIC",L"覆盖层等待游戏",0,14,300,298,42,117);
  ctl(L"STATIC",L"显示缩放 %",0,14,357,138,23,119);
  ctl(L"EDIT",std::to_wstring(scalePercent).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,168,354,102,25,118);
  SendDlgItemMessageW(w,118,EM_SETLIMITTEXT,3,0);
  modernui::tip(GetDlgItem(w,118),L"调整游戏内统计覆盖层的大小，范围 50–250%，默认 100%，即时生效。");
  modernui::tip(GetDlgItem(w,116),L"显示或隐藏游戏内统计覆盖层；快捷键 Ctrl+Alt+H。");
  cSecondary=ctl(L"BUTTON",L"第二显示器显示单位统计",BS_AUTOCHECKBOX|WS_TABSTOP,14,395,298,26,120);SendMessageW(cSecondary,BM_SETCHECK,secondaryEnabled?BST_CHECKED:BST_UNCHECKED,0);
  modernui::tip(cSecondary,L"在另一块屏幕独立显示，拖动统计画面移动位置；没有第二显示器时自动隐藏。");
  ctl(L"STATIC",L"副屏水平位置",0,14,433,116,22,124);
  ctl(L"EDIT",std::to_wstring(secondaryX).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,132,430,55,24,121);
  ctl(L"STATIC",L"垂直",0,196,433,55,22,125);
  ctl(L"EDIT",std::to_wstring(secondaryY).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,252,430,55,24,122);
  for(int id:{121,122})SendDlgItemMessageW(w,id,EM_SETLIMITTEXT,5,0);
  modernui::tip(GetDlgItem(w,121),L"位置按副屏可移动区域的万分比计算：0 为最左，10000 为最右。也可以直接拖动副屏统计窗口。");
  modernui::tip(GetDlgItem(w,122),L"位置按副屏可移动区域计算：0 为顶部，10000 为底部；拖动副屏统计窗口也会自动保存。");
#ifdef RA2_UNIFIED
  createUnitSettings(ctl);
#endif
  EnableWindow(tabModeBox,tabEnabled);controlsReady=true;modernui::styleChildren(w);consoleScale(consoleDpi);
  if(!RegisterHotKey(w,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'H'))visibilityStatus(L"备用快捷键注册失败；请用显示按钮或常驻模式");RegisterHotKey(w,2,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_RIGHT);RegisterHotKey(w,3,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_LEFT);statsFrameClock.start(w,StatsFrameMessage);SetTimer(w,1,16,nullptr);return 0;}
 case WM_DPICHANGED:consoleScale(HIWORD(a));if(auto rect=(RECT*)b)if(!GetPropW(w,L"RA2.Suite.Managed"))SetWindowPos(w,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);return 0;
 case WM_SIZE:layoutConsole();return 0;
 case WM_MOUSEWHEEL:{int amount=pagewheel::delta(w,a,modernui::scale(60,consoleDpi));if(amount){consoleY-=amount;layoutConsole();}return 0;}
 case WM_HSCROLL:case WM_VSCROLL:{int bar=m==WM_HSCROLL?SB_HORZ:SB_VERT;SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(w,bar,&si);int next=si.nPos;switch(LOWORD(a)){case SB_LINEUP:next-=modernui::scale(28,consoleDpi);break;case SB_LINEDOWN:next+=modernui::scale(28,consoleDpi);break;case SB_PAGEUP:next-=si.nPage;break;case SB_PAGEDOWN:next+=si.nPage;break;case SB_THUMBTRACK:case SB_THUMBPOSITION:next=si.nTrackPos;break;case SB_TOP:next=0;break;case SB_BOTTOM:next=si.nMax;break;}(bar==SB_HORZ?consoleX:consoleY)=next;layoutConsole();return 0;}
 case WM_ERASEBKGND:return 1;
 case WM_DISPLAYCHANGE:finishSecondaryDrag();statsRefreshMonitor.invalidate();secondaryMonitor.invalidate();resetSecondary();tick();return 0;
 case StatsFrameMessage:tick(true);statsFrameClock.completed();return 0;
 case WM_TIMER:tick(!statsFrameClock.running());return 0;
 case WM_APP+9:tick();return 0;
 case WM_COMMAND:
#ifdef RA2_UNIFIED
 if(controlsReady&&unitSettingCommand(a))return 0;
#endif
 if(controlsReady&&LOWORD(a)==114&&HIWORD(a)==CBN_SELCHANGE){tabMode=tabui::Mode(SendMessageW(tabModeBox,CB_GETCURSEL,0,0));tabs.mode(tabMode);save();tick();return 0;}
 if(controlsReady&&HIWORD(a)==EN_CHANGE&&LOWORD(a)==118){
  BOOL ok=FALSE;int value=int(GetDlgItemInt(w,118,&ok,FALSE));
  if(ok&&value>=viewport::MinPercent&&value<=viewport::MaxPercent){scalePercent=value;save();renderDirty=secondaryDirty=true;tick();}return 0;
 }
 if(controlsReady&&HIWORD(a)==EN_CHANGE&&(LOWORD(a)==108||LOWORD(a)==110)){
  BOOL ok=FALSE;UINT value=GetDlgItemInt(w,LOWORD(a),&ok,FALSE);
  if(ok&&value<=4096){if(LOWORD(a)==108)topMargin=int(value);else leftMargin=int(value);save();renderDirty=secondaryDirty=true;tick();}return 0;
 }
 if(controlsReady&&HIWORD(a)==EN_CHANGE&&LOWORD(a)>=121&&LOWORD(a)<=122){
  if(syncingSecondaryDragControls)return 0;
  const int id=LOWORD(a);BOOL ok=FALSE;UINT value=GetDlgItemInt(w,id,&ok,FALSE);
  int& setting=id==121?secondaryX:secondaryY;
  if(ok&&value<=10000&&setting!=int(value)){
   setting=int(value);save();tick();
  }return 0;
 }
 if(controlsReady&&HIWORD(a)==EN_KILLFOCUS&&LOWORD(a)>=121&&LOWORD(a)<=122){
  int id=LOWORD(a);UINT value=id==121?secondaryX:secondaryY;
  syncingSecondaryDragControls=true;SetDlgItemInt(w,id,value,FALSE);syncingSecondaryDragControls=false;
  return 0;
 }if(HIWORD(a)==BN_CLICKED){switch(LOWORD(a)){case 100:enabled=!enabled;if(!enabled&&!secondaryEnabled)detach();else if(enabled)lastSearch=0;break;case 101:demo=!demo;demoStart=GetTickCount();enabled=true;hidden=false;manualVisible=false;if(!demo)memset(snap.get(),0,sizeof(*snap));break;case 102:detach();status.clear();lastSearch=0;break;case 103:buildings=SendMessageW(cBuildings,BM_GETCHECK,0,0)==BST_CHECKED;save();break;case 104:showOthers=SendMessageW(cOthers,BM_GETCHECK,0,0)==BST_CHECKED;save();break;case 113:tabEnabled=SendMessageW(cTab,BM_GETCHECK,0,0)!=BST_CHECKED;tabs.reset();hidden=false;manualVisible=false;EnableWindow(tabModeBox,tabEnabled);save();break;case 116:toggleOverlay();break;case 120:secondaryEnabled=SendMessageW(cSecondary,BM_GETCHECK,0,0)==BST_CHECKED;secondaryMonitor.invalidate();resetSecondary();if(!enabled&&!secondaryEnabled)detach();else lastSearch=0;save();break;}renderDirty=secondaryDirty=true;buttonLabel();tick();}return 0;
 case WM_HOTKEY:if(a==1){toggleOverlay();}else if(a==2)page++;else if(a==3)page=std::max(0,page-1);renderDirty=secondaryDirty=true;tick();return 0;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetBkMode((HDC)a,OPAQUE);SetTextColor((HDC)a,modernui::Text);SetBkColor((HDC)a,modernui::Background);SetDCBrushColor((HDC)a,modernui::Background);return (LRESULT)GetStockObject(DC_BRUSH);
 case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)b;HBRUSH br=CreateSolidBrush((d->itemState&ODS_SELECTED)?RGB(47,76,99):RGB(31,48,68));FillRect(d->hDC,&d->rcItem,br);DeleteObject(br);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(223,237,251));SelectObject(d->hDC,font);wchar_t t[50];GetWindowTextW(d->hwndItem,t,50);DrawTextW(d->hDC,t,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);HBRUSH br=CreateSolidBrush(modernui::Background);FillRect(dc,&r,br);DeleteObject(br);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(110,210,248));SelectObject(dc,font);TextOutW(dc,modernui::scale(14,consoleDpi)-consoleX,modernui::scale(12,consoleDpi)-consoleY,L"单位统计",lstrlenW(L"单位统计"));SetTextColor(dc,RGB(148,166,188));TextOutW(dc,modernui::scale(14,consoleDpi)-consoleX,modernui::scale(32,consoleDpi)-consoleY,L"玩家兵力与单位状态",lstrlenW(L"玩家兵力与单位状态"));EndPaint(w,&ps);return 0;}
 case WM_DESTROY:finishSecondaryDrag();statsFrameClock.shutdown();renderWorker.shutdown();secondaryRenderWorker.shutdown();surface.clear();secondarySurface.clear();KillTimer(w,1);UnregisterHotKey(w,1);UnregisterHotKey(w,2);UnregisterHotKey(w,3);detach();DeleteObject(font);PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int){
 HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\RA2_UnitMonitor_UI_v01");if(GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(nullptr,L"单位监控已经运行。",L"单位监控",MB_OK);return 0;}
 SetProcessDPIAware();wchar_t fn[2048];GetModuleFileNameW(nullptr,fn,2048);root=fn;root=root.substr(0,root.find_last_of(L"\\/"));logfile=_wfopen(path(L"UnitMonitor.log").c_str(),L"w, ccs=UTF-8");
 topMargin=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"topMargin",20,path(L"settings.ini").c_str())),0,4096);leftMargin=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"leftMargin",6,path(L"settings.ini").c_str())),0,4096);
 scalePercent=viewport::percent(int(GetPrivateProfileIntW(L"overlay",L"scalePercent",100,path(L"settings.ini").c_str())));
 secondaryEnabled=GetPrivateProfileIntW(L"overlay",L"secondaryEnabled",0,path(L"settings.ini").c_str())!=0;
 secondaryX=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"secondaryX",0,path(L"settings.ini").c_str())),0,10000);
 secondaryY=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"secondaryY",0,path(L"settings.ini").c_str())),0,10000);
 buildings=GetPrivateProfileIntW(L"overlay",L"buildings",0,path(L"settings.ini").c_str())!=0;showOthers=GetPrivateProfileIntW(L"overlay",L"otherPlayers",0,path(L"settings.ini").c_str())!=0;
#ifdef RA2_UNIFIED
 enabled=suite::enabled(1);
#endif
 tabEnabled=GetPrivateProfileIntW(L"overlay",L"tabEnabled",1,path(L"settings.ini").c_str())!=0;tabMode=tabui::Mode(std::clamp(int(GetPrivateProfileIntW(L"overlay",L"tabMode",2,path(L"settings.ini").c_str())),0,2));tabs.mode(tabMode);tabs.holdThreshold(GetPrivateProfileIntW(L"overlay",L"tabHoldMs",300,path(L"settings.ini").c_str()));
 ULONG_PTR token;GdiplusStartupInput start;if(GdiplusStartup(&token,&start,nullptr)!=Ok)return 2;
 WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=overlayProc;wc.lpszClassName=L"RA2UnitTransparentOverlay";RegisterClassW(&wc);
 overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,inst,nullptr);
 secondaryOverlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,L"第二显示器单位统计",WS_POPUP,0,0,1,1,nullptr,nullptr,inst,nullptr);
 wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2UnitMonitorUI";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
 RECT r{0,0,700,440};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL|WS_HSCROLL,FALSE);
 ui=CreateWindowW(wc.lpszClassName,L"单位统计 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL|WS_HSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,inst,nullptr);
#ifdef RA2_UNIFIED
 suite::registerPane(1,ui);
#else
 ShowWindow(ui,SW_SHOW);
#endif
 buttonLabel();

 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}DestroyWindow(overlay);DestroyWindow(secondaryOverlay);iconImages.clear();GdiplusShutdown(token);if(logfile)fclose(logfile);CloseHandle(mutex);return 0;
}


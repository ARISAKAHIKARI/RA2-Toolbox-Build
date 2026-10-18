#include "../../shared/source/modern_ui.h"
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
using namespace Gdiplus;
static HWND ui,overlay,stats,bStart,bDemo,cBuildings,cOthers,eTop,eLeft,cTab,tabModeBox,visibilityLabel;static HFONT font;
static external::ReadSession reader;static um::IconCache iconCache;
static std::unique_ptr<um::Sampler<external::SharedReadView,external::NoResolve>> sampler;
static std::unique_ptr<um::Snapshot> snap(new um::Snapshot{}),incoming(new um::Snapshot{});
static HWND game;static DWORD pid,threadId;static bool enabled=true,demo=false,showOthers=true,buildings=false,hidden=false,manualVisible=false;
static int topMargin=20,leftMargin=6;static bool controlsReady=false;
static int page=0;static DWORD lastSearch=0,lastGood=0;static std::wstring root,status;static FILE* logfile;
static uint32_t demoStart,oldMatch=0;static DWORD lastData=0;
static void statsVisible(bool value){
#ifdef RA2_UNIFIED
 suite::setStatsVisible(value);
#endif
}
static bool tabEnabled=true,renderDirty=true,gameActive=false;
static tabui::Mode tabMode=tabui::Mode::TapOrHold;static tabui::Controller tabs;static tabui::Fade fade;
struct UnitSurface {
 HDC dc=nullptr;HBITMAP bitmap=nullptr,old=nullptr;void* bits=nullptr;int w=0,h=0;
 void clear(){if(dc){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=old=nullptr;bits=nullptr;w=h=0;}
 bool ensure(int width,int height){if(dc&&w==width&&h==height)return true;clear();
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
  HDC screen=GetDC(nullptr);dc=CreateCompatibleDC(screen);bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);ReleaseDC(nullptr,screen);
  if(!dc||!bitmap||!bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);dc=nullptr;bitmap=nullptr;bits=nullptr;return false;}
  old=(HBITMAP)SelectObject(dc,bitmap);w=width;h=height;return true;
 }
} surface;
static int paintedAlpha=-1;static POINT paintedOrigin{LONG_MIN,LONG_MIN};
static bool foregroundGame(){DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return game&&IsWindow(game)&&!IsIconic(game)&&foreground==pid;}
static bool tabEligible(){return tabEnabled&&enabled&&gameActive&&!demo&&snap->status==1&&!snap->chatActive&&DWORD(GetTickCount()-snap->tick)<2000&&foregroundGame();}
static void pollTab(){
 static bool previous=false;bool down=(GetAsyncKeyState(VK_TAB)&0x8000)!=0;
 bool modified=((GetAsyncKeyState(VK_MENU)|GetAsyncKeyState(VK_CONTROL)|GetAsyncKeyState(VK_SHIFT)|GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)!=0;
 if(!tabEligible()||modified){tabs.loseFocus();previous=down;return;}
 if(down!=previous){previous=down;if(tabs.event(down,tabEligible(),modified,false,GetTickCount())){if(down){hidden=false;manualVisible=false;}renderDirty=true;}}
}
static bool changed(const um::Snapshot& a,const um::Snapshot& b){
 return a.match!=b.match||a.status!=b.status||a.typeCount!=b.typeCount||a.playerCount!=b.playerCount||a.paletteReady!=b.paletteReady||
  memcmp(a.palette,b.palette,sizeof a.palette)||memcmp(a.types,b.types,sizeof(um::Type)*a.typeCount)||memcmp(a.players,b.players,sizeof(um::Player)*a.playerCount);
}
static void log(const wchar_t* msg){if(logfile){fwprintf(logfile,L"[%lu] %ls\n",GetTickCount(),msg);fflush(logfile);}}
static std::wstring path(const wchar_t* f){return root+L"\\"+f;}
static void setStatus(const std::wstring& s){if(status!=s){status=s;SetWindowTextW(stats,s.c_str());log(s.c_str());}}
static void save(){WritePrivateProfileStringW(L"overlay",L"tabEnabled",tabEnabled?L"1":L"0",path(L"settings.ini").c_str());WritePrivateProfileStringW(L"overlay",L"tabMode",std::to_wstring(int(tabMode)).c_str(),path(L"settings.ini").c_str());WritePrivateProfileStringW(L"overlay",L"topMargin",std::to_wstring(topMargin).c_str(),path(L"settings.ini").c_str());WritePrivateProfileStringW(L"overlay",L"leftMargin",std::to_wstring(leftMargin).c_str(),path(L"settings.ini").c_str());WritePrivateProfileStringW(L"overlay",L"buildings",buildings?L"1":L"0",path(L"settings.ini").c_str());WritePrivateProfileStringW(L"overlay",L"otherPlayers",showOthers?L"1":L"0",path(L"settings.ini").c_str());}
static void visibilityStatus(const std::wstring& text){
 static std::wstring previous;if(previous==text)return;previous=text;
 if(visibilityLabel)SetWindowTextW(visibilityLabel,text.c_str());log(text.c_str());
}
static void toggleOverlay(){tabui::toggleDisplay(!tabEnabled,tabs.visible(),hidden,manualVisible);renderDirty=true;}
static void detach(){
 statsVisible(false);
 sampler.reset();reader.close();
 game=nullptr;pid=0;threadId=0;lastGood=0;oldMatch=0;page=0;gameActive=false;tabs.reset();fade.hide();renderDirty=true;paintedAlpha=-1;surface.clear();memset(snap.get(),0,sizeof(*snap));memset(incoming.get(),0,sizeof(*incoming));ShowWindow(overlay,SW_HIDE);
}
static bool attach(){
 auto scan=external::discovery::scan();
 if(!scan.best.pid){
  if(scan.snapshotError)setStatus(L"游戏进程快照失败，Win32="+std::to_wstring(scan.snapshotError));
  else if(!scan.processCount)setStatus(L"尚未发现 gamemd 进程");
  else setStatus(L"多个无窗口游戏进程，无法确定目标 PID");
  return false;
 }
 const auto& c=scan.best;
 if(!reader.open(c.pid)){
  setStatus(L"发现 PID "+std::to_wstring(c.pid)+L"，但读取权限不足，Win32="+std::to_wstring(reader.lastOpenError));
  return false;
 }
 pid=c.pid;threadId=c.thread;game=external::discovery::resolveYrppWindow(reader,pid,c.window,&threadId);
 wchar_t image[32768]{};DWORD imageSize=32768;reader.imagePath(image,&imageSize);iconCache.open(std::filesystem::path(image),std::filesystem::path(root)/L"cache"/L"unit-icons");
 sampler=std::make_unique<um::Sampler<external::SharedReadView,external::NoResolve>>(external::SharedReadView{&reader},external::NoResolve{},&iconCache);
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
static void render(int width,int height,POINT origin,int gameWidth,int gameHeight){
 if(width<100||height<100||width>16384||height>16384)return;
 viewport::Scale view;if(!view.set(gameWidth,gameHeight,width,height))return;
 int viewportW=gameWidth,viewportH=gameHeight;
 float top=float(std::clamp(topMargin,0,std::max(0,viewportH-100))),left=float(std::clamp(leftMargin,0,std::max(0,viewportW-100)));
 const float maxWidth=std::min(112.f,std::max(76.f,(viewportW-left-6.f)/8.f-4.f));
 float croppedW=left+8,croppedH=top+50;
 for(int pi=0;pi<snap->playerCount;++pi){auto& p=snap->players[pi];if(!showOthers&&!p.local)continue;
  int entries=0;for(int i=0;i<snap->typeCount;++i)entries+=um::showCell(snap->types[i].category,p.cells[i],buildings);
  int across=entries>1&&maxWidth>=112?2:1;float panelW=across==2?maxWidth:std::min(84.f,maxWidth),rowH=std::min(44.f,(panelW-12)/across-4)*.8f+18;
  int rows=std::max(1,int((viewportH-top-59)/rowH)),per=rows*across,pages=std::max(1,(entries+per-1)/per),start=(page%pages)*per,shownRows=std::max(1,(std::min(entries,start+per)-start+across-1)/across);
  croppedW+=panelW+4;croppedH=std::max(croppedH,top+46+shownRows*rowH);
 }
 width=std::min(viewportW,std::max(1,int(croppedW+2)));height=std::min(viewportH,std::max(1,int(croppedH+2)));
 if(demo){width=viewportW;height=viewportH;}
 width=view.width(width);height=view.height(height);
 if(!surface.ensure(width,height))return;void* bits=surface.bits;memset(bits,0,size_t(width)*height*4);
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
  int rows=std::max(1,int((viewportH-top-59)/rowH)),per=rows*across;
  int pages=std::max(1,(int(entries.size())+per-1)/per),current=page%pages;
  int start=current*per,finish=std::min(int(entries.size()),start+per);
  int shownRows=std::max(1,(finish-start+across-1)/across);
  float panelH=42+shownRows*rowH+4;
  SolidBrush panel(Color(158,12,19,29));g.FillRectangle(&panel,cursorX-panelW-4,6.f,panelW,panelH);
  uint32_t pc=p.color;Pen rule(Color(180,(pc>>16)&255,(pc>>8)&255,pc&255),2.f);g.DrawLine(&rule,x,9.f,x+w,9.f);
  std::wstring name=(const wchar_t*)p.name;if(name.empty())name=L"玩家 "+std::to_wstring(p.houseIndex+1);if(p.local)name=L"我·"+name;
  int maxChars=std::max(3,int(w/11));if(int(name.size())>maxChars)name=name.substr(0,maxChars-1)+L"…";
  text(g,name,x,12,11,color(pc),true);
  text(g,L"拥有 / +生产",x,27,9,Color(255,214,225,237));
  if(entries.empty())text(g,L"暂无单位",x,46,10,Color(255,190,204,221));
  if(pages>1)text(g,std::to_wstring(current+1)+L"/"+std::to_wstring(pages),x+w-26,27,9,Color::White);
  for(int j=start;j<finish;j++){
   int ix=entries[j],slot=j-start;const auto& c=p.cells[ix];int category=snap->types[ix].category;
   float tx=x+(slot%across)*tileW,ty=42+(slot/across)*rowH;
   drawIcon(g,snap->types[ix],*snap,tx,ty,iconW,iconH,pc);
   if(category!=3||buildings)text(g,std::to_wstring(um::shownOwned(category,c,buildings)),tx+1,ty+iconH,12,Color::White,true);
   if(c.queued){float qx=category==3&&!buildings?tx+1:tx+iconW/2; text(g,L"+"+std::to_wstring(c.queued),qx,ty+iconH+1,10,color(pc),true);}
   if(c.active){Pen base(Color(255,16,22,30),4),progress(color(pc),3);g.DrawLine(&base,tx,ty+iconH-2,tx+iconW,ty+iconH-2);g.DrawLine(&progress,tx,ty+iconH-2,tx+iconW*c.progress/54.f,ty+iconH-2);if(c.paused)text(g,L"Ⅱ",tx+iconW-17,ty+3,12,Color(255,245,208,106),true);}
  }
 }
 g.ResetTransform();
 if(demo)text(g,L"布局演示 · 模拟数据 · Ctrl+Alt+H 隐藏",8,height-32,14,Color(255,238,215,135));
 }
 renderDirty=false;paintedAlpha=-1;
}
static void present(POINT origin,int alpha){
 if(!surface.dc||alpha<=0){statsVisible(false);ShowWindow(overlay,SW_HIDE);paintedAlpha=0;return;}
 statsVisible(true);
 if(alpha!=paintedAlpha||origin.x!=paintedOrigin.x||origin.y!=paintedOrigin.y||!IsWindowVisible(overlay)){
  SIZE size{surface.w,surface.h};POINT zero{};BLENDFUNCTION blend{AC_SRC_OVER,0,BYTE(alpha),AC_SRC_ALPHA};
  if(UpdateLayeredWindow(overlay,nullptr,&origin,&size,surface.dc,&zero,0,&blend,ULW_ALPHA)){paintedAlpha=alpha;paintedOrigin=origin;}
 }
 if(!IsWindowVisible(overlay))SetWindowPos(overlay,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

static void demoData(){
 memset(snap.get(),0,sizeof(*snap));snap->status=1;snap->paletteReady=1;snap->typeCount=12;snap->playerCount=3;snap->tick=GetTickCount();
 for(int i=0;i<256;i++)snap->palette[i]=0xFF000000|(i<<16)|(i<<8)|i;snap->palette[0]=0;
 uint32_t colors[]={0xFF55C7FF,0xFFFF6D7E,0xFFFFD66E};const wchar_t* names[]={L"本机玩家",L"玩家 2",L"玩家 3"};
 for(int i=0;i<12;i++){auto& t=snap->types[i];sprintf(t.id,"%s%02d",i%3==0?"AIR":i%3==1?"TANK":"INF",i);t.category=i%4;t.iconReady=1;
  // Symbolic demo silhouettes only; live mode uses the game's actual SHP cameos.
  for(int y=0;y<um::IH;y++)for(int x=0;x<um::IW;x++){bool shape=i%3==0?(abs(x-30)<3&&y>8&&y<40)||(abs(y-24)<4&&abs(x-30)<22):i%3==1?((x>10&&x<49&&y>24&&y<37)||(x>21&&x<40&&y>15&&y<28)||(x>30&&x<54&&y>18&&y<22)):((x-30)*(x-30)+(y-12)*(y-12)<34||(abs(x-30)<6&&y>19&&y<33)||(y>31&&y<42&&(abs(x-25)<3||abs(x-35)<3)));t.icon[y*um::IW+x]=shape?uint8_t(140+i*8):0;}}
 for(int p=0;p<3;p++){auto& v=snap->players[p];v.color=colors[p];v.local=p==0;v.houseIndex=p;memcpy(v.name,names[p],(wcslen(names[p])+1)*2);for(int i=0;i<12-p*3;i++){auto& c=v.cells[i];c.owned=(i*3+p*7)%24+1;if(i%3==0){c.queued=i%5+1;c.active=1;c.progress=((GetTickCount()-demoStart)/150+i*4)%55;c.paused=i==6;}}}
 // Repeat a retirement example every 20 seconds; columns compact automatically.
 if((GetTickCount()-demoStart)/1000%20>=12){snap->players[1]=snap->players[2];snap->playerCount=2;}
}
static void tick(){
 pollTab();
 DWORD now=GetTickCount();
 if(now-lastData>=250){lastData=now;
  if(enabled&&demo){demoData();renderDirty=true;setStatus(L"布局演示：模拟数量与玩家退场；点击演示按钮退出");}
  else if(enabled){
   if(pid&&!reader.alive()){
    detach();setStatus(L"游戏进程已退出，等待重新连接…");
   }else if(pid&&game&&!IsWindow(game)){
    game=nullptr;threadId=0;lastSearch=now-1500;
   }
   if(pid&&!game&&now-lastSearch>=1500){
    lastSearch=now;auto found=external::discovery::scan(pid);
    if(found.best.window&&found.best.pid==pid){game=found.best.window;threadId=found.best.thread;}
   }
   if(!pid&&now-lastSearch>=1500){lastSearch=now;attach();}
   if(sampler){reader.begin();*incoming=sampler->sample(now,true);
     if(incoming->typeCount>=0&&incoming->typeCount<=um::MaxTypes&&incoming->playerCount>=0&&incoming->playerCount<=um::MaxPlayers){
      renderDirty=renderDirty||changed(*snap,*incoming);std::swap(snap,incoming);if(snap->status==1&&now-snap->tick<2000)lastGood=now;
      if(snap->status!=1||oldMatch!=snap->match){
       if(snap->status==1){oldMatch=snap->match;hidden=false;manualVisible=false;
        log((L"单位统计采样代次 "+std::to_wstring(oldMatch)+L"，重建玩家列表与画面").c_str());}
       else lastGood=0;
       page=0;tabs.reset();fade.hide();paintedAlpha=-1;renderDirty=true;surface.clear();
       statsVisible(false);ShowWindow(overlay,SW_HIDE);
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
 if(enabled&&!demo&&foregroundGame()&&reader.valid()){
  displayState=external::display::inspect([&](uint32_t p,void*out,size_t n){return reader.readFresh(p,out,n);});battlefield=displayState.visible();
 }
 bool active=enabled&&(demo||(battlefield&&foregroundGame()&&snap->status==1&&now-lastGood<3000));
 if(!active){
  visibilityStatus(!enabled?L"覆盖层已暂停":!foregroundGame()?L"覆盖层隐藏：游戏未在前台":snap->status!=1||now-lastGood>=3000?L"覆盖层等待有效游戏数据":!displayState.readable?L"覆盖层隐藏：菜单状态读取失败":L"覆盖层隐藏：菜单/对话框/非战场 · tactical="+std::to_wstring(displayState.tactical)+L" dialog="+std::to_wstring(displayState.specialDialog)+L" windows="+std::to_wstring(displayState.dialogCount));
  statsVisible(false);gameActive=false;tabs.loseFocus();fade.hide();paintedAlpha=-1;ShowWindow(overlay,SW_HIDE);return;}
 gameActive=true;
 bool desired=tabui::displayRequested(hidden,demo||!tabEnabled,manualVisible,tabs.visible());fade.target(desired,now);
 visibilityStatus(desired?(manualVisible?L"覆盖层已请求显示：手动开启":!tabEnabled?L"覆盖层已请求显示：常驻模式":L"覆盖层已请求显示：Tab/演示"):hidden?L"覆盖层手动隐藏：按钮或 Ctrl+Alt+H 开启":L"覆盖层等待 Tab：也可用按钮或 Ctrl+Alt+H");
 int alpha=fade.value(now);if(!desired&&alpha==0){statsVisible(false);ShowWindow(overlay,SW_HIDE);paintedAlpha=0;return;}
 RECT r{};POINT pt{};
 if(demo){MONITORINFO mi{sizeof mi};GetMonitorInfoW(MonitorFromWindow(ui,MONITOR_DEFAULTTONEAREST),&mi);r.right=mi.rcMonitor.right-mi.rcMonitor.left;r.bottom=mi.rcMonitor.bottom-mi.rcMonitor.top;pt={mi.rcMonitor.left,mi.rcMonitor.top};}
 else {GetClientRect(game,&r);ClientToScreen(game,&pt);}
 int gameWidth=r.right,gameHeight=r.bottom;
 if(!demo&&(!reader.readFresh(0xA8EB84,&gameWidth,4)||!reader.readFresh(0xA8EB88,&gameHeight,4))){statsVisible(false);ShowWindow(overlay,SW_HIDE);return;}
 viewport::Scale view;if(!view.set(gameWidth,gameHeight,r.right,r.bottom)){statsVisible(false);ShowWindow(overlay,SW_HIDE);return;}
 static int previousW=0,previousH=0,previousGW=0,previousGH=0;
 if(previousW!=r.right||previousH!=r.bottom||previousGW!=gameWidth||previousGH!=gameHeight){previousW=r.right;previousH=r.bottom;previousGW=gameWidth;previousGH=gameHeight;renderDirty=true;
 log((L"Overlay geometry game="+std::to_wstring(gameWidth)+L"x"+std::to_wstring(gameHeight)+L" client="+std::to_wstring(r.right)+L"x"+std::to_wstring(r.bottom)).c_str());}
 if(renderDirty||!surface.dc)render(r.right,r.bottom,pt,gameWidth,gameHeight);
 present(pt,alpha);
}
static void buttonLabel(){
#ifdef RA2_UNIFIED
 suite::reportEnabled(1,enabled);
#endif
 SetWindowTextW(bStart,enabled?L"暂停显示":L"继续显示");SetWindowTextW(bDemo,demo?L"退出演示":L"布局演示");}
static LRESULT CALLBACK overlayProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(m==WM_ERASEBKGND)return 1;return DefWindowProcW(w,m,a,b);}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
#ifdef RA2_UNIFIED
 if(suite::layoutMessage(w,m,b))return 0;
 if(m==suite::ModuleEnable){enabled=bool(a);if(!enabled)detach();else lastSearch=0;buttonLabel();tick();return 0;}
 if(m==WM_CLOSE&&a!=suite::CloseToken){suite::requestClose();return 0;}
#endif
 switch(m){case WM_CREATE:{
  font=modernui::makeFont();
  auto ctl=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int x,int y,int cw,int ch,int id){HWND c=CreateWindowW(cls,title,WS_CHILD|WS_VISIBLE|style,x,y,cw,ch,w,(HMENU)(INT_PTR)id,nullptr,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};
  bStart=ctl(L"BUTTON",L"暂停显示",BS_OWNERDRAW,14,54,94,26,100);bDemo=ctl(L"BUTTON",L"布局演示",BS_OWNERDRAW,116,54,94,26,101);ctl(L"BUTTON",L"重新连接",BS_OWNERDRAW,218,54,94,26,102);
  cBuildings=ctl(L"BUTTON",L"显示建筑",BS_AUTOCHECKBOX,14,89,144,21,103);cOthers=ctl(L"BUTTON",L"其他玩家",BS_AUTOCHECKBOX,168,89,144,21,104);
  SendMessageW(cBuildings,BM_SETCHECK,buildings?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(cOthers,BM_SETCHECK,showOthers?BST_CHECKED:BST_UNCHECKED,0);
  ctl(L"STATIC",L"上边距",0,14,119,48,20,107);eTop=ctl(L"EDIT",std::to_wstring(topMargin).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,64,116,52,23,108);
  ctl(L"STATIC",L"左边距",0,168,119,48,20,109);eLeft=ctl(L"EDIT",std::to_wstring(leftMargin).c_str(),WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,218,116,52,23,110);
  ctl(L"STATIC",L"px",0,120,119,22,20,111);ctl(L"STATIC",L"px",0,274,119,22,20,112);
  SendMessageW(eTop,EM_SETLIMITTEXT,4,0);SendMessageW(eLeft,EM_SETLIMITTEXT,4,0);controlsReady=true;
  cTab=ctl(L"BUTTON",L"常驻显示（不依赖 Tab）",BS_AUTOCHECKBOX,14,149,298,22,113);SendMessageW(cTab,BM_SETCHECK,!tabEnabled?BST_CHECKED:BST_UNCHECKED,0);
  tabModeBox=ctl(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,14,179,298,110,114);SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"按键切换");SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"按住显示");SendMessageW(tabModeBox,CB_ADDSTRING,0,(LPARAM)L"短按切换 / 长按暂显");SendMessageW(tabModeBox,CB_SETCURSEL,int(tabMode),0);
  stats=ctl(L"STATIC",L"等待游戏…",0,14,216,298,34,105);ctl(L"STATIC",L"Tab 不拦截原版菜单；不兼容时用常驻显示",0,14,253,304,20,106);
  ctl(L"STATIC",L"Ctrl+Alt+H 显隐 · Ctrl+Alt+←/→ 翻页",0,14,278,304,20,115);
  ctl(L"BUTTON",L"显示 / 隐藏覆盖层",BS_OWNERDRAW,14,306,298,26,116);
  visibilityLabel=ctl(L"STATIC",L"覆盖层等待游戏",0,14,339,298,42,117);
  EnableWindow(tabModeBox,tabEnabled);
  if(!RegisterHotKey(w,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'H'))visibilityStatus(L"备用快捷键注册失败；请用显示按钮或常驻模式");RegisterHotKey(w,2,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_RIGHT);RegisterHotKey(w,3,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_LEFT);SetTimer(w,1,16,nullptr);return 0;}
 case WM_TIMER:case WM_APP+9:tick();return 0;
 case WM_COMMAND:if(controlsReady&&LOWORD(a)==114&&HIWORD(a)==CBN_SELCHANGE){tabMode=tabui::Mode(SendMessageW(tabModeBox,CB_GETCURSEL,0,0));tabs.mode(tabMode);save();tick();return 0;}
 if(controlsReady&&HIWORD(a)==EN_CHANGE&&(LOWORD(a)==108||LOWORD(a)==110)){
  BOOL ok=FALSE;UINT value=GetDlgItemInt(w,LOWORD(a),&ok,FALSE);
  if(ok&&value<=4096){if(LOWORD(a)==108)topMargin=int(value);else leftMargin=int(value);save();renderDirty=true;tick();}return 0;
 }if(HIWORD(a)==BN_CLICKED){switch(LOWORD(a)){case 100:enabled=!enabled;if(!enabled)detach();else lastSearch=0;break;case 101:demo=!demo;demoStart=GetTickCount();enabled=true;hidden=false;manualVisible=false;if(!demo)memset(snap.get(),0,sizeof(*snap));break;case 102:detach();status.clear();lastSearch=0;break;case 103:buildings=SendMessageW(cBuildings,BM_GETCHECK,0,0)==BST_CHECKED;save();break;case 104:showOthers=SendMessageW(cOthers,BM_GETCHECK,0,0)==BST_CHECKED;save();break;case 113:tabEnabled=SendMessageW(cTab,BM_GETCHECK,0,0)!=BST_CHECKED;tabs.reset();hidden=false;manualVisible=false;EnableWindow(tabModeBox,tabEnabled);save();break;case 116:toggleOverlay();break;}renderDirty=true;buttonLabel();tick();}return 0;
 case WM_HOTKEY:if(a==1){toggleOverlay();}else if(a==2)page++;else if(a==3)page=std::max(0,page-1);renderDirty=true;tick();return 0;
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:SetBkMode((HDC)a,OPAQUE);SetTextColor((HDC)a,modernui::Text);SetBkColor((HDC)a,modernui::Background);SetDCBrushColor((HDC)a,modernui::Background);return (LRESULT)GetStockObject(DC_BRUSH);
 case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)b;HBRUSH br=CreateSolidBrush((d->itemState&ODS_SELECTED)?RGB(47,76,99):RGB(31,48,68));FillRect(d->hDC,&d->rcItem,br);DeleteObject(br);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(223,237,251));SelectObject(d->hDC,font);wchar_t t[50];GetWindowTextW(d->hwndItem,t,50);DrawTextW(d->hDC,t,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);HBRUSH br=CreateSolidBrush(modernui::Background);FillRect(dc,&r,br);DeleteObject(br);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(110,210,248));SelectObject(dc,font);TextOutW(dc,14,12,L"UNIT MONITOR  /  单位监控",lstrlenW(L"UNIT MONITOR  /  单位监控"));SetTextColor(dc,RGB(148,166,188));TextOutW(dc,14,32,L"紧凑面板 · 半透明暗底 · 即时生效",lstrlenW(L"紧凑面板 · 半透明暗底 · 即时生效"));EndPaint(w,&ps);return 0;}
 case WM_DESTROY:surface.clear();KillTimer(w,1);UnregisterHotKey(w,1);UnregisterHotKey(w,2);UnregisterHotKey(w,3);detach();DeleteObject(font);PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR,int){
 HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\RA2_UnitMonitor_UI_v01");if(GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(nullptr,L"单位监控已经运行。",L"单位监控",MB_OK);return 0;}
 SetProcessDPIAware();wchar_t fn[2048];GetModuleFileNameW(nullptr,fn,2048);root=fn;root=root.substr(0,root.find_last_of(L"\\/"));logfile=_wfopen(path(L"UnitMonitor.log").c_str(),L"w, ccs=UTF-8");
 topMargin=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"topMargin",20,path(L"settings.ini").c_str())),0,4096);leftMargin=std::clamp(int(GetPrivateProfileIntW(L"overlay",L"leftMargin",6,path(L"settings.ini").c_str())),0,4096);
 buildings=GetPrivateProfileIntW(L"overlay",L"buildings",0,path(L"settings.ini").c_str())!=0;showOthers=GetPrivateProfileIntW(L"overlay",L"otherPlayers",1,path(L"settings.ini").c_str())!=0;
#ifdef RA2_UNIFIED
 enabled=suite::enabled(1);
#endif
 tabEnabled=GetPrivateProfileIntW(L"overlay",L"tabEnabled",1,path(L"settings.ini").c_str())!=0;tabMode=tabui::Mode(std::clamp(int(GetPrivateProfileIntW(L"overlay",L"tabMode",2,path(L"settings.ini").c_str())),0,2));tabs.mode(tabMode);tabs.holdThreshold(GetPrivateProfileIntW(L"overlay",L"tabHoldMs",300,path(L"settings.ini").c_str()));
 ULONG_PTR token;GdiplusStartupInput start;if(GdiplusStartup(&token,&start,nullptr)!=Ok)return 2;
 WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=overlayProc;wc.lpszClassName=L"RA2UnitTransparentOverlay";RegisterClassW(&wc);
 overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,inst,nullptr);
 wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2UnitMonitorUI";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
 RECT r{0,0,326,392};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
 ui=CreateWindowW(wc.lpszClassName,L"单位统计 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,inst,nullptr);
#ifdef RA2_UNIFIED
 suite::registerPane(1,ui);
#else
 ShowWindow(ui,SW_SHOW);
#endif
 buttonLabel();

 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}DestroyWindow(overlay);iconImages.clear();GdiplusShutdown(token);if(logfile)fclose(logfile);CloseHandle(mutex);return 0;
}

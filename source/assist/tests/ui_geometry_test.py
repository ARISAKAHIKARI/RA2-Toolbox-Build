"""Execute the shipped control construction/layout with portable Win32 stubs.

This checks geometry and state, not Windows rendering, DPI or native focus.
The constructor and layout functions are extracted verbatim from main.cpp so
the test does not maintain a second, potentially stale copy of the UI layout.
"""
from pathlib import Path
import json
import sys
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1] / 'source'
main = (source / 'main.cpp').read_text()
layout = main.split('struct PageControl {', 1)[1].split('static void bindSuper()', 1)[0]
layout = 'struct PageControl {' + layout
create = main.split('switch(msg){case WM_CREATE:{', 1)[1].split('\n case ', 1)[0]
assert create.rstrip().endswith('return 0;}')
labels = main.split('static const wchar_t* labels[]=', 1)[1].split(';', 1)[0]
status = 'static void updateModeStatus(DWORD now){' + main.split('static void updateModeStatus(DWORD now){', 1)[1].split('static void updatePlayerTestUI()', 1)[0]
prefix = r'''
#include "ui_layout.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
#include "additional_settings.h"
#include "scheduler_policy.h"
#include "compute_policy.h"
#include "view_zoom.h"
#include <initializer_list>
#include <cassert>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <iomanip>
#include <codecvt>
#include <locale>
#include <string>
#include <vector>
using namespace assist;
static bool repairUnrepairable=false;static oreconfig::Config oreTypes;static std::wstring oreMiners=L"HARV,CMIN",oreRefineries=L"GAREFN,NAREFN";
static featuretypes::Config featureTypes;static std::wstring featureTypeText[featuretypes::Count];static int featureTypeIndex=0;
using HWND=uintptr_t;using HFONT=uintptr_t;using HMENU=intptr_t;using HMODULE=uintptr_t;
using INT_PTR=intptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using DWORD=uint32_t;using HRGN=uintptr_t;
constexpr int FALSE=0,TRUE=1,WS_CHILD=1,WS_VISIBLE=2,WS_CLIPSIBLINGS=4,WS_EX_CLIENTEDGE=8,
WS_TABSTOP=16,WS_VSCROLL=32,BS_AUTORADIOBUTTON=64,BS_PUSHLIKE=128,BS_AUTOCHECKBOX=256,
BS_PUSHBUTTON=512,ES_NUMBER=1024,ES_AUTOHSCROLL=2048,CBS_DROPDOWNLIST=4096,LBS_NOTIFY=8192,
EM_SETLIMITTEXT=99,CB_ADDSTRING=10,CB_SETCURSEL=11,BM_SETCHECK=1,BST_CHECKED=1,BST_UNCHECKED=0,WM_SETFONT=2,HKM_SETHOTKEY=3,EM_SETCUEBANNER=4,
SIF_RANGE=1,SIF_PAGE=2,SIF_POS=4,SIF_DISABLENOSCROLL=8,SB_VERT=1,SW_SHOW=1,SW_SHOWNA=1,SW_HIDE=0;
constexpr const wchar_t* HOTKEY_CLASSW=L"HOTKEY";
struct RECT{int left,top,right,bottom;};struct POINT{int x,y;};
struct SCROLLINFO{unsigned cbSize,fMask;int nMin,nMax;unsigned nPage;int nPos,nTrackPos;};
struct Window{std::wstring cls,text;int id;uilayout::Rect rect;bool visible=true;RECT clip{};DWORD style=0;bool checked=false;bool enabled=true;};
static std::vector<Window> windows(2);static std::vector<RECT> regions(1);static SCROLLINFO scrollInfo{};
static int clientWidth=812,clientHeight=640;
namespace modernui {static void style(HWND){}static void heading(HWND,unsigned=96){}static void tip(HWND,const std::wstring&){}static unsigned panelDpi(unsigned d,bool){return d;}static unsigned windowDpi(HWND){return 96;}static int scale(int n,unsigned d){return (n*int(d)+48)/96;}static HFONT makeFont(int=14,unsigned=96){return 1;}constexpr int BodyPixels=14;static void mouseOnly(HWND){}static void holdKey(HWND){}}
static HWND ui,repairCheck,repairEdit,keys[12],check[7],rateEdit,marginEdit,swList,swHotkey,techSearch,techList,
 typeList,typeKey,tapEdit,typeCS,csCheck,csKeyEdit,typeIdEdit,csStatus,advancedKeyEdit,unloadKeyEdit,nativeStatus,
 hookChecks[4],ejectCheck,ejectKeyEdit,magnetronCheck,magnetronKeyEdit,oreCheck,oreStatus,detailCheck,toastEdit,
 csHoldCheck,csHoldEdit,anywhereCheck,statusLabel,schedulerEntryCombo,megaQueueCombo,objectQueueCombo;
static AdditionalSettings extra;static HWND extraControls[ExtraControlCount];
static int labelScalePercent=100,constructionScalePercent=100;static bool minimapEnabled=false,minimapSecondary=false;static int minimapMode=0,minimapScale=25,minimapPosition=0,minimapX=20,minimapY=60,minimapAlignX=0,minimapAlignY=0;
static int minimapSecondaryX=10000,minimapSecondaryY=10000,minimapSecondaryWidth=320;static uint32_t computeRequested=compute::Default;
static unsigned schedulerTarget=0,megaQueueMode=0,objectQueueMode=0;static HFONT font;static int scrollY,repairMs=2000,fps=60,topMargin=20,doubleMs=350,toastMs=6000;
static bool infoAdaptive=false;static int informationFPS(){return infoAdaptive?144:fps;}
static DWORD GetTickCount(){return 1606;}
static bool commandFresh(DWORD){return true;}
static unsigned infantryEntryResultMatch=0;static std::wstring infantryEntryResult;
static int techImplementation=0;
static bool techDirectProduce,techMode,csMode,hookAdvanced,hookSmartUnload=true,hookEject,hookAutoCS,ejectMode,magnetronMode,
 oreMode,showDetails,csHoldEnabled,buildAnywhereMode,ready;
static unsigned socialKeys[9]{},autoRevealDone=0;static void publishSuiteDisplay(){}
static unsigned idleAircraftKey=1616;static int advancedRangeCells=2;static unsigned hotkeys[12]{},csKey,advancedKey,unloadKey,ejectKey,magnetronKey,csHoldKey;
static HWND game=1;static bool enabled=true;static std::wstring hoverHelp,statusText;static int advancedScanHz=10,advancedTargetPriority=0,shiftClickMode=0,shiftClickGap=0;static bool shiftClickEnabled=false;
static struct {int techOpen=0,c4TestStatus=0,placementStatus=0,nativeAirStatus=0,renderRevealStatus=0,commandStatus=1,entryReentrant=0,entryContended=0,session=5,lastSubmitError=0,queueSent=0,commandRejected=0,queueFull=0,queueTimingFailure=0,status=1,nativeHookStatus=1,advancedUnits=0,unloadPending=0,ejectWatched=0,csHookStatus=1,oreQueued=0,oreDocking=0,oreManual=0,oreDelivered=0,oreUnverified=0,magnetShots=0,magnetUnverified=0,hookConflicts=0,megaQueueAccepted=0,megaQueueRejected=0,megaQueueStatus=0,objectQueueAccepted=0,objectQueueRejected=0,objectQueueStatus=0;} snap;
static struct {int size=0;} commandQueue;
static struct {bool readFresh(uint32_t,void*,size_t){return false;}} externalReader;
static struct {int techEnhanceStatus=0,techEnhanceCount=0,viewZoomStatus=0,viewZoomApplied=100,viewZoomWidth=800,viewZoomHeight=600,scrollRefreshStatus=0,scrollRefreshRateMilli=0,scrollRefreshMaxGapUs=0,driverMode=0,driverTarget=0,driverStatus=0,driverTicks=0,megaQueueMode=0,objectQueueMode=0;DWORD driverLastTick=0;} mockScheduler;
static decltype(mockScheduler)* shared=nullptr;
static unsigned LOBYTE(unsigned key){return key&255;}
static HWND CreateWindowExW(DWORD,const wchar_t*cls,const wchar_t*text,DWORD style,int x,int y,int w,int h,HWND,HMENU id,HMODULE,void*){
 windows.push_back({cls,text,int(id),{x,y,w,h}});windows.back().style=style;return windows.size()-1;
}
static HWND GetDlgItem(HWND,int id){for(size_t n=0;n<windows.size();++n)if(windows[n].id==id)return n;return 0;}
static void EnableWindow(HWND h,int on){windows[h].enabled=bool(on);}
static HMODULE GetModuleHandleW(void*){return 0;}
static intptr_t SendMessageW(HWND w,int m,WPARAM a,LPARAM){if(m==BM_SETCHECK&&w<windows.size())windows[w].checked=a==BST_CHECKED;return 0;}
static intptr_t SendDlgItemMessageW(HWND,int,int,WPARAM,LPARAM){return 0;}
static void GetClientRect(HWND,RECT*r){*r={0,0,clientWidth,clientHeight};}
static void MoveWindow(HWND h,int x,int y,int w,int ht,int){windows[h].rect={x,y,w,ht};}
static void ShowWindow(HWND h,int show){windows[h].visible=show;}
static HRGN CreateRectRgn(int l,int t,int r,int b){regions.push_back({l,t,r,b});return regions.size()-1;}
static bool SetWindowRgn(HWND h,HRGN r,int){windows[h].clip=regions[r];return true;}
static void DeleteObject(HRGN){}
static void SetScrollInfo(HWND,int bar,const SCROLLINFO*s,int){if(bar==SB_VERT)scrollInfo=*s;}
static void InvalidateRect(HWND,void*,int){}
static int value(const wchar_t*,const wchar_t*,int def){return def;}
static void fillTech(bool){}static void fillTypeBindings(bool){}static void registerKeys(){}
static void SetTimer(HWND,int,int,void*){}
static void SetWindowTextW(HWND h,const wchar_t* text){windows[h].text=text;}
static int GetDlgCtrlID(HWND h){return windows[h].id;}
static void GetWindowTextW(HWND h,wchar_t*out,int n){wcsncpy(out,windows[h].text.c_str(),n-1);out[n-1]=0;}
static POINT cursor{-1,-1};static void GetCursorPos(POINT*p){*p=cursor;}static void ScreenToClient(HWND,POINT*){}
static void GetWindowRect(HWND h,RECT*r){auto a=windows[h].rect;*r={a.x,a.y,a.x+a.w,a.y+a.h};}
static bool PtInRect(const RECT*r,POINT p){return p.x>=r->left&&p.y>=r->top&&p.x<r->right&&p.y<r->bottom;}
static bool IsWindowVisible(HWND h){return windows[h].visible;}

'''
prefix+='using UINT=unsigned;using HDWP=int;\nstatic int MulDiv(int n,int mul,int div){return (n*mul+div/2)/div;}\nconstexpr int SWP_NOZORDER=1,SWP_NOACTIVATE=2,SWP_NOCOPYBITS=4,SWP_NOREDRAW=8,SWP_SHOWWINDOW=16,SWP_HIDEWINDOW=32,RDW_INVALIDATE=1,RDW_ERASE=2,RDW_ALLCHILDREN=4,RDW_UPDATENOW=8,SB_HORZ=0;\nstatic HDWP BeginDeferWindowPos(int){return 1;}\nstatic void SetWindowPos(HWND h,void*,int x,int y,int w,int ht,unsigned flags){MoveWindow(h,x,y,w,ht,0);if(flags&SWP_SHOWWINDOW)windows[h].visible=true;if(flags&SWP_HIDEWINDOW)windows[h].visible=false;}\nstatic HDWP DeferWindowPos(HDWP d,HWND h,void*p,int x,int y,int w,int ht,unsigned flags){SetWindowPos(h,p,x,y,w,ht,flags);return d;}\nstatic bool EndDeferWindowPos(HDWP){return true;}static void RedrawWindow(HWND,void*,void*,unsigned){}static int GetPropW(HWND,const wchar_t*){return 0;}\nnamespace airway {static bool native(){return true;}}\n'
suffix = r'''
static bool intersects(uilayout::Rect a,uilayout::Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
int main(){
 create(1);assert(ready&&pageControls.size()>100);
 for(int id:{446,447,473,474,9094,9095}){bool found=false;for(const auto&c:pageControls)if(GetDlgCtrlID(c.hwnd)==id){assert(c.page==2);found=true;}assert(found);}
 csHoldEnabled=true;csHoldKey='G';csMode=false;hookAutoCS=false;updateModeStatus(1000);
 assert(windows[csStatus].text.find(L"按键 CS：开")!=std::wstring::npos&&windows[csStatus].text.find(L"已就绪")!=std::wstring::npos);
 snap.status=0;updateModeStatus(1200);assert(windows[nativeStatus].text==L"正在读取对局");
 game=0;updateModeStatus(1400);assert(windows[oreStatus].text.find(L"未连接")!=std::wstring::npos);
 game=1;snap.status=1;updateModeStatus(1600);assert(windows[csStatus].text.find(L"已就绪")!=std::wstring::npos);
 enabled=false;updateModeStatus(1800);assert(windows[nativeStatus].text==L"已暂停");
 enabled=true;snap.status=-1;updateModeStatus(2000);assert(windows[nativeStatus].text==L"暂不可用");
 snap.status=1;snap.csHookStatus=-1;updateModeStatus(2200);assert(windows[csStatus].text.find(L"暂不可用")!=std::wstring::npos);
 snap.csHookStatus=1;shared=&mockScheduler;snap.hookConflicts=1;updateModeStatus(2400);
 assert(windows[nativeStatus].text.find(L"冲突")!=std::wstring::npos&&windows[nativeStatus].text.find(L"Hook")==std::wstring::npos);
 snap.hookConflicts=0;shared=nullptr;updateModeStatus(2600);


 // Feature groups and adjacent bindings follow the actual controls.
 auto at=[&](int id)->const PageControl&{for(auto&c:pageControls)if(GetDlgCtrlID(c.hwnd)==id)return c;assert(false);return pageControls[0];};
 for(int id:{100,111,451,455,853,440,724})assert(at(id).rect.y>at(900).rect.y&&at(id).rect.y<at(901).rect.y);
 for(int id:{458,450,477,702,705,719,720,721,736,795,803,109,110,410})assert(at(id).rect.y>at(901).rect.y&&at(id).rect.y<at(902).rect.y);
 for(auto pair:{std::pair<int,int>{450,729},{478,730},{474,731},{704,732},{707,733},{709,734},{726,735},{728,737},{789,738},{811,812}})assert(at(pair.first).rect.y==at(pair.second).rect.y&&at(pair.first).rect.x+at(pair.first).rect.w<at(pair.second).rect.x);
 for(int id:{861,862,863,864})assert(!windows[at(id).hwnd].enabled);
 for(int id:{861})assert(!windows[at(id).hwnd].checked);
 for(int id:{803,894,866})assert(at(id).rect.y>at(901).rect.y&&at(id).rect.y<at(902).rect.y);
 for(int id:{808,809})assert(at(id).page==1);
 for(int id:{799,817})assert(at(id).page==0&&at(id).rect.y>at(902).rect.y&&at(id).rect.y<at(905).rect.y);
 for(int id:{885,895,420,860})assert(at(id).page==2);
 assert(at(885).rect.y>at(904).rect.y&&at(885).rect.y<at(9094).rect.y);
 for(int id:{803,808})assert(windows[at(id).hwnd].enabled&&windows[at(id).hwnd].text.find(L"测试")==std::wstring::npos&&windows[at(id).hwnd].text.find(L"实验")==std::wstring::npos);
 extra.airWaypoints=false;extra.nativeAirWaypoints=extra.renderReveal=true;snap.nativeAirStatus=snap.renderRevealStatus=1;updateModeStatus(2800);
 assert(windows[at(866).hwnd].text==L"飞机路径点：原生模式"&&windows[at(809).hwnd].text==L"地图显示：已启用");
 for(int id:{787,818,788})assert(at(id).visibleHeight==26);
 // Hidden legacy selector cannot appear via scroll, focus or navigation.
 assert(at(760).page==-3&&!windows[at(760).hwnd].visible);
 // Hovering the header/footer must not expose clipped offscreen feature help.
 clientWidth=812;clientHeight=640;scrollTo(at(450).rect.y-uilayout::BodyTop);
 cursor={30,50};updateHoverHelp();assert(hoverHelp.empty());
 cursor={30,uilayout::footerTop(clientHeight)};updateHoverHelp();assert(hoverHelp.empty());
 assert(!at(450).help.empty());
 // The status area is two lines tall; instructions remain concise.
 for(auto&c:pageControls)if(c.page>=0){
  assert(c.help.find(L"GameRuntime")==std::wstring::npos&&c.help.find(L"OutList")==std::wstring::npos&&c.help.find(L"Hook")==std::wstring::npos);
  if(c.help.size()>110){std::cerr<<"Long help id "<<GetDlgCtrlID(c.hwnd)<<" chars "<<c.help.size()<<"\n";assert(false);}
 }

 // Device-pixel layout must follow the font scale, with the same clipping.
 for(unsigned dpi:{96u,120u,144u,192u}){panelDpi=dpi;clientWidth=modernui::scale(812,dpi);clientHeight=modernui::scale(640,dpi);
  scrollTo(at(894).rect.y-uilayout::BodyTop);auto&w=windows[at(894).hwnd];assert(w.rect.w==modernui::scale(at(894).rect.w,dpi));assert(w.rect.y+w.clip.top>=modernui::scale(uilayout::BodyTop,dpi));
 }
 panelDpi=96;
 int inspected=0;
 // Body controls are split across three independent categories. Every supported interactive
 // ID survives the redesign; only redundant save labels/navigation disappear.
 for(int id:{100,111,200,211,300,306,310,311,312,410,411,420,421,422,423,424,425,426,427,428,440,441,443,444,445,446,447,449,450,451,453,454,455,456,458,459,466,467,470,471,473,474,476,477,478,481,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,751,760,761,762,785,786,787,795,796,797,798,799,818,830,831,832,833,850,851,852,853,900,901,902,903,904,910,911,912,915,802,803,804,807,808,809,810,811,812,813,814,815,816,817,990,991}){
  int found=0;for(auto&w:windows)found+=w.id==id;if(found!=1){std::cerr<<"Missing/duplicate ID "<<id<<" count "<<found<<"\n";return 1;}
 }
 for(int id:{108,208,722,913,914,739,790,791,792,805,806,882,883,884,9234,9235})assert(!GetDlgItem(ui,id));
 for(size_t i=0;i<pageControls.size();++i){const auto&a=pageControls[i];if(a.page<0)continue;assert(a.page>=0&&a.page<3&&a.rect.y>=uilayout::BodyTop);
  auto rect=a.rect;rect.h=a.visibleHeight;
  for(size_t j=i+1;j<pageControls.size();++j){const auto&b=pageControls[j];if(b.page!=a.page)continue;auto other=b.rect;other.h=b.visibleHeight;
   if(intersects(rect,other)){std::cerr<<"IDs "<<windows[a.hwnd].id<<","<<windows[b.hwnd].id<<" rectangles "<<rect.x<<","<<rect.y<<","<<rect.w<<","<<rect.h<<" / "<<other.x<<","<<other.y<<","<<other.w<<","<<other.h<<"\n";return 1;}
  }
 }
 for(activePage=0;activePage<3;++activePage)for(int width:{645,812,1040})for(int height:{440,580,720,900}){
  clientWidth=width;clientHeight=height;const int maximum=uilayout::maxScroll(pageBottom(),height);
  for(const auto&c:pageControls)if(c.page==activePage){
   scrollTo(c.rect.y-uilayout::BodyTop);const auto&w=windows[c.hwnd];
   assert(w.visible&&w.rect.y>=uilayout::BodyTop&&w.rect.y+c.visibleHeight<=uilayout::bodyBottom(height));
   assert(w.clip.top==0&&w.clip.bottom==c.visibleHeight);++inspected;
  }
  scrollTo(1000000);assert(scrollY==maximum&&scrollInfo.nPos==maximum);
  for(const auto&c:pageControls){const auto&w=windows[c.hwnd];
   if(c.page==-3){assert(!w.visible);}
   else if(c.page==-2){assert(w.rect.y==uilayout::footerTop(height)+c.rect.y&&w.visible);assert(w.rect.y+w.rect.h<=height);}
   else if(c.page==-1){assert(w.rect.y==c.rect.y);if(w.id>=910&&w.id<=915)assert(w.visible==(activePage==0));else assert(w.visible);}
   else if(w.visible&&w.clip.bottom>w.clip.top){assert(w.rect.y+w.clip.top>=uilayout::BodyTop);assert(w.rect.y+w.clip.bottom<=uilayout::bodyBottom(height));}
  }
 }
 std::cout<<"{\"controls\":"<<pageControls.size()<<",\"fully_reachable_checks\":"<<inspected<<",\"viewport_sizes\":12,\"pages\":3,\"content_bottom\":"<<pageBottom()<<",\"geometry\":[";
 std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8;bool first=true;
 for(auto&c:pageControls){if(!first)std::cout<<",";first=false;auto&w=windows[c.hwnd];std::cout<<"{\"id\":"<<w.id<<",\"page\":"<<c.page<<",\"x\":"<<c.rect.x<<",\"y\":"<<c.rect.y<<",\"w\":"<<c.rect.w<<",\"h\":"<<c.visibleHeight<<",\"toggle\":"<<((w.style&BS_AUTOCHECKBOX)?"true":"false")<<",\"checked\":"<<(w.checked?"true":"false")<<",\"class\":"<<std::quoted(utf8.to_bytes(w.cls))<<",\"text\":"<<std::quoted(utf8.to_bytes(w.text))<<"}";}
 std::cout<<"]}\n";
}
'''
with tempfile.TemporaryDirectory(prefix='ra2-ui-') as directory:
    cpp = Path(directory) / 'ui.cpp'
    binary = Path(directory) / 'ui'
    cpp.write_text(prefix + 'static const wchar_t* labels[]=' + labels + ';\n' +
                   layout + 'static uint32_t commandMatch(){return shared?1:0;}\n' + status + '\nstatic int create(HWND w){' + create + '\n' + suffix)
    subprocess.run(['g++', '-std=c++17', '-O2', '-I', str(source), str(cpp), '-o', str(binary)], check=True)
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    if run.returncode:print(run.stderr, file=sys.stderr)
    run.check_returncode()
    result = json.loads(run.stdout,strict=False) # std::quoted permits literal newlines; dump below emits strict JSON
    if '--dump' in sys.argv:Path(sys.argv[sys.argv.index('--dump')+1]).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    summary={k:v for k,v in result.items() if k!='geometry'}
    print('PASS: shipped UI construction/layout: ' + json.dumps(summary))
    print('PASS: all controls accessible, no overlap, supported interactive IDs retained, three independently scrollable categories, pinned header/footer, clipped body and scroll limits')


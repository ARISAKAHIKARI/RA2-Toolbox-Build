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
create = main.split('switch(msg){case WM_CREATE:{', 1)[1].split('\n case WM_SIZE:', 1)[0]
assert create.rstrip().endswith('return 0;}')
labels = main.split('static const wchar_t* labels[]=', 1)[1].split(';', 1)[0]
status = 'static void updateModeStatus(DWORD now){' + main.split('static void updateModeStatus(DWORD now){', 1)[1].split('static void tick()', 1)[0]
prefix = r'''
#include "ui_layout.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
#include "additional_settings.h"
#include "scheduler_policy.h"
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
SIF_RANGE=1,SIF_PAGE=2,SIF_POS=4,SIF_DISABLENOSCROLL=8,SB_VERT=0,SW_SHOW=1,SW_HIDE=0;
constexpr const wchar_t* HOTKEY_CLASSW=L"HOTKEY";
struct RECT{int left,top,right,bottom;};
struct SCROLLINFO{unsigned cbSize,fMask;int nMin,nMax;unsigned nPage;int nPos,nTrackPos;};
struct Window{std::wstring cls,text;int id;uilayout::Rect rect;bool visible=true;RECT clip{};DWORD style=0;bool checked=false;};
static std::vector<Window> windows(2);static std::vector<RECT> regions(1);static SCROLLINFO scrollInfo{};
static int clientWidth=812,clientHeight=640;
namespace modernui {static void style(HWND){}static void heading(HWND){}static void mouseOnly(HWND){}}
static HWND ui,repairCheck,repairEdit,keys[12],check[7],rateEdit,marginEdit,swList,swHotkey,techSearch,techList,
 typeList,typeKey,tapEdit,typeCS,csCheck,csKeyEdit,typeIdEdit,csStatus,advancedKeyEdit,unloadKeyEdit,nativeStatus,
 hookChecks[4],ejectCheck,ejectKeyEdit,magnetronCheck,magnetronKeyEdit,oreCheck,oreStatus,detailCheck,toastEdit,
 csHoldCheck,csHoldEdit,anywhereCheck,statusLabel,schedulerEntryCombo,megaQueueCombo,objectQueueCombo;
static AdditionalSettings extra;static HWND extraControls[ExtraControlCount];
static unsigned schedulerTarget=0,megaQueueMode=0,objectQueueMode=0;static HFONT font;static int scrollY,repairMs=2000,fps=60,topMargin=20,doubleMs=350,toastMs=6000;
static DWORD GetTickCount(){return 1606;}
static bool techDirectProduce,techMode,csMode,hookAdvanced,hookSmartUnload=true,hookEject,hookAutoCS,ejectMode,magnetronMode,
 oreMode,showDetails,csHoldEnabled,buildAnywhereMode,ready;
static unsigned hotkeys[12]{},csKey,advancedKey,unloadKey,ejectKey,magnetronKey,csHoldKey;
static HWND game=1;static bool enabled=true;
static struct {int commandStatus=1,entryReentrant=0,entryContended=0,session=5,lastSubmitError=0,queueSent=0,commandRejected=0,queueFull=0,queueTimingFailure=0,status=1,nativeHookStatus=1,advancedUnits=0,unloadPending=0,ejectWatched=0,csHookStatus=1,oreQueued=0,oreDocking=0,oreManual=0,oreDelivered=0,oreUnverified=0,magnetShots=0,magnetUnverified=0,hookConflicts=0,megaQueueAccepted=0,megaQueueRejected=0,megaQueueStatus=0,objectQueueAccepted=0,objectQueueRejected=0,objectQueueStatus=0;} snap;
static struct {int size=0;} commandQueue;
static struct {int driverMode=0,driverTarget=0,driverStatus=0,driverTicks=0,megaQueueMode=0,objectQueueMode=0;DWORD driverLastTick=0;} mockScheduler;
static decltype(mockScheduler)* shared=nullptr;
static unsigned LOBYTE(unsigned key){return key&255;}
static HWND CreateWindowExW(DWORD,const wchar_t*cls,const wchar_t*text,DWORD style,int x,int y,int w,int h,HWND,HMENU id,HMODULE,void*){
 windows.push_back({cls,text,int(id),{x,y,w,h}});windows.back().style=style;return windows.size()-1;
}
static HWND GetDlgItem(HWND,int id){for(size_t n=0;n<windows.size();++n)if(windows[n].id==id)return n;return 0;}
static void EnableWindow(HWND,int){}
static HMODULE GetModuleHandleW(void*){return 0;}
static intptr_t SendMessageW(HWND w,int m,WPARAM a,LPARAM){if(m==BM_SETCHECK&&w<windows.size())windows[w].checked=a==BST_CHECKED;return 0;}
static intptr_t SendDlgItemMessageW(HWND,int,int,WPARAM,LPARAM){return 0;}
static void GetClientRect(HWND,RECT*r){*r={0,0,clientWidth,clientHeight};}
static void MoveWindow(HWND h,int x,int y,int w,int ht,int){windows[h].rect={x,y,w,ht};}
static void ShowWindow(HWND h,int show){windows[h].visible=show;}
static HRGN CreateRectRgn(int l,int t,int r,int b){regions.push_back({l,t,r,b});return regions.size()-1;}
static bool SetWindowRgn(HWND h,HRGN r,int){windows[h].clip=regions[r];return true;}
static void DeleteObject(HRGN){}
static void SetScrollInfo(HWND,int,const SCROLLINFO*s,int){scrollInfo=*s;}
static void InvalidateRect(HWND,void*,int){}
static int value(const wchar_t*,const wchar_t*,int def){return def;}
static void fillTech(bool){}static void fillTypeBindings(bool){}static void registerKeys(){}
static void SetTimer(HWND,int,int,void*){}
static void SetWindowTextW(HWND h,const wchar_t* text){windows[h].text=text;}
'''
suffix = r'''
static bool intersects(uilayout::Rect a,uilayout::Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
int main(){
 create(1);assert(ready&&pageControls.size()>100);
 csHoldEnabled=true;csHoldKey='G';csMode=false;hookAutoCS=false;updateModeStatus(1000);
 assert(windows[csStatus].text.find(L"按键 CS：开")!=std::wstring::npos&&windows[csStatus].text.find(L"已就绪")!=std::wstring::npos);
 snap.status=0;updateModeStatus(1001);assert(windows[csStatus].text.find(L"正在读取对局")!=std::wstring::npos&&windows[nativeStatus].text==L"正在读取对局");
 game=0;updateModeStatus(1002);assert(windows[oreStatus].text.find(L"未连接")!=std::wstring::npos);
 game=1;snap.status=1;updateModeStatus(1003);assert(windows[csStatus].text.find(L"已就绪")!=std::wstring::npos);
 enabled=false;updateModeStatus(1004);assert(windows[nativeStatus].text==L"已暂停");
 enabled=true;snap.status=-1;updateModeStatus(1005);assert(windows[nativeStatus].text==L"接口不可用");
 snap.status=1;snap.csHookStatus=-1;updateModeStatus(1006);assert(windows[csStatus].text.find(L"接口不可用")!=std::wstring::npos);
 snap.csHookStatus=1;snap.oreManual=7;updateModeStatus(1206);
 shared=&mockScheduler;mockScheduler.driverMode=1;mockScheduler.driverStatus=-1;
 updateModeStatus(1406);assert(windows[nativeStatus].text.find(L"回退消息")!=std::wstring::npos);
 mockScheduler.driverStatus=2;mockScheduler.driverLastTick=1606;mockScheduler.driverTicks=9;updateModeStatus(1606);
 assert(windows[nativeStatus].text.find(L"游戏逻辑帧")!=std::wstring::npos);
 shared=nullptr;updateModeStatus(1806);assert(windows[oreStatus].text.find(L"手动接管 7")!=std::wstring::npos);
 int inspected=0;
 // All body controls share one continuous page. Every original interactive
 // ID survives the redesign; only redundant save labels/navigation disappear.
 for(int id:{100,111,200,211,300,306,310,311,312,410,411,420,421,422,423,424,425,426,427,428,440,441,443,444,445,446,447,449,450,451,453,454,455,456,458,459,466,467,470,471,473,474,476,477,478,481,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,739,751,760,761,762,990,991}){
  int found=0;for(auto&w:windows)found+=w.id==id;assert(found==1);
 }
 for(size_t i=0;i<pageControls.size();++i){const auto&a=pageControls[i];if(a.page<0)continue;assert(a.page==0&&a.rect.y>=uilayout::BodyTop);
  auto rect=a.rect;rect.h=a.visibleHeight;
  for(size_t j=i+1;j<pageControls.size();++j){const auto&b=pageControls[j];if(b.page<0)continue;auto other=b.rect;other.h=b.visibleHeight;
   if(intersects(rect,other)){std::wcerr<<L"Overlap: "<<windows[a.hwnd].text<<L" / "<<windows[b.hwnd].text<<L"\n";return 1;}
  }
 }
 for(int width:{645,812,1040})for(int height:{440,580,720,900}){
  clientWidth=width;clientHeight=height;const int maximum=uilayout::maxScroll(pageBottom(),height);
  for(const auto&c:pageControls)if(c.page>=0){
   scrollTo(c.rect.y-uilayout::BodyTop);const auto&w=windows[c.hwnd];
   assert(w.visible&&w.rect.y>=uilayout::BodyTop&&w.rect.y+c.visibleHeight<=uilayout::bodyBottom(height));
   assert(w.clip.top==0&&w.clip.bottom==c.visibleHeight);++inspected;
  }
  scrollTo(1000000);assert(scrollY==maximum&&scrollInfo.nPos==maximum);
  for(const auto&c:pageControls){const auto&w=windows[c.hwnd];
   if(c.page==-2){assert(w.rect.y==uilayout::footerTop(height)+c.rect.y&&w.visible);assert(w.rect.y+w.rect.h<=height);}
   else if(c.page==-1){assert(w.rect.y==c.rect.y&&w.visible);}
   else if(w.visible&&w.clip.bottom>w.clip.top){assert(w.rect.y+w.clip.top>=uilayout::BodyTop);assert(w.rect.y+w.clip.bottom<=uilayout::bodyBottom(height));}
  }
 }
 std::cout<<"{\"controls\":"<<pageControls.size()<<",\"fully_reachable_checks\":"<<inspected<<",\"viewport_sizes\":12,\"pages\":1,\"content_bottom\":"<<pageBottom()<<",\"geometry\":[";
 std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8;bool first=true;
 for(auto&c:pageControls){if(!first)std::cout<<",";first=false;auto&w=windows[c.hwnd];std::cout<<"{\"id\":"<<w.id<<",\"page\":"<<c.page<<",\"x\":"<<c.rect.x<<",\"y\":"<<c.rect.y<<",\"w\":"<<c.rect.w<<",\"h\":"<<c.visibleHeight<<",\"toggle\":"<<((w.style&BS_AUTOCHECKBOX)?"true":"false")<<",\"checked\":"<<(w.checked?"true":"false")<<",\"class\":"<<std::quoted(utf8.to_bytes(w.cls))<<",\"text\":"<<std::quoted(utf8.to_bytes(w.text))<<"}";}
 std::cout<<"]}\n";
}
'''
with tempfile.TemporaryDirectory(prefix='ra2-ui-') as directory:
    cpp = Path(directory) / 'ui.cpp'
    binary = Path(directory) / 'ui'
    cpp.write_text(prefix + 'static const wchar_t* labels[]=' + labels + ';\n' +
                   layout + main[main.index('static std::wstring submitDiagnostic(){'):main.index('static void result(){')] + "static uint32_t commandMatch(){return shared?1:0;}\n" + status + '\nstatic int create(HWND w){' + create + '\n' + suffix)
    subprocess.run(['g++', '-std=c++17', '-O2', '-I', str(source), str(cpp), '-o', str(binary)], check=True)
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    if run.returncode:print(run.stderr, file=sys.stderr)
    run.check_returncode()
    result = json.loads(run.stdout)
    if '--dump' in sys.argv:Path(sys.argv[sys.argv.index('--dump')+1]).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    summary={k:v for k,v in result.items() if k!='geometry'}
    print('PASS: shipped UI construction/layout: ' + json.dumps(summary))
    print('PASS: all controls accessible, no overlap, all interactive IDs retained, one continuous page, pinned header/footer, clipped body and scroll limits')
    print('PASS: shipped mode status distinguishes custom CS, live/waiting/disconnected/paused/fault states and restores live status after re-entry')

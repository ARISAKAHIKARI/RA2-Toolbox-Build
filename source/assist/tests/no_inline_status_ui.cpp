#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#define CALLBACK
using HWND=int;using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;
using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;using LRESULT=intptr_t;
constexpr UINT WM_SETTEXT=12,WM_NCDESTROY=130;constexpr int FALSE=0;
using Callback=LRESULT(*)(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
struct Hook {Callback callback=nullptr;UINT_PTR id=0;DWORD_PTR data=0;};
static std::map<int,Hook> hooks;
static std::map<int,std::wstring> texts;
static std::map<int,bool> enabled;
struct Page {HWND hwnd;std::wstring help;};
static std::vector<Page> pageControls;
static HWND ui=1;
static struct {int hookInstalled=0,hookConflicts=0;} snap;
static struct {bool nativeMegaGateway=true,airWaypoints=false,nativeAirWaypoints=false,techEnhance=false;int mapStrategy=0,airPathKey=0;} extra;
struct Shared {int nativeMegaGatewayStatus=0;uint32_t nativeMegaGatewayAccepted=0,nativeMegaGatewaySkipped=0;int techEnhanceStatus=0,techEnhanceCount=0;};
static Shared data;static Shared*shared=&data;static bool fresh=false;
static uint32_t autoRevealDone=0;
static uint32_t commandMatch(){return 42;}
namespace airway {static bool selectedNative=true;static bool native(){return selectedNative;}}
static uint32_t GetTickCount(){return 100;}
static bool commandFresh(uint32_t){return fresh;}
static HWND GetDlgItem(HWND,int id){return id;}
static bool SetWindowSubclass(HWND w,Callback cb,UINT_PTR id,DWORD_PTR d){hooks[w]={cb,id,d};return true;}
static void RemoveWindowSubclass(HWND w,Callback,UINT_PTR){hooks.erase(w);}
static LRESULT DefSubclassProc(HWND w,UINT m,WPARAM,LPARAM b){if(m==WM_SETTEXT)texts[w]=reinterpret_cast<const wchar_t*>(b);return 1;}
static void SetWindowTextW(HWND w,const wchar_t*t){auto i=hooks.find(w);if(i==hooks.end())texts[w]=t;else{auto h=i->second;h.callback(w,WM_SETTEXT,0,reinterpret_cast<LPARAM>(t),h.id,h.data);}}
static void EnableWindow(HWND w,bool e){enabled[w]=e;}
static void create(){
#include "../source/no_inline_status_ui.h"
}
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static bool has(int id,const wchar_t*s){return texts[id].find(s)!=std::wstring::npos;}
int main(){
 const int unavailable[]={476,860,9183,9187,9189,9193,9096};
 const int pending[]={100,459,455,458,477,9042,702,705,719,720,721,9050,799,470,804};
 for(int id:unavailable)pageControls.push_back({id,L"original"});
 for(int id:pending){pageControls.push_back({id,L"original"});enabled[id]=true;}
 create();
 for(int id:unavailable){CHECK(!enabled[id]);for(const auto&p:pageControls)if(p.hwnd==id)CHECK(p.help.find(L"未实现")!=std::wstring::npos);}
 for(int id:pending){CHECK(enabled[id]);CHECK(has(id,L"待实测"));for(const auto&p:pageControls)if(p.hwnd==id)CHECK(p.help.find(L"验证未完成")!=std::wstring::npos);}
 CHECK(has(420,L"待审计"));CHECK(has(454,L"替代未实现"));CHECK(has(902,L"未完成"));
 CHECK(has(9192,L"等待"));
 fresh=true;data.nativeMegaGatewayStatus=1;data.nativeMegaGatewayAccepted=8;data.nativeMegaGatewaySkipped=3;
 SetWindowTextW(9192,L"legacy arbitration enabled");
 CHECK(has(9192,L"候选 8"));CHECK(has(9192,L"未判定区间 3"));CHECK(has(9192,L"不接管自动任务"));CHECK(has(9192,L"按帧采样"));CHECK(has(9192,L"未实现"));
 for(int id:{807,865,9188,9190,9194}){SetWindowTextW(id,L"waiting");CHECK(has(id,L"此分支不可用"));CHECK(has(id,L"未实现"));}
 data.nativeMegaGatewayStatus=-4;SetWindowTextW(9192,L"");CHECK(has(9192,L"LAN / Internet"));
 data.nativeMegaGatewayStatus=-3;SetWindowTextW(9192,L"");CHECK(has(9192,L"采样失败"));
 extra.nativeMegaGateway=false;SetWindowTextW(9192,L"");CHECK(has(9192,L"关闭"));
 extra.nativeMegaGateway=true;shared=nullptr;SetWindowTextW(9192,L"");CHECK(has(9192,L"等待"));shared=&data;
 for(int mode:{1,2,4}){extra.mapStrategy=mode;SetWindowTextW(809,L"old misleading text");CHECK(has(809,L"此分支不可用"));CHECK(has(809,L"未实现"));}
 extra.mapStrategy=3;SetWindowTextW(809,L"");CHECK(has(809,L"等待执行")&&has(809,L"修改探索数据")&&has(809,L"同步风险"));
 autoRevealDone=42;SetWindowTextW(809,L"");CHECK(has(809,L"已执行"));
 autoRevealDone=41;SetWindowTextW(809,L"");CHECK(!has(809,L"已执行"));
 extra.mapStrategy=0;SetWindowTextW(809,L"");CHECK(has(809,L"关闭"));
 extra.airWaypoints=true;SetWindowTextW(866,L"");CHECK(has(866,L"兼容事件模式")&&!has(866,L"不可用"));CHECK(has(866,L"待实测"));
 extra.airWaypoints=false;extra.nativeAirWaypoints=true;SetWindowTextW(866,L"");CHECK(has(866,L"不可用")&&has(866,L"未实现"));
 extra.airPathKey=90;SetWindowTextW(866,L"");CHECK(has(866,L"兼容事件模式"));
 extra.airPathKey=0;airway::selectedNative=false;SetWindowTextW(866,L"");CHECK(has(866,L"兼容事件模式"));
 extra.nativeAirWaypoints=false;SetWindowTextW(866,L"");CHECK(has(866,L"关闭"));
 extra.techEnhance=true;data.techEnhanceStatus=1;data.techEnhanceCount=3;SetWindowTextW(9098,L"");
 CHECK(has(9098,L"已修改类型数据（3 项）")&&has(9098,L"同步风险")&&!has(9098,L"不可用"));
 data.techEnhanceStatus=-2;SetWindowTextW(9098,L"");CHECK(has(9098,L"恢复失败"));
 extra.techEnhance=false;SetWindowTextW(9098,L"");CHECK(has(9098,L"恢复失败"));
 data.techEnhanceStatus=1;SetWindowTextW(9098,L"");CHECK(has(9098,L"等待恢复"));
 data.techEnhanceStatus=0;SetWindowTextW(9098,L"");CHECK(has(9098,L"关闭"));
 shared=nullptr;extra.techEnhance=true;extra.mapStrategy=3;extra.airWaypoints=true;
 for(int id:{809,866,9098}){SetWindowTextW(id,L"");CHECK(has(id,L"等待"));}shared=&data;
 snap.hookConflicts=1;SetWindowTextW(865,L"real conflict");CHECK(has(865,L"real conflict")&&has(865,L"重启游戏"));snap.hookConflicts=0;
 snap.hookInstalled=1;SetWindowTextW(9192,L"residual patch");CHECK(has(9192,L"residual patch")&&has(9192,L"不会写回代码"));snap.hookInstalled=0;
 auto h=hooks[9192];h.callback(9192,WM_NCDESTROY,0,0,h.id,h.data);CHECK(!hooks.count(9192));
 std::printf("PASS: %u UI-policy checks; unfinished/pending labels; patch/data/event distinctions; restart instead of restoring code\n",checks);
}

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
static struct {bool nativeMegaGateway=true;} extra;
struct Shared {int nativeMegaGatewayStatus=0;uint32_t nativeMegaGatewayAccepted=0,nativeMegaGatewaySkipped=0;};
static Shared data;static Shared*shared=&data;static bool fresh=false;
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
int main(){
 for(int id:{476,860,9183,9187,9189,9193,9096})pageControls.push_back({id,L"original"});
 create();
 for(const auto&p:pageControls){CHECK(!enabled[p.hwnd]);CHECK(p.help.find(L"不可用")!=std::wstring::npos);}
 CHECK(texts[9192].find(L"等待")!=std::wstring::npos);
 fresh=true;data.nativeMegaGatewayStatus=1;data.nativeMegaGatewayAccepted=8;data.nativeMegaGatewaySkipped=3;
 SetWindowTextW(9192,L"legacy arbitration enabled");
 CHECK(texts[9192].find(L"候选 8")!=std::wstring::npos);
 CHECK(texts[9192].find(L"未判定区间 3")!=std::wstring::npos);
 CHECK(texts[9192].find(L"不接管自动任务")!=std::wstring::npos);
 for(int id:{807,865,9188,9190,9194}){SetWindowTextW(id,L"waiting");CHECK(texts[id].find(L"此分支不可用")!=std::wstring::npos);}
 data.nativeMegaGatewayStatus=-4;SetWindowTextW(9192,L"");CHECK(texts[9192].find(L"LAN / Internet")!=std::wstring::npos);
 data.nativeMegaGatewayStatus=-3;SetWindowTextW(9192,L"");CHECK(texts[9192].find(L"采样失败")!=std::wstring::npos);
 extra.nativeMegaGateway=false;SetWindowTextW(9192,L"");CHECK(texts[9192].find(L"关闭")!=std::wstring::npos);
 extra.nativeMegaGateway=true;shared=nullptr;SetWindowTextW(9192,L"");CHECK(texts[9192].find(L"等待")!=std::wstring::npos);shared=&data;
 snap.hookConflicts=1;SetWindowTextW(865,L"real conflict");CHECK(texts[865]==L"real conflict");snap.hookConflicts=0;
 snap.hookInstalled=1;SetWindowTextW(9192,L"residual patch");CHECK(texts[9192]==L"residual patch");snap.hookInstalled=0;
 auto h=hooks[9192];h.callback(9192,WM_NCDESTROY,0,0,h.id,h.data);CHECK(!hooks.count(9192));
 std::printf("PASS: %u UI-policy checks; unavailable controls, diagnostic-only labels, real conflicts retained\n",checks);
}

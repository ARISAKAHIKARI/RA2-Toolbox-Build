#ifdef NDEBUG
#error "metadata assertions must be enabled"
#endif
#define RA2_CONSOLE_WEBVIEW2
#include <windows.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "../../assist/source/ui_layout.h"
using namespace assist;
static int checks=0,layouts=0;static HWND ui=HWND(1);static std::set<int>openedSettingGroups;static std::map<HWND,int>ids;static std::map<std::pair<HWND,std::wstring>,HANDLE>props;static std::map<HWND,std::wstring>texts;static std::set<HWND>hidden;
static BOOL setProp(HWND h,LPCWSTR key,HANDLE value){props[{h,key}]=value;return TRUE;}
static HANDLE getProp(HWND h,LPCWSTR key){auto i=props.find({h,key});return i==props.end()?nullptr:i->second;}
static HANDLE removeProp(HWND h,LPCWSTR key){auto old=getProp(h,key);props.erase({h,key});return old;}
static int testID(HWND h){return ids[h];}static BOOL show(HWND h,int mode){if(mode==SW_HIDE)hidden.insert(h);return TRUE;}
static HWND item(HWND,int n){for(auto&i:ids)if(i.second==n)return i.first;return nullptr;}
static BOOL testText(HWND h,LPCWSTR s){texts[h]=s;return TRUE;}static void layoutPanel(){++layouts;}
#define SetPropW setProp
#define GetPropW getProp
#define RemovePropW removeProp
#define GetDlgCtrlID testID
#define ShowWindow show
#define GetDlgItem item
#define SetWindowTextW testText
struct Shared {int techEnhanceParameterStatus=0,sidebarStatus=0;unsigned techEnhanceParameterExceptions=0,techEnhanceParameterError=0;}storage,*shared=&storage;
static struct {bool sidebarEnabled=false;unsigned sidebarMode=0;}extra;static int hides=0,shown=0;static void hideWorldOverlay(){++hides;}
#include CONSOLE_METADATA_ROUTES
static void check(bool v){++checks;if(!v){std::fprintf(stderr,"metadata FAIL check %d\n",checks);std::abort();}}
static HWND add(int n,int page,int y,int fold=0){auto h=HWND(INT_PTR(ids.size()+10));ids[h]=n;pageControls.push_back({h,page,{20,y,200,26},26,{},fold,{20,y,200,26}});return h;}
int main(){
 std::set<int>groups;for(int group=9601;group<=9615;++group){int page=(group-9601)%5,y=(group-9601)*80;groups.insert(group);add(group,page,y);add(10000+group,page,y);add(20000+group,page,y+40,group);}add(9185,4,1800);add(9470,4,1840);auto status=add(9471,4,1880);for(int n:{989,990,991})add(n,4,1900+n);
 openedSettingGroups={9602,9614};const auto before=openedSettingGroups;prepareConsoleSettingsModel();check(openedSettingGroups==before);check(layouts==1);
 for(int group:groups){auto fold=item(ui,group),master=item(ui,10000+group),child=item(ui,20000+group);auto name=getProp(fold,L"RA2.Console.Group");check(name&&name==getProp(master,L"RA2.Console.Group")&&name==getProp(child,L"RA2.Console.Group"));check(getProp(fold,L"RA2.Console.Skip")&&hidden.count(fold));check(!getProp(master,L"RA2.Console.Skip")&&!getProp(child,L"RA2.Console.Skip"));check(getProp(master,L"RA2.Console.GroupHeader")&&!getProp(child,L"RA2.Console.GroupHeader"));check(bool(getProp(child,L"RA2.Console.CollapsedDefault"))==!before.count(group));check(getProp(child,L"RA2.Console.Collapsible")&&getProp(master,L"RA2.Console.Collapsible"));}
 for(int n:{989,990,991})check(getProp(item(ui,n),L"RA2.Console.Skip"));check(getProp(item(ui,9185),L"RA2.Console.Immediate"));check(!getProp(item(ui,9470),L"RA2.Console.Skip")&&!getProp(status,L"RA2.Console.Skip"));
 for(int state=-7;state<=2;++state){storage.techEnhanceParameterStatus=state;storage.techEnhanceParameterError=123;storage.techEnhanceParameterExceptions=5;parameterStatus();check(bool(getProp(status,L"RA2.Console.RuntimeError"))==(state<0));check(texts[status].find(L"命中 5")!=std::wstring::npos);check((texts[status].find(L"错误 123")!=std::wstring::npos)==(state<0));}
 shared=nullptr;parameterStatus();check(!getProp(status,L"RA2.Console.RuntimeError")&&texts[status].find(L"关闭")!=std::wstring::npos);setConsoleRuntimeError(nullptr,true);check(!getProp(nullptr,L"RA2.Console.RuntimeError"));
 shared=&storage;storage={};for(int status:{0,1,2,3})for(bool enabled:{false,true})for(unsigned mode:{0u,1u,2u}){storage.sidebarStatus=status;extra.sidebarEnabled=enabled;extra.sidebarMode=mode;hides=shown=0;worldFallback();const bool masked=status==3||(enabled&&mode==1);check(hides==int(masked)&&shown==int(!masked));}
 shared=nullptr;extra={};hides=shown=0;worldFallback();check(!hides&&shown==1);
 std::printf("{\"checks\":%d,\"groups\":15,\"assertions_enabled\":true}\n",checks);
}

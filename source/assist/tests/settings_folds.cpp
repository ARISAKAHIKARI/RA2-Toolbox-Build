#ifdef NDEBUG
#undef NDEBUG
#endif
#define RA2_UNIFIED
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../source/ui_layout.h"
#include "../source/settings_fold_layout.h"
#include "../../suite/source/settings_categories.h"
using namespace assist;
static HWND ui=HWND(1),featureSearch=HWND(2),focused=nullptr;static int activePage=0,scrollY=0,scrollX=0,layouts=0,checks=0;
static std::map<HWND,int>ids;static std::map<int,std::wstring>captions;static std::set<int>openedSettingGroups;
static std::map<HWND,std::wstring>texts;
static int testID(HWND h){return ids[h];}static int testText(HWND h,wchar_t*out,int size){const auto&text=texts[h];const auto n=std::min<size_t>(text.size(),size-1);std::copy_n(text.c_str(),n,out);out[n]=0;return int(n);}
static BOOL testShow(HWND,int){return TRUE;}static BOOL testCaption(HWND,int id,const wchar_t*s){captions[id]=s;return TRUE;}
static HWND testItem(HWND,int id){for(const auto&p:ids)if(p.second==id)return p.first;return nullptr;}
static HWND testFocus(){return focused;}static HWND testSetFocus(HWND h){auto old=focused;focused=h;return old;}
static BOOL testClient(HWND,RECT*r){*r={0,0,760,540};return TRUE;}
#define GetDlgCtrlID testID
#define GetWindowTextW testText
#define ShowWindow testShow
#define SetDlgItemTextW testCaption
#define GetDlgItem testItem
#define GetFocus testFocus
#define SetFocus testSetFocus
#define GetClientRect testClient
namespace modernui {static void tip(HWND,const std::wstring&){};}
static std::wstring controlHelp(int,const std::wstring&){return L"help";}
static void layoutPanel(){++layouts;}
static int logical(int n){return n;}static void scrollTo(int n){scrollY=n;layoutPanel();}static void notify(const wchar_t*){}
// Actual production PageControl, applySettingsFolds, pageBottom, and category
// packing. Win32 window IO alone is replaced; layout code is not duplicated.
#include SETTINGS_FOLD_ROUTES
static void check(bool okay){assert(okay);++checks;}
static void add(int id,int page,int y,int visible,int group=0,int height=0){HWND h=HWND(INT_PTR(ids.size()+10));ids[h]=id;pageControls.push_back({h,page,{20,y,300,height?height:visible},visible,{},group});}
static PageControl&control(int id){return *std::find_if(pageControls.begin(),pageControls.end(),[&](const auto&c){return ids[c.hwnd]==id;});}
int main(){
 add(705,1,0,26);add(9601,1,0,26);add(0,1,40,26,9601);add(9195,1,40,26,9601,240);add(710,1,80,26,9601);
 add(9050,1,120,26);add(9602,1,120,26);add(9056,1,160,26,9602,180);add(9051,1,200,26,9602);
 add(719,1,240,26);add(455,0,0,26);add(456,0,0,26);add(899,4,0,26);
 add(880,2,0,26);add(9603,2,0,26);add(9040,2,40,26,9603);add(9041,2,80,26,9603);add(881,2,120,26,9603);add(804,2,160,26);
 add(476,4,40,26);add(807,4,80,44);add(420,4,120,26);add(9607,4,120,26);add(896,4,160,26);add(421,4,200,26,9607);add(422,4,240,152,9607);add(423,4,240,26,9607);add(424,4,278,26,9607);add(425,4,316,26,9607);add(815,4,410,65);
 add(9604,0,80,26);for(int n=0;n<9;++n)add(870+n,0,120+n*40,26,9604);
 add(9608,0,500,26);add(440,0,540,26,9608,300);add(441,0,540,26,9608);add(467,0,580,26,9608);add(444,0,620,26,9608);add(445,0,660,26,9608);
 add(458,1,280,26);add(9606,1,280,26);add(450,1,320,26,9606);add(787,1,360,26,9606);add(818,1,400,26,9606);add(802,1,440,26,9606,240);
 add(470,2,200,26);add(9605,2,200,26);add(781,2,240,26,9605);add(783,2,280,26,9605);add(782,2,320,26,9605);add(784,2,360,26,9605);
 add(832,4,520,30);add(9609,4,520,26);for(int n=0;n<5;++n)add(9000+n,4,570+n*40,26,9609,240);add(833,4,790,192);
 add(830,3,0,26);add(9300,3,40,26);add(430,-2,0,26);
 finalizeTacticalLayout();check(layouts==1&&openedSettingGroups.empty());
 for(int id:{705,9050,880,719,804,458,470,420,896,815,832,833})check(!control(id).foldHidden);
 for(int id:{9195,710,9056,9051,9040,9041,881,870,878,783,784,450,787,802,421,422,423,425,440,467,9000,9004})check(control(id).foldHidden);
 check(control(9195).rect.h==240&&control(9195).visibleHeight==26);check(control(807).page==suite::categories::Rules);
 for(int id:{455,456,899})check(control(id).page==suite::categories::Automatic);
 check(control(830).page==-3&&control(9300).page==-3);check(control(422).rect.y==control(423).rect.y);check(control(424).rect.y-control(423).rect.y==38);
 std::vector<int>closedY;for(const auto&c:pageControls)closedY.push_back(c.rect.y);const int closedBottom=pageBottom();
 const int automaticY=control(804).rect.y;openedSettingGroups.insert(9601);applySettingsFolds();check(!control(9195).foldHidden&&!control(710).foldHidden&&control(9051).foldHidden);check(control(804).rect.y==automaticY);check(pageBottom()==closedBottom+80);check(captions[9601]==L"收起设置");
 const std::set<int>allGroups={9601,9602,9603,9604,9605,9606,9607,9608,9609};
 openedSettingGroups=allGroups;applySettingsFolds();for(const auto&c:pageControls){check(!c.foldHidden);check(c.rect.y==c.fullRect.y);}check(control(9195).rect.h==240);
 for(int iteration=0;iteration<20;++iteration){openedSettingGroups.clear();applySettingsFolds();for(size_t n=0;n<pageControls.size();++n)check(pageControls[n].rect.y==closedY[n]);openedSettingGroups=allGroups;applySettingsFolds();}
 openedSettingGroups.clear();applySettingsFolds();check(pageBottom()==closedBottom&&captions[9602]==L"展开设置");
 // Overlapping list/button columns remain one row, even with mixed groups.
 auto mixed=settingsfold::layout({{0,100,80,0},{0,100,26,9601},{0,140,26,9601},{0,194,26,0}},{});
 check(!mixed[0].hidden&&mixed[1].hidden&&mixed[2].hidden&&!mixed[3].hidden&&mixed[3].y==194);
 // Actual click handler is generic: every registered group opens on its own,
 // without changing the child text/key/choice contents or any other group.
 for(const auto&c:pageControls)texts[c.hwnd]=L"saved setting "+std::to_wstring(ids[c.hwnd]);const auto saved=texts;
 for(int group:allGroups){check(toggleSettingsFold(group));check(openedSettingGroups==std::set<int>{group});for(const auto&c:pageControls)check(c.foldHidden==(c.fold&&c.fold!=group));check(texts==saved);check(toggleSettingsFold(group)&&openedSettingGroups.empty());}
 const int priorLayouts=layouts;check(!toggleSettingsFold(420)&&layouts==priorLayouts&&openedSettingGroups.empty());
 // Closing while an edit owns focus moves focus to its visible button. A
 // queued old focus notification must not reopen the group after closing.
 check(toggleSettingsFold(9607));focused=control(421).hwnd;check(toggleSettingsFold(9607)&&focused==control(9607).hwnd&&control(421).foldHidden);focusRoute(ui,LPARAM(control(421).hwnd));check(!openedSettingGroups.count(9607));
 focused=control(421).hwnd;focusRoute(ui,LPARAM(focused));check(openedSettingGroups.count(9607)&&!control(421).foldHidden);check(toggleSettingsFold(9607));
 // Actual search branch opens a matching child, but only on the active page.
 activePage=suite::categories::Rules;auto query=control(421).hwnd;texts[query]=L"unique technology permission";texts[featureSearch]=L"TECHNOLOGY PERMISSION";searchRoute();check(openedSettingGroups.count(9607)&&!control(422).foldHidden);check(!control(420).foldHidden&&!control(896).foldHidden&&!control(815).foldHidden);
 check(toggleSettingsFold(9607));activePage=suite::categories::Automatic;searchRoute();check(!openedSettingGroups.count(9607));
 printf("{\"checks\":%d,\"passed\":true,\"groups\":9,\"scope\":\"actual category packing/fold/click/focus/search routes; simulated Win32 windows\"}\n",checks);
}
